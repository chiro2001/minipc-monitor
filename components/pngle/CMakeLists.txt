
# file(GLOB app_sources "src/*.c")
set(app_sources "src/pngle.c")

idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "src/" REQUIRES esp_rom)

component_compile_options(-Wno-error=misleading-indentation -DPNGLE_CUSTOM_MINIZ=1)
