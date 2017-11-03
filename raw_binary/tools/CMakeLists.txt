include_directories(${XML_INCLUDES})
include_directories(${TIFFINC})
include_directories(${GEOTIFF_INC})
include_directories(${HDFINC})
include_directories(${HDFEOS_INC})
include_directories(${JBIGINC})
include_directories(${ZLIBINC})

add_executable(convert_lpgs_to_espa convert_lpgs_to_espa.c)
target_link_libraries(convert_lpgs_to_espa
                      espa_format_conversion espa_raw_binary espa_common
                      gctp3
                      ${XML_LIBS} ${TIFF_LIBS} ${GEOTIFF_LIBS} m)

add_executable(convert_espa_to_hdf convert_espa_to_hdf.c)
target_link_libraries(convert_espa_to_hdf
                      espa_format_conversion espa_raw_binary espa_common
                      ${XML_LIBS} ${HDF4_LIBS})

add_executable(convert_espa_to_gtif convert_espa_to_gtif.c)
target_link_libraries(convert_espa_to_gtif
                      espa_format_conversion espa_raw_binary espa_common
                      ${XML_LIBS})

add_executable(espa_band_subset espa_band_subset.c)
target_link_libraries(espa_band_subset
                      espa_format_conversion espa_raw_binary espa_common
                      ${XML_LIBS} m)

add_executable(espa_product_subset espa_product_subset.c)
target_link_libraries(espa_product_subset
                      espa_format_conversion espa_raw_binary espa_common
                      ${XML_LIBS})

add_executable(convert_modis_to_espa convert_modis_to_espa.c)
target_link_libraries(convert_modis_to_espa
                      espa_format_conversion espa_raw_binary espa_common gctp3
                      ${XML_LIBS} ${HDF4_LIBS} ${HDFEOS_LIBS} m)

add_executable(create_angle_bands create_angle_bands.c)
target_link_libraries(create_angle_bands
                      espa_band_angles espa_l8_ang espa_landsat_ang
                      espa_raw_binary espa_common
                      ${XML_LIBS} m)

add_executable(create_land_water_mask create_land_water_mask.c)
target_link_libraries(create_land_water_mask
                      espa_land_water_mask espa_l8_ang espa_raw_binary
                      espa_common gctp3
                      ${XML_LIBS} m)

add_executable(convert_espa_to_bip convert_espa_to_bip.c)
target_link_libraries(convert_espa_to_bip
                      espa_format_conversion espa_raw_binary espa_common
                      ${XML_LIBS})

add_executable(create_date_bands create_date_bands.c)
target_link_libraries(create_date_bands
                      espa_level1_libs espa_raw_binary espa_common
                      ${XML_LIBS})

add_executable(clip_band_misalignment clip_band_misalignment.c)
target_link_libraries(clip_band_misalignment
                      espa_level1_libs espa_raw_binary espa_common
                      ${XML_LIBS})

add_executable(convert_espa_to_netcdf convert_espa_to_netcdf.c)
target_link_libraries(convert_espa_to_netcdf
                      espa_format_conversion espa_raw_binary espa_common
                      ${XML_LIBS} ${HDF4_LIBS} ${NETCDF_LIBS})

add_executable(create_landsat_angle_bands create_landsat_angle_bands.c)
target_link_libraries(create_landsat_angle_bands
                      espa_band_angles espa_landsat_ang espa_raw_binary
                      espa_common
                      ${XML_LIBS} m)

add_executable(create_l8_angle_bands create_l8_angle_bands.c)
target_link_libraries(create_l8_angle_bands
                      espa_band_angles espa_l8_ang espa_raw_binary
                      espa_common
                      ${XML_LIBS} m)

set (ALL_TARGETS convert_lpgs_to_espa convert_espa_to_hdf convert_espa_to_gtif
                 espa_band_subset espa_product_subset convert_modis_to_espa
                 create_angle_bands create_land_water_mask convert_espa_to_bip
                 create_date_bands clip_band_misalignment
                 convert_espa_to_netcdf create_landsat_angle_bands
                 create_l8_angle_bands)

install(TARGETS ${ALL_TARGETS}
        DESTINATION espa-product-formatter/raw_binary/bin)


foreach (file ${ALL_TARGETS})
    install(CODE "execute_process(COMMAND
                        ln -sf ../espa-product-formatter/raw_binary/bin/${file}
                               ${CMAKE_INSTALL_PREFIX}/bin)")
endforeach ()
