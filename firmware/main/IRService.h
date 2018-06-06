#pragma once

#include "irrc.h"

#ifdef __cplusplus
extern "C" {
#endif

bool startIRService();

bool sendIRData(IRRC_PROTOCOL protocol, int32_t bits, const uint8_t* data);

bool startIRReciever();
bool getIRRecievedData(IRRC_PROTOCOL* protocol, int32_t* bits, uint8_t* data);

#ifdef __cplusplus
}
#endif
