cmake_minimum_required(VERSION 3.2)

include_directories(C:\\Work\\stingray\\runtime\\sdk)

project(city_plugin)
add_library(city_plugin SHARED plugin.cpp)

set_target_properties(city_plugin PROPERTIES
	DEBUG_OUTPUT_NAME city_pluginw64_debug
	DEV_OUTPUT_NAME city_pluginw64_dev
	RELEASE_OUTPUT_NAME city_pluginw64_release
	RUNTIME_OUTPUT_DIRECTORY_DEBUG dlls
	RUNTIME_OUTPUT_DIRECTORY_DEV dlls
	RUNTIME_OUTPUT_DIRECTORY_RELEASE dlls
)

add_definitions(-DWINDOWSPC)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/move-dll.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/move-dll.cmake
	COPYONLY
)

add_custom_command(TARGET city_plugin PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/move-dll.cmake
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /PDBALTPATH:%_PDB%")
