#include <esp_log.h>
#include <Task.h>
#include "Config.h"
#include "mdnsService.h"
#include "WifiService.h"
#include "WifiApService.h"
#include "WebService.h"
#include "irserver.h"
#include "LEDService.h"
#include "ButtonService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "main";

_Noreturn static void systemFault(){
    ledSetBlinkMode(LEDBM_SYSTEM_FAULT);
    while (true){
	vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

class MainTask : public Task {
    void run(void *data) override;
};

void MainTask::run(void *data){
    //--------------------------------------------------------------
    // initialize UI LED
    //--------------------------------------------------------------
    startLedService();

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
    if (mode == Config::Configuration){
	elfletConfig->setBootMode(Config::Normal);
	elfletConfig->commit();
    }

    //--------------------------------------------------------------
    // start user interface services
    //--------------------------------------------------------------
    startButtonService();
    
    //--------------------------------------------------------------
    // start network related services
    //--------------------------------------------------------------
    if (mode == Config::FactoryReset || mode == Config::Configuration){
	ledSetDefaultMode(
	    mode == Config::FactoryReset ? LEDDM_FACTORY_RESET :
	                                   LEDDM_CONFIGURATION);
	if (!startWifiApService()){
	    systemFault();
	}
	startWebService();
    }else{
	ledSetDefaultMode(LEDDM_SCAN_WIFI);
	if (!startWifiService()){
	    systemFault();
	}
	startWebService();
	startIRServer();
    }
}

extern "C" void app_main() {
    auto task = new MainTask;
    task->start();
}
