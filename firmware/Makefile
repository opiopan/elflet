#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME 		:= elflet
EXCLUDE_COMPONENTS	:= asio aws_iot coap \
			   esp_adc_cal esp_http_server esp_https_ota \
			   esp_https_server espmqtt expat freemodbus \
			   jsmn libsodium openssl \
			   protocomm
SIGNEDIMAGE		:= build/$(PROJECT_NAME).sbin

myall : all $(SIGNEDIMAGE)

all: update_version

include $(IDF_PATH)/make/project.mk

update_version:
	OLD=`if [ build/.version ];then cat build/.version;fi`;\
	NEW=`tools/version`;\
	if ! [ "$$NEW" = "$$OLD" ];then\
	    echo GEN main/version.string;\
	    echo 'static const char verstr[] = "'`tools/version`'";' > \
	        main/version.string;\
	    echo "$$NEW" > build/.version;\
	    rm -f build/elflet.bin;\
	    rm -f build/elflet.sbin;\
	fi

#
# Rules regarding OTA update
#
$(SIGNEDIMAGE): build/$(PROJECT_NAME).bin
	tools/signimage $(CONFIG_OTA_IMAGE_SIGNING_KEY) \
	                build/$(PROJECT_NAME).bin
	echo -n '{"FirmwareVersion":"'`cat build/.version`'",'\
	     '"FirmwareUri":"'$(CONFIG_FIRMWARE_DOWNLOAD_URI)'"}' > \
	    build/$(PROJECT_NAME).json

otaflash: $(SIGNEDIMAGE)
	if [ "$(OTAUSER)" = "" ] || [ "$(OTAPASS)" = "" ] || \
	   [ "$(OTAADDR)" = "" ]; then \
	    echo "Parameters need to proceed OTA are not specified." >&2; \
	    echo "Make sure following variables are specified:" >&2; \
	    echo "    OTAUSER" >&2; \
	    echo "    OTAPATH" >&2; \
	    echo "    OTAADDR" >&2; \
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
	echo `tools/version`
