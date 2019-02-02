#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <mdns.h>
#include <string.h>
#include <GeneralUtils.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include "Config.h"
#include "mdnsService.h"
#include "WifiService.h"
#include "LEDService.h"
#include "TimeService.h"
#include "reboot.h"
#include "PubSubService.h"
#include "IRService.h"
#include "DeepSleep.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "WifiService";

//----------------------------------------------------------------------
// Observing WiFi connection
//----------------------------------------------------------------------
class ObserverTask : public Task {
protected:
    static const int EV_CONNECTED = 1;
    static const int EV_GOTADDRESS = 2;
    EventGroupHandle_t events;
public:
    ObserverTask(){
        events = xEventGroupCreate();
    }

    virtual ~ObserverTask(){
        vEventGroupDelete(events);
    }

    void connected(){
        xEventGroupSetBits(events, EV_CONNECTED);
    }

    void gotAddress(){
        xEventGroupSetBits(events, EV_GOTADDRESS);
    }

    void waitForReady(){
        xEventGroupWaitBits(
            events, EV_GOTADDRESS, pdFALSE, pdFALSE, portMAX_DELAY);
    }
    
protected:
    void run(void *data) override{
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        auto ev = xEventGroupWaitBits(
            events, EV_CONNECTED, pdFALSE, pdFALSE,
            60000 / portTICK_PERIOD_MS);
        if (ev){
            if (elfletConfig->getWakeupCause() != WC_TIMER){
                ledSetDefaultMode(LEDDM_STANDBY);
            }
            return;
        }else{
            if (elfletConfig->getWakeupCause() == WC_BUTTON){
                enterDeepSleep(0);
            }
            
            // fall back to configuration mode
            elfletConfig->setBootMode(Config::Configuration);
            elfletConfig->commit();
            rebootIn(0);
        }
    };
};

class ObserverTask* observerTask;

//----------------------------------------------------------------------
// WiFi event handler
//----------------------------------------------------------------------
static esp_err_t event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        observerTask->connected();
        /* enable ipv6 */
        tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        observerTask->gotAddress();
        notifySMTPserverAccesivility();
        enablePubSub();
        enableIRReceiver();
        break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        break;
    default:
        break;
    }
    mdns_handle_system_event(ctx, event);
    return ESP_OK;
}
    
//----------------------------------------------------------------------
// start WiFi function as Soft STA
//----------------------------------------------------------------------
#define ESPERR_RET(x, msg) {\
    int err = (x);\
    if (err != ESP_OK){\
        ESP_LOGE(tag, msg ": %s", GeneralUtils::errorToString((err)));\
        return false;\
    }\
}

bool startWifiService(){
    //
    // start mDNS service
    //
    startmdnsService();
    
    //
    // adapter init
    //
    tcpip_adapter_init();

    //
    // configure and start WiFi
    //
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESPERR_RET(esp_wifi_init(&wifi_init_config), "esp_wifi_init");
    ESPERR_RET(esp_wifi_set_storage(WIFI_STORAGE_RAM), "esp_wifi_set_storage");
    ESPERR_RET(esp_wifi_set_mode(WIFI_MODE_STA), "esp_wifi_set_mode");

    wifi_config_t wifi_config;
    auto ssid = elfletConfig->getSSIDtoConnect();
    auto pass = elfletConfig->getWifiPassword();
    if (ssid.length() > sizeof(wifi_config.sta.ssid) ||
        pass.length() > sizeof(wifi_config.sta.password)){
        ESP_LOGE(tag, "SSID or PASSWORD length too long");
        return false;
    }
    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, ssid.data(), ssid.length()) ;
    memcpy(wifi_config.sta.password, pass.data(), pass.length());
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    ESPERR_RET(esp_wifi_set_config(WIFI_IF_STA, &wifi_config),
               "esp_wifi_set_config");

    ESPERR_RET(esp_wifi_start(), "esp_wifi_start");

    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    observerTask = new ObserverTask;
    observerTask->start();
    observerTask->waitForReady();
    
    return true;
}
