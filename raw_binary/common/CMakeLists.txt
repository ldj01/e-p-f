file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h")

add_library(espa_common STATIC ${SOURCES})
set_target_properties(espa_common PROPERTIES PUBLIC_HEADER "${HEADERS}")
target_include_directories(espa_common PUBLIC .)

install(TARGETS espa_common
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
