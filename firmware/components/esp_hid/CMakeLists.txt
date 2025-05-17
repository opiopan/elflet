idf_component_register(
    SRCS
        src/ble_hidd.c src/ble_hidh.c src/bt_hidh.c src/esp_hid_common.c
        src/esp_hid_gap.c src/esp_hidd.c src/esp_hidh.c
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS private
    REQUIRES bt
    PRIV_REQUIRES
)

