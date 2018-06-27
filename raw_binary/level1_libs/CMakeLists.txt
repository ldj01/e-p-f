include_directories(${XML_INCLUDES})

file(GLOB SOURCES "*.c")
file(GLOB HEADERS "*.h")

add_library(espa_level1_libs STATIC ${SOURCES})
set_target_properties(espa_level1_libs PROPERTIES PUBLIC_HEADER "${HEADERS}")
target_include_directories(espa_level1_libs PUBLIC . ../common ../io_libs
                           ${CMAKE_CURRENT_BINARY_DIR}/../common)

install(TARGETS espa_level1_libs
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
