#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define BLEHID_MAX_KEYCODE_NUM 6

typedef enum {
    BLEHID_KEYCODE,
    BLEHID_CONSUMERCODE
} BLEHID_CODE_TYPE;

typedef struct {
    BLEHID_CODE_TYPE type;
    int duration;
    union {
        struct {
            uint8_t mask;
            uint8_t keynum;
            uint8_t keys[BLEHID_MAX_KEYCODE_NUM];
        } keycode;
        uint8_t consumercode;
    } data;
} BleHidCodeData;

extern const BleHidCodeData BLEHID_DEFAULT_CODE;

void initBleHidService(const char* uname);
bool startBleHidService();
void stopBleHidService();
void releaseBleResource();

bool bleHidSendCode(const BleHidCodeData* data);
bool bleHidSendKeyValue(uint8_t specialMask, const uint8_t* keybuf, int buflen,
                        int duration);
bool bleHidSendConsumerValue(int8_t code, int duration);

#ifdef __cplusplus
}
#endif
