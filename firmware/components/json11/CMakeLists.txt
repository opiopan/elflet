idf_component_register(SRCS json11/json11.cpp
                       INCLUDE_DIRS json11
                       REQUIRES
                       PRIV_REQUIRES )

target_compile_options(${COMPONENT_LIB} PRIVATE -DMG_ENABLE_HTTP_STREAMING_MULTIPART  -DMG_ENABLE_FILESYSTEM=1)
