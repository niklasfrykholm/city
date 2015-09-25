cmake_minimum_required(VERSION 3.2)

include_directories(C:\\Work\\code\\runtime\\plugins)
set(ENGINE_PLUGINS_INSTALL_DIR "C:\\Work\\code\\build\\binaries\\engine\\win64\\debug\\plugins")

project(city_plugin)
add_library(city_plugin SHARED plugin.cpp)

set_target_properties(city_plugin PROPERTIES
	DEBUG_OUTPUT_NAME city_pluginw64_debug
	DEV_OUTPUT_NAME city_pluginw64_dev
	RELEASE_OUTPUT_NAME city_pluginw64_release
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ENGINE_PLUGINS_INSTALL_DIR}"
	RUNTIME_OUTPUT_DIRECTORY_DEV "${ENGINE_PLUGINS_INSTALL_DIR}"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ENGINE_PLUGINS_INSTALL_DIR}"
)