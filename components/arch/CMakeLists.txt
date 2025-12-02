#file(GLOB_RECURSE srcs "src/*.c" "src/*.cpp")

idf_component_register(
  SRCS ""
  INCLUDE_DIRS "./inc"
  REQUIRES freertos
)