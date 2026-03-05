idf_component_register(
    SRCS "ClockTools.cpp"
    INCLUDE_DIRS "."
    REQUIRES HttpTools lwip freertos
)

## Suppress false-positive format-truncation: snprintf date/time buffers are
## always large enough for real-world years; GCC assumes worst-case int range.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-format-truncation)
