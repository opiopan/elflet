set(COMPONENT_TARGET libspiffsimage.a)
set(MAKE_FLAGS
    BUILD_DIR_BASE=${BUILD_DIR};
    COMPONENT_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR};
    COMPONENT_PATH=${COMPONENT_DIR};
    CONFIG_HW_VERSION=${CONFIG_HW_VERSION};
    CONFIG_ELFLET_NODE_NAME=${CONFIG_ELFLET_NODE_NAME};
    CONFIG_ELFLET_ADMIN_PASSWORD=${CONFIG_ELFLET_ADMIN_PASSWORD};
    CONFIG_OTA_IMAGE__VERIFICATION_KEY=${CONFIG_OTA_IMAGE__VERIFICATION_KEY};
    )
set(IMAGE ${BUILD_DIR}/spiffsimage.bin)

idf_component_register(
    EMBED_FILES ${IMAGE}
)

add_custom_command(
    OUTPUT make_spiffsimage
    COMMAND make -C${COMPONENT_DIR} ${MAKE_FLAGS} build
)
add_custom_target(spiffsimage-build ALL DEPENDS make_spiffsimage)
add_custom_command(
    OUTPUT ${IMAGE}
    COMMAND true
    DEPENDS spiffsimage-build
    VERBATIM
)
add_custom_target(spiffsimage-files ALL DEPENDS ${IMAGE})

set_directory_properties(
	PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
	${IMAGE}
)
