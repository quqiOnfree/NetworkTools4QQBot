cmake_minimum_required(VERSION 3.24)

project(network_utils_externel_cpp)

set(CMAKE_CXX_STANDARD 23)

find_package(Python3 3.12 COMPONENTS Interpreter Development REQUIRED)
message(STATUS "Found Python3: ${Python3_EXECUTABLE} (version: ${Python3_VERSION})")

add_subdirectory(pybind11)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE asio/include)

pybind11_add_module(network_utils_externel_cpp src/main.cpp)
target_link_libraries(network_utils_externel_cpp PRIVATE asio)
add_custom_command(
    TARGET network_utils_externel_cpp
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:network_utils_externel_cpp> ${CMAKE_CURRENT_LIST_DIR}/../src/plugins/networktools4qqbot/NetworkTools
    COMMENT "Copying the executable to the bin directory"
    )

add_executable(test src/test.cpp)
target_link_libraries(test PRIVATE asio)
