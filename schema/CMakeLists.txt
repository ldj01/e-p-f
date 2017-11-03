file(GLOB SCHEMAS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.xsd")

install(FILES ${SCHEMAS} DESTINATION espa-product-formatter/schema)
foreach (file ${SCHEMAS})
    install(CODE "execute_process(COMMAND
                               ln -sf ../espa-product-formatter/schema/${file}
                                      ${CMAKE_INSTALL_PREFIX}/schema)")
endforeach ()
