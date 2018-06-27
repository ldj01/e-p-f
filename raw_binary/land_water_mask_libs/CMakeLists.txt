include_directories(${IAS_INCLUDES})
include_directories(${GCTP3_INCLUDES})
include_directories(${GCTP_INCLUDES})
include_directories(${XML_INCLUDES})
include_directories(${HDF4_INCLUDES})
if (USE_HDFEOS)
    include_directories(${HDFEOS_INCLUDES})
else ()
    include_directories(${GCTP2_INCLUDES})
endif ()

file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h")

add_library(espa_land_water_mask STATIC ${SOURCES})
target_include_directories(espa_land_water_mask
                           PUBLIC . ../common ../io_libs
                                  ../format_conversion_libs
                                  ${CMAKE_CURRENT_BINARY_DIR}/../common)
