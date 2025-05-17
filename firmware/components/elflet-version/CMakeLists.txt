idf_component_register(INCLUDE_DIRS .)

set(COMPONENT_TARGET libelflet-version.a)
set(MAKE_FLAGS
	BUILD_DIR_BASE=${BUILD_DIR};
	COMPONENT_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR};
	COMPONENT_PATH=${COMPONENT_DIR})

externalproject_add(
	elflet-version-build
	PREFIX ${COMPONENT_DIR}
        SOURCE_DIR ${COMPONENT_DIR}
	CONFIGURE_COMMAND ""
	BUILD_IN_SOURCE 1
	BUILD_COMMAND make -C${COMPONENT_DIR} ${MAKE_FLAGS} build
	INSTALL_COMMAND ""
	)

# Add libquirc.a to the build process
add_library(elflet-version STATIC IMPORTED GLOBAL)
add_dependencies(elflet-version elflet-version-build)

set_target_properties(elflet-version
	PROPERTIES IMPORTED_LOCATION
	${CMAKE_CURRENT_BINARY_DIR}/${COMPONENT_TARGET})
set_target_properties(elflet-version
	PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
	${COMPONENT_DIR})

set_directory_properties(PROPERTIES
	ADDITIONAL_MAKE_CLEAN_FILES
	"${CMAKE_CURRENT_BINARY_DIR}/${COMPONENT_TARGET}")
