idf_component_register(
  SRCS "main.cpp"
  INCLUDE_DIRS "."
  REQUIRES freertos
)
# Pick your C++ standard (IDF 5.x uses GCC 13+ typically)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
# Typical embedded flags (tweak as you like)
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-exceptions -fno-rtti -fno-unwind-tables)
