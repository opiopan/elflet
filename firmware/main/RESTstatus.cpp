#include <esp_log.h>
#include <esp_system.h>
#include <GeneralUtils.h>
#include <json11.hpp>
#include <iostream>
#include <sstream>
#include "webserver.h"
#include "Config.h"
#include "REST.h"
#include "Stat.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "RESTstatus";

extern const char JSON_INITIAL_HEAP[] = "InitialHeapSize";
extern const char JSON_FREE_HEAP[] = "FreeHeapSize";
extern const char JSON_STAT[] = "Statistics";

class StatusHandler : public WebServerHandler {
    void recieveRequest(WebServerConnection& connection) override{
	auto req = connection.request();
	auto resp = connection.response();
	if (req->method() != HttpRequest::MethodGet){
	    resp->setHttpStatus(HttpResponse::RESP_400_BadRequest);
	    resp->close();
	    return;
	}

	auto conf = elfletConfig;
	auto heapSize = xPortGetFreeHeapSize();
	auto obj = json11::Json::object({
		{JSON_BOARDTYPE, "elflet"},
		{JSON_BOARDVERSION, conf->getBoardVersion()},
		{JSON_FWVERSION, getVersionString()},
		{JSON_NODENAME, conf->getNodeName()},
		{JSON_INITIAL_HEAP, (int32_t)initialHeapSize},
		{JSON_FREE_HEAP, (int32_t)heapSize},
	    });
	obj[JSON_STAT] = getStatisticsJson();

	stringPtr body(new std::string(json11::Json(obj).dump()));
	resp->setBody(body);
	resp->setHttpStatus(HttpResponse::RESP_200_OK);
	resp->addHeader("Content-Type", "application/json");
	resp->close();
    }
};

void registerStatusRESTHandler(WebServer* server){
    server->setHandler(new StatusHandler, "/manage/status");
}
