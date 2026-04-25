cmake_minimum_required(VERSION 3.20)
project(gazebo_world_plugin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(gazebo QUIET)

if(NOT gazebo_FOUND)
    message(WARNING "Gazebo not found — world environment plugin will not be built.")
    return()
endif()

include_directories(
    include
    ${GAZEBO_INCLUDE_DIRS}
)
link_directories(${GAZEBO_LIBRARY_DIRS})
list(APPEND CMAKE_CXX_FLAGS "${GAZEBO_CXX_FLAGS}")

add_library(world_environment_plugin SHARED
    src/world_environment_plugin.cpp
)
target_include_directories(world_environment_plugin PRIVATE include)
target_compile_options(world_environment_plugin PRIVATE -Wall -Wextra -Werror)
target_link_libraries(world_environment_plugin ${GAZEBO_LIBRARIES})

install(TARGETS world_environment_plugin DESTINATION lib)
