#include <esp_log.h>
#include <esp_system.h>
#include <GeneralUtils.h>
#include <json11.hpp>
#include <iostream>
#include <sstream>
#include "webserver.h"
#include "Config.h"
#include "ShadowDevice.h"
#include "REST.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "RESTshadow";

//----------------------------------------------------------------------
// utilities
//----------------------------------------------------------------------
static std::string nameFromUri(const WebString& uri, size_t offset){
    auto begin = uri.data() + offset;
    int i;
    for (i = 0; i < uri.length() - offset; i++){
	if (begin[i] == '/'){
	    return std::move(std::string());
	}
    }
    return std::move(std::string(begin, i));
}

//----------------------------------------------------------------------
// listing shadow
//----------------------------------------------------------------------
class ListShadowsHandler: public WebServerHandler{
    void recieveRequest(WebServerConnection& connection) override{
	auto req = connection.request();
	auto resp = connection.response();
	resp->setHttpStatus(HttpResponse::RESP_200_OK);
	resp->addHeader("Content-Type", "application/json");

	if (req->method() != HttpRequest::MethodGet){
	    resp->setHttpStatus(HttpResponse::RESP_500_InternalServerError);
	    resp->addHeader("Content-Type", "text/plain");
	    resp->setBody("invalid method");
	}else{
	    std::stringstream out;
	    dumpShadowDeviceNames(out);
	    stringPtr msgPtr(new std::string(std::move(out.str())));
	    resp->setBody(msgPtr);
	}
	resp->close();
    };
};

//----------------------------------------------------------------------
// Shadow status management
//----------------------------------------------------------------------
class ShadowStatusHandler: public WebServerHandler{
    void recieveRequest(WebServerConnection& connection) override{
	auto req = connection.request();
	auto resp = connection.response();
	auto httpStatus = HttpResponse::RESP_200_OK;
	auto contentType = "text/plain";
	auto name = nameFromUri(req->uri(), 8);

	auto getShadowStatus = [&](ShadowDevice* shadow){
	    contentType = "application/json";
	    std::stringstream out;
	    shadow->dumpStatus(out);
	    stringPtr msgPtr(new std::string(std::move(out.str())));
	    resp->setBody(msgPtr);
	};
	auto setShadowStatus =
	    [&](ShadowDevice* shadow, const std::string& body){
	    std::string err;
	    auto status = json11::Json::parse(body, err);
	    if (!status.is_object()){
		httpStatus = HttpResponse::RESP_500_InternalServerError;
		resp->setBody(WebString("Shadow status must "
					"be specified as Json object."));
	    }else if (!shadow->setStatus(status, err)){
		httpStatus = HttpResponse::RESP_500_InternalServerError;
		stringPtr msgPtr(new std::string(std::move(err)));
		resp->setBody(msgPtr);
	    }
	};

	if (!(req->method() == HttpRequest::MethodGet ||
	      req->method() == HttpRequest::MethodPost)){
	    httpStatus = HttpResponse::RESP_500_InternalServerError;
	    resp->setBody("invalid method");
	}else if (name.length() > 0){
	    if (elfletConfig->getIrrcRecieverMode() ==
		Config::IrrcRecieverContinuous){
		auto shadow = findShadowDevice(name);
		if (shadow){
		    if (req->method() == HttpRequest::MethodGet){
			getShadowStatus(shadow);
		    }else{
			if (req->header("Content-Type") == "application/json"){
			    auto body = req->body();
			    setShadowStatus(shadow,
					    std::string(body.data(),
							body.length()));
			}else{
			    httpStatus =
				HttpResponse::RESP_500_InternalServerError;
			    resp->setBody("invalid request body");
			}
		    }
		}else{
		    httpStatus = HttpResponse::RESP_404_NotFound;
		    resp->setBody("Specified shadow does not exist.");
		}
	    }else{
		httpStatus = HttpResponse::RESP_500_InternalServerError;
		resp->setBody("IR reciever must be in continuous mode.");
	    }
	}else{
	    httpStatus = HttpResponse::RESP_404_NotFound;
	    resp->setBody("Invalid shadow name was specified.");
	}

	resp->setHttpStatus(httpStatus);
	resp->addHeader("Content-Type", contentType);
	resp->close();
    };
};


//----------------------------------------------------------------------
// shadow definition management
//----------------------------------------------------------------------
class ShadowDefsHandler : public WebServerHandler {
    bool needDigestAuthentication(HttpRequest& req) override{
	if (req.method() == HttpRequest::MethodGet){
	    return false;
	}else{
	    return elfletConfig->getBootMode() == Config::Normal;
	}
    };
    
    void recieveRequest(WebServerConnection& connection) override{
	auto req = connection.request();
	auto resp = connection.response();
	auto httpStatus = HttpResponse::RESP_200_OK;
	auto contentType = "text/plain";
	auto name = nameFromUri(req->uri(), 12);
	if (name.length() > 0){
	    std::string msg;
	    if (req->method() == HttpRequest::MethodGet){
		//
		// Dump shadow definition
		//
		auto shadow = findShadowDevice(name);
		if (shadow){
		    contentType = "application/json";
		    std::stringstream out;
		    shadow->serialize(out);
		    stringPtr msgPtr(new std::string(std::move(out.str())));
		    resp->setBody(msgPtr);
		}else{
		    httpStatus = HttpResponse::RESP_404_NotFound;
		    resp->setBody("Specified shadow does not exist.");
		}
	    }else if (req->method() == HttpRequest::MethodPost &&
		req->header("Content-Type") == "application/json"){
		//
		// Add shadow
		//
		auto body = req->body();
		if (!addShadowDevice(name,
				     std::string(body.data(), body.length()),
				     msg)){
		    httpStatus = HttpResponse::RESP_500_InternalServerError;
		    stringPtr msgPtr(new std::string(std::move(msg)));
		    resp->setBody(msgPtr);
		}
	    }else if (req->method() == HttpRequest::MethodDelete){
		//
		// Delete shadow
		//
		if (!deleteShadowDevice(name)){
		    httpStatus = HttpResponse::RESP_404_NotFound;
		    resp->setBody("Specified shadow does not exist.");
		}
	    }else{
		httpStatus = HttpResponse::RESP_500_InternalServerError;
		resp->setBody("invalid request body or method");
	    }
	}else{
	    httpStatus = HttpResponse::RESP_404_NotFound;
	    resp->setBody("Invalid shadow name was specified.");
	}
	resp->setHttpStatus(httpStatus);
	resp->addHeader("Content-Type", contentType);
	resp->close();
    }
};

//----------------------------------------------------------------------
// initialize handlers
//----------------------------------------------------------------------
void registerShadowRESTHandler(WebServer* server){
    auto listHandler = new ListShadowsHandler;
    server->setHandler(listHandler, "/shadowDefs");
    server->setHandler(listHandler, "/shadow");
    server->setHandler(new ShadowDefsHandler, "/shadowDefs/", false);
    server->setHandler(new ShadowStatusHandler, "/shadow/", false);
}
