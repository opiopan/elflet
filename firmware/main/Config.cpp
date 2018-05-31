#include <esp_log.h>
#include <esp_system.h>
#include <esp_spiffs.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <sys/stat.h>
#include <stdio.h>
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

static const std::string JSON_NODENAME = "NodeName";
static const std::string JSON_APSSID = "AP_SSID";
static const std::string JSON_ADMINPASSWORD = "AdminPassword";
static const std::string JSON_BOARDVERSION = "BoardVersion";
static const std::string JSON_SSID = "SSID";
static const std::string JSON_WIFIPASSWORD = "WiFiPassword";

static const int MAX_NODENAME_LEN = 32;
static const int MAX_PASSWORD_LEN = 64;

Config* elfletConfig = NULL;

//----------------------------------------------------------------------
// initialize global configuration
//----------------------------------------------------------------------
bool initConfig(){
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
    elfletConfig = new Config();
    return elfletConfig->load();
}

//----------------------------------------------------------------------
// Config object initialize / deinitialize
//----------------------------------------------------------------------
Config::Config() : fromStorage(false), isDirtyBootMode(false), isDirty(false){
}

Config::~Config(){
}

Config& Config::operator = (const Config& src){
    isDirty = true;
    isDirtyBootMode = true;

    bootMode = src.bootMode;
    fileGeneration = src.fileGeneration;
    boardVersion = src.boardVersion;
    nodeName = src.nodeName;
    apssid = src.apssid;
    adminPassword = src.adminPassword;
    ssidToConnect = src.ssidToConnect;
    wifiPassword = src.wifiPassword;

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
	if (nvs.get(CONFIGGEN_KEY, fileGeneration) != ESP_OK){
	    bootMode = FactoryReset;
	    fileGeneration = 0;
	}
    }

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
    applyValue(config, JSON_BOARDVERSION, boardVersion);
    applyValue(config, JSON_NODENAME, nodeName);
    applyValue(config, JSON_APSSID, apssid);
    applyValue(config, JSON_ADMINPASSWORD, adminPassword);
    applyValue(config, JSON_SSID, ssidToConnect);
    applyValue(config, JSON_WIFIPASSWORD, wifiPassword);

    isDirty = false;
    isDirtyBootMode = false;
    
    return true;
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
		{JSON_BOARDVERSION, boardVersion},
		{JSON_NODENAME, nodeName},
		{JSON_APSSID, apssid},
		{JSON_ADMINPASSWORD, adminPassword},
		{JSON_SSID, ssidToConnect},
		{JSON_WIFIPASSWORD, wifiPassword}
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
bool Config::setbootMode(BootMode mode){
    bootMode = mode;
    isDirtyBootMode = true;
    return true;
}
    
bool Config::setNodeName(const std::string& name){
    if (name.length() > MAX_NODENAME_LEN){
	return false;
    }
    nodeName = name;
    isDirty = true;
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

bool Config::setSSIDtoConnect(std::string& ssid){
    if (ssid.length() > MAX_NODENAME_LEN){
	return false;
    }
    ssidToConnect = ssid;
    isDirty = true;
    return true;
}

bool Config::setWifiPassword(std::string& pass){
    if (pass.length() > MAX_PASSWORD_LEN){
	return false;
    }
    wifiPassword = pass;
    isDirty = true;
    return true;
}

const char* Config::getVerificationKeyPath(){
    return VERIFICATION_KEY_PATH;
}
