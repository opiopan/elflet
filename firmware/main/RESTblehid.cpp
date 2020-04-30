#include <esp_log.h>
#include <esp_system.h>
#include <GeneralUtils.h>
#include <json11.hpp>
#include <iostream>
#include <sstream>
#include "webserver.h"
#include "Config.h"
#include "BleHidJson.h"
#include "REST.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "RESTblehid";

static void sendData(const WebString& data){
    std::string err;
    auto body =
        json11::Json::parse(std::string(data.data(), data.length()), err);

    if (body.is_object()){
        auto code = bleHidJsonToData(body);
        
        if (code.type == BLEHID_KEYCODE){
            ESP_LOGI(tag, "accept %d bytes key value : duration [%d]",
                     code.data.keycode.keynum, code.duration);
        }else{
            ESP_LOGI(tag, "accept consumer value [%d] : duration [%d]",
                     code.data.consumercode, code.duration);
        }

        bleHidSendCode(&code);
    }
}

class BleHidHandler : public WebServerHandler {
    void receiveRequest(WebServerConnection& connection) override{
        auto req = connection.request();
        auto resp = connection.response();

        if (req->method() != HttpRequest::MethodPost ||
            !(req->header("Content-Type") == "application/json")){
            resp->setHttpStatus(HttpResponse::RESP_400_BadRequest);
            resp->close();
            return;
        }

        sendData(req->body());

        resp->setHttpStatus(HttpResponse::RESP_200_OK);
        resp->close();
    };
};

void registerBleHidRESTHandler(WebServer* server){
    server->setHandler(new BleHidHandler, "/blehid");
}
