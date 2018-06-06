#include <esp_log.h>
#include <esp_system.h>
#include <GeneralUtils.h>
#include <json11.hpp>
#include <iostream>
#include <sstream>
#include "webserver.h"
#include "Config.h"
#include "IRService.h"
#include "REST.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "RESTir";

static const std::string JSON_FORMATED = "FormatedIRStream";
static const std::string JSON_PROTOCOL = "Protocol";
static const std::string JSON_BITCOUNT = "BitCount";
static const std::string JSON_DATA = "Data";

static void replyRecievedData(HttpRequest* req, HttpResponse* resp){
    uint8_t buf[32];
    int32_t bits = sizeof(buf) * 8;
    IRRC_PROTOCOL protocol;
    if (getIRRecievedData(&protocol, &bits, buf)){
	resp->setHttpStatus(HttpResponse::RESP_200_OK);
	resp->addHeader("Content-Type", "application/json");
	if (protocol == IRRC_UNKNOWN){
	    resp->setBody("{\"Protocol\" : \"UNKNOWN\"}");
	}else{
	    char hexstr[65];
	    for (int i = 0; i < ((bits + 7) / 8) * 2; i++){
		int data = (buf[i/2] >> (i & 1 ? 0 : 4)) & 0xf;
		static const unsigned char dic[] = "0123456789abcdef";
		hexstr[i] = dic[data];
	    }
	    hexstr[((bits + 7) / 8) * 2] = 0;
		    
	    std::stringstream body;
	    body << "{\"" << JSON_FORMATED << "\":{\""
		 << JSON_PROTOCOL << "\":\""
		 << (protocol == IRRC_NEC ? "NEC" :
		     protocol == IRRC_AEHA ? "AEHA" : "SONY")
		 << "\",\"" << JSON_BITCOUNT << "\":" << bits << ",\""
		 << JSON_DATA << "\":\"" << hexstr << "\"}}";
	    stringPtr bodyStr(new std::string(body.str()));
	    resp->setBody(bodyStr);
	}
    }else{
	resp->setHttpStatus(HttpResponse::RESP_204_NoContent);
    }
    resp->close();
}

class IrHandler : public WebServerHandler {
    void recieveRequest(WebServerConnection& connection) override{
	auto req = connection.request();
	auto resp = connection.response();

	if (req->uri() == "/irrc/recievedData" &&
	    req->method() == HttpRequest::MethodGet){
	    replyRecievedData(req, resp);
	}
    };
};

void registerIrRESTHandler(WebServer* server){
    server->setHandler(new IrHandler, "/irrc/", false);
}
