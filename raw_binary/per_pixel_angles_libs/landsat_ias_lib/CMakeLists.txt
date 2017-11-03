add_definitions(-DIAS_NO_SENSOR_META_SUPPORT -DIAS_NOVAS_NOT_AVAILABLE)

file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h")

add_library(espa_landsat_ang STATIC ${SOURCES})
set_target_properties(espa_landsat_ang PROPERTIES PUBLIC_HEADER "${HEADERS}")
target_include_directories(espa_landsat_ang PUBLIC . ../../common)

install(TARGETS espa_landsat_ang
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
