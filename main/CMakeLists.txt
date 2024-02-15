idf_component_register(
SRCS
        "minipc-monitor.c"
        "epd29.c"
        "rpc_api.c"
        "rpc_ble.c"
INCLUDE_DIRS "."
REQUIRES
        esp_jpeg
        esp_rom
        nvs_flash
        spiffs
        vfs
        pngle
        esp_timer
        driver
        protocomm
)

spiffs_create_partition_image(storage ../files
#        FLASH_IN_PROJECT
)

# make compliler happy
# idf_build_set_property(COMPILE_OPTIONS "-Wformat=0" APPEND)
component_compile_options(-Wno-error=format= -Wno-format)
