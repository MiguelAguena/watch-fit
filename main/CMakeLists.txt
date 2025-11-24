file(GLOB_RECURSE srcs "node.cpp" "./apps/ble/main/src/*.c" "./apps/ble/main/src/*.cpp")

idf_component_register(
  SRCS "Application.cpp" "${srcs}"
  INCLUDE_DIRS "./apps/ble/main/include"
  REQUIRES bt nvs_flash freertos
)