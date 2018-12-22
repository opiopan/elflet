COMPONENT_OWNBUILDTARGET	:= 1
COMPONENT_OWNCLEANTARGET 	:= 1
COMPONENT_ADD_LDFLAGS		:=

IMAGE		:= $(BUILD_DIR_BASE)/spiffsimage.bin
FSSRCDIR	:= $(COMPONENT_BUILD_DIR)/root
PDATA		:= $(FSSRCDIR)/pdata.json
VKEY		:= $(FSSRCDIR)/verificationkey.pem
ARCHIVE		:= $(COMPONENT_BUILD_DIR)/libspiffsimage.a

build: $(IMAGE)

clean:
	rm -rf $(FSSRCDIR) $(IMAGE)

$(IMAGE): $(PDATA) $(VKEY)
	mkspiffs -c $(FSSRCDIR) -b 4096 -p 256 -s 0x10000 $@

$(PDATA): $(FSSRCDIR) $(COMPONENT_PATH)/pdata.json.in $(ARCHIVE)
	m4 -DHW_VERSION='"'$(CONFIG_HW_VERSION)'"' \
	   $(COMPONENT_PATH)/pdata.json.in > $@

$(ARCHIVE): $(COMPONENT_PATH)/dummy.c
	$(CC) -c -o $(COMPONENT_BUILD_DIR)/dummy.o $(COMPONENT_PATH)/dummy.c
	$(AR) r $@ $(COMPONENT_BUILD_DIR)/dummy.o

#$(VKEY): $(FSSRCDIR) $(CONFIG_OTA_IMAGE__VERIFICATION_KEY)
$(VKEY): $(FSSRCDIR)
	cp $(CONFIG_OTA_IMAGE__VERIFICATION_KEY) $@

$(FSSRCDIR):
	mkdir $@
