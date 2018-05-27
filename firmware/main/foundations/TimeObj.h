#pragma once

#include <time.h>

class Time {
    time_t time;
    char   buf[32];
    
public:
    static bool shouldAdjust();
    static void startSNTP();
    static bool waitForFinishAdjustment(time_t timeout = 0);

    static void setTZ(const char* tz);
    
    Time();
    Time(time_t time);

    enum FORMAT{
	SIMPLE_DATE,
	SIMPLE_TIME,
	SIMPLE_DATETIME
    };
    
    const char* format(FORMAT type);
};
