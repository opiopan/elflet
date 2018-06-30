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
#include "PubSubService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "IRService";

class RecieverTask;
class TransmitterTask;

static RecieverTask* rxTask;
static TransmitterTask* txTask;

static const std::string JSON_FORMATED = "FormatedIRStream";
static const std::string JSON_PROTOCOL = "Protocol";
static const std::string JSON_BITCOUNT = "BitCount";
static const std::string JSON_DATA = "Data";
static const std::string JSON_RAW = "RawIRStream";
static const std::string JSON_LEVEL = "Level";
static const std::string JSON_DURATION = "Duration";

//----------------------------------------------------------------------
// transmitter task inmprementation
//----------------------------------------------------------------------
class TransmitterTask : public Task {
protected:
    enum Status{ST_IDLE, ST_RUNNING};
    static const int EV_WAKE_SERVER = 1;

    Status status;
    Mutex mutex;
    EventGroupHandle_t events;
    IRRC irContext;
    IRRC_PROTOCOL protocol;
    const uint8_t* data;
    int32_t bits;

public:
    TransmitterTask();
    virtual ~TransmitterTask();

    bool sendData(IRRC_PROTOCOL protocol, int32_t bits, const uint8_t* data);

protected:
    void run(void *data) override;
};

TransmitterTask::TransmitterTask(): status(ST_IDLE){
    events = xEventGroupCreate();
    IRRCInit(&irContext, IRRC_TX, IRRC_NEC, GPIO_IRLED);
}

TransmitterTask::~TransmitterTask(){
    IRRCDeinit(&irContext);
}

bool TransmitterTask::sendData(IRRC_PROTOCOL protocol,
			       int32_t bits, const uint8_t* data){
    
    LockHolder holder(mutex);
    if (status != ST_IDLE){
	return false;
    }
    status = ST_RUNNING;
    this->protocol = protocol;
    this->bits = bits;
    this->data = data;
    xEventGroupSetBits(events, EV_WAKE_SERVER);
    return true;
}

void TransmitterTask::run(void *){
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

	IRRCChangeProtocol(&irContext, protocol);
	IRRCSend(&irContext, data, bits);

	mutex.lock();
	status = ST_IDLE;
	mutex.unlock();
    }
}


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

bool RecieverTask::getRecievedDataRaw(
    const rmt_item32_t** data, int32_t* length){
    *data = IRRC_ITEMS(&irContext);
    *length = IRRC_ITEM_LENGTH(&irContext);
    return true;
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
	if (IRRCRecieve(&irContext, 30 * 1000)){
	    publishIrrcData();
	}
	ledSetBlinkMode(LEDBM_DEFAULT);

	mutex.lock();
	status = ST_IDLE;
	mutex.unlock();
    }
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
    txTask = new TransmitterTask;
    txTask->start();
    
    return true;;
}

bool sendIRData(IRRC_PROTOCOL protocol, int32_t bits, const uint8_t* data){
    txTask->sendData(protocol, bits, data);
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

bool getIRRecievedDataJson(std::ostream& out){
    uint8_t buf[32];
    int32_t bits = sizeof(buf) * 8;
    IRRC_PROTOCOL protocol;

    if (getIRRecievedData(&protocol, &bits, buf)){
	if (protocol == IRRC_UNKNOWN){
	    out << "{\"Protocol\" : \"UNKNOWN\"}";
	}else{
	    char hexstr[65];
	    for (int i = 0; i < ((bits + 7) / 8) * 2; i++){
		int data = (buf[i/2] >> (i & 1 ? 0 : 4)) & 0xf;
		static const unsigned char dic[] = "0123456789abcdef";
		hexstr[i] = dic[data];
	    }
	    hexstr[((bits + 7) / 8) * 2] = 0;
		    
	    out << "{\"" << JSON_FORMATED << "\":{\""
		<< JSON_PROTOCOL << "\":\""
		<< (protocol == IRRC_NEC ? "NEC" :
		    protocol == IRRC_AEHA ? "AEHA" : "SONY")
		<< "\",\"" << JSON_BITCOUNT << "\":" << bits << ",\""
		<< JSON_DATA << "\":\"" << hexstr << "\"}}";
	}
	return true;
    }else{
	return false;
    }
}

bool getIRRecievedDataRawJson(std::ostream& out){
    const rmt_item32_t* items;
    int32_t itemNum;
    if (getIRRecievedDataRaw(&items, &itemNum)){
	out << "{\"" << JSON_RAW << "\":[";
	for (int i = 0; i < itemNum; i++){
	    if (i > 0){
		out << ",";
	    }
	    out << "{\"" << JSON_LEVEL << "\":1,\""
		<< JSON_DURATION << "\":" << items[i].duration0 << "},{\""
		<< JSON_LEVEL << "\":0,\""
		<< JSON_DURATION << "\":" << items[i].duration1 << "}";
	}
	out << "]}";
	return true;
    }else{
	return false;
    }
}

