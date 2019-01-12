#include <esp_log.h>
#include <esp_system.h>
#include <GeneralUtils.h>
#include <json11.hpp>
#include <iostream>
#include <sstream>
#include <functional>
#include "webserver.h"
#include "reboot.h"
#include "Config.h"
#include "REST.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "RESTconfig";

static const char* functionModeStr[]{
    "FullSpec", "SensorOnly", NULL
};

static const char* sessionTypeStr[]{
    "TCP", "TSL", "WebSocket", "WebSocketSecure", NULL
};

static const char* irrcReceiverModeStr[]{
    "OnDemand", "Continuous", NULL
};

static bool ApplyValue(const json11::Json& json, const std::string& key,
		       std::function<bool(const std::string&)> apply){
    auto obj = json[key];
    if (obj.is_string()){
	return apply(obj.string_value());
    }
    return true;
}

static bool ApplyValue(const json11::Json& json, const std::string& key,
		       std::function<bool(int32_t)> apply){
    auto obj = json[key];
    if (obj.is_number()){
	return apply(obj.int_value());
    }
    return true;
}

static bool ApplyBoolValue(const json11::Json& json, const std::string& key,
			   std::function<bool(bool)> apply){
    auto obj = json[key];
    if (obj.is_bool()){
	return apply(obj.bool_value());
    }
    return true;
}

enum ApplyResult {AR_ERROR, AR_OK, AR_NEEDCOMMIT};

static void serializeConfig(HttpResponse* resp){
    auto conf = elfletConfig;
    auto obj = json11::Json::object({
	    {JSON_BOARDTYPE, "elflet"},
	    {JSON_BOARDVERSION, conf->getBoardVersion()},
	    {JSON_FWVERSION, getVersionString()},
	    {JSON_FUNCTIONMODE, functionModeStr[conf->getFunctionMode()]},
	    {JSON_NODENAME, conf->getNodeName()},
	    {JSON_SSID, conf->getSSIDtoConnect()},
	    {JSON_NTPSERVER, conf->getNtpServer()},
	    {JSON_TIMEZONE, conf->getTimezone()},
	    {JSON_SENSORFREQUENCY, conf->getSensorFrequency()},
	    {JSON_IRRCRECEIVERMODE,
		    irrcReceiverModeStr[conf->getIrrcReceiverMode()]},
	    {JSON_BLEHID, conf->getBleHid()},
	});
    auto pubsub = json11::Json::object({
	    {JSON_PUBSUBSERVERADDR, conf->getPubSubServerAddr()},
	    {JSON_PUBSUBSESSIONTYPE,
		    sessionTypeStr[conf->getPubSubSessionType()]},
	    {JSON_PUBSUBSERVERCERT, conf->getPubSubServerCert()},
	    {JSON_PUBSUBUSER, conf->getPubSubUser()},
	    {JSON_SENSORTOPIC, conf->getSensorTopic()},
	    {JSON_IRRCRECEIVETOPIC, conf->getIrrcReceiveTopic()},
	    {JSON_IRRCRECEIVEDDATATOPIC, conf->getIrrcReceivedDataTopic()},
	    {JSON_IRRCSENDTOPIC, conf->getIrrcSendTopic()},
	    {JSON_SHADOWTOPIC, conf->getShadowTopic()},
	    {JSON_DOWNLOADFIRMWARETOPIC, conf->getDownloadFirmwareTopic()},
    });
    
    if (conf->getNodeName() != conf->getAPSSID()){
	obj[JSON_APSSID] = conf->getAPSSID();
    }
    if (conf->getBootMode() == Config::FactoryReset ||
	conf->getBootMode() == Config::Configuration){
	obj[JSON_WIFIPASSWORD] = conf->getWifiPassword();
	pubsub[JSON_PUBSUBPASSWORD] = conf->getPubSubPassword();
    }

    obj[JSON_PUBSUB] = pubsub;
    
    stringPtr ptr(new std::string(json11::Json(obj).dump()));
    resp->setBody(ptr);
}

static bool applyPubSub(const json11::Json& input, const char** rmsg){
    const char* msg = NULL;
    
    ApplyValue(input, JSON_PUBSUBSERVERADDR,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setPubSubServerAddr(v);});
    ApplyValue(input, JSON_PUBSUBSESSIONTYPE,
	       [](const std::string& v) -> bool{
		   int i;
		   for (i = 0; sessionTypeStr[i]; i++){
		       if (v == sessionTypeStr[i]){
			   break;
		       }
		   }
		   if (sessionTypeStr[i]){
		       return elfletConfig->setPubSubSessionType(
			   (Config::SessionType)i);
		   }else{
		       return false;
		   }
	       }) ||
	(msg = "session type for publishing and subscribing is invalid");
    ApplyValue(input, JSON_PUBSUBSERVERCERT,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setPubSubServerCert(v);});
    ApplyValue(input, JSON_PUBSUBUSER,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setPubSubUser(v);});
    ApplyValue(input, JSON_PUBSUBPASSWORD,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setPubSubPassword(v);});
    ApplyValue(input, JSON_SENSORTOPIC,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setSensorTopic(v);});
    ApplyValue(input, JSON_IRRCRECEIVETOPIC,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setIrrcReceiveTopic(v);});
    ApplyValue(input, JSON_IRRCRECEIVEDDATATOPIC,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setIrrcReceivedDataTopic(v);});
    ApplyValue(input, JSON_IRRCSENDTOPIC,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setIrrcSendTopic(v);});
    ApplyValue(input, JSON_DOWNLOADFIRMWARETOPIC,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setDownloadFirmwareTopic(v);});
    ApplyValue(input, JSON_SHADOWTOPIC,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setShadowTopic(v);});

    if (msg){
	*rmsg = msg;
	return false;
    }else{
	return true;
    }
}

static ApplyResult applyConfig(const WebString& json, const char** msg){
    std::string err;
    auto input = json11::Json::parse(std::string(json.data(), json.length()),
				     err);
    if (!input.is_object()){
	*msg = "invalid request body format";
	return AR_ERROR;
    }

    *msg = NULL;

    if (elfletConfig->getBootMode() == Config::Normal &&
	(input[JSON_ADMINPASSWORD].is_string() ||
	 input[JSON_SSID].is_string() ||
	 input[JSON_WIFIPASSWORD].is_string()||
	 input[JSON_PUBSUBPASSWORD].is_string())){
	*msg = "Password and WiFi connection information"
	       "must be changed in configuration mode.";
	return AR_ERROR;
    }
    
    ApplyValue(input, JSON_NODENAME,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setNodeName(v);}) ||
	(*msg = "node name must be less than 32 bytes");
    ApplyValue(input, JSON_APSSID,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setAPSSID(v);}) ||
	(*msg = "SSID as accesspoint  must be less than 32 bytes");
    ApplyValue(input, JSON_ADMINPASSWORD,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setAdminPassword(v);}) ||
	(*msg = "SSID as accesspoint  must be grater than 8 bytes and "
	        "less than 64 bytes");
    ApplyValue(input, JSON_SSID,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setSSIDtoConnect(v);}) ||
	(*msg = "SSID to connect WiFi must be less than 32 bytes");
    ApplyValue(input, JSON_WIFIPASSWORD,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setWifiPassword(v);}) ||
	(*msg = "password to connect WiFi must be less than 64 bytes");
    ApplyValue(input, JSON_NTPSERVER,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setNtpServer(v);});
    ApplyValue(input, JSON_TIMEZONE,
	       [](const std::string& v) -> bool{
		   return elfletConfig->setTimezone(v);});
    ApplyValue(input, JSON_SENSORFREQUENCY,
	       [](int32_t v) -> bool{
		   return elfletConfig->setSensorFrequency(v);}) ||
	(*msg = "frequency of sensor update  must be grater than 0");

    auto pubsub = input[JSON_PUBSUB];
    if (pubsub.is_object()){
	applyPubSub(json11::Json(pubsub.object_items()), msg);
    }else{
	applyPubSub(input, msg);
    }

    ApplyValue(input, JSON_FUNCTIONMODE, [&](const std::string& v) -> bool{
	    int i;
	    for (i = 0; functionModeStr[i]; i++){
		if (v == functionModeStr[i]){
		    break;
		}
	    }
	    if (functionModeStr[i]){
		bool rc =
		    elfletConfig->setFunctionMode((Config::FunctionMode)i);
		if (!rc){
		    *msg = "PubSub server address needs to set "
		    	"if sensor only mode is enabled";
		}
		return rc;
	    }else{
		*msg = "invalid function mode is specified";
		return false;
	    }
	});

    ApplyValue(input, JSON_IRRCRECEIVERMODE, [&](const std::string& v) -> bool{
	    int i;
	    for (i = 0; irrcReceiverModeStr[i]; i++){
		if (v == irrcReceiverModeStr[i]){
		    break;
		}
	    }
	    if (irrcReceiverModeStr[i]){
		elfletConfig->setIrrcReceiverMode((Config::IrrcReceiverMode)i);
		return true;
	    }else{
		*msg = "invalid irrc receiver mode is specified";
		return false;
	    }
	});

    ApplyBoolValue(input, JSON_BLEHID,
	       [](const bool v) -> bool{
		   return elfletConfig->setBleHid(v);});
    
    bool commit =
	input[JSON_COMMIT].is_bool() && input[JSON_COMMIT].bool_value();
    
    return *msg != NULL ? AR_ERROR : commit ? AR_NEEDCOMMIT : AR_OK;
}

static bool commitAndReboot(){
    if (elfletConfig->getSSIDtoConnect().length() == 0){
	return false;
    }

    elfletConfig->setBootMode(Config::Normal);
    elfletConfig->commit();
    rebootIn(1000);

    return true;
}

class SetConfigHandler : public WebServerHandler {
    bool needDigestAuthentication(HttpRequest& req) override{
	if (req.method() == HttpRequest::MethodGet){
	    return false;
	}else{
	    return elfletConfig->getBootMode() == Config::Normal;
	}
    };

    void receiveRequest(WebServerConnection& connection) override{
	auto req = connection.request();
	auto resp = connection.response();
	auto httpStatus = HttpResponse::RESP_200_OK;
	auto contentType = "text/plain";
	if (req->method() == HttpRequest::MethodGet){
	    serializeConfig(resp);
	    contentType = "application/json";
	}else if (req->method() == HttpRequest::MethodPost &&
	    req->header("Content-Type") == "application/json"){
	    const char* msg;
	    auto result = applyConfig(req->body(), &msg);
	    if (result == AR_ERROR){
		httpStatus = HttpResponse::RESP_500_InternalServerError;
		resp->setBody("invalid request body");
	    }else if (result == AR_NEEDCOMMIT && !commitAndReboot()){
		httpStatus = HttpResponse::RESP_500_InternalServerError;
		resp->setBody("cannot transition to normalmode "
			      "due to there is no information to "
			      "connect to WiFi");
	    }
	}else{
	    httpStatus = HttpResponse::RESP_500_InternalServerError;
	    resp->setBody("invalid request body or method");
	}
	resp->setHttpStatus(httpStatus);
	resp->addHeader("Content-Type", contentType);
	resp->close();
    };
    
};

void registerConfigRESTHandler(WebServer* server){
    server->setHandler(new SetConfigHandler, "/manage/config");
}
