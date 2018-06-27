configure_file(espa_common.h.in espa_common.h)

file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h" ${CMAKE_CURRENT_BINARY_DIR}/*.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(espa_common STATIC ${SOURCES})
set_target_properties(espa_common PROPERTIES PUBLIC_HEADER "${HEADERS}")
target_include_directories(espa_common PUBLIC .)

install(TARGETS espa_common
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
