#pragma once

#include <iostream>
#include "irrc.h"

#ifdef __cplusplus
extern "C" {
#endif

bool startIRService();

bool sendIRData(IRRC_PROTOCOL protocol, int32_t bits, const uint8_t* data);

bool startIRReciever();
bool getIRRecievedData(IRRC_PROTOCOL* protocol, int32_t* bits, uint8_t* data);
bool getIRRecievedDataRaw(const rmt_item32_t** data, int32_t* length);
  
bool getIRRecievedDataJson(std::ostream& out);
bool getIRRecievedDataRawJson(std::ostream& out);
    
#ifdef __cplusplus
}
#endif
