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
#include "BleHidService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "WebService";

static std::function<void(OTAPHASE)> otaHandler = [](OTAPHASE phase){
    if (phase == OTA_BEGIN){
        stopBleHidService();
    }else if (phase == OTA_END){
        elfletConfig->incrementOtaCount();
    }else if (phase == OTA_ERROR){
        startBleHidService();
    }
};

static WebServer* webserver;
static std::string hostName;

static const char* redirectFilter(const WebString& host){
    if (host == "elflet.setup" ||
	host == "192.168.55.1" ||
	host == elfletConfig->getNodeName().c_str() ||
	host == hostName.c_str()){
	return NULL;
    }else{
	return "http://elflet.setup";
    }
}

bool startWebService(){
    auto domain = "elflet";
    
    htdigestfs_init("/auth");
    htdigestfs_register("admin", domain,
                        elfletConfig->getAdminPassword().c_str());
    
    webserver = new WebServer();
    webserver->setHtdigest(htdigestfs_fp(), domain);
    webserver->setHandler(
        getOTAWebHandler(elfletConfig->getVerificationKeyPath(), true,
                         &otaHandler),
        "/manage/otaupdate");
    registerConfigRESTHandler(webserver);
    registerStatusRESTHandler(webserver);
    registerIrRESTHandler(webserver);
    registerSensorRESTHandler(webserver);
    registerShadowRESTHandler(webserver);
    registerBleHidRESTHandler(webserver);
    webserver->setContentProvider(createContentProvider());
    std::stringstream name;
    name << "elflet/" << getVersionString();
    webserver->setServerName(stringPtr(new std::string(name.str())));
    if (elfletConfig->getBootMode() != Config::Normal){
	hostName = elfletConfig->getNodeName();
	hostName += ".local";
	webserver->setRedirectFilter(redirectFilter);
    }
    
    webserver->startServer(elfletConfig->getBootMode() == Config::Normal ?
        "80" : "192.168.55.01:80");

    ESP_LOGI(tag, "http server has been started. port: 80");

    return true;
}

const WebServer::Stat* getWebStat(){
    return webserver->getStat();
}
