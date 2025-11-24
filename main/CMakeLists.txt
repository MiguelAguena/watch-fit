file(GLOB_RECURSE srcs "src/*.c" "src/*.cpp")

idf_component_register(
  SRCS "${srcs}"
  INCLUDE_DIRS "./include"
  REQUIRES bt nvs_flash freertos ble
)