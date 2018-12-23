#include <esp_log.h>
#include <esp_system.h>
#include <GeneralUtils.h>
#include <json11.hpp>
#include <iostream>
#include <sstream>
#include "webserver.h"
#include "Config.h"
#include "BleHidService.h"
#include "REST.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "RESTblehid";

static const char JSON_KEYCODES[] = "KeyCodes";
static const char JSON_SPECHIAL_MASK[] = "SpecialKeyMask";
static const char JSON_CONSUMER[] = "ConsumerCode";
// static const char JSON_DURATION[] = "Duration";

#define MAX_KEYCODE_NUM 6

static void sendData(const WebString& data){
    std::string err;
    auto body =
	json11::Json::parse(std::string(data.data(), data.length()), err);

    if (body.is_object()){
	auto keyCodes = body[JSON_KEYCODES];
	auto special = body[JSON_SPECHIAL_MASK];
	auto consumer = body[JSON_CONSUMER];
	auto duration = body[JSON_DURATION];

	uint8_t buf[MAX_KEYCODE_NUM];
	
	if (keyCodes.is_array()){
	    int i = 0;
	    for (auto &code : keyCodes.array_items()){
		if (i >= MAX_KEYCODE_NUM || !code.is_number()){
		    break;
		}
		buf[i] = code.int_value() & 0xff;
		i++;
	    }
	    if (i > 0){
		auto dvalue =
		    duration.is_number() ? duration.int_value() & 0xff : 100;
		ESP_LOGI(tag, "accept %d bytes key value : duration [%d]",
			 i, dvalue);
		bleHidSendKeyValue(
		    special.is_number() ? special.int_value() & 0xff : 0,
		    buf, i, dvalue);
	    }
	}else if (consumer.is_number()){
	    auto value = consumer.int_value() & 0xff;
	    auto dvalue =
		duration.is_number() ? duration.int_value() & 0xff : 100;
	    ESP_LOGI(tag, "accept consumer value [%d] : duration [%d]",
		     value, dvalue);
	    bleHidSendConsumerValue(value, dvalue);
	}
    }
}

class BleHidHandler : public WebServerHandler {
    void recieveRequest(WebServerConnection& connection) override{
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

void registerIrRESTHandler(WebServer* server){
    server->setHandler(new BleHidHandler, "/blehid");
}
