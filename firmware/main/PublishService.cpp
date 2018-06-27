#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <GeneralUtils.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include <mqtt_client.h>
#include "Mutex.h"
#include "Config.h"
#include "SensorService.h"
#include "PublishService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "PublishService";

//----------------------------------------------------------------------
// Publishing task
//----------------------------------------------------------------------
class PublishTask : public Task {
protected:
    static const int EV_WAKE_SERVER = 1;
    static const int EV_PUBLISH = 2;
    static const int EV_CONNECTED = 4;
    static const int EV_PUBLISHED = 8;
    EventGroupHandle_t events;
    Mutex mutex;
    
    int enableFlag;


public:
    PublishTask();
    virtual ~PublishTask();

    void enablePublishing();
    void publish();

protected:
    void run(void *data) override;
    static esp_err_t mqttEventHandler(esp_mqtt_event_handle_t event);
};

PublishTask::PublishTask() {
    events = xEventGroupCreate();
}

PublishTask::~PublishTask(){
}

void PublishTask::enablePublishing(){
    xEventGroupSetBits(events, EV_WAKE_SERVER);
}

void PublishTask::publish(){
    xEventGroupSetBits(events, EV_PUBLISH);
}

void PublishTask::run(void *data){
    xEventGroupWaitBits(events, EV_WAKE_SERVER,
			pdTRUE, pdFALSE,
			portMAX_DELAY);

    static const char* schemes[] = {"mqtt://", "mqtts://", "ws://", "wss://"};
    std::string uri = schemes[elfletConfig->getPublishSessionType()];
    uri += elfletConfig->getPublishServerAddr();
    esp_mqtt_client_config_t mqttCfg;
    memset(&mqttCfg, 0, sizeof(mqttCfg));
    mqttCfg.user_context = this;
    mqttCfg.uri = uri.c_str();
    mqttCfg.event_handle = mqttEventHandler;
    const auto cert = elfletConfig->getPublishServerCert();
    const auto user = elfletConfig->getPublishUser();
    const auto pass = elfletConfig->getPublishPassword();
    if (cert.length() > 0){
	mqttCfg.cert_pem = cert.c_str();
    }
    if (user.length() > 0){
	mqttCfg.username = user.c_str();
    }
    if (pass.length() > 0){
	mqttCfg.password = pass.c_str();
    }
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqttCfg);

    bool first = true;
    while(true){
	xEventGroupWaitBits(events, EV_PUBLISH,
			    pdTRUE, pdFALSE,
			    portMAX_DELAY);
	if (first){
	    first = false;
	    esp_mqtt_client_start(client);
	    xEventGroupWaitBits(events, EV_CONNECTED,
				pdTRUE, pdFALSE,
				portMAX_DELAY);
	    ESP_LOGI(tag, "connected to mqtt broker: %s", uri.c_str());
	}

	std::stringstream out;
	getSensorValueAsJson(out);
	const auto data = out.str();
	esp_mqtt_client_publish(
	    client, elfletConfig->getPublishTopic().c_str(),
	    data.data(), data.length(), 0, 0);
    }
};

esp_err_t PublishTask::mqttEventHandler(esp_mqtt_event_handle_t event){
    auto self = (PublishTask*)event->user_context;

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
	xEventGroupSetBits(self->events, EV_CONNECTED);
	break;
    case MQTT_EVENT_PUBLISHED:
	xEventGroupSetBits(self->events, EV_PUBLISHED);
	break;
    default:
	break;
    }    
    return ESP_OK;
}


//----------------------------------------------------------------------
// interfaces for outer module
//----------------------------------------------------------------------
static PublishTask* task = NULL;

bool startPublishService(){
    if (!task){
	task = new PublishTask();
	task->start();
    }
    return true;
}

void enablePublishing(){
    if (task){
	task->enablePublishing();
    }
}

void publishSensorData(){
    if (task){
	task->publish();
    }
}
