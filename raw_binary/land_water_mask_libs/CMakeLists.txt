include_directories(${XML_INCLUDES})
include_directories(${HDF4_INCLUDES})
include_directories(${HDFEOS_INCLUDES})

file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h")

add_library(espa_land_water_mask STATIC ${SOURCES})
set_target_properties(espa_land_water_mask
                      PROPERTIES PUBLIC_HEADER "${HEADERS}")
target_include_directories(espa_land_water_mask
                           PUBLIC . ../common ../io_libs
                                  ../format_conversion_libs
                                  ../per_pixel_angles_libs/l8_ias_lib
                                  GCTP3)

install(TARGETS espa_land_water_mask
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)

add_subdirectory(GCTP3)
