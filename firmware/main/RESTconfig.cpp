#include <esp_log.h>
#include <esp_system.h>
#include <GeneralUtils.h>
#include <json11.hpp>
#include "webserver.h"
#include "reboot.h"
#include "Config.h"
#include "REST.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "RESTconfig";

static const std::string JSON_NODENAME = "NodeName";
static const std::string JSON_APSSID = "AP_SSID";
static const std::string JSON_ADMINPASSWORD = "AdminPassword";
static const std::string JSON_BOARDVERSION = "BoardVersion";
static const std::string JSON_SSID = "SSID";
static const std::string JSON_WIFIPASSWORD = "WiFiPassword";
static const std::string JSON_COMMIT = "commit";

static bool ApplyValue(const json11::Json& json, const std::string& key,
		       bool (*apply)(const std::string&)){
    auto obj = json[key];
    if (obj.is_string()){
	return apply(obj.string_value());
    }
    return true;
}

enum ApplyResult {AR_ERROR, AR_OK, AR_NEEDCOMMIT};

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
	 input[JSON_WIFIPASSWORD].is_string())){
	*msg = "Admin password and WiFi connection information"
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
    bool needDigestAuthentication() override{
	return elfletConfig->getBootMode() == Config::Normal;;
    };

    void recieveRequest(WebServerConnection& connection) override{
	auto req = connection.request();
	auto resp = connection.response();
	auto httpStatus = HttpResponse::RESP_200_OK;
	if (req->method() == HttpRequest::MethodPost &&
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
	resp->addHeader("Content-Type", "text/plain");
	resp->close();
    };
    
};

void registerConfigRESTHandler(WebServer* server){
    server->setHandler(new SetConfigHandler, "/manage/config");
}
