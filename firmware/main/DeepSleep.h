#pragma once

enum WakeupCause{
    WC_NOTSLEEP, WC_TIMER, WC_BUTTON
};

WakeupCause initDeepSleep();
int32_t getSleepTimeMs();
void enterDeepSleep(int32_t ms);
