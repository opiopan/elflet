#pragma once

#include <stdint.h>
#include "driver/rmt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    IRRC_UNKNOWN = -1,
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
    int32_t option;
    int32_t gpio;
    rmt_config_t rmt;
    rmt_item32_t* buff;
    int32_t buffLen;
    int32_t usedLen;
    RingbufHandle_t rb;
    int32_t started;
} IRRC;

#define IRRC_OPT_CONTINUOUS 1

extern bool IRRCInit(IRRC* ctx, IRRC_MODE mode,
		     IRRC_PROTOCOL protocol, int32_t gpio);
extern void IRRCDeinit(IRRC* ctx);
extern void IRRCChangeProtocol(IRRC* ctx, IRRC_PROTOCOL protocol);
extern void IRRCSend(IRRC* ctx, const uint8_t* data, int32_t bits);
extern bool IRRCReceive(IRRC* ctx, int32_t timeout);
extern bool IRRCDecodeReceivedData(IRRC* ctx,
				   IRRC_PROTOCOL* protocol,
				   uint8_t* data, int32_t* bits);

#define IRRC_SET_OPT(ctx, opt) ((ctx)->option = opt)
    
#define IRRC_PROTOCOL(ctx) ((ctx)->protocol)
#define IRRC_ITEM_LENGTH(ctx) ((ctx)->usedLen)
#define IRRC_ITEMS(ctx) ((const rmt_item32_t*)(ctx)->buff)

#ifdef __cplusplus
}
#endif
