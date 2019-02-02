#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>    
#include <string.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include <GeneralUtils.h>
#include "Config.h"
#include "mdnsService.h"
#include "WifiApService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "WifiServiceAP";

static const int EV_STARTED = 1;
static EventGroupHandle_t events;

//----------------------------------------------------------------------
// WiFi event handler
//----------------------------------------------------------------------
static esp_err_t event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
    case SYSTEM_EVENT_AP_START:
        // start mDNS service
        //startmdnsService();
        xEventGroupSetBits(events, EV_STARTED);
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        break;
    default:
        break;
    }
    mdns_handle_system_event(ctx, event);
    return ESP_OK;
}
    
//----------------------------------------------------------------------
// start WiFi function as Soft AP
//----------------------------------------------------------------------
#define ESPERR_RET(x, msg) {\
    int err = (x);\
    if (err != ESP_OK){\
        ESP_LOGE(tag, msg ": %s", GeneralUtils::errorToString((err)));\
        return false;\
    }\
}

bool startWifiApService(){
    events = xEventGroupCreate();
    
    startmdnsService();

    tcpip_adapter_init();

    //
    // configure inerface & DHCP server
    //
    ESPERR_RET(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP),
               "failed to stop DHCP");
    tcpip_adapter_ip_info_t info;
    memset(&info, 0, sizeof(info));
    IP4_ADDR(&info.ip, 192, 168, 55, 1);
    IP4_ADDR(&info.gw, 192, 168, 55, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    ESPERR_RET(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info),
               "failed to initalize IF adapter");
    ESPERR_RET(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP),
               "failed to start DHCP server");

    //
    // configure and start WiFi
    //
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESPERR_RET(esp_wifi_init(&wifi_init_config), "esp_wifi_init");
    ESPERR_RET(esp_wifi_set_storage(WIFI_STORAGE_RAM), "esp_wifi_set_storage");
    ESPERR_RET(esp_wifi_set_mode(WIFI_MODE_AP), "esp_wifi_set_mode");

    wifi_config_t ap_config;
    auto ssid = elfletConfig->getAPSSID();
    auto pass = elfletConfig->getAdminPassword();
    if (ssid.length() > sizeof(ap_config.ap.ssid) ||
        pass.length() + 1 > sizeof(ap_config.ap.password)){
        ESP_LOGE(tag, "SSID or PASSWORD length too long");
        return false;
    }
    memset(&ap_config, 0, sizeof(ap_config));
    memcpy(ap_config.ap.ssid, ssid.data(), ssid.length()) ;
    memcpy(ap_config.ap.password, pass.c_str(), pass.length() + 1);;
    ap_config.ap.ssid_len = ssid.length();
    ap_config.ap.channel = 3;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.ssid_hidden = 0;
    ap_config.ap.max_connection = 5;
    ap_config.ap.beacon_interval = 100;
    ESPERR_RET(esp_wifi_set_config(WIFI_IF_AP, &ap_config),
               "esp_wifi_set_config");

    wifi_country_t country;
    esp_wifi_get_country(&country);
    strncpy(country.cc,  CONFIG_WIFI_COUNTRY_CODE, sizeof(country.cc));
    esp_wifi_set_country(&country);

    ESPERR_RET(esp_wifi_start(), "esp_wifi_start");

    //esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

    xEventGroupWaitBits(
        events, EV_STARTED, pdFALSE, pdFALSE, portMAX_DELAY);
    
    return true;
}
