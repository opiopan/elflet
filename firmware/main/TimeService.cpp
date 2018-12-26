#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <GeneralUtils.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include "Mutex.h"
#include "Config.h"
#include "TimeObj.h"
#include "SensorService.h"
#include "TimeService.h"
#include "Stat.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "TimeService";

class TimeTask;

static TimeTask* task;

//----------------------------------------------------------------------
// SNTP time adjustment task
//----------------------------------------------------------------------
class TimeTask : public Task {
protected:
    static const int EV_WAKE_SERVER = 1;
    EventGroupHandle_t events;
    
public:
    TimeTask();
    virtual ~TimeTask();

    void enable();

protected:
    void run(void *data) override;
};

TimeTask::TimeTask(){
    events = xEventGroupCreate();
}

TimeTask::~TimeTask(){
}

void TimeTask::enable(){
    xEventGroupSetBits(events, EV_WAKE_SERVER);
}

void TimeTask::run(void *data){
    ESP_LOGI(tag, "timezone set to: %s", elfletConfig->getTimezone());
    Time::setTZ(elfletConfig->getTimezone());

    xEventGroupWaitBits(events, EV_WAKE_SERVER,
			pdTRUE, pdFALSE,
			portMAX_DELAY);
    
    bool first = true;
    while (true){
	if (Time::shouldAdjust(elfletConfig->getWakeupCause() != WC_NOTSLEEP)){
	    ESP_LOGI(tag, "start SNTP & wait for finish adjustment");
	    Time::startSNTP();
	    if (!Time::waitForFinishAdjustment(3)){
		ESP_LOGE(tag, "fail to adjust time by SNTP");
	    }
	    Time now;
	    ESP_LOGI(tag, "adjusted time: %s",
		     now.format(Time::SIMPLE_DATETIME));
	    if (baseStat.boottime < 60 * 60 * 24 * 365){
		baseStat.boottime = now.getTime();
	    }
	}

	if (first){
	    first = false;
	    enableSensorCapturing();
	}

	vTaskDelay(60 * 60 * 1000 / portTICK_PERIOD_MS);
    }
}

//----------------------------------------------------------------------
// module interface
//----------------------------------------------------------------------
bool startTimeService(){
    if (!task){
	task = new TimeTask;
	task->setStackSize(2048);
	task->start();
    }
    return true;
}

void notifySMTPserverAccesivility(){
    task->enable();
}
