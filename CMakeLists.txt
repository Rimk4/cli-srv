cmake_minimum_required(VERSION 3.25)

project(cli_srv LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(app1_cli)
add_subdirectory(app2_server)
