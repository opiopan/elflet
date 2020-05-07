#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <GeneralUtils.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include "Mutex.h"
#include "Config.h"
#include "IRService.h"
#include "BleHidService.h"
#include "ButtonService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "ButtonService";

#define FILTER_DELAY (30 / portTICK_PERIOD_MS)

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
    //bool level = gpio_get_level((gpio_num_t)GPIO_BUTTON) != 0;
    bool level = 0;
    auto waitForEvent = [&](int timeToWait) -> bool {
        auto ev = xEventGroupWaitBits(events, 1, pdTRUE, pdFALSE, timeToWait);
        LockHolder holder(this->mutex);
        level = this->level;
        return ev != 0;
    };

    while (true){
        while (!level){
            waitForEvent(portMAX_DELAY);
        }
        ESP_LOGI(tag, "button down event detected");
        while (level){
            if (!waitForEvent(5 * 1000 / portTICK_PERIOD_MS)){
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
                ESP_LOGI(tag, "button up event detected");
                auto mode = elfletConfig->getBootMode();
                if (mode == Config::Normal){
                    if (elfletConfig->getWakeupCause() == WC_BUTTON &&
                        elfletConfig->getFunctionMode() == Config::SensorOnly){
                        //enterDeepSleep(500);
                        esp_restart();
                    }else if (elfletConfig->getFunctionMode() ==
                              Config::FullSpec){
                        auto bmode = elfletConfig->getButtonMode();
                        if (bmode == Config::ButtonIrrcReciever){
                            ESP_LOGI(tag, "start IR receiver");
                            startIRReceiver();
                        }else{
                            ESP_LOGI(tag, "sent HID code");
                            auto code = elfletConfig->getButtonBleHidCode();
                            bleHidSendCode(&code);
                        }
                    }
                }else if (mode == Config::Configuration){
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
    vTaskDelay(2000 / portTICK_PERIOD_MS);

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

    auto level = gpio_get_level((gpio_num_t)GPIO_BUTTON);
    while (true){
        uint32_t num;
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
    if (!eventTask || !filterTask){
        filterTask = new ButtonFilterTask;
        filterTask->setStackSize(2048);
        eventTask = new ButtonEventTask;
        eventTask->setStackSize(4096);
        filterTask->start();
        eventTask->start();
    }
            
    return true;
}
