#include <esp_log.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <esp_log.h>
#include <esp32/ulp.h>
#include <GeneralUtils.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "irrc.h"
#include "TimeObj.h"
#include "Config.h"
#include "Stat.h"
#include "WebService.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "Stat";

static const char JSON_STAT_UPTIME[] = "UpTime";
static const char JSON_STAT_DEEPSLEEP[] = "DeepSleepCount";
static const char JSON_STAT_OTACOUNT[] = "FirmwareUpdateCount";

static const char JSON_STAT_HTTP[] = "Http";
static const char JSON_STAT_HTTP_SESSION[] = "SessionCount";

static const char JSON_STAT_IRRC[] = "IRRC";
static const char JSON_STAT_IRRC_RPULSE[] = "RcvPulses";
static const char JSON_STAT_IRRC_SPULSE[] = "SndPulses";
static const char JSON_STAT_IRRC_RCMD_NEC[] = "NecRcvCommands";
static const char JSON_STAT_IRRC_SCMD_NEC[] = "NecSndCommands";
static const char JSON_STAT_IRRC_RBIT_NEC[] = "NecRcvBits";
static const char JSON_STAT_IRRC_SBIT_NEC[] = "NecSndBits";
static const char JSON_STAT_IRRC_RCMD_SONY[] = "SonyRcvCommands";
static const char JSON_STAT_IRRC_SCMD_SONY[] = "SonySndCommands";
static const char JSON_STAT_IRRC_RBIT_SONY[] = "SonyRcvBits";
static const char JSON_STAT_IRRC_SBIT_SONY[] = "SonySndBits";
static const char JSON_STAT_IRRC_RCMD_AEHA[] = "AehaRcvCommands";
static const char JSON_STAT_IRRC_SCMD_AEHA[] = "AehaSndCommands";
static const char JSON_STAT_IRRC_RBIT_AEHA[] = "AehaRcvBits";
static const char JSON_STAT_IRRC_SBIT_AEHA[] = "AehaSndBits";
static const char JSON_STAT_IRRC_RCMD_UNKNOWN[] = "UnknownRcvCommands";
static const char JSON_STAT_IRRC_RPULSE_UNKNOWN[] = "UnknownRcvPulses";

static const char JSON_STAT_PUBSUB[] = "PubSub";
static const char JSON_STAT_PUBSUB_PUBSENSOR[] = "PublishSensorCount";
static const char JSON_STAT_PUBSUB_PUBIRRC[] = "PublishIrrcCount";
static const char JSON_STAT_PUBSUB_PUBSHADOW[] = "PublishShadowCount";

static const char JSON_STAT_SENSOR[] = "Sensor";
static const char JSON_STAT_SENSOR_BME280[] = "BME280CaptureCount";
static const char JSON_STAT_SENSOR_LUMINO[] = "LuminocityCaptureCount";

static const char JSON_STAT_SHADOW[] = "Shadow";
static const char JSON_STAT_SHADOW_RECOGNIZE[] = "RecognizeCount";
static const char JSON_STAT_SHADOW_SYNTHESIZE[] = "SynthesizeCount";

extern "C" {
    RTC_DATA_ATTR struct BaseStat baseStat = {0,0};
    struct IrrcStat irrcStat;
    struct PubSubStat pubsubStat;
    struct SensorStat sensorStat;
    struct ShadowStat shadowStat;
};

const json11::Json::object getStatisticsJson(){
    Time now;
    int32_t elapse = now.getTime() - baseStat.boottime;
    int32_t day = elapse / 24 / 60 / 60;
    int32_t rhour = elapse - day * 24 * 60 * 60;
    int32_t hour = rhour / 60 / 60;
    int32_t rmin = rhour - hour * 60 * 60;
    int32_t min = rmin / 60;
    int32_t sec = rmin - min * 60;
    std::stringstream out;
    if (day){
	out << day << " days ,";
    }
    if (day || hour){
	out << std::setfill('0');
	out << std::setw(2) << hour << ":" << std::setw(2) << min;
    }else if (min){
	out << min << " minutes, " << sec << " seconds";
    }else{
	out << sec << " seconds";
    }
    
    json11::Json::object root({
	    {JSON_STAT_UPTIME, out.str()},
	    {JSON_STAT_DEEPSLEEP, baseStat.deepSleepCount},
	    {JSON_STAT_OTACOUNT, elfletConfig->getOtaCount()},
	});
    json11::Json::object http({
	    {JSON_STAT_HTTP_SESSION, getWebStat()->sessionCount},
	});
    json11::Json::object irrc({
	    {JSON_STAT_IRRC_RPULSE, irrcStat.rcvPulses},
	    {JSON_STAT_IRRC_SPULSE, irrcStat.sendPulses},
	    {JSON_STAT_IRRC_RCMD_NEC, irrcStat.protocol[IRRC_NEC].rcvCmds},
	    {JSON_STAT_IRRC_SCMD_NEC, irrcStat.protocol[IRRC_NEC].sendCmds},
	    {JSON_STAT_IRRC_RBIT_NEC, irrcStat.protocol[IRRC_NEC].rcvBits},
	    {JSON_STAT_IRRC_SBIT_NEC, irrcStat.protocol[IRRC_NEC].sendBits},
	    {JSON_STAT_IRRC_RCMD_SONY, irrcStat.protocol[IRRC_SONY].rcvCmds},
	    {JSON_STAT_IRRC_SCMD_SONY, irrcStat.protocol[IRRC_SONY].sendCmds},
	    {JSON_STAT_IRRC_RBIT_SONY, irrcStat.protocol[IRRC_SONY].rcvBits},
	    {JSON_STAT_IRRC_SBIT_SONY, irrcStat.protocol[IRRC_SONY].sendBits},
	    {JSON_STAT_IRRC_RCMD_AEHA, irrcStat.protocol[IRRC_AEHA].rcvCmds},
	    {JSON_STAT_IRRC_SCMD_AEHA, irrcStat.protocol[IRRC_AEHA].sendCmds},
	    {JSON_STAT_IRRC_RBIT_AEHA, irrcStat.protocol[IRRC_AEHA].rcvBits},
	    {JSON_STAT_IRRC_SBIT_AEHA, irrcStat.protocol[IRRC_AEHA].sendBits},
	    {JSON_STAT_IRRC_RCMD_UNKNOWN, irrcStat.rcvUnknownCmds},
	    {JSON_STAT_IRRC_RPULSE_UNKNOWN, irrcStat.rcvUnknownPulses},
	});
    json11::Json::object pubsub({
	    {JSON_STAT_PUBSUB_PUBSENSOR, pubsubStat.publishSensorCount},
	    {JSON_STAT_PUBSUB_PUBIRRC, pubsubStat.publishIrrcCount},
	    {JSON_STAT_PUBSUB_PUBSHADOW, pubsubStat.publishShadowCount},
	});
    json11::Json::object sensor({
	    {JSON_STAT_SENSOR_BME280, sensorStat.bme280CaptureCount},
	    {JSON_STAT_SENSOR_LUMINO, sensorStat.luminocityCaptureCount},
	});
    json11::Json::object shadow({
	    {JSON_STAT_SHADOW_RECOGNIZE, shadowStat.recognizeCount},
	    {JSON_STAT_SHADOW_SYNTHESIZE, shadowStat.synthesizeCount},
	});

    root[JSON_STAT_HTTP] = http;
    root[JSON_STAT_IRRC] = irrc;
    root[JSON_STAT_PUBSUB] = pubsub;
    root[JSON_STAT_SENSOR] = sensor;
    root[JSON_STAT_SHADOW] = shadow;
    
    return std::move(root);
}
