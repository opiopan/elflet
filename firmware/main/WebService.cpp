#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <GeneralUtils.h>
#include "webserver.h"
#include "htdigestfs.h"
#include "ota.h"
#include "Config.h"
#include "WebService.h"
#include "REST.h"
#include "WebContents.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "WebService";

static std::function<void()> completeOtaHandler = [](){
    elfletConfig->incrementOtaCount();
};

static WebServer* webserver;

bool startWebService(){
    auto domain = "elflet";
    
    htdigestfs_init("/auth");
    htdigestfs_register("admin", domain,
			elfletConfig->getAdminPassword().c_str());
    
    webserver = new WebServer();
    webserver->setHtdigest(htdigestfs_fp(), domain);
    webserver->setHandler(
	getOTAWebHandler(elfletConfig->getVerificationKeyPath(), true,
			 &completeOtaHandler),
	"/manage/otaupdate");
    registerConfigRESTHandler(webserver);
    registerStatusRESTHandler(webserver);
    registerIrRESTHandler(webserver);
    registerSensorRESTHandler(webserver);
    registerShadowRESTHandler(webserver);
    webserver->setContentProvider(createContentProvider());
    std::stringstream name;
    name << "elflet/" << getVersionString();
    webserver->setServerName(stringPtr(new std::string(name.str())));
    
    webserver->startServer(elfletConfig->getBootMode() == Config::Normal ?
	"80" : "192.168.55.01:80");

    ESP_LOGI(tag, "http server has been started. port: 80");

    return true;
}

const WebServer::Stat* getWebStat(){
    return webserver->getStat();
}
