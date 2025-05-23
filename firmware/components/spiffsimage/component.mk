COMPONENT_OWNBUILDTARGET	:= build
COMPONENT_OWNCLEANTARGET 	:= clean
COMPONENT_ADD_LDFLAGS		:=

MAKEPARAM			:= BUILD_DIR_BASE=$(BUILD_DIR_BASE) \
				   COMPONENT_BUILD_DIR=$(COMPONENT_BUILD_DIR) \
				   COMPONENT_PATH=$(COMPONENT_PATH) \
				   CONFIG_HW_VERSION=$(CONFIG_HW_VERSION) \
				   CONFIG_ELFLET_NODE_NAME=$(CONFIG_ELFLET_NODE_NAME) \
				   CONFIG_ELFLET_ADMIN_PASSWORD=$(CONFIG_ELFLET_ADMIN_PASSWORD) \
				   CONFIG_OTA_IMAGE__VERIFICATION_KEY=$(CONFIG_OTA_IMAGE__VERIFICATION_KEY)

build:
	make -C$(COMPONENT_PATH) $(MAKEPARAM) build

clean:
	make -C$(COMPONENT_PATH) $(MAKEPARAM) clean
