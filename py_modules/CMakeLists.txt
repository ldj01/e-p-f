file(GLOB PYTHON_MODULES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.py")

install(FILES ${PYTHON_MODULES} DESTINATION espa-product-formatter/python)
foreach (file ${PYTHON_MODULES})
    install(CODE "execute_process(COMMAND
                               ln -sf ../espa-product-formatter/python/${file}
                                      ${CMAKE_INSTALL_PREFIX}/python)")
endforeach ()
