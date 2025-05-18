IMAGE		:= $(BUILD_DIR_BASE)/spiffsimage.bin
FSSRCDIR	:= $(COMPONENT_BUILD_DIR)/root
PDATA		:= $(FSSRCDIR)/pdata.json
VKEY		:= $(FSSRCDIR)/verificationkey.pem
ARCHIVE		:= $(COMPONENT_BUILD_DIR)/libspiffsimage.a

build: checkvar $(IMAGE)

clean:
	rm -rf $(FSSRCDIR) $(IMAGE)

checkvar:
	@if [ "$(BUILD_DIR_BASE)" = "" ] || \
	    [ "$(COMPONENT_BUILD_DIR)" = "" ] || \
	    [ "$(COMPONENT_PATH)" = "" ] || \
	    [ "$(CONFIG_HW_VERSION)" = "" ] || \
	    [ "$(CONFIG_ELFLET_NODE_NAME)" = "" ] || \
	    [ "$(CONFIG_ELFLET_ADMIN_PASSWORD)" = "" ] || \
	    [ "$(CONFIG_OTA_IMAGE__VERIFICATION_KEY)" = "" ]; then \
	    echo 'ERROR: following variables must be defined:' >&2; \
	    echo '    BUILD_DIR_BASE' >&2; \
	    echo '    COMPONENT_BUILD_DIR' >&2; \
	    echo '    COMPONENT_PATH' >&2; \
	    echo '    CONFIG_HW_VERSION' >&2; \
	    echo '    CONFIG_ELFLET_NODE_NAME' >&2; \
	    echo '    CONFIG_ELFLET_ADMIN_PASSWORD' >&2; \
	    echo '    CONFIG_OTA_IMAGE__VERIFICATION_KEY' >&2; \
	    exit 1;\
	fi || exit 1

$(IMAGE): $(PDATA) $(VKEY)
	mkspiffs -c $(FSSRCDIR) -b 4096 -p 256 -s 0x10000 $@

$(PDATA): $(FSSRCDIR) $(COMPONENT_PATH)/pdata.json.in $(ARCHIVE)
	m4 -DHW_VERSION='"'$(CONFIG_HW_VERSION)'"' \
	   -DNODE_NAME='"'$(CONFIG_ELFLET_NODE_NAME)'"' \
	   -DADMIN_PASSWORD='"'$(CONFIG_ELFLET_ADMIN_PASSWORD)'"' \
	   $(COMPONENT_PATH)/pdata.json.in > $@

$(ARCHIVE): $(COMPONENT_PATH)/dummy.c
	$(CC) -c -o $(COMPONENT_BUILD_DIR)/dummy.o $(COMPONENT_PATH)/dummy.c
	$(AR) r $@ $(COMPONENT_BUILD_DIR)/dummy.o

$(VKEY): $(FSSRCDIR)
	cp $(CONFIG_OTA_IMAGE__VERIFICATION_KEY) $@

$(FSSRCDIR): $(COMPONENT_BUILD_DIR)
	mkdir $@ || true

$(COMPONENT_BUILD_DIR):
	mkdir $@ || true
