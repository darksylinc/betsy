
#set( CMAKE_TOOLCHAIN_FILE CMake/iOS.cmake )

cmake_minimum_required( VERSION 3.5 )
project( betsy )

set( EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}" )

include( CMake/Bootstrap.cmake )

if( WIN32 )
	set( SDL2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/SDL/Windows/include" )
	set( ARCH_SUFFIX "x64" )
	if( ${CMAKE_SIZEOF_VOID_P} EQUAL 4 )
		set( ARCH_SUFFIX "x86" )
	endif()
	set( SDL2_LIBRARY
		"${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/SDL/Windows/lib/${ARCH_SUFFIX}/SDL2.lib" )
	set( SDL2_BINARY
		"${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/SDL/Windows/lib/${ARCH_SUFFIX}/SDL2.dll" )
	file( COPY ${SDL2_BINARY} DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/bin/Debug/" )
	file( COPY ${SDL2_BINARY} DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/bin/Release/" )
	file( COPY ${SDL2_BINARY} DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/bin/RelWithDebInfo/" )
	file( COPY ${SDL2_BINARY} DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/bin/MinSizeRel/" )
else()
	include( CMake/Dependencies/FindSDL2.cmake )
	find_package( SDL2 REQUIRED )
endif()

add_subdirectory( Dependencies )

include_directories( "./include" )
include_directories( ${SDL2_INCLUDE_DIR} )
include_directories( "./Dependencies/FreeImage/Source" )

add_recursive( ./src SOURCES )
add_recursive( ./include HEADERS )

if( APPLE )
	file( GLOB_RECURSE RESOURCES ./src/*.storyboard )
	set( RESOURCES ${RESOURCES} ./Data/Resources.cfg ./bin/Data )
endif()

add_library( FreeImage_betsy ALIAS FreeImage )

add_executable( ${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES} )

target_link_libraries( ${PROJECT_NAME} ${SDL2_LIBRARY} FreeImage_betsy )

if( WIN32 )
	target_link_libraries( ${PROJECT_NAME} opengl32.lib )
elseif( UNIX )
	target_link_libraries( ${PROJECT_NAME} dl )
endif()

if( APPLE )
	set_target_properties( ${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_ENABLE_BITCODE "NO" )
	set_target_properties( ${PROJECT_NAME} PROPERTIES RESOURCE "${RESOURCES}" )
	set_target_properties( ${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist )
	#set_target_properties( ${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE SampleBrowser_OSX.icns)
	set( CMAKE_EXE_LINKER_FLAGS "-framework Foundation -framework CoreGraphics -framework QuartzCore -framework UIKit -framework Metal -framework MetalKit -framework ModelIO" )
endif()

