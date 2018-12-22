#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME 		:= elflet
OCOMPO_DIR		:= components
#EXTRA_COMPONENT_DIRS	:= $(OCOMPO_DIR)
EXCLUDE_COMPONENTS	:= asio aws_iot coap \
			   esp_adc_cal esp_http_server esp_https_ota \
			   esp_https_server espmqtt expat freemodbus \
			   jsmn libsodium openssl \
			   protocomm
NKOLBAN_LIB 		:= cpp_utils
OUTERCOMPONENTS		:= $(OCOMPO_DIR)/$(NKOLBAN_LIB) $(OCOMPO_DIR)/posix
SIGNEDIMAGE		:= build/$(PROJECT_NAME).sbin

myall: all $(SIGNEDIMAGE)

include $(IDF_PATH)/make/project.mk

menuconfig defconfig: $(OUTERCOMPONENTS)

#
# Rules to prepare components from outer repositories
#
$(OCOMPO_DIR)/$(NKOLBAN_LIB): $(OCOMPO_DIR)
	if [ "$(NKOLBAN_ESP32_PATH)" = "" ]; then \
	    echo "NKOLBAN_ESP32_PATH environment variable is need to"\
	         "indicate a directory install nkolban's esp32 snipets." >&2;\
            exit 1;\
	fi
	rm -f $@
	ln -s $(NKOLBAN_ESP32_PATH)/$(NKOLBAN_LIB) $(OCOMPO_DIR)/

$(OCOMPO_DIR)/posix: $(OCOMPO_DIR)
	echo BUILDING $@ target
	cp -R $(NKOLBAN_ESP32_PATH)/posix $@
	touch $@/component.mk

$(OCOMPO_DIR):
	mkdir $@

#
# Rules regarding OTA update
#
$(SIGNEDIMAGE): build/$(PROJECT_NAME).bin
	tools/signimage $(CONFIG_OTA_IMAGE_SIGNING_KEY) \
	                build/$(PROJECT_NAME).bin

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
	rm -rf $(OUTERCOMPONENTS)
	rm -rf sdkconfig sdkconfig.old
	make -C main -f component.mk extra-clean

#
# show version
#
version:
	echo `tools/version`
