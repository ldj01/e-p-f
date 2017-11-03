file(GLOB SOURCES "*.c")
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/br_gctp.c")

add_library(gctp3 STATIC ${SOURCES})
set_target_properties(gctp3 PROPERTIES PUBLIC_HEADER gctp.h)

install(TARGETS gctp3
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
