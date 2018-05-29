#include <esp_log.h>
#include "Config.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "main";

extern "C" void app_main() {
    if (!initConfig()){
	// nothing to do since serious situation
	// TODO: enter LED to emergency mode
	return;
    }

    auto mode = elfletConfig->getBootMode();
    ESP_LOGI(tag, "elflet goes to mode: %s",
	     mode == Config::FactoryReset ? "FACTORY RESET" :
	     mode == Config::Configuration ? "CONFIGURAtiON" :
	     "NORMAL");
}
