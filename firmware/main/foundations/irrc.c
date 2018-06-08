#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "irrc.h"

static const char tag[] = "irrc";

#define CMDBUFFLEN 1024

#define RMT_RX_CHANNEL RMT_CHANNEL_0
#define RMT_TX_CHANNEL RMT_CHANNEL_4

#define RMT_CLK_DIV 80 /* APB clock is 80Mhz and RMT tick set to 1Mhz */

typedef bool (*MakeSendDataFunc)(IRRC* ctx,
				 const uint8_t* data, int32_t length);
static bool makeSendData(IRRC* ctx, const uint8_t* data, int32_t length);

//----------------------------------------------------------------------
// Protocol definition
//----------------------------------------------------------------------
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
	.make = makeSendData
    }
};
    
//----------------------------------------------------------------------
// transmitter implementation
//----------------------------------------------------------------------
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

static bool makeSendData(IRRC* ctx, const uint8_t* data, int32_t bits)
{
    if (bits < 1){
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
    for (int i = 0; i < bits; i++){
	uint8_t on = data[i / 8] & (0x1 << (i % 8));
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

//----------------------------------------------------------------------
// reciever implementation
//----------------------------------------------------------------------
static void initRx(IRRC* ctx, IRRC_PROTOCOL protocol, int32_t gpio)
{
    ctx->rmt = (rmt_config_t){
	.rmt_mode = RMT_MODE_RX,
	.channel = RMT_RX_CHANNEL,
	.gpio_num = gpio,
	.mem_block_num = 4,
	.clk_div = RMT_CLK_DIV,
	.rx_config = {
	    .filter_en = false,
	    .idle_threshold = 10000,
	}
    };
    
    rmt_config(&ctx->rmt);
    rmt_driver_install(ctx->rmt.channel, 2048, 0);
}

typedef struct {
    int32_t max;
    int32_t min;
} RANGE;
#define RANGEVAL(v) {(v) * 1.2, (v) * 0.80}
#define INRANGE(v, r) ((v) < (r).max && (v) > (r).min)
static RANGE necUnitRange = RANGEVAL(NEC_UNIT);
static RANGE necLongRange = RANGEVAL(NEC_UNIT * 3);
static RANGE necLeader0Range = RANGEVAL(NEC_UNIT * 16);
static RANGE necLeader1Range = RANGEVAL(NEC_UNIT * 8);
static RANGE sonyUnitRange = RANGEVAL(SONY_UNIT);
static RANGE sonyLongRange = RANGEVAL(SONY_UNIT * 2);
static RANGE sonyLeader0Range = RANGEVAL(SONY_UNIT * 4);
static RANGE sonyLeader1Range = RANGEVAL(SONY_UNIT);

static IRRC_PROTOCOL presumeProtocol(IRRC* ctx){
    rmt_item32_t* items = ctx->buff;

    if (ctx->usedLen == 0){
	return IRRC_UNKNOWN;
    }
    
    if (INRANGE(items[0].duration0, necLeader0Range) &&
	INRANGE(items[0].duration1, necLeader1Range)){
	// it seems NEC format
	printf("nec format?\n");
	for (int i = 1; i < ctx->usedLen - 1; i++){
	    if (INRANGE(items[i].duration0, necUnitRange) &&
		(INRANGE(items[i].duration1, necUnitRange) ||
		 INRANGE(items[i].duration1, necLongRange))){
		continue;
	    }else{
		return IRRC_UNKNOWN;
	    }
	}
	return IRRC_NEC;
    }
    if (INRANGE(items[0].duration0, sonyLeader0Range) &&
	INRANGE(items[0].duration1, sonyLeader1Range)){
	// it seems SONY format
	printf("sony format?\n");
	for (int i = 1; i < ctx->usedLen - 1; i++){
	    if (INRANGE(items[i].duration1, sonyUnitRange) &&
		(INRANGE(items[i].duration0, sonyUnitRange) ||
		 INRANGE(items[i].duration0, sonyLongRange))){
		continue;
	    }else{
		return IRRC_UNKNOWN;
	    }
	}
	return IRRC_SONY;
    }else{
	// in this case, it might AHEA format
	printf("AHEA format?\n");
	int sum = 0;
	for (int i = 1; i < ctx->usedLen - 1; i++){
	    sum += items[i].duration0;
	}
	int unit = sum / (ctx->usedLen - 2);
	printf("unit length: %d\n", unit);
	RANGE unitRange = RANGEVAL(unit);
	RANGE longRange = RANGEVAL(unit * 3);
	RANGE leader0Range = RANGEVAL(unit * 8);
	RANGE leader1Range = RANGEVAL(unit * 4);
	if (INRANGE(items[0].duration0, leader0Range) &&
	    INRANGE(items[0].duration1, leader1Range)){
	    printf("leader is OK\n");
	    for (int i = 1; i < ctx->usedLen - 1; i++){
		if (INRANGE(items[i].duration0, unitRange) &&
		    (INRANGE(items[i].duration1, unitRange) ||
		     INRANGE(items[i].duration1, longRange))){
		    continue;
		}else{
		    printf("data NG: pos(%d) : %d, %d\n",
			   i, items[i].duration0, items[i].duration1);
		    return IRRC_UNKNOWN;
		}
	    }
	    return IRRC_AEHA;
	}
    }
    
    return IRRC_UNKNOWN;
}

//----------------------------------------------------------------------
// interface for outer code
//----------------------------------------------------------------------
bool IRRCInit(IRRC* ctx, IRRC_MODE mode, IRRC_PROTOCOL protocol, int32_t gpio)
{
    configASSERT(protocol == IRRC_NEC || protocol == IRRC_AEHA ||
		 protocol == IRRC_SONY);

    *ctx = (IRRC){
	.protocol = mode == IRRC_TX ? protocol : IRRC_UNKNOWN,
	.mode = mode,
	.gpio = gpio,
	.buff = malloc(CMDBUFFLEN),
	.buffLen = CMDBUFFLEN,
	.usedLen = 0
    };

    if (ctx->buff){
	if (ctx->mode == IRRC_TX){
	    initTx(ctx, protocol, gpio);
	}else{
	    initRx(ctx, protocol, gpio);
	}
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

void IRRCSend(IRRC* ctx, const uint8_t* data, int32_t bits)
{
    ESP_LOGI(tag, "IRRCSend: format[%s] bits[%d]",
	     ProtocolDef[ctx->protocol].name, bits);
    ESP_LOG_BUFFER_HEX(tag, data, (bits + 7) / 8);
    if (ProtocolDef[ctx->protocol].make(ctx, data, bits)){
	rmt_write_items(ctx->rmt.channel, ctx->buff, ctx->usedLen, 1);
	rmt_wait_tx_done(ctx->rmt.channel, portMAX_DELAY);
    }
}

bool IRRCRecieve(IRRC* ctx, int32_t timeout)
{
    ESP_LOGI(tag,"start IR recieving");
    ctx->usedLen = 0;

    RingbufHandle_t rb = NULL;
    rmt_get_ringbuf_handle(ctx->rmt.channel, &rb);

    rmt_rx_start(ctx->rmt.channel, 1);
    uint32_t rx_size;
    rmt_item32_t *items =
	(rmt_item32_t*)xRingbufferReceive(rb, &rx_size,
					  timeout / portTICK_PERIOD_MS);
    rmt_rx_stop(ctx->rmt.channel);

    if (items){
	ctx->usedLen = rx_size / sizeof(*items);
	memcpy(ctx->buff, items, rx_size);
	ESP_LOGI(tag,"%d unit data recieved", ctx->usedLen);
	ctx->protocol = presumeProtocol(ctx);
	ESP_LOGI(tag, "it seems %s format",
		 ctx->protocol == IRRC_NEC ? "NEC" : 
		 ctx->protocol == IRRC_AEHA ? "AEHA" : 
		 ctx->protocol == IRRC_SONY ? "SONY" :
		 "unknown");
	vRingbufferReturnItem(rb, (void*) items);
	return true;
    }else{
	ESP_LOGI(tag,"timeout, no data recieved");
	return false;
    }
}

bool IRRCDecodeRecievedData(IRRC* ctx,
			    IRRC_PROTOCOL* protocol,
			    uint8_t* data, int32_t* bits)
{
    if (ctx->usedLen == 0){
	return false;
    }

    *protocol = ctx->protocol;
    if (ctx->protocol == IRRC_UNKNOWN){
	*bits = 0;
	return true;
    }

    memset(data, 0, (*bits + 7) / 8);
    int32_t dataBits = ctx->protocol == IRRC_SONY ?
	ctx->usedLen - 1 : ctx->usedLen -2;
    for (int i = 0; i < dataBits; i++){
	bool on = false;
	if (ctx->protocol == IRRC_SONY){
	    if (ctx->buff[i + 1].duration0 > SONY_UNIT * 1.5){
		on = true;
	    }
	}else{
	    if (ctx->buff[i + 1].duration1 > ctx->buff[i + 1].duration0 * 2){
		on = true;
	    }
	}
	if (i < *bits && on){
	    data[i / 8] |= 1 << (i % 8);
	}
    }

    *bits = dataBits;
    
    return true;
}
