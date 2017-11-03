include_directories(${XML_INCLUDES})
include_directories(${NETCDF_INCLUDES})
include_directories(${TIFF_INCLUDES})
include_directories(${GEOTIFF_INCLUDES})
#include_directories(${HDFEOS_INCLUDES})
#include_directories(${HDF4_INCLUDES})
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -idirafter ${HDFEOS_INCLUDES} \
                                     -idirafter ${HDF4_INCLUDES}")

file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h")

add_library(espa_format_conversion STATIC ${SOURCES})
set_target_properties(espa_format_conversion
                      PROPERTIES PUBLIC_HEADER "${HEADERS}")
target_include_directories(espa_format_conversion
                           PUBLIC . ../common ../io_libs)

install(TARGETS espa_format_conversion
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
