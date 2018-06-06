#pragma once

#include "irrc.h"

#ifdef __cplusplus
extern "C" {
#endif

bool startIRService();

bool sendIRData(IRRC_PROTOCOL protocol, int32_t bits, const uint8_t* data);

bool startIRReciever();
bool getIRRecievedData(IRRC_PROTOCOL* protocol, int32_t* bits, uint8_t* data);
bool getIRRecievedDataRaw(const rmt_item32_t** data, int32_t* length);

#ifdef __cplusplus
}
#endif
