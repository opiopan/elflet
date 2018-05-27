#include <esp_log.h>
#include <iostream>
#include <sstream>
#include "webserver.h"

#include "sdkconfig.h"

static const char* tag = "WebServer";

//----------------------------------------------------------------------
// WebString helper functions
//----------------------------------------------------------------------
std::ostream& operator<<(std::ostream& os, const WebString& ws){
    os.write(ws.data(), ws.length());
    return os;
}

//----------------------------------------------------------------------
// HttpRequest imprementation
//----------------------------------------------------------------------
static const mg_str methodStrings[] = {
    MG_MK_STR("GET"),
    MG_MK_STR("HEAD"),
    MG_MK_STR("POST"),
    MG_MK_STR("PUT"),
    MG_MK_STR("DELETE"),
    MG_MK_STR("CONNECT"),
    MG_MK_STR("OPTIONS"),
    MG_MK_STR("TRASE"),
    MG_MK_STR("LINK"),
    MG_MK_STR("UNLINK")
};

static WebString webStringWithCopy(const mg_str& src){
    return WebString(stringPtr(new std::string(src.p, src.len)));
}

static WebString webStringWithoutCopy(const mg_str& src){
    return WebString(src);
};

void HttpRequest::reset(http_message* msg, bool copy){
    ESP_LOGI(tag, "HttpRequest::reset");
    headerData.clear();
    parametersData.clear();
    delete[] parametersBuf;
    parametersBuf = NULL;
    methodCode = MethodUnknown;

    if (msg == NULL){
	return;
    }

    auto genstr = copy ? webStringWithCopy : webStringWithoutCopy;
    methodData = genstr(msg->method);
    uriData = genstr(msg->uri);
    protocolData = genstr(msg->proto);
    queryStringData = genstr(msg->query_string);
    if (copy){
	bodyData = "";
    }else{
	bodyData = genstr(msg->body);;
    }

    WebString method = WebString(msg->method);
    for (int i = 0; i < MethodUnknown; i++){
	if (method == methodStrings[i]){
	    methodCode = (Method)i;
	    break;
	}
    }

    for (int i = 0;
	 i < sizeof(msg->header_names) / sizeof(*msg->header_names); i++){
	if (msg->header_names[i].p == NULL){
	    break;
	}
	headerData[genstr(msg->header_names[i])] =
	    genstr(msg->header_values[i]);
    }

    const WebString ctype = headerData[WebString("Content-Type")];
    if (methodCode == MethodGet && msg->query_string.len > 0){
	setParameters(&(msg->query_string));
    }else if (ctype == "application/x-www-form-urlencoded"){
	setParameters(&msg->body);
    }
}

void HttpRequest::addParameter(const WebString& key, const WebString& value){
    parametersData[key] = value;
}

void HttpRequest::setParameters(const mg_str* src) {
    parametersBuf = new char[src->len + 1];
    int begin = 0;
    for (int end = 0; end < src->len + 1; end++){
	if (end == src->len || src->p[end] == '&'){
	    int len = mg_url_decode(src->p + begin, end - begin,
				    parametersBuf + begin, end -begin + 1,
				    true);
	    if (len > 0){
		for (int separator = begin; separator < end; separator++){
		    if (parametersBuf[separator] == '='){
			const auto key =
			    WebString(parametersBuf + begin,
				      separator - begin);
			const auto value =
			    WebString(parametersBuf + separator + 1,
				      end - separator - 1);
			parametersData[key] = value;
		    }
		}
	    }
	    begin = end + 1;
	}
    }
}

//----------------------------------------------------------------------
// HttpResponse imprementation
//----------------------------------------------------------------------
static const char* httpStatusStr[] = {
    "100 Continue",
    "101 Switching Protocols",
    "200 OK",
    "201 Created",
    "202 Accepted",
    "203 Non-Authoritative Information",
    "204 No Content",
    "205 ResetContent",
    "206 Partial Content",
    "300 Multiple Choices",
    "301 Moved Permanently",
    "302 Found",
    "303 See Other",
    "304 Not Modified",
    "305 Use Proxy",
    "307 Temporary Redirect",
    "400 Bad Request",
    "401 Unauthorized",
    "402 Payment Required",
    "403 Forbidden",
    "404 Not Found",
    "405 Method Not Allowed",
    "406 Not Acceptable",
    "407 Proxy Authentication Required",
    "408 Request Timeout",
    "409 Conflict",
    "410 Gone",
    "411 Length Required",
    "412 Precondition Failed",
    "413 Request Entity Too Large",
    "414 Request-URI Too Long",
    "415 Unsupported Media Type",
    "416 Requested Range Not Satisfiable",
    "417 Expectation Failed",
    "500 Internal Server Error",
    "501 Not Implemented",
    "502 Bad Gateway",
    "503 Service Unavailable",
    "504 Gateway Timeout",
    "505 HTTP Version Not Supported"
};

void HttpResponse::reset(){
    status = ST_OPEN;
    httpStatus = RESP_200_OK;
    headerData.clear();
    bodyData = "";
}

void HttpResponse::close(Status status){
    this->status = status;
}

void HttpResponse::flush(mg_connection* con){
    if (status == ST_OPEN){
	ESP_LOGE(tag, "HTTP response object is not closed");
    }
    if (status != ST_FLUSHED){
	std::stringstream resp;
	resp << "HTTP/1.0 " << httpStatusStr[httpStatus] << std::endl;
	for (auto i = headerData.begin(); i != headerData.end(); i++){
	    const WebString key = i->first;
	    resp << key << ": " << i->second << "\r\n";
	}
	resp << "\r\n";
	mg_send(con, resp.str().data(), resp.str().length());	
	if (bodyData.length() > 0){
	    mg_send(con, bodyData.data(), bodyData.length());
	}
    }
    status = ST_FLUSHED;
}

//----------------------------------------------------------------------
// WebServerConnection imprementation
//----------------------------------------------------------------------
void WebServerConnection::dispatchEvent(int ev, void* p){
    switch (ev){
    case MG_EV_HTTP_REQUEST: {
	ESP_LOGI(tag, "EV_HTTP_REQUEST");
	auto hm = (http_message*)p;
	requestData.reset(hm, false);
	responseData.reset();
	handler = server->findHandler(requestData.uri());
	status = CON_COMPLETE;
	authenticate(hm);
	break;
    }
    case MG_EV_HTTP_MULTIPART_REQUEST: {
	ESP_LOGI(tag, "EV_HTTP_MULTIPART_REQUEST");
	status = CON_MPART_INIT;
	auto hm = (http_message*)p;
	requestData.reset(hm, true);
	responseData.reset();
	handler = server->findHandler(requestData.uri());
	if (!authenticate(hm)){
	    status = CON_COMPLETE;
	    break;
	}
	if (handler){
	    handler->beginMultipart(*this);
	    if (responseData.getStatus() == HttpResponse::ST_CLOSE){
		status = CON_COMPLETE;
		break;
	    }
	    auto header = requestData.header();
	    if (header[WebString("Expect")] == "100-continue"){
		mg_printf(connection, "HTTP/1.1 100 continue\r\n\r\n");
	    }
	}else if (requestData.method() != HttpRequest::MethodGet){
	    // call makeFileResponse() to build 404 response
	    makeFileResponse();
	    status = CON_COMPLETE;
	}
	break;
    }
    case MG_EV_HTTP_MULTIPART_REQUEST_END:{
	ESP_LOGI(tag, "EV_HTTP_MULTIPART_REQUEST_END");
	if (handler){
	    handler->endMultipart(*this);
	}
	status = CON_COMPLETE;
	break;
    }
    case MG_EV_HTTP_PART_BEGIN: {
	ESP_LOGI(tag, "EV_HTTP_PART_BEGIN");
	if (status == CON_COMPLETE){
	    break;
	}
	status = CON_MPART_DATA;
	if (handler && responseData.getStatus() == HttpResponse::ST_OPEN){
	    auto mp = (mg_http_multipart_part*)p;
	    handler->beginMultipartData(*this, mp->var_name);
	}
	break;
    }
    case MG_EV_HTTP_PART_DATA: {
	ESP_LOGI(tag, "EV_HTTP_PART_DATA");
	if (status == CON_COMPLETE){
	    break;
	}
	if (handler && responseData.getStatus() == HttpResponse::ST_OPEN){
	    auto mp = (mg_http_multipart_part*)p;
	    handler->updateMultipartData(
		*this, mp->var_name, mp->data.p, mp->data.len);
	}
	break;
    }
    case MG_EV_HTTP_PART_END: {
	ESP_LOGI(tag, "EV_HTTP_PART_END");
	if (handler && responseData.getStatus() == HttpResponse::ST_OPEN){
	    handler->endMultipartData(*this);
	}
	if (status != CON_COMPLETE){
	    status = CON_MPART_INIT;
	}
	break;
    }
    }

    if (status == CON_COMPLETE &&
	responseData.getStatus() != HttpResponse::ST_FLUSHED){
	ESP_LOGI(tag, "proceed http response");
	if (responseData.getStatus() == HttpResponse::ST_OPEN){
	    if (handler){
		handler->recieveRequest(*this);
	    }else{
		makeFileResponse();
	    }
	}
	if (responseData.getStatus() == HttpResponse::ST_OPEN){
	    responseData.setHttpStatus(
		HttpResponse::RESP_501_NotImplemented);
	    responseData.close();
	}
	responseData.flush(connection);
	connection->flags |= MG_F_SEND_AND_CLOSE;
    }
}

void WebServerConnection::makeFileResponse(){
    auto provider = server->getContentProvider();
    if (requestData.method() == HttpRequest::MethodGet && provider){
	auto content = provider->getContent(requestData.uri());
	if (content.length() > 0){
	    responseData.setHttpStatus(HttpResponse::RESP_200_OK);
	    responseData.setBody(content);
	    auto type = findMimeType(requestData.uri());
	    responseData.addHeader(WebString("Contet-Type"), type);
	    responseData.close();
	}
    }
    if (responseData.getStatus() == HttpResponse::ST_OPEN){
	responseData.setHttpStatus(HttpResponse::RESP_404_NotFound);
	responseData.addHeader(WebString("Content-Type"),
			       WebString("text/html"));
	responseData.setBody(WebString("<h1>Not Found</h1>\n"));
	responseData.close();
    }
}

bool WebServerConnection::authenticate(http_message* hm){
    if (handler && handler->needDigestAuthentication()){
	auto htdigest = server->getHtdigest();
	fseek(htdigest->fp, 0, SEEK_SET);
	if (!mg_http_check_digest_auth(hm, htdigest->domain,
				       htdigest->fp)){
	    ESP_LOGE(tag, "authorize failed");
	    mg_http_send_digest_auth_request(connection, htdigest->domain);
	    responseData.close(HttpResponse::ST_FLUSHED);
	    connection->flags |= MG_F_SEND_AND_CLOSE;;
	    return false;
	}
    }
    return true;
}

//----------------------------------------------------------------------
// WebServer imprementation
//----------------------------------------------------------------------
WebServer::WebServer() : contentProvider(NULL), listener(NULL){
    htdigest.fp = NULL;
    htdigest.domain = NULL;
}

WebServer::~WebServer(){
}

void WebServer::setHandler(WebServerHandler* handler,
			   const char* path, bool exactMatch){
    stringPtr pathPtr = stringPtr(new std::string(path));
    
    if (exactMatch){
	matchHandlers[WebString(pathPtr)] = handler;
    }else{
	prefixHandlers[WebString(pathPtr)] = handler;
    }
}

WebServerHandler* WebServer::findHandler(const WebString& path) const{
    auto im = matchHandlers.find(path);
    if (im != matchHandlers.end()){
	return im->second;
    }

    auto ipe = prefixHandlers.find(path);
    if (ipe != prefixHandlers.end()){
	return ipe->second;
    }

    auto ipr = prefixHandlers.upper_bound(path);
    if (ipr != prefixHandlers.end() &&
	ipr->first.length() <= path.length() &&
	strncmp(ipr->first.data(), path.data(), ipr->first.length()) == 0){
	return ipr->second;
    }
    
    return NULL;
}

bool WebServer::startServer(const char* address){
    mg_mgr_init(this, this);
    listener = mg_bind(this, address, handler);
    if (listener == NULL){
	ESP_LOGE(tag, "Error setting up listener");
	return false;
    }
    mg_set_protocol_http_websocket(listener);
    this->start();

    return true;
}

void WebServer::handler(struct mg_connection* con, int ev, void* p){
    WebServer* self = (WebServer*)con->mgr->user_data;

    switch (ev){
    case MG_EV_ACCEPT: {
	char addr[32];
	mg_sock_addr_to_str(&con->sa, addr, sizeof(addr),
			    MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
	ESP_LOGI(tag, "Connection %p from %s", con, addr);

	auto wcon = new WebServerConnection(self, con);
	con->user_data = wcon;
	break;
    }
    case MG_EV_CLOSE: {
	ESP_LOGI(tag, "Connection %p closed", con);
	auto wcon = (WebServerConnection*)con->user_data;
	delete wcon;
	con->user_data = NULL;
	break;
    }
    case MG_EV_HTTP_REQUEST:
    case MG_EV_HTTP_MULTIPART_REQUEST:
    case MG_EV_HTTP_MULTIPART_REQUEST_END:
    case MG_EV_HTTP_PART_BEGIN:
    case MG_EV_HTTP_PART_DATA:
    case MG_EV_HTTP_PART_END:{
	auto wcon = (WebServerConnection*)con->user_data;
	wcon->dispatchEvent(ev, p);
	break;
    }
    }
}

void WebServer::run(void *data){
    while (1) {
	mg_mgr_poll(this, 1000);
    }
}
