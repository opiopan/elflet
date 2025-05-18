set(COMPONENT_TARGET libelflet-version.a)
set(MAKE_FLAGS
	BUILD_DIR_BASE=${BUILD_DIR};
	COMPONENT_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR};
	COMPONENT_PATH=${COMPONENT_DIR})
set(VERSION_FILES ${COMPONENT_DIR}/version.string ${BUILD_DIR}/.version)

idf_component_register(
	INCLUDE_DIRS .
	EMBED_FILES ${VERSION_FILES}
)

add_custom_command(
	OUTPUT elflet-version-build-always
	COMMAND make -C${COMPONENT_DIR} ${MAKE_FLAGS} build	
)
add_custom_target(elflet-version-build ALL DEPENDS elflet-version-build-always)

foreach(file ${VERSION_FILES})
	add_custom_command(
		OUTPUT ${file}
		COMMAND true
		DEPENDS elflet-version-build
		VERBATIM
	)
endforeach()
add_custom_target(elflet-version-files ALL DEPENDS ${VERSION_FILES})
	

set_directory_properties(
	PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
	"${CMAKE_CURRENT_BINARY_DIR}/${COMPONENT_TARGET}" ${VERSION_FILES}
)
