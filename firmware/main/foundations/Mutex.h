#pragma once

#include <freertos/semphr.h>

class Mutex {
private:
    SemaphoreHandle_t sem;

public:
    Mutex()
    {
	sem = xSemaphoreCreateMutex();
    }
    ~Mutex()
    {
	vSemaphoreDelete(sem);
    }

    void lock()
    {
	xSemaphoreTake(sem, portMAX_DELAY);
    }
    void unlock()
    {
	xSemaphoreGive(sem);
    }
};

class LockHolder {
private:
    Mutex& mutex;

public:
    LockHolder(Mutex& target) : mutex(target){
	mutex.lock();
    };
    ~LockHolder()
    {
	mutex.unlock();
    }
};
