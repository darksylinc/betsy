add_executable(shader-processor
${CMAKE_CURRENT_SOURCE_DIR}/shader-processor/main.cpp
${CMAKE_CURRENT_SOURCE_DIR}/shader-processor/IncludeParser.cpp
${CMAKE_CURRENT_SOURCE_DIR}/shader-processor/IncludeParser.h)

set_property(TARGET shader-processor PROPERTY CXX_STANDARD 17)