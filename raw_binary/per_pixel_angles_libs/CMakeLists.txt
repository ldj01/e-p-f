include_directories(${IAS_INCLUDES})
include_directories(${XML_INCLUDES})

add_definitions(-DIAS_NO_SENSOR_META_SUPPORT)

file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h")

add_library(espa_band_angles STATIC ${SOURCES})
target_include_directories(espa_band_angles
                           PUBLIC . ../common ../io_libs
                                  ${CMAKE_CURRENT_BINARY_DIR}/../common)
