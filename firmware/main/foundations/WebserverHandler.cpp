#include <esp_log.h>
#include <iostream>
#include <sstream>
#include "webserver.h"

#include "sdkconfig.h"

static const char* tag = "WebServerHandler";

//----------------------------------------------------------------------
//  handle comlete request message
//     nothing to do on default
//----------------------------------------------------------------------
void WebServerHandler::recieveRequest(WebServerConnection& connection){
}


//----------------------------------------------------------------------
//  handle multipart streaming
//     translate multipart data to parametaer map object to be enable
//     handling as same as case of x-www-form-urlencoded transparency
//----------------------------------------------------------------------
class MultipartContext {
public:
    WebString key;
    stringPtr value;
};

void WebServerHandler::beginMultipart(WebServerConnection& connection){
    auto context = new MultipartContext;
    connection.setUserContext(context);
}

void WebServerHandler::endMultipart(WebServerConnection& connection){
    auto context = (MultipartContext*)connection.getUserContext();
    delete context;
    connection.setUserContext(NULL);
}

void WebServerHandler::beginMultipartData(WebServerConnection& connection,
					  const char* key){
    auto context = (MultipartContext*)connection.getUserContext();
    auto keyPtr = stringPtr(new std::string(key));
    context->key = keyPtr;
    context->value = stringPtr(new std::string());;
}

void WebServerHandler::updateMultipartData(WebServerConnection& connection,
					   const char* key,
					   const void* data, size_t length){
    auto context = (MultipartContext*)connection.getUserContext();
    if (context->value->length() + length > DEFAULT_MULTIPART_DATA_MAX){
	connection.response()->setHttpStatus(
	    HttpResponse::RESP_500_InternalServerError);
	connection.response()->addHeader(
	    WebString("Content-Type"), WebString("text/plain"));
	std::stringstream body;
	body << "parameter \"" << key << "\" is too long";
	WebString bodyStr = stringPtr(new std::string(body.str()));
	connection.response()->setBody(bodyStr);
	connection.response()->close();
    }else{
	context->value->append((const char*)data, length);
    }
}

void WebServerHandler::endMultipartData(WebServerConnection& connection){
    auto context = (MultipartContext*)connection.getUserContext();
    connection.request()->addParameter(context->key,
				       WebString(context->value));
}
