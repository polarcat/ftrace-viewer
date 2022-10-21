cmake_minimum_required(VERSION 3.0)
project(ftrace-viewer C CXX)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_PROTOTYPES")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES")

# GLFW
find_package(glfw3 REQUIRED)
#set(GLFW_DIR ../../../glfw) # Set this to point to an up-to-date GLFW repo
#add_subdirectory(${GLFW_DIR} binary_dir EXCLUDE_FROM_ALL)
#include_directories(${GLFW_DIR}/include)

option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
option(GLFW_DOCUMENT_INTERNALS "Include internals in documentation" OFF)

# Dear ImGui
set(IMGUI_DIR imgui)
include_directories(${IMGUI_DIR} ${IMGUI_DIR}/backends implot include)

# Libraries
if(USE_VULKAN)
	find_package(Vulkan REQUIRED)
	set(LIBRARIES "pthread;glfw;m;Vulkan::Vulkan")
	set(gui_engine ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp)
	add_compile_definitions(USE_VULKAN=1)
else()
	set(LIBRARIES "pthread;glfw;m;dl")
	set(gui_engine ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
endif()

# Use vulkan headers from glfw:
include_directories(${GLFW_DIR}/deps)

file(GLOB sources src/*.cpp)

add_executable(${PROJECT_NAME}
	${sources}
	${gui_engine}
	${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
	implot/implot.cpp
	implot/implot_items.cpp
	implot/implot_demo.cpp
)

target_link_libraries(${PROJECT_NAME} ${LIBRARIES})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
