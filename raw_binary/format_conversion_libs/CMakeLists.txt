include_directories(${XML_INCLUDES})
include_directories(${NETCDF_INCLUDES})
if (USE_HDFEOS)
    include_directories(${HDFEOS_INCLUDES})
else ()
    include_directories(${GCTP2_INCLUDES})
endif ()
include_directories(${HDF4_INCLUDES})
include_directories(${TIFF_INCLUDES})
include_directories(${GEOTIFF_INCLUDES})
include_directories(${IAS_INCLUDES})
include_directories(${GCTP3_INCLUDES})

file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h")
if (NOT USE_HDFEOS)
    list(FILTER SOURCES EXCLUDE REGEX "convert_modis_to_espa")
endif ()

add_library(espa_format_conversion STATIC ${SOURCES})
set_target_properties(espa_format_conversion
                      PROPERTIES PUBLIC_HEADER "${HEADERS}")
target_include_directories(espa_format_conversion
                           PUBLIC . ../common ../io_libs
                           ${CMAKE_CURRENT_BINARY_DIR}/../common)

install(TARGETS espa_format_conversion
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
