#include <esp_log.h>
#include <esp_ota_ops.h>
#include <string.h>
#include "signature.h"
#include "ota.h"

static const char tag[] = "ota";

class OTAimp : public OTA {
private:
    esp_ota_handle_t otaHandle;
    const char* pubkeyPath;
    const esp_partition_t* partition;
    size_t siglen;
    uint8_t signature[OTA_SIGNATURE_LENGTH];
    SHA256  digest;
    
public:
    OTAimp(const char* pubkey);
    virtual ~OTAimp();

    OTARESULT start(size_t imageSize);
    OTARESULT end(bool needCommit);
    
    virtual OTARESULT addDataFlagment(const void* flagment, size_t length);
};

OTAimp::OTAimp(const char* pubkey) :
    otaHandle(0), pubkeyPath(pubkey), partition(NULL), siglen(0){
}

OTAimp::~OTAimp() {
    if (otaHandle){
	end(false);
    }
}

OTARESULT OTAimp::start(size_t imageSize){
    partition = esp_ota_get_next_update_partition(NULL);
    if (partition == NULL){
	return OTA_NOT_FOUND_PARTITION;
    }
    esp_err_t rc = esp_ota_begin(partition, imageSize, &otaHandle);
    if (rc != ESP_OK){
	switch (rc) {
	case ESP_ERR_OTA_PARTITION_CONFLICT:
	    return OTA_BUSY;
	default:
	    return OTA_CANNOT_START;
	}
    }
    ESP_LOGI(tag, "ota preparation done");
    return OTA_SUCCEED;
}

OTARESULT OTAimp::end(bool needCommit) {
    if (otaHandle){
	ESP_LOGI(tag, "closing ota...");
	esp_err_t rc = esp_ota_end(otaHandle);
	otaHandle = 0;
	if (rc != ESP_OK){
	    return OTA_FAILED_WRITE;
	}

	if (siglen != sizeof(signature)){
	    ESP_LOGE(tag, "signature verification failed");
	    return OTA_FAILED_VERIFICATION;
	}
	PK pk;
	int res = pk.parsePublicKeyfile(pubkeyPath);
	if (res != 0){
	    ESP_LOGE(tag, "parsing verification key failed");
	    return OTA_FAILED_PARSE_PUBKEY;
	}
	digest.finish();
	if (pk.verify(&digest, signature, siglen) != 0){
	    ESP_LOGE(tag, "signature verification failed");
	    return OTA_FAILED_VERIFICATION;
	}

	if (needCommit){
	    ESP_LOGI(tag, "switch boot partition");
	    rc = esp_ota_set_boot_partition(partition);
	    if (rc != ESP_OK){
		return OTA_FAILED_CHANGE_BOOT;
	    }
	}
    }
    
    return OTA_SUCCEED;
}

OTARESULT OTAimp::addDataFlagment(const void* flagment, size_t length) {
    int offset = 0;
    if (siglen < sizeof(signature)){
	offset = sizeof(signature) - siglen;
	if (offset > length){
	    offset = length;
	}
	memcpy(signature + siglen, flagment, offset);
	siglen += offset;
	ESP_LOGI(tag, "signature retrieved");
    }

    if (length - offset > 0){
	const uint8_t* data = (uint8_t*)flagment + offset;
	size_t datalen = length - offset;
	digest.update(data, datalen);
	esp_err_t rc = esp_ota_write(otaHandle, data, datalen);
	if (rc != ESP_OK){
	    return OTA_FAILED_WRITE;
	}
    }
    return OTA_SUCCEED;
}

OTARESULT startOTA(const char* pubkey_path, size_t imageSize, OTA** outp) {
    OTAimp* ota = new OTAimp(pubkey_path);
    OTARESULT rc = ota->start(imageSize);
    if (rc == OTA_SUCCEED){
	*outp = ota;
    }
    return rc;
}

OTARESULT endOTA(const OTA* handle, bool needCommit) {
    OTAimp* ota = (OTAimp*)handle;
   OTARESULT rc =  ota->end(needCommit);
   delete ota;
   return rc;
}
