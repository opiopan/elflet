#include <esp_log.h>
#include <esp_system.h>
#include <GeneralUtils.h>
#include <json11.hpp>
#include <iostream>
#include <sstream>
#include "webserver.h"
#include "Config.h"
#include "SensorService.h"
#include "REST.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "RESTsensor";

class SensorHandler : public WebServerHandler {
    void receiveRequest(WebServerConnection& connection) override{
	auto req = connection.request();
	auto resp = connection.response();

	if (req->method() != HttpRequest::MethodGet){
	    resp->setHttpStatus(HttpResponse::RESP_400_BadRequest);
	    resp->close();
	    return;
	}

	std::stringstream out;
	getSensorValueAsJson(out);

	stringPtr body(new std::string(out.str()));
	resp->setBody(body);
	resp->setHttpStatus(HttpResponse::RESP_200_OK);
	resp->addHeader("Content-Type", "application/json");
	resp->close();
    }
};

void registerSensorRESTHandler(WebServer* server){
    server->setHandler(new SensorHandler, "/sensor");
}
