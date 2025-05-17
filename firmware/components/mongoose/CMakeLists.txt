idf_component_register(SRCS mongoose/mongoose.c
                       INCLUDE_DIRS mongoose
                       REQUIRES
                       PRIV_REQUIRES)

target_compile_options(${COMPONENT_LIB} PRIVATE -DMG_ENABLE_HTTP_STREAMING_MULTIPART  -DMG_ENABLE_FILESYSTEM=1 -Wformat-truncation=0)
