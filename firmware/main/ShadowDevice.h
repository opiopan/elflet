#pragma once

#include <string>
#include <iostream>
#include "irrc.h"

class ShadowDevice {
public:
    virtual bool isOn() = 0;
    virtual void setPowerStatus(bool isOn) = 0;
    virtual void serialize(std::ostream& out) = 0;
    virtual void dumpStatus(std::ostream& out) = 0;
};

struct IRCommand {
    IRRC_PROTOCOL protocol;
    int bits;
    void* data;
};

bool initShadowDevicePool();
bool resetShadowDevicePool();
bool addShadowDevice(
    const std::string& name, const std::string& json, std::string& err);
bool deleteShadowDevice(const std::string& name);
ShadowDevice* findShadowDevice(const std::string& name);
bool dumpShadowDeviceNames(std::ostream& out);
bool applyIRCommand(const IRCommand* cmd);