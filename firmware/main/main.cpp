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
#include "IRService.h"
#include "SensorService.h"
#include "TimeService.h"
#include "PubSubService.h"

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

    auto wakeupCause = elfletConfig->getWakeupCause();
    if (wakeupCause != WC_NOTSLEEP){
	ESP_LOGI(tag, "wakeup from deep sleep by %s",
		 wakeupCause == WC_TIMER ? "time out" : "pushing a button");
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
    // start peripheral services
    //--------------------------------------------------------------
    startTimeService();
    startButtonService();
    if (wakeupCause != WC_BUTTON){
	startIRService();
	startSensorService();
	if (wakeupCause == WC_TIMER){
	    enableSensorCapturing();
	}
    }
    
    //--------------------------------------------------------------
    // start network services
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
	if (wakeupCause == WC_NOTSLEEP){
	    ledSetDefaultMode(LEDDM_SCAN_WIFI);
	}else if (wakeupCause == WC_BUTTON){
	    ledSetBlinkMode(LEDBM_RESTRICT_DEEP_SLEEP);
	}
	if (elfletConfig->getPubSubServerAddr().length() > 0 &&
	    wakeupCause != WC_BUTTON){
	    startPubSubService();
	}
	if (!startWifiService()){
	    systemFault();
	}
	if (wakeupCause != WC_TIMER){
	    startWebService();
	}
	if (wakeupCause == WC_NOTSLEEP){
	    startIRServer();
	}
    }
}

extern "C" void app_main() {
    auto task = new MainTask;
    task->start();
}
