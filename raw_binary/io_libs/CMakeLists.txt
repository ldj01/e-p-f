include_directories(${XML_INCLUDES})
include_directories(${TIFF_INCLUDES})
include_directories(${GEOTIFF_INCLUDES})

file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h")

add_library(espa_raw_binary STATIC ${SOURCES})
set_target_properties(espa_raw_binary PROPERTIES PUBLIC_HEADER "${HEADERS}")
target_include_directories(espa_raw_binary PUBLIC . ../common)

install(TARGETS espa_raw_binary
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
