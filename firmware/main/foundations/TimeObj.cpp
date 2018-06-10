#include <esp_system.h>
#include <esp_log.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <apps/sntp/sntp.h>
#include "TimeObj.h"

#include "sdkconfig.h"


//----------------------------------------------------------------------
// SNTP operation
//----------------------------------------------------------------------
RTC_DATA_ATTR static time_t lastAdjustTime = 0;

bool Time::shouldAdjust(){
    time_t now;
    ::time(&now);
    if (now < 30 * 365 * 24 * 60){
	return true;
    }

    if (now - lastAdjustTime > 24 * 60){
	return true;
    }

    return false;
}

void Time::startSNTP(){
    sntp_stop();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char*)"ntp.nict.jp");
    sntp_init();
    time_t now;
    ::time(&now);
    lastAdjustTime = now;
}

bool Time::waitForFinishAdjustment(time_t timeout){
    time_t now, start;
    ::time(&now);
    start = now;
    while (now < 30 * 365 * 24 * 60){
	if (now - start > timeout){
	    return false;
	}
	vTaskDelay(1000 / portTICK_PERIOD_MS);;
	::time(&now);
    }
    
    return true;
}

//----------------------------------------------------------------------
// set time zone
//----------------------------------------------------------------------
void Time::setTZ(const char* tz){
    setenv("TZ", tz, 1);
    tzset();
}

//----------------------------------------------------------------------
// initialization
//----------------------------------------------------------------------
Time::Time() {
    ::time(&time);
}

Time::Time(time_t time) : time(time){
}

//----------------------------------------------------------------------
// formatting
//----------------------------------------------------------------------
const char* Time::format(FORMAT type){
    if (type == RFC1123){
	return formatRFC1123();
    }
    
    const char* fmt =
	type == SIMPLE_DATE ? "%Y-%m-%d" :
	type == SIMPLE_TIME ? "%H:%M:%S" :
	                      "%Y-%m-%d %H:%M:%S";
    tm timeinfo;
    localtime_r(&time, &timeinfo);
    return strftime(buf, sizeof(buf), fmt, &timeinfo) > 0 ? buf : NULL;
}

const char* Time::formatRFC1123(){
    tm timeinfo;
    gmtime_r(&time, &timeinfo);
    auto rc = strftime(buf, sizeof(buf),
		       "%a, %d %b %Y %H:%M:%S GMT", &timeinfo);
    return rc > 0 ? buf : NULL;
}
