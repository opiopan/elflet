#include <esp_log.h>
#include <esp_system.h>
#include <GeneralUtils.h>
#include <json11.hpp>
#include <iostream>
#include <sstream>
#include "webserver.h"
#include "Config.h"
#include "IRService.h"
#include "irserverProtocol.h"
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

static void replyRecievedData(HttpRequest* req, HttpResponse* resp){
    std::stringstream body;
    bool rc = false;
    auto params = req->parameters();
    if (params["type"] == "raw"){
	rc = getIRRecievedDataRawJson(body);
    }else{
	rc = getIRRecievedDataJson(body);
    }
    
    if (rc){
	resp->setHttpStatus(HttpResponse::RESP_200_OK);
	resp->addHeader("Content-Type", "application/json");
	stringPtr bodyStr(new std::string(body.str()));
	resp->setBody(bodyStr);
    }else{
	resp->setHttpStatus(HttpResponse::RESP_204_NoContent);
    }
    resp->close();
}

static bool sendFormatedData(const json11::Json& obj){
    auto protocol = obj[JSON_PROTOCOL];
    auto protocolValue = IRRC_UNKNOWN;
    if (protocol.is_string()){
	auto value = protocol.string_value();
	if (value == "NEC"){
	    protocolValue = IRRC_NEC;
	}else if (value == "AEHA"){
	    protocolValue = IRRC_AEHA;
	}else if (value == "SONY"){
	    protocolValue = IRRC_SONY;
	}
    }

    auto data = obj[JSON_DATA];
    char dataStream[IRS_REQMAXSIZE];
    auto dataLength = -1;
    if (data.is_string()){
	auto value = data.string_value();
	if (value.length() & 1 || value.length() > sizeof(dataStream) * 2){
	    return false;
	}
	dataLength = value.length() / 2;

	auto hex = [](int c) -> int {
	    if (c >= '0' && c <= '9'){
		return c - '0';
	    }else if (c >= 'a' && c <= 'f'){
		return c - 'a' + 0xa;
	    }else if (c >= 'A' && c <= 'F'){
		return c - 'A' + 0xa;
	    }
	    return -1;
	};

	for (auto i = 0; i < value.length(); i += 2){
	    auto lh = hex(value[i]);
	    auto sh = hex(value[i + 1]);
	    if (lh < 0 || sh < 0){
		return false;
	    }
	    dataStream[i / 2] = (lh << 4) | sh;
	}
    }

    if (protocolValue == IRRC_UNKNOWN || dataLength <= 0){
	return false;
    }

    auto bitCount = obj[JSON_BITCOUNT];
    auto bitCountValue = 0;
    if (bitCount.is_number()){
	bitCountValue = bitCount.int_value();
    }
    if (bitCountValue <= 0){
	bitCountValue = dataLength * 8;
    }

    sendIRData(protocolValue, bitCountValue, (uint8_t*)dataStream);
    
    return true;
}

static void sendData(HttpRequest* req, HttpResponse* resp){
    if (!(req->header("Content-Type") == "application/json")){
	resp->setHttpStatus(HttpResponse::RESP_400_BadRequest);
	resp->close();
	return;
    }

    std::string err;
    auto body = json11::Json::parse(
	std::string(req->body().data(), req->body().length()),
	err);

    if (body.is_object()){
	auto formatedData = body[JSON_FORMATED];
	bool rc = false;
	if (formatedData.is_object()){
	    rc = sendFormatedData(json11::Json(formatedData.object_items()));
	}else{
	    rc = sendFormatedData(body);
	}
	if (rc){
	    resp->setHttpStatus(HttpResponse::RESP_200_OK);
	    resp->close();
	    return;
	}
    }

    resp->setHttpStatus(HttpResponse::RESP_400_BadRequest);
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
	}else if (req->uri() == "/irrc/send" &&
	    req->method() == HttpRequest::MethodPost){
	    sendData(req, resp);
	}
    };
};

void registerIrRESTHandler(WebServer* server){
    server->setHandler(new IrHandler, "/irrc/", false);
}
