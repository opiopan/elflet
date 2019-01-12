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

extern const char JSON_HEAP[] = "Heap";
extern const char JSON_INITIAL_HEAP[] = "InitialHeapSize";
extern const char JSON_FREE_HEAP[] = "FreeHeapSize";
extern const char JSON_STORAGE[] = "SpiFlash";
extern const char JSON_SPIFFS_TOTAL[] = "SpiffsTotal";
extern const char JSON_SPIFFS_USED[] = "SpiffsUsed";
extern const char JSON_NVS_TOTAL[] = "NvsTotalEntries";
extern const char JSON_NVS_USED[] = "NvsUsedEntries";
extern const char JSON_NVS_NSCOUNT[] = "NvsNamespaceCount";
extern const char JSON_STAT[] = "Statistics";

class StatusHandler : public WebServerHandler {
    void receiveRequest(WebServerConnection& connection) override{
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
	    });
	auto heap = json11::Json::object({
		{JSON_INITIAL_HEAP, (int32_t)initialHeapSize},
		{JSON_FREE_HEAP, (int32_t)heapSize},
	    });
	size_t fstotal, fsused;
	conf->getSpiffsInfo(&fstotal, &fsused);
	nvs_stats_t nvs;
	conf->getNvsInfo(&nvs);
	auto storage = json11::Json::object({
		{JSON_SPIFFS_TOTAL, (int32_t)fstotal},
		{JSON_SPIFFS_USED, (int32_t)fsused},
		{JSON_NVS_TOTAL, (int32_t)nvs.total_entries},
		{JSON_NVS_USED, (int32_t)nvs.used_entries},
		{JSON_NVS_NSCOUNT, (int32_t)nvs.namespace_count},
	    });
	obj[JSON_HEAP] = heap;
	obj[JSON_STORAGE] = storage;
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
