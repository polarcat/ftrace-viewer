cmake_minimum_required(VERSION 3.0)

project(tools)
include_directories(include)

add_subdirectory(perfmon)

find_package(Vulkan)
if (Vulkan_FOUND)
	add_subdirectory(vkmon)
else()
	message("== skip 'vkmon' target, required dependency is not found")
endif()
