#include <esp_log.h>
#include <Task.h>
#include "Config.h"
#include "mdnsService.h"
#include "WifiApService.h"
#include "WebService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "main";

_Noreturn static void systemFault(){
    while (true){
	vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

extern "C" void app_main() {
    //--------------------------------------------------------------
    // initialize periferals
    //--------------------------------------------------------------
    

    //--------------------------------------------------------------
    // load configuration
    //--------------------------------------------------------------
    if (!initConfig()){
	// nothing to do since serious situation
	systemFault();;
    }

    auto mode = elfletConfig->getBootMode();
    ESP_LOGI(tag, "elflet goes to mode: %s",
	     mode == Config::FactoryReset ? "FACTORY RESET" :
	     mode == Config::Configuration ? "CONFIGURAtiON" :
	     "NORMAL");

    //--------------------------------------------------------------
    // start user interface services
    //--------------------------------------------------------------
    
    //--------------------------------------------------------------
    // start network related services
    //--------------------------------------------------------------
    if (mode == Config::FactoryReset || mode == Config::Configuration){
	if (!startWifiApService()){
	    systemFault();
	}
	startWebService();
    }else{
    }
}
