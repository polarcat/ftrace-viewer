cmake_minimum_required(VERSION 3.0)

project(vkmon DESCRIPTION "Vulkan instance layer with ftraces" VERSION 0.0.1)
add_library("${PROJECT_NAME}" SHARED vkmon.c)
target_compile_features("${PROJECT_NAME}" PRIVATE c_std_99)
configure_file(${PROJECT_NAME}.json.in ${PROJECT_NAME}.json)

include(GNUInstallDirs)

install(
	FILES
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.json"
	DESTINATION
	"${CMAKE_INSTALL_DATADIR}/vulkan/implicit_layer.d"
)

install(TARGETS "${PROJECT_NAME}")
