#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <GeneralUtils.h>
#include <webserver.h>
#include <htdigestfs.h>
#include "ota.h"
#include "Config.h"
#include "WebService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "WebService";

bool startWebService(){
    htdigestfs_init("/auth");
    htdigestfs_register("admin", "elflet",
			elfletConfig->getAdminPassword().c_str());
    
    auto webserver = new WebServer();
    webserver->setHtdigest(htdigestfs_fp(), "elflet");
    webserver->setHandler(
	getOTAWebHandler(elfletConfig->getVerificationKeyPath(), true),
	"/manage/otaupdate");
    webserver->startServer("80");

    return true;
}
