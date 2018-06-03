#include <esp_log.h>
#include <esp_system.h>
#include <driver/ledc.h>
#include <string.h>
#include <GeneralUtils.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include "Mutex.h"
#include "Config.h"
#include "LEDService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "LEDService";

class LEDTask;
static LEDTask* ledTask;

//----------------------------------------------------------------------
// LED blink sequense definitions
//----------------------------------------------------------------------
enum LED {LED_RED = 0, LED_GREEN = 1, LED_BLUE = 2};

enum SEQ_UNIT_KIND {SU_FADE, SU_WAIT, SU_LOOP, SU_END};

struct BlinkSeqUnit{
    SEQ_UNIT_KIND kind;
    uint16_t duration;
    uint16_t duty[3];
};

#define SEQ_FADE(d, r, g, b) {SU_FADE, (d), {(r), (g), (b)}}
#define SEQ_WAIT(d) {SU_WAIT, (d), {0, 0, 0}}
#define SEQ_END {SU_END, 0, {0, 0, 0}}
#define SEQ_LOOP {SU_LOOP, 0, {0, 0, 0}}

BlinkSeqUnit SEQ_STANDBY[] = {
    SEQ_FADE(300, 0, 0, 0),
    SEQ_END
};

BlinkSeqUnit SEQ_SCAN_WIFI[] = {
    SEQ_FADE(250, 600, 600, 600),
    SEQ_FADE(250, 0, 0, 0),
    SEQ_WAIT(1000),
    SEQ_LOOP
};

BlinkSeqUnit SEQ_CONFIGURATION[] = {
    SEQ_FADE(250, 600, 300, 0),
    SEQ_FADE(250, 0, 0, 0),
    SEQ_WAIT(1000),
    SEQ_LOOP
};

BlinkSeqUnit SEQ_FACTORY_RESET[] = {
    SEQ_FADE(125, 600, 300, 0),
    SEQ_FADE(125, 0, 0, 0),
    SEQ_FADE(125, 600, 355, 0),
    SEQ_FADE(125, 0, 0, 0),
    SEQ_WAIT(1000),
    SEQ_LOOP
};


BlinkSeqUnit SEQ_SYSTEM_FAULT[] = {
    SEQ_FADE(300, 2000, 0, 0),
    SEQ_END
};

BlinkSeqUnit SEQ_IRRX[] = {
    SEQ_FADE(250, 0, 461, 600),
    SEQ_FADE(250, 0, 0, 0),
    SEQ_WAIT(1000),
    SEQ_LOOP
};


BlinkSeqUnit* defaultSequences[] = {
    SEQ_STANDBY, SEQ_SCAN_WIFI, SEQ_CONFIGURATION, SEQ_FACTORY_RESET
};

BlinkSeqUnit* otherSequences[] = {
    SEQ_SYSTEM_FAULT, SEQ_IRRX
};

//----------------------------------------------------------------------
// LED control task definition & implementation
//----------------------------------------------------------------------
class LEDTask : public Task {
protected:
    static const auto MODE_CHANGE_EVENT = 1;
    static const auto FADE_END_EVENT = 2;
    
    EventGroupHandle_t events;
    LED_DefaultMode defaultMode;
    LED_BlinkMode blinkMode;
    Mutex mutex;
    ledc_channel_config_t ledConfigs[3];
    ledc_isr_handle_t isrHandle;
    ledc_timer_config_t timerConfig;

public:
    LEDTask();
    virtual ~LEDTask();

    bool startService();

    void setDefaultMode(LED_DefaultMode mode);
    void setBlinkMode(LED_BlinkMode mode);
    
protected:
    static void IRAM_ATTR isrHandler(void* arg);
    void initLED();
    void run(void *data) override;
};

LEDTask::LEDTask() : defaultMode(LEDDM_STANDBY), blinkMode(LEDBM_DEFAULT) {
};

LEDTask::~LEDTask(){
};

bool LEDTask::startService(){
    events = xEventGroupCreate();
    start();
    return true;
}

void LEDTask::setDefaultMode(LED_DefaultMode mode){
    LockHolder holder(mutex);
    defaultMode = mode;
    xEventGroupSetBits(events, MODE_CHANGE_EVENT);
}

void LEDTask::setBlinkMode(LED_BlinkMode mode){
    LockHolder holder(mutex);
    blinkMode = mode;
    xEventGroupSetBits(events, MODE_CHANGE_EVENT);
}

void IRAM_ATTR LEDTask::isrHandler(void* arg){
    auto task = (LEDTask*)arg;
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    auto rc = xEventGroupSetBitsFromISR(task->events,
					FADE_END_EVENT,
					&higherPriorityTaskWoken);
    if (rc == pdPASS){
	//portYIELD_FROM_ISR(higherPriorityTaskWoken);
	portYIELD_FROM_ISR();
    }
}

void LEDTask::run(void *data){
    initLED();
    
    LED_DefaultMode defaultMode = LEDDM_STANDBY;;
    LED_BlinkMode blinkMode = LEDBM_DEFAULT;
    BlinkSeqUnit* sequence = NULL;
    int unitIndex = 0;

    auto reflectNewMode = [&](int timeToWait) -> bool {
	auto ev = xEventGroupWaitBits(
	    events, MODE_CHANGE_EVENT, pdTRUE, pdFALSE, timeToWait);
	LockHolder holder(this->mutex);
	if (ev & MODE_CHANGE_EVENT &&
	    (blinkMode != this->blinkMode ||
	     (blinkMode == LEDBM_DEFAULT &&
	      defaultMode != this->defaultMode))){
	    blinkMode = this->blinkMode;
	    defaultMode = this->defaultMode;
	    sequence = (blinkMode == LEDBM_DEFAULT) ?
				defaultSequences[defaultMode] :
		                otherSequences[blinkMode];
	    unitIndex = 0;
	    xEventGroupClearBits(events, ev);
	    return true;
	}
	return false;
    };

    reflectNewMode(portMAX_DELAY);

    while (true){
	auto unit = sequence[unitIndex];
	switch (unit.kind){
	case SU_FADE:
	    for (int i = 0; i < 3; i++){
		ledc_set_fade_with_time(
		    ledConfigs[i].speed_mode, ledConfigs[i].channel,
		    unit.duty[i], unit.duration);
		ledc_fade_start(
		    ledConfigs[i].speed_mode, ledConfigs[i].channel,
		    i < 3 ? LEDC_FADE_NO_WAIT : LEDC_FADE_WAIT_DONE);
		
	    }
	    vTaskDelay((unit.duration + 50)/ portTICK_PERIOD_MS);
	    unitIndex++;
	    reflectNewMode(0);
	    break;
	case SU_WAIT:
	    if (!reflectNewMode(unit.duration / portTICK_PERIOD_MS)){
		unitIndex++;
	    }
	    break;
	case SU_LOOP:
	    unitIndex = 0;
	    break;
	case SU_END:
	    reflectNewMode(portMAX_DELAY);
	    break;
	}
    }
}


void LEDTask::initLED(){
    memset(&timerConfig, 0, sizeof(timerConfig));
    timerConfig.duty_resolution = LEDC_TIMER_13_BIT;
    timerConfig.freq_hz = 5000;
    timerConfig.speed_mode = LEDC_HIGH_SPEED_MODE;
    timerConfig.timer_num = LEDC_TIMER_0;
    ledc_timer_config(&timerConfig);

    memset(&ledConfigs, 0, sizeof(ledConfigs));
    ledConfigs[LED_RED].channel    = LEDC_CHANNEL_0;
    ledConfigs[LED_RED].gpio_num   = GPIO_LEDR;
    ledConfigs[LED_RED].speed_mode = LEDC_HIGH_SPEED_MODE;
    ledConfigs[LED_RED].duty       = 0;
    ledConfigs[LED_RED].intr_type  = LEDC_INTR_DISABLE;
    ledConfigs[LED_RED].timer_sel  = LEDC_TIMER_0;
    ledc_channel_config(&ledConfigs[LED_RED]);
    
    ledConfigs[LED_GREEN].channel    = LEDC_CHANNEL_1;
    ledConfigs[LED_GREEN].gpio_num   = GPIO_LEDG;
    ledConfigs[LED_GREEN].speed_mode = LEDC_HIGH_SPEED_MODE;
    ledConfigs[LED_GREEN].duty       = 0;
    ledConfigs[LED_GREEN].intr_type  = LEDC_INTR_DISABLE;
    ledConfigs[LED_GREEN].timer_sel  = LEDC_TIMER_0;
    ledc_channel_config(&ledConfigs[LED_GREEN]);

    ledConfigs[LED_BLUE].channel    = LEDC_CHANNEL_2;
    ledConfigs[LED_BLUE].gpio_num   = GPIO_LEDB;
    ledConfigs[LED_BLUE].speed_mode = LEDC_HIGH_SPEED_MODE;
    ledConfigs[LED_BLUE].duty       = 0;
    ledConfigs[LED_BLUE].intr_type  = LEDC_INTR_DISABLE;
    ledConfigs[LED_BLUE].timer_sel  = LEDC_TIMER_0;
    ledc_channel_config(&ledConfigs[LED_BLUE]);

    /*
    ledc_isr_register(isrHandler, this,
		      ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_IRAM,
		      &isrHandle);
    */
    
    ledc_fade_func_install(0);
}

//----------------------------------------------------------------------
// externed interface
//----------------------------------------------------------------------
bool startLedService(){
    if (ledTask == NULL){
	ledTask = new LEDTask;
	return ledTask->startService();
    }
    
    return true;
}

void ledSetDefaultMode(enum LED_DefaultMode mode){
    ledTask->setDefaultMode(mode);
}

void ledSetBlinkMode(enum LED_BlinkMode mode){
    ledTask->setBlinkMode(mode);
}

