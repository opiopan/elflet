#pragma once

#include <iostream>
#include "webserver.h"
#include "irrc.h"

#ifdef __cplusplus
extern "C" {
#endif

bool startIRService();

bool sendIRData(IRRC_PROTOCOL protocol, int32_t bits, const uint8_t* data);
bool sendIRDataJson(const WebString& data);

void enableIRReceiver();
bool startIRReceiver();
bool getIRReceivedData(IRRC_PROTOCOL* protocol, int32_t* bits, uint8_t* data);
bool getIRReceivedDataRaw(const rmt_item32_t** data, int32_t* length);
  
bool getIRReceivedDataJson(std::ostream& out);
bool getIRReceivedDataRawJson(std::ostream& out);
    
#ifdef __cplusplus
}
#endif
