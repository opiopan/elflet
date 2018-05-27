#pragma once

#include <stdint.h>
#include <esp_ota_ops.h>

#define OTA_SIGNATURE_LENGTH 128

enum OTARESULT {
    OTA_SUCCEED = 0,
    OTA_BUSY                = 0x0001,
    OTA_NOT_FOUND_PARTITION = 0x0101,
    OTA_CANNOT_START        = 0x0102,
    OTA_FAILED_WRITE        = 0x0103,
    OTA_FAILED_PARSE_PUBKEY = 0x0501,
    OTA_FAILED_VERIFICATION = 0x1001,
    OTA_FAILED_CHANGE_BOOT  = 0x2001
};

class OTA {
public:
    virtual OTARESULT addDataFlagment(const void* flagment, size_t length) = 0;
};

OTARESULT startOTA(const char* pubkey_path, size_t imageSize, OTA** outp);
OTARESULT endOTA(const OTA* handle, bool needCommit);
