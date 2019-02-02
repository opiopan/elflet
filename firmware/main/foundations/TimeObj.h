#pragma once

#include <time.h>

class Time {
    time_t time;
    char   buf[32];
    
public:
    static bool shouldAdjust(bool inDeepSleepCycle = false);
    static void startSNTP(const char* server = NULL);
    static bool waitForFinishAdjustment(time_t timeout = 0);

    static void setTZ(const char* tz);
    
    Time();
    Time(time_t time);

    Time& operator = (time_t time){this->time = time; return *this;};
    Time& operator = (const Time& src){time = src.time; return *this;};

    time_t getTime(){return time;};

    enum FORMAT{
        SIMPLE_DATE,
        SIMPLE_TIME,
        SIMPLE_DATETIME,
        ELAPSED_TIME,
        RFC1123
    };
    
    const char* format(FORMAT type);
    
protected:
    const char* formatRFC1123();
};
