#include <esp_log.h>
#include <esp_system.h>
#include <string.h>
#include <GeneralUtils.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include <json11.hpp>
#include "Mutex.h"
#include "Config.h"
#include "irserverProtocol.h"
#include "IRService.h"
#include "LEDService.h"
#include "PubSubService.h"
#include "ShadowDevice.h"
#include "Stat.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "IRService";

class ReceiverTask;
class TransmitterTask;

static ReceiverTask* rxTask;
static TransmitterTask* txTask;

//----------------------------------------------------------------------
// transmitter task inmprementation
//----------------------------------------------------------------------
class TransmitterTask : public Task {
protected:
    enum Status{ST_IDLE, ST_RUNNING};
    static constexpr uint32_t EV_WAKE_SERVER = 1;
    static constexpr uint32_t EV_DONE_TRANSMIT = 2;

    Status status;
    Mutex mutex;
    EventGroupHandle_t events;
    IRRC irContext;
    IRRC_PROTOCOL protocol;
    uint8_t data[IRS_REQMAXSIZE];
    int32_t bits;
    int32_t sendCount{0};

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
    mutex.lock();
    if (status != ST_IDLE){
        return false;
    }
    if (bits / 8 > sizeof(this->data)){
        return false;
    }
    auto count = sendCount;
    status = ST_RUNNING;
    this->protocol = protocol;
    this->bits = bits;
    memcpy(this->data, data, bits / 8);
    xEventGroupSetBits(events, EV_WAKE_SERVER);
    mutex.unlock();

    mutex.lock();
    while (sendCount == count){
        mutex.unlock();
        xEventGroupWaitBits(events, EV_DONE_TRANSMIT,
                            pdTRUE, pdFALSE,
                            portMAX_DELAY);
        mutex.lock();
    }
    mutex.unlock();

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
        irrcStat.protocol[protocol].sendCmds++;
        irrcStat.protocol[protocol].sendBits += bits;
        irrcStat.sendPulses += bits + 2;

        mutex.lock();
        status = ST_IDLE;
        sendCount++;
        xEventGroupSetBits(events, EV_DONE_TRANSMIT);
        mutex.unlock();
    }
}

static bool sendFormatedData(const json11::Json& obj){
    auto protocol = obj[JSON_PROTOCOL];
    auto protocolValue = IRRC_UNKNOWN;
    if (protocol.is_string()){
        auto value = protocol.string_value();
        if (value == "NEC"){
            protocolValue = IRRC_NEC;
        }else if (value == "AEHA"){
            protocolValue = IRRC_AEHA;
        }else if (value == "SONY"){
            protocolValue = IRRC_SONY;
        }
    }

    auto data = obj[JSON_DATA];
    char dataStream[IRS_REQMAXSIZE];
    auto dataLength = -1;
    if (data.is_string()){
        auto value = data.string_value();
        if (value.length() & 1 || value.length() > sizeof(dataStream) * 2){
            return false;
        }
        dataLength = value.length() / 2;

        auto hex = [](int c) -> int {
            if (c >= '0' && c <= '9'){
                return c - '0';
            }else if (c >= 'a' && c <= 'f'){
                return c - 'a' + 0xa;
            }else if (c >= 'A' && c <= 'F'){
                return c - 'A' + 0xa;
            }
            return -1;
        };

        for (auto i = 0; i < value.length(); i += 2){
            auto lh = hex(value[i]);
            auto sh = hex(value[i + 1]);
            if (lh < 0 || sh < 0){
                return false;
            }
            dataStream[i / 2] = (lh << 4) | sh;
        }
    }

    if (protocolValue == IRRC_UNKNOWN || dataLength <= 0){
        return false;
    }

    auto bitCount = obj[JSON_BITCOUNT];
    auto bitCountValue = 0;
    if (bitCount.is_number()){
        bitCountValue = bitCount.int_value();
    }
    if (bitCountValue <= 0){
        bitCountValue = dataLength * 8;
    }

    sendIRData(protocolValue, bitCountValue, (uint8_t*)dataStream);
    
    return true;
}

//----------------------------------------------------------------------
// receiver task inmprementation
//----------------------------------------------------------------------
class ReceiverTask : public Task {
protected:
    enum Status{ST_IDLE, ST_RUNNING};
    static const int EV_WAKE_SERVER = 1;

    Status status;
    Mutex mutex;
    EventGroupHandle_t events;
    IRRC irContext;

    IRRC_PROTOCOL rcvProtocol;
    int32_t rcvBits;
    uint8_t rcvBuf[48];

public:
    ReceiverTask();
    virtual ~ReceiverTask();

    void enableReceiver();
    bool startReceiver();
    bool getReceivedData(IRRC_PROTOCOL* protocol,
                         int32_t* bits, uint8_t* data);
    bool getReceivedDataRaw(const rmt_item32_t** data, int32_t* length);
    
protected:
    void run(void *data) override;
};

ReceiverTask::ReceiverTask() : status(ST_IDLE),
                               rcvProtocol(IRRC_UNKNOWN), rcvBits(0){
    events = xEventGroupCreate();
    IRRCInit(&irContext, IRRC_RX, IRRC_NEC, GPIO_IRRX);
}

ReceiverTask::~ReceiverTask(){
    IRRCDeinit(&irContext);
}

void ReceiverTask::enableReceiver(){
    if (elfletConfig->getIrrcReceiverMode() == Config::IrrcReceiverContinuous){
        startReceiver();
    }
}

bool ReceiverTask::startReceiver(){
    LockHolder holder(mutex);
    if (status != ST_IDLE){
        return false;
    }
    status = ST_RUNNING;
    xEventGroupSetBits(events, EV_WAKE_SERVER);
    return true;
}

bool ReceiverTask::getReceivedData(IRRC_PROTOCOL* protocol, int32_t* bits,
                                   uint8_t* data){
    LockHolder holder(mutex);
    *protocol = rcvProtocol;
    *bits = rcvBits;
    memcpy(data, rcvBuf, (rcvBits + 7) / 8);
    return true;
}

bool ReceiverTask::getReceivedDataRaw(
    const rmt_item32_t** data, int32_t* length){
    *data = IRRC_ITEMS(&irContext);
    *length = IRRC_ITEM_LENGTH(&irContext);
    return true;
}

void ReceiverTask::run(void *data){
    auto isContinuousMode =
        elfletConfig->getIrrcReceiverMode() == Config::IrrcReceiverContinuous;
    if (isContinuousMode){
        IRRC_SET_OPT(&irContext, IRRC_OPT_CONTINUOUS);
    }
    
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

        if (!isContinuousMode){
            ledSetBlinkMode(LEDBM_IRRX);
        }
        if (IRRCReceive(&irContext, 30 * 1000)){
            mutex.lock();
            auto pulses = IRRC_ITEM_LENGTH(&irContext);
            irrcStat.rcvPulses += pulses;
            rcvBits = sizeof(rcvBuf) * 8;
            if (irContext.protocol == IRRC_UNKNOWN ||
                !IRRCDecodeReceivedData(&irContext,
                                        &rcvProtocol, rcvBuf, &rcvBits)){
                irrcStat.rcvUnknownCmds++;
                irrcStat.rcvUnknownPulses += pulses;
                rcvProtocol = IRRC_UNKNOWN;
                rcvBits = 0;
            }else if (rcvBits > 0){
                ESP_LOGI(tag, "received IR command [%s : %d bits]",
                         rcvProtocol == IRRC_NEC ? "NEC" :
                         rcvProtocol == IRRC_AEHA ? "AEHA" :
                         rcvProtocol == IRRC_SONY ? "SONY" :
                                                    "Unknown",
                         rcvBits);
                irrcStat.protocol[rcvProtocol].rcvCmds++;
                irrcStat.protocol[rcvProtocol].rcvBits += rcvBits;
                if (elfletConfig->getIrrcReceiverMode() ==
                    Config::IrrcReceiverContinuous){
                    IRCommand cmd;
                    cmd.protocol = rcvProtocol;
                    cmd.bits = rcvBits;
                    cmd.data = rcvBuf;
                    applyIRCommandToShadow(&cmd);
                }
            }
            //ESP_LOG_BUFFER_HEX(tag, rcvBuf, (rcvBits + 7) / 8);
            mutex.unlock();
            if (rcvBits > 0){
                publishIrrcData();
            }
        }
        if (!isContinuousMode){
            ledSetBlinkMode(LEDBM_DEFAULT);
            mutex.lock();
            status = ST_IDLE;
            mutex.unlock();
        }
    }
}

//----------------------------------------------------------------------
// interfaces for outer module
//----------------------------------------------------------------------
bool startIRService(){
    if (rxTask || txTask){
        return false;
    }

    initShadowDevicePool();

    rxTask = new ReceiverTask;
    rxTask->start();
    txTask = new TransmitterTask;
    txTask->start();
    
    return true;;
}

bool sendIRData(IRRC_PROTOCOL protocol, int32_t bits, const uint8_t* data){
    txTask->sendData(protocol, bits, data);
    return false;
}

bool sendIRDataJson(const WebString& data){
    std::string err;
    auto body =
        json11::Json::parse(std::string(data.data(), data.length()), err);

    if (body.is_object()){
        auto formatedData = body[JSON_FORMATED];
        bool rc = false;
        if (formatedData.is_object()){
            rc = sendFormatedData(json11::Json(formatedData.object_items()));
        }else{
            rc = sendFormatedData(body);
        }
        return rc;
    }

    return false;
}

void enableIRReceiver(){
    if (rxTask){
        rxTask->enableReceiver();
    }
}

bool startIRReceiver(){
    return rxTask->startReceiver();
}
    
bool getIRReceivedData(IRRC_PROTOCOL* protocol, int32_t* bits, uint8_t* data){
    return rxTask->getReceivedData(protocol, bits, data);
}

bool getIRReceivedDataRaw(const rmt_item32_t** data, int32_t* length){
    return rxTask->getReceivedDataRaw(data, length);
}

bool getIRReceivedDataJson(std::ostream& out){
    uint8_t buf[48];
    int32_t bits = sizeof(buf) * 8;
    IRRC_PROTOCOL protocol;

    if (getIRReceivedData(&protocol, &bits, buf)){
        if (protocol == IRRC_UNKNOWN){
            out << "{\"Protocol\" : \"UNKNOWN\"}";
        }else{
            char hexstr[sizeof(buf) * 2 + 1];
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

bool getIRReceivedDataRawJson(std::ostream& out){
    const rmt_item32_t* items;
    int32_t itemNum;
    if (getIRReceivedDataRaw(&items, &itemNum)){
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
