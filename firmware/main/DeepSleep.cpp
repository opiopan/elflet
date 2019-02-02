#include <esp_log.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <esp_log.h>
#include <esp32/ulp.h>
#include <time.h>
#include <sys/time.h>
#include <GeneralUtils.h>
#include <iostream>
#include <sstream>
#include <functional>
#include <Task.h>
#include "Mutex.h"
#include "Config.h"
#include "DeepSleep.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "DeepSleep";

static RTC_DATA_ATTR struct timeval sleepEnterTime;
static timeval now;

WakeupCause initDeepSleep(){
    gettimeofday(&now, NULL);
    
    switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT1:
        ESP_LOGI(tag, "wake up by pussing button");
        return WC_BUTTON;
    case ESP_SLEEP_WAKEUP_TIMER:
        ESP_LOGI(tag, "wake up by timter");
        return WC_TIMER;
    default:
        ESP_LOGI(tag, "normal power on state");
        return WC_NOTSLEEP;
    }
}

int32_t getSleepTimeMs(){
    return (now.tv_sec - sleepEnterTime.tv_sec) * 1000 +
        (now.tv_usec - sleepEnterTime.tv_usec) / 1000;;
}

static Mutex mutex;
static int suspending = 0;
static bool mustEnterDeepSleep = false;

static void transitToDeepSleepMode(){
    ESP_LOGI(tag, "enter deep sleep state: wake up in %d sec",
             elfletConfig->getSensorFrequency());
    vTaskDelay(20 / portTICK_PERIOD_MS);
    gettimeofday(&sleepEnterTime, NULL);
    esp_deep_sleep_start();
}

void enterDeepSleep(int32_t ms){
    esp_sleep_enable_timer_wakeup(
        elfletConfig->getSensorFrequency() * 1000000);

    const int gpio = GPIO_BUTTON;;
    const uint64_t gpioMask = 1ULL << gpio;
    esp_sleep_enable_ext1_wakeup(gpioMask, ESP_EXT1_WAKEUP_ANY_HIGH);

    vTaskDelay(ms / portTICK_PERIOD_MS);

    mutex.lock();
    mustEnterDeepSleep = true;;
    if (suspending){
        mutex.unlock();
        vTaskDelay(portMAX_DELAY);
    }
    transitToDeepSleepMode();
}

void suspendEnterDeepSleep(){
    ESP_LOGI(tag, "suspend to enter deep sleep");
    LockHolder holder(mutex);
    suspending++;
}

void resumeEnterDeepSleep(){
    LockHolder holder(mutex);
    suspending--;
    if (suspending == 0 && mustEnterDeepSleep){
        transitToDeepSleepMode();
    }
}
