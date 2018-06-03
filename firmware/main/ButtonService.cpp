#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <GeneralUtils.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include "Mutex.h"
#include "Config.h"
#include "BUttonService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "ButtonService";

#define FILTER_DELAY (100 / portTICK_PERIOD_MS)

class ButtonFilterTask;
class ButtonEventTask;

static ButtonFilterTask* filterTask;
static ButtonEventTask* eventTask;

//----------------------------------------------------------------------
// Button event handler
//----------------------------------------------------------------------
class ButtonEventTask : public Task{
protected:
    EventGroupHandle_t events;
    Mutex mutex;
    bool level;

public:
    ButtonEventTask();
    void sendEvent(bool level);
    
protected:
    void run(void *data) override;
};

ButtonEventTask::ButtonEventTask(): level(false){
    events = xEventGroupCreate();
}

void ButtonEventTask::sendEvent(bool level){
    LockHolder holder(mutex);
    if (this->level != level){
	this->level = level;
	xEventGroupSetBits(events, 1);
    }
}

void ButtonEventTask::run(void *data){
    bool level = false;
    auto waitForEvent = [&](int timeToWait) -> bool {
	auto ev = xEventGroupWaitBits(events, 1, pdTRUE, pdFALSE, timeToWait);
	LockHolder holder(this->mutex);
	level = this;
	return ev != 0;
    };

    while (true){
	while (!level){
	    waitForEvent(portMAX_DELAY);
	}
	printf("button down\n");

	while (level){
	    if (!waitForEvent(6 * 1000 / portTICK_PERIOD_MS)){
		auto mode = elfletConfig->getBootMode();
		ESP_LOGI(tag,
			 "long button press detected, boot mode is changing");
		if (mode == Config::Normal){
		    elfletConfig->setBootMode(Config::Configuration);
		    elfletConfig->commit();
		    esp_restart();
		}else if (mode == Config::Configuration){
		    elfletConfig->setBootMode(Config::FactoryReset);
		    elfletConfig->commit();
		    esp_restart();
		}
	    }else{
		printf("button up\n");
		auto mode = elfletConfig->getBootMode();
		if (mode == Config::Configuration){
		    ESP_LOGI(tag, "go back to normal mode");
		    esp_restart();
		}
	    }
	}
    }
}

//----------------------------------------------------------------------
// Button chattering filter
//----------------------------------------------------------------------
class ButtonFilterTask : public Task{
protected:
    xQueueHandle evtQueue;
    
protected:
    static void IRAM_ATTR isrHandler(void* arg);
    void initButton();
    void run(void *data) override;
};

void IRAM_ATTR ButtonFilterTask::isrHandler(void* arg){
    auto obj = (ButtonFilterTask*)arg;
    uint32_t num = 0;
    xQueueSendFromISR(obj->evtQueue, &num, NULL);
}

void ButtonFilterTask::initButton(){
    evtQueue = xQueueCreate(10, sizeof(uint32_t));

    gpio_config_t conf;
    conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_ANYEDGE;
    conf.pin_bit_mask = 1ULL << GPIO_BUTTON;
    conf.mode = GPIO_MODE_INPUT;
    conf.pull_down_en = (gpio_pulldown_t)0;
    conf.pull_up_en = (gpio_pullup_t)0;
    gpio_config(&conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add((gpio_num_t)GPIO_BUTTON, isrHandler, this);
}

void ButtonFilterTask::run(void *data){
    initButton();

    while (true){
	uint32_t num;
	auto level = 0;
        if(xQueueReceive(evtQueue, &num, portMAX_DELAY)) {
	    auto now = gpio_get_level((gpio_num_t)GPIO_BUTTON);
	    if (now != level){
		vTaskDelay(FILTER_DELAY);
		if (now == gpio_get_level((gpio_num_t)GPIO_BUTTON)){
		    level = now;
		    eventTask->sendEvent(level != 0);
		}
	    }
	}
    }
}


//----------------------------------------------------------------------
// Set up button function
//----------------------------------------------------------------------
bool startButtonService(){
    if (!eventTask){
	eventTask = new ButtonEventTask;
	eventTask->start();
    }
    
    if (!filterTask){
	filterTask = new ButtonFilterTask;
	filterTask->start();
    }
	    
    return true;
}
