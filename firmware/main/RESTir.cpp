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

static void replyReceivedData(HttpRequest* req, HttpResponse* resp){
    std::stringstream body;
    bool rc = false;
    auto params = req->parameters();
    if (params["type"] == "raw"){
        rc = getIRReceivedDataRawJson(body);
    }else{
        rc = getIRReceivedDataJson(body);
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

static void sendData(HttpRequest* req, HttpResponse* resp){
    if (!(req->header("Content-Type") == "application/json")){
        resp->setHttpStatus(HttpResponse::RESP_400_BadRequest);
        resp->close();
        return;
    }

    if (sendIRDataJson(req->body())){
        resp->setHttpStatus(HttpResponse::RESP_200_OK);
    }else{
        resp->setHttpStatus(HttpResponse::RESP_400_BadRequest);
    }
    resp->close();
}

class IrHandler : public WebServerHandler {
    void receiveRequest(WebServerConnection& connection) override{
        auto req = connection.request();
        auto resp = connection.response();

        if (req->uri() == "/irrc/receivedData" &&
            req->method() == HttpRequest::MethodGet){
            replyReceivedData(req, resp);
        }else if (req->uri() == "/irrc/startReceiver" &&
            req->method() == HttpRequest::MethodGet){
            startIRReceiver();
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
