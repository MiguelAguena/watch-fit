file(GLOB_RECURSE srcs "main.cpp" "src/*.c" "src/*.cpp")

idf_component_register(
  SRCS "${srcs}"
  INCLUDE_DIRS "./include"
  PRIV_REQUIRES bt nvs_flash
  REQUIRES freertos
)