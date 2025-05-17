set(C_SRCS ./BleHidService.c foundations/htdigestfs.c foundations/irrc.c)
set(CPP_SRCS
    ./BleHidJson.cpp ./ButtonService.cpp ./Config.cpp ./DeepSleep.cpp
    ./FirmwareDownloader.cpp ./irserver.cpp ./IRService.cpp
    ./JsonTags.cpp ./LEDService.cpp ./main.cpp ./mdnsService.cpp
    ./PubSubService.cpp ./RESTblehid.cpp ./RESTconfig.cpp ./RESTir.cpp
    ./RESTsensor.cpp ./RESTshadow.cpp ./RESTstatus.cpp
    ./SensorService.cpp ./ShadowDevice.cpp ./Stat.cpp ./TimeService.cpp
    ./version.cpp ./WatchDog.cpp ./WebContents.cpp ./WebService.cpp
    ./WifiApService.cpp ./WifiService.cpp foundations/bme280.cpp
    foundations/ContentTypeDict.cpp foundations/NameResolver.cpp
    foundations/ota.cpp foundations/otaRest.cpp foundations/reboot.cpp
    foundations/session.cpp foundations/TimeObj.cpp
    foundations/webserver.cpp foundations/WebserverHandler.cpp
)
set(REQUIRES elflet-version esp_http_client app_update bt ulp mdns spiffs mqtt)
set(PRIV_REQUIRES cpp_utils json11 mongoose esp_hid esp-dns-hijack-srv)
set(WEB_FILES
    html/index.html html/index.js html/jquery-3.3.1.min.js
    html/style.css html/wizard.html html/wizard.css
    html/wizard.js
)
set(COMPRESSED_FILES)
foreach(file ${WEB_FILES})
    set(compressed ${COMPONENT_DIR}/${file}z)
    list(APPEND COMPRESSED_FILES ${compressed})
endforeach()

idf_component_register(
    SRCS ${C_SRCS} ${CPP_SRCS}
    EMBED_FILES ${COMPRESSED_FILES}
    INCLUDE_DIRS . foundations
    REQUIRES ${REQUIRES}
    PRIV_REQUIRES ${PRIV_REQUIRES}
)

# Set compiler options for each source file
set(C_FLAGS "-DMG_ENABLE_HTTP_STREAMING_MULTIPART")
set(CXX_FLAGS "-DMG_ENABLE_HTTP_STREAMING_MULTIPART -Wunused-function")
foreach(file ${C_SRCS})
    set_source_files_properties(${file} PROPERTIES COMPILE_FLAGS ${C_FLAGS})
endforeach()
foreach(file ${CPP_SRCS})
    set_source_files_properties(${file} PROPERTIES COMPILE_FLAGS ${CXX_FLAGS})
endforeach()

# Rules to build the compressed web contents for embedding in the binary
foreach(file ${WEB_FILES})
    set(compressed ${COMPONENT_DIR}/${file}z)
    add_custom_command(
        OUTPUT ${compressed}
        COMMAND ${CMAKE_COMMAND} -E echo "ZIP ${compressed}"
        COMMAND gzip -c ${COMPONENT_DIR}/${file} > ${compressed}
        DEPENDS ${COMPONENT_DIR}/${file}
        COMMENT "Compressing ${file} -> ${compressed}"
        VERBATIM
    )
endforeach()
add_custom_target(compress_web_files ALL DEPENDS ${COMPRESSED_FILES})
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${COMPRESSED_FILES})
