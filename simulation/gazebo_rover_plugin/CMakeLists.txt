cmake_minimum_required(VERSION 3.20)
project(gazebo_rover_plugin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(gz-sim8 QUIET)
find_package(gz-transport13 QUIET)
find_package(gz-plugin2 QUIET)

if(NOT gz-sim8_FOUND)
    message(WARNING "gz-sim8 not found — rover drive plugin will not be built. Install Gazebo Harmonic to enable.")
    return()
endif()

add_library(rover_drive_plugin SHARED
    src/rover_drive_plugin.cpp
)
target_include_directories(rover_drive_plugin PRIVATE include)
target_compile_options(rover_drive_plugin PRIVATE -Wall -Wextra -Werror)
target_link_libraries(rover_drive_plugin
    gz-sim8::gz-sim8
    gz-transport13::gz-transport13
    gz-plugin2::gz-plugin2
)

install(TARGETS rover_drive_plugin DESTINATION lib)
