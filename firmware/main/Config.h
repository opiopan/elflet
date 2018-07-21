#pragma once

#include <stdint.h>
#include <string>
#include <json11.hpp>
#include "DeepSleep.h"

class Config {
public:
    enum BootMode{FactoryReset = 0, Configuration = 1, Normal = 2};
    enum FunctionMode{FullSpec, SensorOnly};
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

    WakeupCause wakeupCause;

    int32_t functionMode;

    std::string boardVersion;
    
    std::string nodeName;
    std::string apssid;
    std::string adminPassword;

    std::string ssidToConnect;
    std::string wifiPassword;

    std::string ntpServer;
    std::string timezone;

    int32_t sensorFrequency;

    std::string pubSubServerAddr;
    int32_t pubSubSessionType;
    std::string pubSubServerCert;
    std::string pubSubUser;
    std::string pubSubPassword;

    std::string sensorTopic;
    std::string irrcRecieveTopic;
    std::string irrcRecievedDataTopic;
    std::string irrcSendTopic;
    std::string downloadFirmwareTopic;

    std::string defaultSensorTopic;
    std::string defaultIrrcRecieveTopic;
    std::string defaultIrrcRecievedDataTopic;
    std::string defaultIrrcSendTopic;
    
public:
    Config(WakeupCause cause);
    virtual ~Config();

    Config& operator = (const Config& src);

    bool load();
    bool commit();

    BootMode getBootMode() const{return bootModeCurrent;};
    WakeupCause getWakeupCause() const{return wakeupCause;};
    FunctionMode getFunctionMode() const{return (FunctionMode)functionMode;};
    const std::string& getBoardVersion() const{return boardVersion;};
    const std::string& getNodeName() const{return nodeName;};
    const std::string& getAPSSID() const{
	return apssid.length() > 0 ? apssid : nodeName;
    };
    const std::string& getAdminPassword() const{return adminPassword;};
    const std::string& getSSIDtoConnect() const{return ssidToConnect;};
    const std::string& getWifiPassword() const{return wifiPassword;};

    const char* getNtpServer() const{
	return ntpServer.length() == 0 ? "ntp.nict.jp" : ntpServer.c_str();
    };
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
    const std::string& getIrrcRecieveTopic() const{
	return irrcRecieveTopic.length() == 0 ?
	    defaultIrrcRecieveTopic : irrcRecieveTopic;
    };
    const std::string& getIrrcRecievedDataTopic() const{
	return irrcRecievedDataTopic.length() == 0 ?
	    defaultIrrcRecievedDataTopic : irrcRecievedDataTopic;
    };
    const std::string& getIrrcSendTopic() const{
	return irrcSendTopic.length() == 0 ?
	    defaultIrrcSendTopic : irrcSendTopic;
    };
    const std::string& getDownloadFirmwareTopic() const{
	return downloadFirmwareTopic;
    }
    
    const char* getVerificationKeyPath();
    
    bool setBootMode(BootMode mode);

    bool setFunctionMode(FunctionMode mode);
    
    bool setNodeName(const std::string& name);
    bool setAPSSID(const std::string& ssid);
    bool setAdminPassword(const std::string& pass);

    bool setSSIDtoConnect(const std::string& ssid);
    bool setWifiPassword(const std::string& pass);

    bool setNtpServer(const std::string& server);
    bool setTimezone(const std::string& tz);

    bool setSensorFrequency(int32_t frequency);
    bool setPubSubServerAddr(const std::string& addr);
    bool setPubSubSessionType(SessionType type);
    bool setPubSubServerCert(const std::string& cert);
    bool setPubSubUser(const std::string& user);
    bool setPubSubPassword(const std::string& pass);
    bool setSensorTopic(const std::string& topic);
    bool setIrrcRecieveTopic(const std::string& topic);
    bool setIrrcRecievedDataTopic(const std::string& topic);
    bool setIrrcSendTopic(const std::string& topic);
    bool setDownloadFirmwareTopic(const std::string& topic);

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

extern const char JSON_FUNCTIONMODE[];
extern const char JSON_BOARDTYPE[];
extern const char JSON_BOARDVERSION[];
extern const char JSON_FWVERSION[];
extern const char JSON_FWVERSION_MAJOR[];
extern const char JSON_FWVERSION_MINOR[];
extern const char JSON_FWVERSION_BUILD[];
extern const char JSON_FWURI[];

extern const char JSON_NODENAME[];
extern const char JSON_APSSID[];
extern const char JSON_ADMINPASSWORD[];
extern const char JSON_SSID[];
extern const char JSON_WIFIPASSWORD[];
extern const char JSON_COMMIT[];
extern const char JSON_NTPSERVER[];
extern const char JSON_TIMEZONE[];

extern const char JSON_PUBSUB[];
extern const char JSON_SENSORFREQUENCY[];
extern const char JSON_PUBSUBSERVERADDR[];
extern const char JSON_PUBSUBSESSIONTYPE[];
extern const char JSON_PUBSUBSERVERCERT[];
extern const char JSON_PUBSUBUSER[];
extern const char JSON_PUBSUBPASSWORD[];
extern const char JSON_SENSORTOPIC[];
extern const char JSON_IRRCRECIEVETOPIC[];
extern const char JSON_IRRCRECIEVEDDATATOPIC[];
extern const char JSON_IRRCSENDTOPIC[];
extern const char JSON_DOWNLOADFIRMWARETOPIC[];

extern const char JSON_DATE[];
extern const char JSON_TEMP_UNIT[];
extern const char JSON_HUM_UNIT[];
extern const char JSON_PRESS_UNIT[];
extern const char JSON_LUMINO_UNIT[];
extern const char JSON_TEMP[];
extern const char JSON_HUM[];
extern const char JSON_PRESS[];
extern const char JSON_LUMINO[];

extern const char JSON_FORMATED[];
extern const char JSON_PROTOCOL[];
extern const char JSON_BITCOUNT[];
extern const char JSON_DATA[];
extern const char JSON_RAW[];
extern const char JSON_LEVEL[];
extern const char JSON_DURATION[];
