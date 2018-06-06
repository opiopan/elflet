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
static const std::string JSON_RAW = "RawIRStream";
static const std::string JSON_LEVEL = "Level";
static const std::string JSON_DURATION = "Duration";

static void replyRecievedDataRaw(HttpRequest* req, HttpResponse* resp){
    const rmt_item32_t* items;
    int32_t itemNum;
    getIRRecievedDataRaw(&items, &itemNum);

    std::stringstream body;
    body << "{\"" << JSON_RAW << "\":[";
    for (int i = 0; i < itemNum; i++){
	if (i > 0){
	    body << ",";
	}
	body << "{\"" << JSON_LEVEL << "\":1,\""
	     << JSON_DURATION << "\":" << items[i].duration0 << "},{\""
	     << JSON_LEVEL << "\":0,\""
	     << JSON_DURATION << "\":" << items[i].duration1 << "}";
    }
    body << "]}";

    stringPtr bodyStr(new std::string(body.str()));
    resp->setBody(bodyStr);
    resp->close();
}

static void replyRecievedData(HttpRequest* req, HttpResponse* resp){
    uint8_t buf[32];
    int32_t bits = sizeof(buf) * 8;
    IRRC_PROTOCOL protocol;
    if (getIRRecievedData(&protocol, &bits, buf)){
	resp->setHttpStatus(HttpResponse::RESP_200_OK);
	resp->addHeader("Content-Type", "application/json");

	auto params = req->parameters();
	if (params["type"] == "raw"){
	    replyRecievedDataRaw(req, resp);
	    return;
	}
	
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
	}else if (req->uri() == "/irrc/startReciever" &&
	    req->method() == HttpRequest::MethodGet){
	    startIRReciever();
	    resp->setHttpStatus(HttpResponse::RESP_200_OK);
	    resp->close();
	}
    };
};

void registerIrRESTHandler(WebServer* server){
    server->setHandler(new IrHandler, "/irrc/", false);
}
