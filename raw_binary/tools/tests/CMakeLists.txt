# Tests
add_executable(test_read_gtif test_read_gtif.c)
target_link_libraries(test_read_gtif espa_format_conversion espa_raw_binary
                      espa_common
                      ${XML_LIBS} ${TIFF_LIBS} ${GEOTIFF_LIBS})
add_test(raw_binary_tools test_read_gtif)
    
if (NOT IN_SOURCE_BUILD)
    add_custom_target(raw_binary_tools_test_links ALL
                      COMMAND ln -sf ${CMAKE_CURRENT_SOURCE_DIR}/*.csh
                                     ${CMAKE_CURRENT_BINARY_DIR})
    #add_dependencies(build-tests raw_binary_tools_test_links)
endif ()

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "L*")

