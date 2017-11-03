include_directories(${XML_INCLUDES})

add_definitions(-DIAS_NO_SENSOR_META_SUPPORT)

file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h")

add_library(espa_band_angles STATIC ${SOURCES})
set_target_properties(espa_band_angles PROPERTIES PUBLIC_HEADER "${HEADERS}")
target_include_directories(espa_band_angles
                           PUBLIC . l8_ias_lib landsat_ias_lib ../common
                                  ../io_libs)

install(TARGETS espa_band_angles
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)

add_subdirectory(l8_ias_lib)
add_subdirectory(landsat_ias_lib)
