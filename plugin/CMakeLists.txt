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

execute_process(
	COMMAND ${CMAKE_COMMAND} -E rename city_pluginw64_debug.dll city_pluginw64_debug.dll.old
	WORKING_DIRECTORY dlls
	ERROR_QUIET
)

execute_process(
	COMMAND ${CMAKE_COMMAND} -E rename city_pluginw64_debug.pdb city_pluginw64_debug.pdb.old
	WORKING_DIRECTORY dlls
	ERROR_QUIET
)

execute_process(
	COMMAND ${CMAKE_COMMAND} -E remove city_pluginw64_debug.pdb
	WORKING_DIRECTORY dlls
	ERROR_QUIET
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /PDBALTPATH:%_PDB%")
