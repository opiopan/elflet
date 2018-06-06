#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <GeneralUtils.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include "Mutex.h"
#include "Config.h"
#include "IRService.h"
#include "LEDService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "IRService";

class RecieverTask;
class TransmitterTask;

static RecieverTask* rxTask;
static TransmitterTask* txTask;

//----------------------------------------------------------------------
// reciever task inmprementation
//----------------------------------------------------------------------
class RecieverTask : public Task {
protected:
    enum Status{ST_IDLE, ST_RUNNING};
    static const int EV_WAKE_SERVER = 1;

    Status status;
    Mutex mutex;
    EventGroupHandle_t events;
    IRRC irContext;

public:
    RecieverTask();
    virtual ~RecieverTask();

    bool startReciever();
    bool getRecievedData(IRRC_PROTOCOL* protocol,
			 int32_t* bits, uint8_t* data);
    bool getRecievedDataRaw(const rmt_item32_t** data, int32_t* length);
    
protected:
    void run(void *data) override;
};

RecieverTask::RecieverTask() : status(ST_IDLE){
    events = xEventGroupCreate();
    IRRCInit(&irContext, IRRC_RX, IRRC_NEC, GPIO_IRRX);
}

RecieverTask::~RecieverTask(){
    IRRCDeinit(&irContext);
}

bool RecieverTask::startReciever(){
    LockHolder holder(mutex);
    if (status != ST_IDLE){
	return false;
    }
    status = ST_RUNNING;
    xEventGroupSetBits(events, EV_WAKE_SERVER);
    return true;
}

bool RecieverTask::getRecievedData(IRRC_PROTOCOL* protocol, int32_t* bits,
				   uint8_t* data){
    LockHolder holder(mutex);
    if (status != ST_IDLE){
	return false;
    }
    return IRRCDecodeRecievedData(&irContext, protocol, data, bits);
}

void RecieverTask::run(void *data){
    while (true){
	mutex.lock();
	while (status == ST_IDLE){
	    mutex.unlock();
	    xEventGroupWaitBits(events, EV_WAKE_SERVER,
				pdTRUE, pdFALSE,
				portMAX_DELAY);
	    mutex.lock();
	}
	mutex.unlock();

	ledSetBlinkMode(LEDBM_IRRX);
	IRRCRecieve(&irContext, 30 * 1000);
	ledSetBlinkMode(LEDBM_DEFAULT);

	mutex.lock();
	status = ST_IDLE;
	mutex.unlock();
    }
}

bool RecieverTask::getRecievedDataRaw(
    const rmt_item32_t** data, int32_t* length){
    *data = IRRC_ITEMS(&irContext);
    *length = IRRC_ITEM_LENGTH(&irContext);
    return true;
}


//----------------------------------------------------------------------
// interfaces for outer module
//----------------------------------------------------------------------
bool startIRService(){
    if (rxTask || txTask){
	return false;
    }

    rxTask = new RecieverTask;
    rxTask->start();
    
    return true;;
}

bool sendIRData(IRRC_PROTOCOL protocol, int32_t bits, const uint8_t* data){
    return false;
}

bool startIRReciever(){
    return rxTask->startReciever();
}
    
bool getIRRecievedData(IRRC_PROTOCOL* protocol, int32_t* bits, uint8_t* data){
    return rxTask->getRecievedData(protocol, bits, data);
}

bool getIRRecievedDataRaw(const rmt_item32_t** data, int32_t* length){
    return rxTask->getRecievedDataRaw(data, length);
}
