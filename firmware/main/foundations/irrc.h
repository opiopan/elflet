#pragma once

#include <stdint.h>
#include "driver/rmt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IRRC_NEC = 0,
    IRRC_AEHA,
    IRRC_SONY
}IRRC_PROTOCOL;

typedef enum {
    IRRC_TX,
    IRRC_RX 
}IRRC_MODE;

typedef struct {
    IRRC_PROTOCOL protocol;
    IRRC_MODE mode;
    int32_t gpio;
    rmt_config_t rmt;
    rmt_item32_t* buff;
    int32_t buffLen;
    int32_t usedLen;
} IRRC;

extern bool IRRCInit(IRRC* ctx, IRRC_MODE mode,
		     IRRC_PROTOCOL protocol, int32_t gpio);
extern void IRRCDeinit(IRRC* ctx);
extern void IRRCChangeProtocol(IRRC* ctx, IRRC_PROTOCOL protocol);
extern void IRRCSend(IRRC* ctx, uint8_t* data, int32_t length);

#ifdef __cplusplus
}
#endif
