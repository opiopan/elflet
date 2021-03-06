#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <GeneralUtils.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include <mqtt_client.h>
#include <json11.hpp>
#include "TimeObj.h"
#include "Mutex.h"
#include "webserver.h"
#include "NameResolver.h"
#include "reboot.h"
#include "Config.h"
#include "SensorService.h"
#include "PubSubService.h"
#include "IRService.h"
#include "FirmwareDownloader.h"
#include "Stat.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "PubSubService";

//----------------------------------------------------------------------
// Publishing task
//----------------------------------------------------------------------
class PubSub : public Task {
protected:
    static const int EV_WAKE_SERVER = 1;
    static const int EV_PUBLISH = 2;
    static const int EV_CONNECTED = 4;
    static const int EV_PUBLISHED = 8;
    EventGroupHandle_t events;
    Mutex mutex;
    static const int PUB_SENSOR = 1;
    static const int PUB_IRRC = 2;
    static const int PUB_SHADOW = 4;
    int publish;
    bool publishing;
    
    esp_mqtt_client_handle_t client;
    int subscribeStage;

    ShadowDevice* shadowToPublish;

public:
    PubSub();
    virtual ~PubSub();

    void enable();
    void publishSensorData();
    void publishIrrcData();
    void publishShadowStatus(ShadowDevice* shadow);

protected:
    void run(void *data) override;
    void connect(const std::string& uri);
    void subscribe();
    static esp_err_t mqttEventHandler(esp_mqtt_event_handle_t event);
};

PubSub::PubSub() : publish(0), publishing(false), client(NULL),
                   shadowToPublish(NULL){
    events = xEventGroupCreate();
}

PubSub::~PubSub(){
    if (client){
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
    }
}

void PubSub::enable(){
    xEventGroupSetBits(events, EV_WAKE_SERVER);
}

void PubSub::publishSensorData(){
    auto holder = LockHolder(mutex);
    publish |= PUB_SENSOR;
    xEventGroupSetBits(events, EV_PUBLISH);
}

void PubSub::publishIrrcData(){
    auto holder = LockHolder(mutex);
    publish |= PUB_IRRC;
    xEventGroupSetBits(events, EV_PUBLISH);
}

void PubSub::publishShadowStatus(ShadowDevice* shadow){
    auto holder = LockHolder(mutex);
    if (shadowToPublish){
        return;
    }
    shadowToPublish = shadow;
    publish |= PUB_SHADOW;
    xEventGroupSetBits(events, EV_PUBLISH);
}

void PubSub::run(void *data){
    xEventGroupWaitBits(events, EV_WAKE_SERVER,
                        pdTRUE, pdFALSE,
                        portMAX_DELAY);

    static const char* schemes[] = {"mqtt://", "mqtts://", "ws://", "wss://"};
    std::string uri = schemes[elfletConfig->getPubSubSessionType()];
    uri += elfletConfig->getPubSubServerAddr();
    ESP_LOGI(tag, "mqtt broker is %s", uri.c_str());
    while (resolveHostname(uri) == RNAME_FAIL_TO_RESOLVE){
        ESP_LOGE(tag, "fail to resolve mdns name, retry in 5 sec.");
        delay(5000);
    }
    connect(uri);
    xEventGroupWaitBits(events, EV_CONNECTED,
                        pdTRUE, pdFALSE,
                        portMAX_DELAY);
    ESP_LOGI(tag, "ready to publish to  mqtt broker: %s", uri.c_str());
    
    while(true){
        xEventGroupWaitBits(events, EV_PUBLISH,
                            pdTRUE, pdFALSE,
                            portMAX_DELAY);
        int request = 0;
        {
            auto holder = LockHolder(mutex);
            request = publish;
            publish = 0;
        }
        
        if (request & PUB_SENSOR){
            std::stringstream out;
            getSensorValueAsJson(out);
            const auto data = out.str();
            int bits = 0;
            esp_mqtt_client_publish(
                client, elfletConfig->getSensorTopic().c_str(),
                data.data(), data.length(), 1, 0);
            publishing = true;
            bits = xEventGroupWaitBits(events, EV_PUBLISHED,
                                       pdTRUE, pdFALSE,
                                       3000 / portTICK_PERIOD_MS);
            publishing = false;
            if (bits == 0){
                ESP_LOGE(tag, "fail to publish sensor data");
            }
            if (elfletConfig->getBootMode() == Config::Normal &&
                elfletConfig->getWakeupCause() != WC_BUTTON &&
                elfletConfig->getFunctionMode() == Config::SensorOnly){
                esp_mqtt_client_stop(client);
                esp_mqtt_client_destroy(client);
                enterDeepSleep(0);
            }
        }
        if (request & PUB_IRRC){
            uint8_t buf[48];
            int32_t bits = sizeof(buf) * 8;
            IRRC_PROTOCOL protocol;
            if (getIRReceivedData(&protocol, &bits, buf)){
                std::stringstream out;
                if (protocol == IRRC_UNKNOWN){
                    continue;
                }
                getIRReceivedDataJson(out);
                const auto data = out.str();
                esp_mqtt_client_publish(
                    client, elfletConfig->getIrrcReceivedDataTopic().c_str(),
                    data.data(), data.length(), 0, 0);
            }
        }
        if (request & PUB_SHADOW){
            std::stringstream out;
            shadowToPublish->dumpStatus(out);
            const auto data = out.str();
            esp_mqtt_client_publish(
                    client, elfletConfig->getShadowTopic().c_str(),
                    data.data(), data.length(), 0, 0);
            shadowToPublish = NULL;
        }
    }
}

void PubSub::connect(const std::string& uri){
    esp_mqtt_client_config_t mqttCfg;
    memset(&mqttCfg, 0, sizeof(mqttCfg));
    mqttCfg.user_context = this;
    mqttCfg.uri = uri.c_str();
    mqttCfg.event_handle = mqttEventHandler;
    const auto cert = elfletConfig->getPubSubServerCert();
    const auto user = elfletConfig->getPubSubUser();
    const auto pass = elfletConfig->getPubSubPassword();
    if (cert.length() > 0){
        mqttCfg.cert_pem = cert.c_str();
    }
    if (user.length() > 0){
        mqttCfg.username = user.c_str();
    }
    if (pass.length() > 0){
        mqttCfg.password = pass.c_str();
    }
    client = esp_mqtt_client_init(&mqttCfg);

    ESP_LOGI(tag, "connecting to mqtt broker: %s", uri.c_str());
    esp_mqtt_client_start(client);
}

void PubSub::subscribe(){
    const std::string* topics[] = {
        &elfletConfig->getDownloadFirmwareTopic(),
        &elfletConfig->getIrrcSendTopic(),
        &elfletConfig->getIrrcReceiveTopic(),
    };

    for (;subscribeStage < sizeof(topics) / sizeof(topics[0]);
         subscribeStage++){
        if (topics[subscribeStage]->length() > 0){
            auto topic = topics[subscribeStage]->c_str();
            ESP_LOGI(tag, "subscribing: %s", topic);
            esp_mqtt_client_subscribe(client, topic, 1);
            subscribeStage++;
            return;
        }
    }
    xEventGroupSetBits(events, EV_CONNECTED);
}

esp_err_t PubSub::mqttEventHandler(esp_mqtt_event_handle_t event){
    auto self = (PubSub*)event->user_context;

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED: {
        self->subscribeStage = 0;
        self->subscribe();
        break;
    }
    case MQTT_EVENT_SUBSCRIBED: {
        self->subscribe();
        break;
    }
    case MQTT_EVENT_PUBLISHED: {
        xEventGroupSetBits(self->events, EV_PUBLISHED);
        break;
    }
    case MQTT_EVENT_DATA:{
        WebString topic(event->topic, event->topic_len);
        auto irrcSend = elfletConfig->getIrrcSendTopic();
        auto irrcReceive = elfletConfig->getIrrcReceiveTopic();
        auto downloadFirmware = elfletConfig->getDownloadFirmwareTopic();

        if (event->total_data_len > event->data_len){
            ESP_LOGI(tag, "too large data received.");
            break;
        }

        if (topic == irrcSend.c_str()){
            ESP_LOGI(tag, "receive subscribed mqtt data: IrrcSend");
            WebString data(event->data, event->data_len);
            sendIRDataJson(data);
        }else if (topic == irrcReceive.c_str()){
            ESP_LOGI(tag, "receive subscribed mqtt data: IrrcReceive");
            startIRReceiver();
        }else if (topic == downloadFirmware.c_str()){
            ESP_LOGI(tag, "receive subscribed mqtt data: DwonloadFirmware");
            std::string err;
            auto msg = json11::Json::parse(std::string(event->data,
                                                       event->data_len),
                                           err);
            if (msg.is_object()){
                auto currentVersion = getVersion();
                auto versionString = msg[JSON_FWVERSION];
                auto uri = msg[JSON_FWURI];
                if (versionString.is_string() && uri.is_string()){
                    Version version(versionString.string_value().c_str());
                    if (version > *currentVersion){
                        ESP_LOGI(tag, "Firmware will be updated: "
                                 "%s -> %s\n\t\t%s",
                                 getVersionString(),
                                 versionString.string_value().c_str(),
                                 uri.string_value().c_str());
                        suspendEnterDeepSleep();
                        const char* uristr = uri.string_value().c_str();
                        DFCallback callback = [](DFStatus st)->void{
                            if (st == dfSucceed){
                                elfletConfig->incrementOtaCount();
                                rebootIn(100);
                            }else{
                                resumeEnterDeepSleep();
                            }
                        };
                        if (httpDownloadFirmware(uristr, callback)
                            != dfSucceed){
                            resumeEnterDeepSleep();
                        }
                    }
                }
            }
        }
        break;
    }
    case MQTT_EVENT_ERROR:{
        /*
        printf("MQTT_EVENT_ERROR\n");
        if (self->publishing){
            xEventGroupSetBits(self->events, EV_PUBLISHED);
        }
        */
        break;
    }
    default:
        break;
    }    
    return ESP_OK;
}

//----------------------------------------------------------------------
// interfaces for outer module
//----------------------------------------------------------------------
static PubSub* task = NULL;

bool startPubSubService(){
    if (!task){
        task = new PubSub();
        task->start();
    }
    return true;
}

void enablePubSub(){
    if (task){
        task->enable();
    }
}

void publishSensorData(){
    if (task &&
        elfletConfig->getSensorTopic().length() > 0){
        task->publishSensorData();
        pubsubStat.publishSensorCount++;
    }
}

void publishIrrcData(){
    if (task &&
        elfletConfig->getIrrcReceivedDataTopic().length() > 0){
        task->publishIrrcData();
        pubsubStat.publishIrrcCount++;
    }
}

void publishShadowStatus(ShadowDevice* shadow){
    if (task &&
        elfletConfig->getShadowTopic().length() > 0){
        task->publishShadowStatus(shadow);
        pubsubStat.publishShadowCount++;
    }
}
