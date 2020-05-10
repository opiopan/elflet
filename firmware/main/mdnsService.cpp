#include <esp_log.h>
#include <esp_system.h>
#include <esp_event_loop.h>
#include <mdns.h>
#include <string.h>
#include <GeneralUtils.h>
#include "Config.h"
#include "mdnsService.h"
#include "irserver.h"
#include "irserverProtocol.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "mdnsService";

bool startmdnsService(){
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(
                        elfletConfig->getNodeName().c_str()));
    ESP_ERROR_CHECK(mdns_instance_name_set("elflet home IoT unit"));

    mdns_txt_item_t serviceTxtData[] = {
        {(char*)"board", (char*)"elflet home IoT device"}
    };

    //initialize service
    ESP_ERROR_CHECK( mdns_service_add( "elflet WebServer", "_http", "_tcp", 80,
                                       serviceTxtData, 1) );
    ESP_ERROR_CHECK( mdns_service_add("irserver", "_irserver", "_tcp",
                                      IRSERVER_PORT, NULL, 0) );
        
    return true;
}

