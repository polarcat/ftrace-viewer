cmake_minimum_required(VERSION 3.0)

project(perfmon DESCRIPTION "Basic monitors with ftraces" VERSION 0.0.1)
add_executable(${PROJECT_NAME} perfmon.c)
target_compile_features("${PROJECT_NAME}" PRIVATE c_std_99)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
