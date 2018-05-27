#include <stdlib.h>
#include <stdbool.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "irrc.h"

static const char tag[] = "irrc";

#define CMDBUFFLEN 1024

#define RMT_RX_CHANNEL RMT_CHANNEL_0
#define RMT_TX_CHANNEL RMT_CHANNEL_4

#define RMT_CLK_DIV 80 /* APB clock is 80Mhz and RMT tick set to 1Mhz */

typedef bool (*MakeSendDataFunc)(IRRC* ctx, uint8_t* data, int32_t length);
static bool makeSendData(IRRC* ctx, uint8_t* data, int32_t length);
static bool makeSendDataSony(IRRC* ctx, uint8_t* data, int32_t length);

static struct ProtocolDef_t{
    const char*  name;
    int32_t      carrier;
    int32_t      duty;
    bool         needLeader;
    rmt_item32_t leader;
    bool         needTrailer;
    rmt_item32_t trailer;
    rmt_item32_t on;
    rmt_item32_t off;
    MakeSendDataFunc make;
} ProtocolDef[] = {
    [IRRC_NEC] = {
	#define NEC_UNIT 562
	.name = "NEC",
	.carrier = 38000,
	.duty = 33,
	.needLeader = true,
	.leader = {
	    .duration0 = 16 * NEC_UNIT,
	    .level0 = 1,
	    .duration1 = 8 * NEC_UNIT,
	    .level1 = 0
	},
	.needTrailer = true,
	.trailer = {
	    .duration0 = NEC_UNIT,
	    .level0 = 1,
	    .duration1 = 16 * NEC_UNIT,
	    .level1 = 0
	},
	.on = {
	    .duration0 = NEC_UNIT,
	    .level0 = 1,
	    .duration1 = 3 * NEC_UNIT,
	    .level1 = 0
	},
	.off = {
	    .duration0 = NEC_UNIT,
	    .level0 = 1,
	    .duration1 = NEC_UNIT,
	    .level1 = 0
	},
	.make = makeSendData
    },
    [IRRC_AEHA] = {
	#define AEHA_UNIT 425
	.name = "AEHA",
	.carrier = 38000,
	.duty = 33,
	.needLeader = true,
	.leader = {
	    .duration0 = 8 * AEHA_UNIT,
	    .level0 = 1,
	    .duration1 = 4 * AEHA_UNIT,
	    .level1 = 0
	},
	.needTrailer = true,
	.trailer = {
	    .duration0 = AEHA_UNIT,
	    .level0 = 1,
	    .duration1 = 16 * AEHA_UNIT,
	    .level1 = 0
	},
	.on = {
	    .duration0 = AEHA_UNIT,
	    .level0 = 1,
	    .duration1 = 3 * AEHA_UNIT,
	    .level1 = 0
	},
	.off = {
	    .duration0 = AEHA_UNIT,
	    .level0 = 1,
	    .duration1 = AEHA_UNIT,
	    .level1 = 0
	},
	.make = makeSendData
    },
    [IRRC_SONY] = {
	#define SONY_UNIT 600
	.name = "SONY",
	.carrier = 40000,
	.duty = 33,
	.needLeader = true,
	.leader = {
	    .duration0 = SONY_UNIT,
	    .level0 = 0,
	    .duration1 = 4 * SONY_UNIT,
	    .level1 = 1
	},
	.needTrailer = false,
	.on = {
	    .duration0 = SONY_UNIT,
	    .level0 = 0,
	    .duration1 = 2 * SONY_UNIT,
	    .level1 = 1
	},
	.off = {
	    .duration0 = SONY_UNIT,
	    .level0 = 0,
	    .duration1 = SONY_UNIT,
	    .level1 = 1
	},
	.make = makeSendDataSony
    }
};
    
static void initTx(IRRC* ctx, IRRC_PROTOCOL protocol, int32_t gpio)
{
    ctx->rmt = (rmt_config_t){
	.rmt_mode = RMT_MODE_TX,
	.channel = RMT_TX_CHANNEL,
	.gpio_num = gpio,
	.mem_block_num = 4,
	.clk_div = RMT_CLK_DIV,
	.tx_config.loop_en = false,
	.tx_config.carrier_duty_percent = ProtocolDef[protocol].duty,
	.tx_config.carrier_freq_hz = ProtocolDef[protocol].carrier,
	.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH,
	.tx_config.carrier_en = 1,
	.tx_config.idle_level = RMT_IDLE_LEVEL_LOW,
	.tx_config.idle_output_en = true
    };
    rmt_config(&ctx->rmt);
    rmt_driver_install(ctx->rmt.channel, 0, 0);
}

static void changeProtocolTx(IRRC* ctx, IRRC_PROTOCOL protocol)
{
    uint32_t TICK_IN_HZ_PERCENT = 80000000 / 100;
    uint32_t carrier = ProtocolDef[protocol].carrier;
    uint32_t duty = ProtocolDef[protocol].duty;
        
    ESP_ERROR_CHECK(rmt_set_tx_carrier(
	RMT_TX_CHANNEL, 1,
	TICK_IN_HZ_PERCENT * duty / carrier,
	TICK_IN_HZ_PERCENT * (100 - duty) / carrier,
	RMT_CARRIER_LEVEL_HIGH));
}

static bool makeSendData(IRRC* ctx, uint8_t* data, int32_t length)
{
    if (length < 1){
	ESP_LOGE(tag, "invalid data length");
	return false;
    }

    struct ProtocolDef_t* protocol = ProtocolDef + ctx->protocol;
    ctx->usedLen = 0;

    /* leader*/
    if (protocol->needLeader){
	ctx->buff[ctx->usedLen] = protocol->leader;
	ctx->usedLen++;
    }

    /* data */
    for (int i = 0; i < length; i++){
	for (int bit = 0; bit < 8; bit++){
	    uint8_t on = data[i] & (0x1 << bit);
	    ctx->buff[ctx->usedLen] = on ? protocol->on : protocol->off;
	    ctx->usedLen++;
	}
    }

    /* trailer */
    if (protocol->needTrailer){
	ctx->buff[ctx->usedLen] = protocol->trailer;
	ctx->usedLen++;
    }

    return true;
}

static bool makeSendDataSony(IRRC* ctx, uint8_t* data, int32_t length)
{
    if (length < 1){
	ESP_LOGE(tag, "invalid data length");
	return false;
    }

    int bits = data[0];
    if (length != (bits + 15) / 8){
	ESP_LOGE(tag, "invalid data format: "
		 "first byte must indicate data bit number");
	return false;
    }

    struct ProtocolDef_t* protocol = ProtocolDef + ctx->protocol;
    ctx->usedLen = 0;

    /* leader*/
    if (protocol->needLeader){
	ctx->buff[ctx->usedLen] = protocol->leader;
	ctx->usedLen++;
    }

    /* data */
    for (int i = 0; i < bits; i++){
	uint8_t on = data[i / 8 + 1] & (0x1 << (i % 8));
	ctx->buff[ctx->usedLen] = on ? protocol->on : protocol->off;
	ctx->usedLen++;
    }

    /* trailer */
    if (protocol->needTrailer){
	ctx->buff[ctx->usedLen] = protocol->trailer;
	ctx->usedLen++;
    }

    return true;
}


bool IRRCInit(IRRC* ctx, IRRC_MODE mode, IRRC_PROTOCOL protocol, int32_t gpio)
{
    configASSERT(mode == IRRC_TX);
    configASSERT(protocol == IRRC_NEC || protocol == IRRC_AEHA ||
		 protocol == IRRC_SONY);

    *ctx = (IRRC){
	.protocol = protocol,
	.mode = mode,
	.gpio = gpio,
	.buff = malloc(CMDBUFFLEN),
	.buffLen = CMDBUFFLEN,
	.usedLen = 0
    };

    if (ctx->buff){
	initTx(ctx, protocol, gpio);
	return true;
    }else{
	return false;
    }
}

void IRRCDeinit(IRRC* ctx)
{
    rmt_driver_uninstall(ctx->rmt.channel);
    free(ctx->buff);
    ctx->buff = NULL;
}

void IRRCChangeProtocol(IRRC* ctx, IRRC_PROTOCOL protocol)
{
    if (ctx->protocol != protocol && 
	protocol >= 0 &&
	protocol < sizeof(ProtocolDef) / sizeof(ProtocolDef[0])){
	ctx->protocol = protocol;
	if (ctx->mode == IRRC_TX){
	    changeProtocolTx(ctx, protocol);
	}else{
	}
    }
}

void IRRCSend(IRRC* ctx, uint8_t* data, int32_t length)
{
    ESP_LOGI(tag, "IRRCSend: format[%s] bytes[%d]",
	     ProtocolDef[ctx->protocol].name, length);
    ESP_LOG_BUFFER_HEX(tag, data, length);
    if (ProtocolDef[ctx->protocol].make(ctx, data, length)){
	rmt_write_items(ctx->rmt.channel, ctx->buff, ctx->usedLen, 1);
	rmt_wait_tx_done(ctx->rmt.channel, portMAX_DELAY);
    }
}
