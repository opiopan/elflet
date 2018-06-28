#pragma once

#include <stdint.h>
#include <string>
#include <json11.hpp>

class Config {
public:
    enum BootMode{FactoryReset = 0, Configuration = 1, Normal = 2};
    enum SessionType{SessionTCP, SessionTLS,
		     SessionWebSocket, SessionWebSocketSecure};
    
protected:
    static const char* defaultTimezone;
    
    bool fromStorage;
    bool isDirtyBootMode;
    bool isDirty;
    
    BootMode bootMode;
    BootMode bootModeCurrent;
    uint32_t fileGeneration;

    std::string boardVersion;
    
    std::string nodeName;
    std::string apssid;
    std::string adminPassword;

    std::string ssidToConnect;
    std::string wifiPassword;

    std::string timezone;

    int32_t sensorFrequency;
    std::string pubSubServerAddr;
    int32_t pubSubSessionType;
    std::string pubSubServerCert;
    std::string pubSubUser;
    std::string pubSubPassword;
    std::string sensorTopic;

    std::string defaultSensorTopic;
    
public:
    Config();
    virtual ~Config();

    Config& operator = (const Config& src);

    bool load();
    bool commit();

    BootMode getBootMode() const{return bootModeCurrent;};
    const std::string& getBoardVersion() const{return boardVersion;};
    const std::string& getNodeName() const{return nodeName;};
    const std::string& getAPSSID() const{
	return apssid.length() > 0 ? apssid : nodeName;
    };
    const std::string& getAdminPassword() const{return adminPassword;};
    const std::string& getSSIDtoConnect() const{return ssidToConnect;};
    const std::string& getWifiPassword() const{return wifiPassword;};

    const char* getTimezone() const{
	return timezone.length() == 0 ? defaultTimezone : timezone.c_str();
    };
    int32_t getSensorFrequency() const{
	return sensorFrequency == 0? 60 : sensorFrequency;
    };
    const std::string& getPubSubServerAddr() const{return pubSubServerAddr;};
    SessionType getPubSubSessionType() const{
	return (SessionType)pubSubSessionType;
    };
    const std::string& getPubSubServerCert() const{return pubSubServerCert;};
    const std::string& getPubSubUser() const{return pubSubUser;};
    const std::string& getPubSubPassword() const{return pubSubPassword;};
    const std::string& getSensorTopic() const{
	return sensorTopic.length() == 0 ? defaultSensorTopic : sensorTopic;
    };
    
    const char* getVerificationKeyPath();
    
    bool setBootMode(BootMode mode);
    
    bool setNodeName(const std::string& name);
    bool setAPSSID(const std::string& ssid);
    bool setAdminPassword(const std::string& pass);

    bool setSSIDtoConnect(const std::string& ssid);
    bool setWifiPassword(const std::string& pass);

    bool setTimezone(const std::string& tz);

    bool setSensorFrequency(int32_t frequency);
    bool setPubSubServerAddr(const std::string& addr);
    bool setPubSubSessionType(SessionType type);
    bool setPubSubServerCert(const std::string& cert);
    bool setPubSubUser(const std::string& user);
    bool setPubSubPassword(const std::string& pass);
    bool setSensorTopic(const std::string& topic);
    
protected:
    void applyValue(const json11::Json& json, const std::string& key,
		    bool& value);
    void applyValue(const json11::Json& json, const std::string& key,
		    int32_t& value);
    void applyValue(const json11::Json& json, const std::string& key,
		    std::string& value);

    void updateDefaultSensorTopic();
};

extern Config* elfletConfig;

bool initConfig();
