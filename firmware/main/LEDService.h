#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum LED_DefaultMode {
    LEDDM_STANDBY,
    LEDDM_SCAN_WIFI,
    LEDDM_CONFIGURATION,
    LEDDM_FACTORY_RESET,
};

enum LED_BlinkMode {
    LEDBM_DEFAULT = -1,
    LEDBM_SYSTEM_FAULT = 0,
    LEDBM_IRRX = 1,
    LEDBM_RESTRICT_DEEP_SLEEP = 2,
};

bool startLedService();

void ledSetDefaultMode(enum LED_DefaultMode mode);
void ledSetBlinkMode(enum LED_BlinkMode mode);

#ifdef __cplusplus
}
#endif
