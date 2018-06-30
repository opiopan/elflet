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
#include "PubSubService.h"
#include "IRService.h"

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
    EventGroupHandle_t events;
    Mutex mutex;
    static const int PUB_SENSOR = 1;
    static const int PUB_IRRC = 2;
    int publish;
    
    esp_mqtt_client_handle_t client;
    int msgidIrrcSend;
    int msgidIrrcRecieve;
    int msgidDownloadFirmware;

public:
    PubSub();
    virtual ~PubSub();

    void enable();
    void publishSensorData();
    void publishIrrcData();

protected:
    void run(void *data) override;
    static esp_err_t mqttEventHandler(esp_mqtt_event_handle_t event);
};

PubSub::PubSub() : publish(0), client(NULL){
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

void PubSub::run(void *data){
    xEventGroupWaitBits(events, EV_WAKE_SERVER,
			pdTRUE, pdFALSE,
			portMAX_DELAY);

    static const char* schemes[] = {"mqtt://", "mqtts://", "ws://", "wss://"};
    std::string uri = schemes[elfletConfig->getPubSubSessionType()];
    uri += elfletConfig->getPubSubServerAddr();
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

    bool first = true;
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
	
	if (first){
	    first = false;
	    esp_mqtt_client_start(client);
	    xEventGroupWaitBits(events, EV_CONNECTED,
				pdTRUE, pdFALSE,
				portMAX_DELAY);
	    ESP_LOGI(tag, "connected to mqtt broker: %s", uri.c_str());
	}

	if (request & PUB_SENSOR){
	    std::stringstream out;
	    getSensorValueAsJson(out);
	    const auto data = out.str();
	    esp_mqtt_client_publish(
		client, elfletConfig->getSensorTopic().c_str(),
		data.data(), data.length(), 0, 0);
	}
	if (request & PUB_IRRC){
	    uint8_t buf[32];
	    int32_t bits = sizeof(buf) * 8;
	    IRRC_PROTOCOL protocol;
	    if (getIRRecievedData(&protocol, &bits, buf)){
		std::stringstream out;
		if (protocol == IRRC_UNKNOWN){
		    getIRRecievedDataRawJson(out);
		}else{
		    getIRRecievedDataJson(out);		    
		}
		const auto data = out.str();
		esp_mqtt_client_publish(
		    client, elfletConfig->getIrrcRecievedDataTopic().c_str(),
		    data.data(), data.length(), 0, 0);
		
	    }
	}
    }
}

esp_err_t PubSub::mqttEventHandler(esp_mqtt_event_handle_t event){
    auto self = (PubSub*)event->user_context;

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
	xEventGroupSetBits(self->events, EV_CONNECTED);
	break;
    case MQTT_EVENT_PUBLISHED:
	//xEventGroupSetBits(self->events, EV_PUBLISHED);
	break;
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
    if (task){
	task->publishSensorData();
    }
}

void publishIrrcData(){
    if (task){
	task->publishIrrcData();
    }
}
