#include <TimeObj.h>
#include "WatchDog.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static Time watch_dog;

void updateWatchDog(){
    watch_dog = Time();
}

time_t getWatchDogTimerInterval(){
    return Time().getTime() - watch_dog.getTime();
}
