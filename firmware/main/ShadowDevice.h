#pragma once

#include <string>
#include <iostream>
#include "irrc.h"

class ShadowDevice {
public:
    class Attribute {
    public:
	virtual float getNumericValue()const  = 0;
	virtual const std::string& getStringValue()const  = 0;
	virtual bool isVisible()const = 0;
	virtual bool printKV(std::ostream& out,
			     const std::string& name, bool needSep)const = 0;
    };
    virtual bool isOn()const = 0;
    virtual void setPowerStatus(bool isOn) = 0;
    virtual void serialize(std::ostream& out) = 0;
    virtual void dumpStatus(std::ostream& out) = 0;
    virtual const Attribute* getAttribute(const std::string& name)const = 0;
    virtual bool setStatus(const std::string& json, std::string& err) = 0;
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
