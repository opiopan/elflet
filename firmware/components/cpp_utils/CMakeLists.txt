idf_component_register(
    SRCS ./CPPNVS.cpp ./FreeRTOS.cpp ./GeneralUtils.cpp ./I2C.cpp ./SPI.cpp ./Task.cpp
    INCLUDE_DIRS .
    REQUIRES nvs_flash
    PRIV_REQUIRES
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Dringbuf_type_t=RingbufferType_t")
