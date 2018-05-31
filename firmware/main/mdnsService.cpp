#include <esp_log.h>
#include <esp_system.h>
#include <mdns.h>
#include <string.h>
#include <GeneralUtils.h>
#include "Config.h"
#include "mdnsService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "mdnsService";

bool startmdnsService(){
    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(
			elfletConfig->getNodeName().c_str()));
    ESP_ERROR_CHECK(mdns_instance_name_set("elflet home IoT unit"));

    /*
    ESP_ERROR_CHECK( mdns_service_add("World Wide Web", "_http", "_tcp",
				      80, NULL, 0) );
    */


        //structure with TXT records
    mdns_txt_item_t serviceTxtData[3] = {
        {"board","esp32"},
        {"u","user"},
        {"p","password"}
    };

    //initialize service
    ESP_ERROR_CHECK( mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData, 3) );
    //ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "path", "/foobar") );
    //ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "u", "admin") );
	
    return true;
}

