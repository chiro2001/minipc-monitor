idf_component_register(
SRCS
        "minipc-monitor.c"
        "epd29.c"
        "rpc_api.c"
        "rpc_ble.c"
        "fonts.c"
        "gui.c"
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
EMBED_TXTFILES
        ${project_dir}/resources/unicode2gbk.bin
        ${project_dir}/resources/fonts/ASC12
        ${project_dir}/resources/fonts/ASC16
        ${project_dir}/resources/fonts/ASC48
        ${project_dir}/resources/fonts/HZK12
#        ${project_dir}/resources/fonts/HZK16
#        ${project_dir}/resources/fonts/HZK24S
#        ${project_dir}/resources/fonts/HZK32
)

spiffs_create_partition_image(storage ../files
#        FLASH_IN_PROJECT
)

# make compliler happy
# idf_build_set_property(COMPILE_OPTIONS "-Wformat=0" APPEND)
component_compile_options(-Wno-error=format= -Wno-format)
