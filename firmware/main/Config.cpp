#include <esp_log.h>
#include <esp_system.h>
#include <esp_spiffs.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <sys/stat.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <GeneralUtils.h>
#include <CPPNVS.h>
#include <json11.hpp>
#include "Config.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "Config";

static const char NVS_NS[] = "elflet";
static const std::string BOOTMODE_KEY = "bootmode";
static const std::string CONFIGGEN_KEY = "configgen";

static const char SPIFFS_MP[] = "/spiffs";
static const char VERIFICATION_KEY_PATH[] = "/spiffs/verificationkey.pem";
static const char* CONFIGPATH[] = {
    "/spiffs/pdata.json",
    "/spiffs/config01.json",
    "/spiffs/config02.json"
};

static const int MAX_NODENAME_LEN = 32;
static const int MAX_PASSWORD_LEN = 64;

Config* elfletConfig = NULL;

const char* Config::defaultTimezone = "JST-9";

//----------------------------------------------------------------------
// initialize global configuration
//----------------------------------------------------------------------
bool initConfig(){
    // retrieve wakeup cause
    auto wakeupCause = initDeepSleep();
    
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
	// NVS partition was truncated and needs to be erased
	// Retry nvs_flash_init
	err = nvs_flash_erase();
	if (err != ESP_OK){
	    ESP_LOGE(tag, "failed to erase nvs: %s",
		     GeneralUtils::errorToString(err));
	    return false;
	}
	err = nvs_flash_init();
    }
    if (err != ESP_OK){
	ESP_LOGE(tag, "failed to initialize nvs: %s",
		 GeneralUtils::errorToString(err));
	return false;
    }

    // mount spiffs
    esp_vfs_spiffs_conf_t conf = {
      .base_path = SPIFFS_MP,
      .partition_label = "spiffs",
      .max_files = 5,
      .format_if_mount_failed = false
    };
    err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK){
	ESP_LOGE(tag, "failed to mount spiffs partition: %s",
		 GeneralUtils::errorToString(err));
	return false;
    }
    
    // load configuration
    elfletConfig = new Config(wakeupCause);
    return elfletConfig->load();
}

//----------------------------------------------------------------------
// Config object initialize / deinitialize
//----------------------------------------------------------------------
Config::Config(WakeupCause cause) :
    fromStorage(false), isDirtyBootMode(false), isDirty(false),
    wakeupCause(cause),
    functionMode(FullSpec), sensorFrequency(0), 
    pubSubSessionType(SessionTCP){
}

Config::~Config(){
}

Config& Config::operator = (const Config& src){
    isDirty = true;
    isDirtyBootMode = true;

    bootMode = src.bootMode;
    wakeupCause = src.wakeupCause;
    fileGeneration = src.fileGeneration;
    functionMode = src.functionMode;
    boardVersion = src.boardVersion;
    nodeName = src.nodeName;
    apssid = src.apssid;
    adminPassword = src.adminPassword;
    ssidToConnect = src.ssidToConnect;
    wifiPassword = src.wifiPassword;
    ntpServer = src.ntpServer;
    timezone = src.timezone;
    sensorFrequency = src.sensorFrequency;
    pubSubServerAddr = src.pubSubServerAddr;
    pubSubSessionType = src.pubSubSessionType;
    pubSubServerCert = src.pubSubServerCert;
    pubSubUser = src.pubSubUser;
    pubSubPassword = src.pubSubPassword;
    sensorTopic = src.sensorTopic;
    irrcRecieveTopic = src.irrcRecieveTopic;
    irrcRecievedDataTopic = src.irrcRecievedDataTopic;
    irrcSendTopic = src.irrcSendTopic;
    downloadFirmwareTopic = src.downloadFirmwareTopic;

    updateDefaultSensorTopic();

    return *this;
}

//----------------------------------------------------------------------
// Load / Save
//----------------------------------------------------------------------
bool Config::load(){
    // decide bootmode & config file from sotred data in NVS
    NVS nvs(NVS_NS, NVS_READONLY);
    uint32_t mode;
    if (nvs.get(BOOTMODE_KEY, mode) != ESP_OK){
	bootMode = FactoryReset;
	fileGeneration = 0;
    }else{
	bootMode = (BootMode)mode;
	if (nvs.get(CONFIGGEN_KEY, fileGeneration) != ESP_OK ||
	    bootMode == FactoryReset){
	    bootMode = FactoryReset;
	    fileGeneration = 0;
	}
    }
    bootModeCurrent = bootMode;

    // parse configuration file
    auto path = CONFIGPATH[fileGeneration];
    struct stat sbuf;
    int rc = stat(path, &sbuf);
    if (rc != 0 || S_ISDIR(sbuf.st_mode)){
	ESP_LOGE(tag, "cannot access configuration file: %s", path);
	return false;
    }
    auto length = sbuf.st_size;
    char* buf = new char[length];
    FILE* fp = fopen(path, "r");
    rc = fread(buf, length, 1, fp);
    if (rc != 1){
	ESP_LOGE(tag, "cannot read configuration file: %s", path);
	fclose(fp);
	delete buf;
	return false;
    }
    fclose(fp);
    std::string contents(buf, length);
    delete buf;

    std::string err;
    auto config = json11::Json::parse(contents, err);
    if (!config.is_object()){
	ESP_LOGE(tag, "configuration file might corrupted: %s", path);
	return false;
    }
    
    // reflect configuration file to object attribute
    applyValue(config, JSON_FUNCTIONMODE, functionMode);
    applyValue(config, JSON_BOARDVERSION, boardVersion);
    applyValue(config, JSON_NODENAME, nodeName);
    applyValue(config, JSON_APSSID, apssid);
    applyValue(config, JSON_ADMINPASSWORD, adminPassword);
    applyValue(config, JSON_SSID, ssidToConnect);
    applyValue(config, JSON_WIFIPASSWORD, wifiPassword);
    applyValue(config, JSON_NTPSERVER, ntpServer);
    applyValue(config, JSON_TIMEZONE, timezone);
    applyValue(config, JSON_SENSORFREQUENCY, sensorFrequency);
    applyValue(config, JSON_PUBSUBSERVERADDR, pubSubServerAddr);
    applyValue(config, JSON_PUBSUBSESSIONTYPE, pubSubSessionType);
    applyValue(config, JSON_PUBSUBSERVERCERT, pubSubServerCert);
    applyValue(config, JSON_PUBSUBUSER, pubSubUser);
    applyValue(config, JSON_PUBSUBPASSWORD, pubSubPassword);
    applyValue(config, JSON_SENSORTOPIC, sensorTopic);
    applyValue(config, JSON_IRRCRECIEVETOPIC, irrcRecieveTopic);
    applyValue(config, JSON_IRRCRECIEVEDDATATOPIC, irrcRecievedDataTopic);
    applyValue(config, JSON_IRRCSENDTOPIC, irrcSendTopic);
    applyValue(config, JSON_DOWNLOADFIRMWARETOPIC, downloadFirmwareTopic);

    updateDefaultSensorTopic();

    isDirty = false;
    isDirtyBootMode = false;
    
    return true;
}

void Config::applyValue(const json11::Json& json, const std::string& key,
		bool& value){
    auto obj = json[key];
    if (obj.is_bool()){
	value = obj.bool_value();
    }
}

void Config::applyValue(const json11::Json& json, const std::string& key,
		int32_t& value){
    auto obj = json[key];
    if (obj.is_number()){
	value = obj.int_value();
    }
}

void Config::applyValue(const json11::Json& json, const std::string& key,
		std::string& value){
    auto obj = json[key];
    if (obj.is_string()){
	value = obj.string_value();
    }
}

bool Config::commit(){
    NVS nvs(NVS_NS, NVS_READWRITE);

    if (isDirty && bootMode != FactoryReset){
	auto obj = json11::Json::object({
		{JSON_FUNCTIONMODE, functionMode},
		{JSON_BOARDVERSION, boardVersion},
		{JSON_NODENAME, nodeName},
		{JSON_APSSID, apssid},
		{JSON_ADMINPASSWORD, adminPassword},
		{JSON_SSID, ssidToConnect},
		{JSON_WIFIPASSWORD, wifiPassword},
		{JSON_NTPSERVER, ntpServer},
		{JSON_TIMEZONE, timezone},
		{JSON_SENSORFREQUENCY, sensorFrequency},
		{JSON_PUBSUBSERVERADDR, pubSubServerAddr},
		{JSON_PUBSUBSESSIONTYPE, (int32_t)pubSubSessionType},
		{JSON_PUBSUBSERVERCERT, pubSubServerCert},
		{JSON_PUBSUBUSER, pubSubUser},
		{JSON_PUBSUBPASSWORD, pubSubPassword},
		{JSON_SENSORTOPIC, sensorTopic},
		{JSON_IRRCRECIEVETOPIC, irrcRecieveTopic},
		{JSON_IRRCRECIEVEDDATATOPIC, irrcRecievedDataTopic},
		{JSON_IRRCSENDTOPIC, irrcSendTopic},
		{JSON_DOWNLOADFIRMWARETOPIC, downloadFirmwareTopic},
	    });
	
	fileGeneration = (fileGeneration & 1) + 1;
	auto path = CONFIGPATH[fileGeneration];
	auto out = std::ofstream(path);
	out << json11::Json(obj).dump();
	out.close();
	
	nvs.set(CONFIGGEN_KEY, fileGeneration);
    }

    if (isDirtyBootMode){
	nvs.set(BOOTMODE_KEY, (uint32_t)bootMode);
    }

    nvs.commit();
    isDirty = false;
    isDirtyBootMode = false;
    
    return true;
}

//----------------------------------------------------------------------
// update value
//----------------------------------------------------------------------
bool Config::setBootMode(BootMode mode){
    bootMode = mode;
    isDirtyBootMode = true;
    return true;
}

bool Config::setFunctionMode(FunctionMode mode){
    if (mode == SensorOnly && pubSubServerAddr.length() == 0){
	return false;
    }
    functionMode = (int32_t)mode;
    isDirty = true;
    return true;
}

bool Config::setNodeName(const std::string& name){
    if (name.length() > MAX_NODENAME_LEN){
	return false;
    }
    nodeName = name;
    isDirty = true;
    updateDefaultSensorTopic();
    return true;
}

bool Config::setAPSSID(const std::string& ssid){
    if (ssid.length() > MAX_NODENAME_LEN){
	return false;
    }
    apssid = ssid;
    isDirty = true;
    return true;
}

bool Config::setAdminPassword(const std::string& pass){
    if (pass.length() < 8 || pass.length() > MAX_PASSWORD_LEN){
	return false;
    }
    adminPassword = pass;
    isDirty = true;
    return true;
}

bool Config::setSSIDtoConnect(const std::string& ssid){
    if (ssid.length() > MAX_NODENAME_LEN){
	return false;
    }
    ssidToConnect = ssid;
    isDirty = true;
    return true;
}

bool Config::setWifiPassword(const std::string& pass){
    if (pass.length() > MAX_PASSWORD_LEN){
	return false;
    }
    wifiPassword = pass;
    isDirty = true;
    return true;
}

bool Config::setNtpServer(const std::string& server){
    ntpServer = server;
    isDirty = true;
    return true;
}

bool Config::setTimezone(const std::string& tz){
    timezone = tz;
    isDirty = true;
    return true;
}

bool Config::setSensorFrequency(int32_t frequency){
    if (frequency < 0){
	return false;
    }
    sensorFrequency = frequency;
    isDirty = true;
    return true;
}

bool Config::setPubSubServerAddr(const std::string& addr){
    pubSubServerAddr = addr;
    isDirty = true;
    return true;
}

bool Config::setPubSubSessionType(SessionType type){
    pubSubSessionType = type;
    isDirty = true;
    return true;
}

bool Config::setPubSubServerCert(const std::string& cert){
    pubSubServerCert = cert;
    isDirty = true;
    return true;
}

bool Config::setPubSubUser(const std::string& user){
    pubSubUser = user;
    isDirty = true;
    return true;
}

bool Config::setPubSubPassword(const std::string& pass){
    pubSubPassword = pass;
    isDirty = true;
    return true;
}

bool Config::setSensorTopic(const std::string& topic){
    sensorTopic = topic;
    return true;
}

bool Config::setIrrcRecieveTopic(const std::string& topic){
    irrcRecieveTopic = topic;
    return true;
}

bool Config::setIrrcRecievedDataTopic(const std::string& topic){
    irrcRecievedDataTopic = topic;
    return true;
}

bool Config::setIrrcSendTopic(const std::string& topic){
    irrcSendTopic = topic;
    return true;
}

bool Config::setDownloadFirmwareTopic(const std::string& topic){
    downloadFirmwareTopic = topic;
    return true;
}

void Config::updateDefaultSensorTopic(){
    defaultSensorTopic = nodeName;
    defaultSensorTopic += "/sensor";
    defaultIrrcRecieveTopic = nodeName;
    defaultIrrcRecieveTopic += "/irrcRecieve";
    defaultIrrcRecievedDataTopic = nodeName;
    defaultIrrcRecievedDataTopic += "/irrcRecievedData";
    defaultIrrcSendTopic = nodeName;
    defaultIrrcSendTopic += "/irrcSend";
}

//----------------------------------------------------------------------
// static value handling
//----------------------------------------------------------------------
const char* Config::getVerificationKeyPath(){
    return VERIFICATION_KEY_PATH;
}
