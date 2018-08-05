#pragma once

#include <string>
#include <iostream>

class ShadowDevice {
public:
    virtual bool isOn() = 0;
    virtual void setPowerStatus(bool isOn) = 0;
};

bool initShadowDevicePool();
bool addShadowDevice(const std::string& json);
bool deleteShadowDevice(const std::string& name);
ShadowDevice* findShadowDevice(const std::string& name);
bool getShadowDeviceDefs(std::ostream& out);
