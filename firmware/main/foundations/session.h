#pragma once

#include <stdint.h>

#define SESSION_USER_NAME_MAX 32
#define SESSION_ID_LENGTH 32

class Session {
public:
    virtual const char* getId() = 0;
    virtual const char* getUser() = 0;

    virtual void expire() = 0;
};

void initSessionPool(int32_t seed);
void deinitSessionPool();
Session* createSession(const uint8_t* user);
Session* findSession(const uint8_t* id);
