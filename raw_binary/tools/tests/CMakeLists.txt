# Tests
add_executable(test_read_gtif test_read_gtif.c)
target_link_libraries(test_read_gtif espa_format_conversion espa_raw_binary
                      espa_common
                      ${XML_LIBS} ${TIFF_LIBS} ${GEOTIFF_LIBS})
                  add_test(raw_binary_tools
                      ${CMAKE_CURRENT_SOURCE_DIR}/test_convert_lpgs.csh
                      ${CMAKE_CURRENT_BINARY_DIR}/.. )

set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "L*")

