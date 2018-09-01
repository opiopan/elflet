#pragma once
#include <time.h>

#ifdef __cplusplus
#include <json11.hpp>
extern "C" {
#endif

    struct BaseStat{
	time_t boottime;
	int32_t deepSleepCount;
    };
    extern struct BaseStat baseStat;

    struct IrrcStat{
	int32_t rcvPulses;
	int32_t sendPulses;
	struct {
	    int32_t rcvCmds;
	    int32_t sendCmds;
	    int32_t rcvBits;
	    int32_t sendBits;
	}protocol[3];
	int32_t rcvUnknownCmds;
	int32_t rcvUnknownPulses;
    };
    extern struct IrrcStat irrcStat;
    
    struct PubSubStat{
	int32_t publishSensorCount;
	int32_t publishIrrcCount;
	int32_t publishShadowCount;
    };
    extern struct PubSubStat pubsubStat;

    struct SensorStat{
	int32_t bme280CaptureCount;
	int32_t luminocityCaptureCount;
    };
    extern struct SensorStat sensorStat;

    struct ShadowStat{
	int32_t recognizeCount;
	int32_t synthesizeCount;
    };
    extern struct ShadowStat shadowStat;
    
#ifdef __cplusplus
}

const json11::Json::object getStatisticsJson();
#endif
