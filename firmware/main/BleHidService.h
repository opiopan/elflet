#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void initBleHidService(const char* uname);
bool startBleHidService();
void stopBleHidService();
void releaseBleResource();

bool bleHidSendKeyValue(uint8_t specialMask, uint8_t* keybuf, int buflen,
                        int duration);
bool bleHidSendConsumerValue(int8_t code, int duration);

#ifdef __cplusplus
}
#endif
