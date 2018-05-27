#pragma once

#include <stdint.h>
#include <string.h>
#include <string>
#include <iostream>
#include <map>
#include <Task.h>
#include <mongoose.h>
#include "SmartPtr.h"

#define DEFAULT_MULTIPART_DATA_MAX 1024

class HttpResponse;
class HttpRequest;
class WebServer;
class WebServerHandler;
class ContentProvider;

typedef SmartPtr<std::string> stringPtr;

class WebString : protected mg_str{
protected:
    stringPtr entity;

public:
    WebString(){
	setValue(NULL, 0);
    };
    WebString(const WebString& src) : entity(src.entity){
	if (entity.isNull()){
	    setValue(src.p, src.len);
	}else{
	    setValue(entity->data(), entity->length());
	}
    };
    WebString(const mg_str& str) : mg_str(str){};
    WebString(const char* str, size_t len){
	setValue(str, len);
    };
    WebString(const char* str){
	setValue(str, strlen(str));
    };
    WebString(const stringPtr& ptr) : entity(ptr) {
	if (entity.isNull()){
	    setValue(NULL, 0);
	}else{
	    setValue(entity->data(), entity->length());
	}
    };
    virtual ~WebString(){};

    virtual void setValue(const char* str, size_t len){
	mg_str::p = str;
	mg_str::len = len;
    };

    virtual WebString& operator = (const WebString& src){
	entity = src.entity;
	if (entity.isNull()){
	    setValue(src.p, src.len);
	}else{
	    setValue(entity->data(), entity->length());
	}
	return *this;
    };
    virtual WebString& operator = (const mg_str& src){
	entity.setNull();
	setValue(src.p, src.len);
	return *this;
    };
    virtual WebString& operator = (const char* cstr){
	entity.setNull();
	setValue(cstr, strlen(cstr));
	return *this;
    };
    virtual WebString& operator = (const stringPtr& ptr){
	entity = ptr;
	if (entity.isNull()){
	    setValue(NULL, 0);
	}else{
	    setValue(entity->data(), entity->length());
	}
	return *this;
    };

    size_t length() const {
	return len;
    };
    const char* data() const {
	return p;
    };
    const mg_str* operator ()() const{
	return (const mg_str*)this;
    };

    int compareTo(const char* dest, size_t destLen) const{
	size_t clen = len < destLen ? len : destLen;
	int rc = memcmp(p, dest, clen);
	if (rc == 0){
	    rc = len - destLen;
	}
	return rc;
    };
    int comapreTo(const WebString& dest) const{
	return compareTo(dest.p, dest.len);
    };
    int compareTo(const mg_str& dest) const{
	return compareTo(dest.p, dest.len);
    };
    int compareTo(const char* dest) const{
	size_t dlen = strlen(dest);
	return compareTo(dest, dlen);
    };

    bool isEqual(const char* dest, size_t destLen) const{
	return compareTo(dest, destLen) == 0;
    };
    bool operator == (const WebString& dest) const{
	return comapreTo(dest) == 0;
    };
    bool operator == (const mg_str& dest) const{
	return compareTo(dest) == 0;
    };
    bool operator == (const char* dest) const{
	return compareTo(dest) == 0;
    };

    bool lessThan(const char* dest, size_t destLen) const{
	return compareTo(dest, destLen) < 0;
    };
    bool operator < (const WebString& dest) const{
	return comapreTo(dest) < 0;
    };
    bool operator < (const mg_str& dest) const{
	return compareTo(dest) < 0;
    };
    bool operator < (const char* dest) const{
	return compareTo(dest) < 0;
    };

    bool graterThan(const char* dest, size_t destLen) const{
	return compareTo(dest, destLen) > 0;
    };
    bool operator > (const WebString& dest) const{
	return comapreTo(dest) > 0;
    };
    bool operator > (const mg_str& dest) const{
	return compareTo(dest) > 0;
    };
    bool operator > (const char* dest) const{
	return compareTo(dest) > 0;
    };

    bool lessEqual(const char* dest, size_t destLen)const {
	return compareTo(dest, destLen) <= 0;
    };
    bool operator <= (const WebString& dest) const{
	return comapreTo(dest) <= 0;
    };
    bool operator <= (const mg_str& dest) const{
	return compareTo(dest) <= 0;
    };
    bool operator <= (const char* dest) const{
	return compareTo(dest) <= 0;
    };

    bool graterEqual(const char* dest, size_t destLen) const{
	return compareTo(dest, destLen) >= 0;
    };
    bool operator >= (const WebString& dest) const{
	return comapreTo(dest) >= 0;
    };
    bool operator >= (const mg_str& dest) const{
	return compareTo(dest) >= 0;
    };
    bool operator >= (const char* dest) const{
	return compareTo(dest) >= 0;
    };
};

std::ostream& operator<<(std::ostream& os, const WebString& ws);

class HttpRequest{
public:
    enum Method{
	MethodGet = 0, MethodHead, MethodPost, MethodPut,
	MethodDelete, MethodConnect, MethodOptions, MethodTrase,
	MethodLink, MethodUnlink,
	MethodUnknown
    };
    
protected:
    Method methodCode;
    WebString methodData;
    WebString uriData;
    WebString protocolData;
    WebString queryStringData;
    WebString bodyData;
    std::map<WebString, WebString> headerData;
    std::map<WebString, WebString> parametersData;
    char* parametersBuf;

public:
    HttpRequest():
	methodCode(MethodUnknown), parametersBuf(NULL){};
    virtual ~HttpRequest(){
	delete[] parametersBuf;
    };


    void reset(http_message* msg, bool copy = false);
    void addParameter(const WebString& key, const WebString& value);

    const WebString& body() const{
	return bodyData;
    };

    Method method() const{
	return methodCode;
    };
    const WebString& methodString() const{
	return methodData;
    };
    const WebString& uri() const{
	return uriData;
    };
    const WebString& protocol() const{
	return protocolData;
    };
    const WebString& queryString() const{
	return queryStringData;
    };
    const std::map<WebString, WebString>& header() const{
	return headerData;
    };
    const std::map<WebString, WebString>& parameters() const{
	return parametersData;
    };

protected:
    void setParameters(const mg_str* src);
};

class HttpResponse {
public:
    enum Status{
	ST_OPEN, ST_CLOSE, ST_FLUSHED,
    };

    enum HttpStatus{
	RESP_100_Continue,
	RESP_101_SwitchingProtocols,
	RESP_200_OK,
	RESP_201_Created,
	RESP_202_Accepted,
	RESP_203_Non_AuthoritativeInformation,
	RESP_204_NoContent,
	RESP_205_ResetContent,
	RESP_206_PartialContent,
	RESP_300_MultipleChoices,
	RESP_301_MovedPermanently,
	RESP_302_Found,
	RESP_303_SeeOther,
	RESP_304_NotModified,
	RESP_305_UseProxy,
	RESP_307_TemporaryRedirect,
	RESP_400_BadRequest,
	RESP_401_Unauthorized,
	RESP_402_PaymentRequired,
	RESP_403_Forbidden,
	RESP_404_NotFound,
	RESP_405_MethodNotAllowed,
	RESP_406_NotAcceptable,
	RESP_407_ProxyAuthenticationRequired,
	RESP_408_RequestTimeout,
	RESP_409_Conflict,
	RESP_410_Gone,
	RESP_411_LengthRequired,
	RESP_412_PreconditionFailed,
	RESP_413_RequestEntityTooLarge,
	RESP_414_Request_URITooLong,
	RESP_415_UnsupportedMediaType,
	RESP_416_RequestedRangeNotSatisfiable,
	RESP_417_ExpectationFailed,
	RESP_500_InternalServerError,
	RESP_501_NotImplemented,
	RESP_502_BadGateway,
	RESP_503_ServiceUnavailable,
	RESP_504_GatewayTimeout,
	RESP_505_HTTPVersionNotSupported
    };
    
protected:
    Status status;

    HttpStatus httpStatus;
    std::map<WebString, WebString> headerData;
    WebString bodyData;

public:
    HttpResponse() : status(ST_OPEN){};
    virtual ~HttpResponse(){};

    void reset();
    void close(Status status = ST_CLOSE);
    void flush(mg_connection* con);
    Status getStatus(){
	return status;
    };

    void setHttpStatus(HttpStatus stat){
	httpStatus = stat;
    };

    void addHeader(const WebString& key, const WebString& value){
	headerData[key] = value;
    };

    void setBody(const WebString& body){
	bodyData = body;
    };
    
};

class WebServerConnection {
protected:
    enum Status {
	CON_INIT, CON_MPART_INIT, CON_MPART_DATA, CON_COMPLETE
    };
    Status status;
    const WebServer* server;
    mg_connection* connection;
    HttpRequest requestData;
    HttpResponse responseData;
    WebServerHandler* handler;
    void* userContext;

public:
    WebServerConnection(WebServer* server, mg_connection* src){
	status = CON_INIT;
	this->server = server;
	connection = src;
	userContext = NULL;
    };
    virtual ~WebServerConnection(){};

    HttpRequest* request(){
	return &requestData;
    };

    HttpResponse* response(){
	return &responseData;
    };

    void setUserContext(void* context){
	userContext = context;
    };

    void* getUserContext(){
	return userContext;
    };
    
    void dispatchEvent(int ev, void* p);

protected:
    void makeFileResponse();
    bool authenticate(http_message* hm);
};

class WebServer : protected Task, mg_mgr {
protected:
    const  ContentProvider* contentProvider;
    mg_connection* listener;
    std::map<WebString, WebServerHandler*> matchHandlers;
    std::map<WebString, WebServerHandler*,
	     std::greater<WebString> > prefixHandlers;
    struct HTDIGEST{
	FILE* fp;
	const char* domain;
    } htdigest;
    
public:
    WebServer();
    virtual ~WebServer();

    void setContentProvider(const ContentProvider* provider){
	contentProvider = provider;
    };
    const ContentProvider* getContentProvider() const{
	return contentProvider;
    };
    void setHandler(WebServerHandler* handler,
		    const char* path, bool exactMatch = true);
    WebServerHandler* findHandler(const WebString& path) const;

    void setHtdigest(FILE* fp, const char* domain){
	htdigest.fp = fp;
	htdigest.domain = domain;
    };
    const HTDIGEST* getHtdigest() const{
	return &htdigest;
    };
    
    bool startServer(const char* address);

protected:
    static void handler(struct mg_connection* con, int ev, void* p);
    void run(void *data) override;
};

class WebServerHandler {
public:
    WebServerHandler(){};
    virtual ~WebServerHandler(){};

    virtual bool needDigestAuthentication(){return false;};

    virtual void recieveRequest(WebServerConnection& connection);

    virtual void beginMultipart(WebServerConnection& connection);
    virtual void endMultipart(WebServerConnection& connection);

    virtual void beginMultipartData(WebServerConnection& connection,
				    const char* key);
    virtual void updateMultipartData(WebServerConnection& connection,
				     const char* key,
				     const void* data, size_t length);
    virtual void endMultipartData(WebServerConnection& connection);
};

class ContentProvider {
public:
    virtual WebString getContent(const WebString& path) const = 0;
};

const char* findMimeType(const WebString& path);
