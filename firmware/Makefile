#
# This is a project Makefile. It is assumed the directory this Makefile
# resides in is a project subdirectory.
#
IDF_SKIP_CHECK_SUBMODULES := 1

PROJECT_NAME 		:= elflet
EXCLUDE_COMPONENTS	:= asio aws_iot coap \
			   esp_adc_cal esp_http_server esp_https_ota \
			   esp_https_server espmqtt expat freemodbus \
			   jsmn libsodium openssl esp_local_ctrl\
			   protocomm wifi_provisioning
SIGNEDIMAGE		:= build/$(PROJECT_NAME).sbin
OTAUSER			:= admin
OTAPASS			:=
OTAADDR			:=

myall : all $(SIGNEDIMAGE)

all:

include $(IDF_PATH)/make/project.mk

#
# Rules regarding OTA update
#
$(SIGNEDIMAGE): build/$(PROJECT_NAME).bin
	tools/signimage $(CONFIG_OTA_IMAGE_SIGNING_KEY) \
	                build/$(PROJECT_NAME).bin
	echo '{"FirmwareVersion":"'`cat build/.version`'",'\
	     '"FirmwareUri":"'$(CONFIG_FIRMWARE_DOWNLOAD_URI)'"}' > \
	    build/$(PROJECT_NAME).json

otaflash: $(SIGNEDIMAGE)
	if [ "$(OTAUSER)" = "" ] || [ "$(OTAPASS)" = "" ] || \
	   [ "$(OTAADDR)" = "" ]; then \
	    echo >&2; \
	    echo "Parameters need to proceed OTA are not specified." >&2; \
	    echo "Make sure following variables are specified:" >&2; \
	    echo "    OTAPASS" >&2; \
	    echo "    OTAADDR" >&2; \
	    echo >&2; \
	    exit 1;\
	fi
	tools/ota "$(OTAUSER)" "$(OTAPASS)" "$(OTAADDR)" $(SIGNEDIMAGE)

#
# Rules to clean completely
#
complete-clean: clean
	rm -rf build
	rm -rf sdkconfig sdkconfig.old
	make -C main -f component.mk extra-clean

#
# show version
#
version:
	echo `components//elflet-version/version`
