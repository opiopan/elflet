#include <esp_log.h>
#include <Task.h>
#include <esp_bt.h>
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
#include "Stat.h"
#include "BleHidService.h"
#include "WatchDog.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "main";

_Noreturn static void systemFault(){
    ledSetBlinkMode(LEDBM_SYSTEM_FAULT);
    while (true){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void showBanner(){
    printf(
        "\033[96m"
        "========================================"
        "========================================\n"
        "        Home IoT controller based on ESP-WROOM-32\n\n"
        "                      ___       ___  ___           __\n"
        "                     /\\_ \\    /'___\\/\\_ \\         /\\ \\__\n"
        "                   __\\//\\ \\  /\\ \\__/\\//\\ \\      __\\ \\ ,_\\ \n"
        "                 /'__`\\\\ \\ \\ \\ \\ ,__\\ \\ \\ \\   /'__`\\ \\ \\/ \n"
        "                /\\  __/ \\_\\ \\_\\ \\ \\_/  \\_\\ \\_/\\  __/\\ \\ \\_ \n"
        "                \\ \\____\\/\\____\\\\ \\_\\   /\\____\\ \\____\\\\ \\__\\ \n"
        "                 \\/____/\\/____/ \\/_/   \\/____/\\/____/ \\/__/ \n\n"
        "        Fiemware Version: %s\n"
        "        Author:           Hiroshi Murayama <opiopan@gmail.com>\n"
        "        Web:              http://github.com/opiopan/elflet/\n"
        "========================================"
        "========================================\n"
        "\033[0m",
        getVersionString());
}

class MainTask : public Task {
    void run(void *data) override;
};

void MainTask::run(void *data){
    updateWatchDog();

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
        baseStat.deepSleepCount++;
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
    auto isSensorOnly =
        (elfletConfig->getFunctionMode() == Config::SensorOnly);
    auto enableBLE = 
        (wakeupCause == WC_NOTSLEEP && mode == Config::Normal &&
         elfletConfig->getBleHid());

    //--------------------------------------------------------------
    // release memory for Bluetooth if it's possible
    //--------------------------------------------------------------
    if (!enableBLE){
        releaseBleResource();
        auto heapSize = xPortGetFreeHeapSize();
        if (initialHeapSize < heapSize){
            initialHeapSize = heapSize;
            ESP_LOGI(tag, "initial heap size increase to %d bytes", heapSize);
        }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    //--------------------------------------------------------------
    // start peripheral services
    //--------------------------------------------------------------
    startTimeService();
    startButtonService();
    if (wakeupCause != WC_BUTTON){
        if (!isSensorOnly){
            startIRService();
        }
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
    }else if (!elfletConfig->getWifi()){
        ledSetDefaultMode(LEDDM_BOOT_ONLY_BLE);
        initBleHidService(elfletConfig->getNodeName().c_str());
        startBleHidService();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        ledSetDefaultMode(LEDDM_STANDBY);
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
        if (!isSensorOnly || wakeupCause == WC_BUTTON){
            startWebService();
        }
        if (!isSensorOnly){
            startIRServer();
        }
        if (enableBLE){
            initBleHidService(elfletConfig->getNodeName().c_str());
            startBleHidService();
        }
    }

    //--------------------------------------------------------------
    // survey watch dog timer
    //--------------------------------------------------------------
    if (mode == Config::Configuration){
        while (true){
            if (getWatchDogTimerInterval() > 10 * 60){
                ESP_LOGI(tag, "go back to normal mode");
                esp_restart();
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

extern "C" void app_main() {
    initialHeapSize = xPortGetFreeHeapSize();
    showBanner();
    ESP_LOGI(tag, "free heap size: %d", initialHeapSize);
    auto task = new MainTask;
    task->setStackSize(15000);
    task->start();
}
