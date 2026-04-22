cmake_minimum_required(VERSION 3.20)
project(gazebo_rover_plugin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(gazebo QUIET)

if(NOT gazebo_FOUND)
    message(WARNING "Gazebo not found — rover drive plugin will not be built. Install Gazebo Harmonic to enable.")
    return()
endif()

include_directories(
    include
    ${GAZEBO_INCLUDE_DIRS}
)
link_directories(${GAZEBO_LIBRARY_DIRS})
list(APPEND CMAKE_CXX_FLAGS "${GAZEBO_CXX_FLAGS}")

add_library(rover_drive_plugin SHARED
    src/rover_drive_plugin.cpp
)
target_include_directories(rover_drive_plugin PRIVATE include)
target_compile_options(rover_drive_plugin PRIVATE -Wall -Wextra -Werror)
target_link_libraries(rover_drive_plugin ${GAZEBO_LIBRARIES})

install(TARGETS rover_drive_plugin DESTINATION lib)
