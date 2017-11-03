file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h")

add_library(espa_l8_ang STATIC ${SOURCES})
set_target_properties(espa_l8_ang PROPERTIES PUBLIC_HEADER "${HEADERS}")
target_include_directories(espa_l8_ang PUBLIC . ../../common)

install(TARGETS espa_l8_ang
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
