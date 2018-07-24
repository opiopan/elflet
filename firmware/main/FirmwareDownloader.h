#pragma once

#include <functional>

enum DFStatus{
    dfSucceed = 0,
    dfBusy,
    dfOpenError,
    dfReadError,
    dfHttpError,
    dfOtaError,
};

typedef std::function<void(DFStatus)> DFCallback;

DFStatus httpDownloadFirmware(const char* uri, DFCallback callback = nullptr);
