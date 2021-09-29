# Collect shader input files
file(GLOB GLSLS ${CMAKE_SOURCE_DIR}/shaders/*)

set(GENERATED_GLSL)

# Iterate through input files
foreach(GLSL ${GLSLS})
  get_filename_component(GLSL_NAME ${GLSL} NAME) 

  message(STATUS "Add target for preprocessing ${GLSL_NAME}")
  set(OUT_FILE ${CMAKE_BINARY_DIR}/shaders/${GLSL_NAME}.c)
  add_custom_command(OUTPUT ${OUT_FILE}
                     DEPENDS ${GLSL}
                     COMMAND shader-processor ${GLSL} ${OUT_FILE})
  list(APPEND GENERATED_GLSL ${OUT_FILE})
endforeach()

add_recursive( ${CMAKE_SOURCE_DIR}/src SOURCES )
add_recursive( ${CMAKE_SOURCE_DIR}/include HEADERS )

add_executable( ${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES} ${GENERATED_GLSL})

find_package(OpenGL REQUIRED)
target_link_libraries( ${PROJECT_NAME} glfw OpenGL::GL)

target_include_directories( ${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include )
target_include_directories( ${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/contrib/stb )