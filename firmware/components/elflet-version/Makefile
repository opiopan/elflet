RAWVERSION	:= $(BUILD_DIR_BASE)/.version
HDRVERSION	:= version.string
ARCHIVE		:= $(COMPONENT_BUILD_DIR)/libelflet-version.a

GENFILES	:= $(RAWVERSION) $(HDRVERSION) $(ARCHIVE)

GENVERSION	:= ./version

build: update_version $(ARCHIVE)

update_version:
	@if [ "$(BUILD_DIR_BASE)" = "" ] || \
	    [ "$(COMPONENT_BUILD_DIR)" = "" ] || \
	    [ "$(COMPONENT_PATH)" = "" ] ;then\
	    echo error: ether of the following variables is not specified >&2;\
	    echo "    BUILD_DIR_BASE" >&2;\
	    echo "    COMPONENT_BUILD_DIR" >&2;\
	    echo "    COMPONENT_PATH" >&2;\
	    exit 1;\
	fi || exit 1
	@OLD=`if [ -f $(RAWVERSION) ];then cat $(RAWVERSION);fi`;\
	NEW=`$(GENVERSION)`;\
	if ! [ "$$NEW" = "$$OLD" ];then\
	    echo GEN $(HDRVERSION);\
	    echo 'static const char verstr[] = "'`$(GENVERSION)`'";' > \
	        $(HDRVERSION);\
	    echo "$$NEW" > $(RAWVERSION);\
	    rm -f $(BUILD_DIR_BASE)/elflet.bin;\
	    rm -f $(BUILD_DIR_BASE)/elflet.sbin;\
	fi


$(ARCHIVE): $(COMPONENT_PATH)/dummy.c
	$(CC) -c -o $(COMPONENT_BUILD_DIR)/dummy.o $(COMPONENT_PATH)/dummy.c
	$(AR) r $@ $(COMPONENT_BUILD_DIR)/dummy.o

.PHONY: clean
clean:
	rm -f $(GENFILES)
