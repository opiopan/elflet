#include <string.h>
#include <string>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <GeneralUtils.h>
#include <Task.h>
#include <freertos/event_groups.h>
#include "Mutex.h"
#include "ota.h"
#include "Config.h"
#include "NameResolver.h"
#include "BleHidService.h"
#include "FirmwareDownloader.h"

#include "boardconfig.h"
#include "sdkconfig.h"

static const char tag[] = "FirmwareDownloader";

static const int MAX_HTTP_RECV_BUFFER = 2048;

class DownloadFirmware;
static Mutex mutex;
static DownloadFirmware* task;

//----------------------------------------------------------------------
// Download task
//----------------------------------------------------------------------
class DownloadFirmware : public Task {
public:
    bool isRunning;
    
protected:
    static const int EV_START_DOWNLOAD = 1;
    EventGroupHandle_t events;
    std::string uri;
    DFCallback callback;

public:
    DownloadFirmware();
    void download(const char* uri, DFCallback callback);

protected:
    void run(void *data) override;
};

DownloadFirmware::DownloadFirmware() : isRunning(false){
    events = xEventGroupCreate();
}

void DownloadFirmware::download(const char* uri, DFCallback callback){
    this->uri = uri;
    this->callback = callback;
    if (!isRunning){
	isRunning = true;
	xEventGroupSetBits(events, EV_START_DOWNLOAD);
    }
}

void DownloadFirmware::run(void *data){
    
    while (true){
	xEventGroupWaitBits(events, EV_START_DOWNLOAD,
			    pdTRUE, pdFALSE,
			    portMAX_DELAY);
	{
	    LockHolder holder(mutex);
	    if (!isRunning){
		continue;
	    }
	}

	resolveHostname(uri);
	ESP_LOGI(tag, "start download firmware: %s", uri.c_str());

	auto callback = this->callback;
	auto invokeCallback = [&](DFStatus st) -> void {
	    if (callback){
		callback(st);
	    }
	};

	char* buffer = (char*)malloc(MAX_HTTP_RECV_BUFFER);
	if (buffer == NULL) {
	    ESP_LOGE(tag, "cannot malloc http receive buffer");
	    abort();
	}

	esp_http_client_handle_t client = NULL;
	OTA* ota = NULL;
	bool needCommit = false;
	auto finalResult = dfSucceed;
	{
	    //
	    // establish HTTP connection & issue request
	    //
	    stopBleHidService();
	    esp_http_client_config_t config;
	    memset(&config, 0, sizeof(config));
	    config.url = uri.c_str();
	    client = esp_http_client_init(&config);
	    esp_err_t err;
	    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
		ESP_LOGE(tag, "failed to open HTTP connection: %s",
			 esp_err_to_name(err));
		invokeCallback(dfOpenError);
		goto CLEAN1;
	    }

	    //
	    // read response header
	    //
	    auto contentLength =  esp_http_client_fetch_headers(client);
	    if (contentLength < 0){
		ESP_LOGE(tag, "failed to read response header");
		invokeCallback(dfReadError);
		goto CLEAN2;
	    }
	    auto statusCode = esp_http_client_get_status_code(client);
	    if (statusCode != 200){
		ESP_LOGE(tag, "HTTP server returned error: %d", statusCode);
		invokeCallback(dfHttpError);
		goto CLEAN2;
	    }
	    OTARESULT otarc;
	    if ((otarc = startOTA(elfletConfig->getVerificationKeyPath(),
				  contentLength, &ota)) != OTA_SUCCEED){
		ESP_LOGE(tag, "failed to proceed OTAr: 0x%x", otarc);
		invokeCallback(dfOtaError);
		goto CLEAN2;
	    }

	    //
	    // read firmware image
	    //
	    int length;
	    while ((length = esp_http_client_read(client, buffer,
						  MAX_HTTP_RECV_BUFFER)) > 0){
		otarc = ota->addDataFlagment(buffer, length);
		if (otarc != OTA_SUCCEED){
		    ESP_LOGE(tag, "failed to proceed OTAr: 0x%x", otarc);
		    invokeCallback(dfOtaError);
		    goto CLEAN3;
		}
	    }
	    if (length < 0){
		ESP_LOGE(tag, "failed to read content");
		invokeCallback(dfReadError);
		goto CLEAN3;
	    }
	}

	needCommit = true;

	//
	// clean up
	//
    CLEAN3:
	if (endOTA(ota, needCommit) != OTA_SUCCEED){
	    finalResult = dfOtaError;
	}
    CLEAN2:
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
    CLEAN1:
	free(buffer);

	if (needCommit){
	    invokeCallback(finalResult);
	}else{
	    startBleHidService();
	}

	{
	    LockHolder holder(mutex);
	    isRunning = false;
	}
    }
}

//----------------------------------------------------------------------
// interfaces for outer module
//----------------------------------------------------------------------

DFStatus httpDownloadFirmware(const char* uri, DFCallback callback){
    LockHolder holder(mutex);
    if (!task){
	task = new DownloadFirmware;
	task->start();
    }
    if (task->isRunning){
	return dfBusy;
    }
    task->download(uri, callback);
    
    return dfSucceed;
}
