
#set( CMAKE_TOOLCHAIN_FILE CMake/iOS.cmake )

cmake_minimum_required( VERSION 3.5 )
project( betsy )

include( cmake/Bootstrap.cmake )

add_subdirectory(contrib)
add_subdirectory(tools)
add_subdirectory(src)