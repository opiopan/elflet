#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <iostream>
#include <GeneralUtils.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include "Mutex.h"
#include "TimeObj.h"
#include "Config.h"
#include "SensorService.h"
#include "bme280.h"
#include "PubSubService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "SensorService";

static const char JSON_DATE[] = "date";
static const char JSON_TEMP_UNIT[] = "temperatureUnit";
static const char JSON_HUM_UNIT[] = "humidityUnit";
static const char JSON_PRESS_UNIT[] = "pressureUnit";
static const char JSON_LUMINO_UNIT[] = "luminosityeUnit";
static const char JSON_TEMP[] = "temperature";
static const char JSON_HUM[] = "humidity";
static const char JSON_PRESS[] = "pressure";
static const char JSON_LUMINO[] = "luminositye";

class SensorTask;
static SensorTask* sensorTask;

//----------------------------------------------------------------------
// Sensor observation task
//----------------------------------------------------------------------
class SensorTask : public Task {
protected:
    static const int EV_WAKE_SERVER = 1;
    EventGroupHandle_t events;
    Mutex mutex;
    
    BME280_I2C* bme280;

    int enableFlag;
    Time captureTime;

    int32_t temperature;
    int32_t humidity;
    int32_t pressure;
    uint32_t luminosity;

public:
    SensorTask();
    virtual ~SensorTask();

    void enableCapture();
    void getValue(SensorValue* value);

protected:
    void run(void *data) override;
};

SensorTask::SensorTask() : enableFlag(0){
    events = xEventGroupCreate();

    bme280 = new BME280_I2C(0x77,
			    (gpio_num_t)GPIO_I2C_SDA, (gpio_num_t)GPIO_I2C_SCL,
			    I2C::DEFAULT_CLK_SPEED,
			    I2C_NUM_0, false);
    bme280->init();
}

SensorTask::~SensorTask(){
    delete bme280;
}

void SensorTask::enableCapture(){
    xEventGroupSetBits(events, EV_WAKE_SERVER);
}

void SensorTask::getValue(SensorValue* value){
    LockHolder holder(mutex);
    value->enableFlag = enableFlag;
    value->temperature = temperature;
    value->humidity = humidity;
    value->pressure = pressure;
    value->luminosity = luminosity;
}

void SensorTask::run(void *data){
    xEventGroupWaitBits(events, EV_WAKE_SERVER,
			pdTRUE, pdFALSE,
			portMAX_DELAY);

    int workingSensors = 0;
    if (bme280->isWorking()){
	workingSensors = 
		SensorValue::TEMPERATURE |
		SensorValue::HUMIDITY |
		SensorValue::PRESSURE;
    }
    
    while (true){
	if (bme280->isWorking()){
	    bme280->start(true);
	    vTaskDelay(100 / portTICK_PERIOD_MS);
	    bme280->measure();
	    Time now;
	}

	{
	    LockHolder holder(mutex);
	    enableFlag = workingSensors;
	    temperature = bme280->getTemperature();
	    humidity = bme280->getHumidity();
	    pressure = bme280->getPressure();
	}

	publishSensorData();
	
	/*
	printf("%s Temp[%.1f dig] Hum[%.1f %%] Press[%.1f hPa]\n",
	       now.format(Time::SIMPLE_DATETIME),
	       bme280->getTemperatureFloat(),
	       bme280->getHumidityFloat(),
	       bme280->getPressureFloat());
	*/

	vTaskDelay(
	    elfletConfig->getSensorFrequency() * 1000 / portTICK_PERIOD_MS);
    }

    while (true){
    }
};

//----------------------------------------------------------------------
// interfaces for outer module
//----------------------------------------------------------------------
bool startSensorService(){
    if (!sensorTask){
	sensorTask = new SensorTask();
	sensorTask->start();
    }

    return true;
}

void enableSensorCapturing(){
    sensorTask->enableCapture();
}

void getSensorValue(SensorValue* value){
    sensorTask->getValue(value);
}

void getSensorValueAsJson(std::ostream& out){
    SensorValue value;
    getSensorValue(&value);

    bool first = true;
    auto sep = [&]() -> void {
	if (first){
	    first = false;
	}else{
	    out << ",";
	}
    };

    out << "{";
    if (value.enableFlag != 0){
	sep();
	out << "\"" << JSON_DATE << "\":\""
	    << value.captureTime.format(Time::RFC1123) << "\"";
    }
    if (value.enableFlag & SensorValue::TEMPERATURE){
	sep();
	out << "\"" << JSON_TEMP_UNIT << "\":\"celsius\",\""
	    << JSON_TEMP << "\":"
	    << value.temperatureFloat();
    }
    if (value.enableFlag & SensorValue::HUMIDITY){
	sep();
	out << "\"" << JSON_HUM_UNIT << "\":\"percentage\",\""
	    << JSON_HUM << "\":"
	    << value.humidityFloat();
    }
    if (value.enableFlag & SensorValue::PRESSURE){
	sep();
	out << "\"" << JSON_PRESS_UNIT << "\":\"hPa\",\""
	    << JSON_PRESS << "\":"
	    << value.pressureFloat();
    }
    out << "}";
}
