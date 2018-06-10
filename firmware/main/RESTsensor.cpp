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

static const char JSON_DATE[] = "date";
static const char JSON_TEMP_UNIT[] = "temperatureUnit";
static const char JSON_HUM_UNIT[] = "humidityUnit";
static const char JSON_PRESS_UNIT[] = "pressureUnit";
static const char JSON_LUMINO_UNIT[] = "luminosityeUnit";
static const char JSON_TEMP[] = "temperature";
static const char JSON_HUM[] = "humidity";
static const char JSON_PRESS[] = "pressure";
static const char JSON_LUMINO[] = "luminositye";

class SensorHandler : public WebServerHandler {
    void recieveRequest(WebServerConnection& connection) override{
	auto req = connection.request();
	auto resp = connection.response();

	if (req->method() != HttpRequest::MethodGet){
	    resp->setHttpStatus(HttpResponse::RESP_400_BadRequest);
	    resp->close();
	    return;
	}

	SensorValue value;
	getSensorValue(&value);

	std::stringstream out;
	bool first = true;
	auto sep = [&]() -> void {
	    if (first){
		first = false;
	    }else{
		out << ",";
	    }
	};

	out << "{";
	if (value.enableFlag != 0){
	    sep();
	    out << "\"" << JSON_DATE << "\":\""
		<< value.captureTime.format(Time::RFC1123) << "\"";
	}
	if (value.enableFlag & SensorValue::TEMPERATURE){
	    sep();
	    out << "\"" << JSON_TEMP_UNIT << "\":\"celsius\",\""
		<< JSON_TEMP << "\":"
		<< value.temperatureFloat();
	}
	if (value.enableFlag & SensorValue::HUMIDITY){
	    sep();
	    out << "\"" << JSON_HUM_UNIT << "\":\"percentage\",\""
		<< JSON_HUM << "\":"
		<< value.humidityFloat();
	}
	if (value.enableFlag & SensorValue::PRESSURE){
	    sep();
	    out << "\"" << JSON_PRESS_UNIT << "\":\"hPa\",\""
		<< JSON_PRESS << "\":"
		<< value.pressureFloat();
	}
	out << "}";

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
