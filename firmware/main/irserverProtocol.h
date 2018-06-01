#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
    char    strID[4];
    int32_t intID;
} IRSID;

typedef enum {
    IRServerCmdAck = 0,
    IRServerCmdTxFormat
} IRSCmd;
    
typedef struct irserver_hdr_t {
    IRSID    id;
    int16_t  cmd;
    int16_t  size;
} IRSHeader;

typedef enum {
    IRSFORMAT_NEC = 0, IRSFORMAT_AEHA, IRSFORMAT_SONY
} IRSFormat;
    
typedef struct {
    int16_t   format : 16;
    int16_t   dataLen;
} IRSTxFormatData;

#define IRSIDSTR "IRSV"
#define IRSIDINT (*((int32_t*)IRSIDSTR))

#define IRS_REQMAXSIZE 128
#define IRS_RESMAXSIZE 64
    
#define IRSERVER_PORT 8888

#ifdef __cplusplus
}
#endif
