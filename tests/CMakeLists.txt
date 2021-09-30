set(CODEC_LIST "bc1" "bc3" "bc4" "bc4_snorm" "bc5" "bc5_snorm" "bc6h" "etc1" "etc2_rgba_etc1" "etc2_rgb" "etc2_rgba" "eac_r11" "eac_rg11")

foreach(CODEC ${CODEC_LIST})
  add_test(NAME test_cli_${CODEC} 
          COMMAND ${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/data/bricks.jpg out_${CODEC}.ktx --codec=${CODEC})

  add_test(NAME test_nrm_cli_${CODEC} 
          COMMAND ${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/data/bricks_nrm.jpg out_nrm_${CODEC}.ktx --codec=${CODEC})
endforeach()