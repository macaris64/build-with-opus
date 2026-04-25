cmake_minimum_required(VERSION 3.20)
project(gazebo_cryobot_plugin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(gazebo QUIET)

if(NOT gazebo_FOUND)
    message(WARNING "Gazebo not found — cryobot physics plugin will not be built.")
    return()
endif()

include_directories(
    include
    ${GAZEBO_INCLUDE_DIRS}
)
link_directories(${GAZEBO_LIBRARY_DIRS})
list(APPEND CMAKE_CXX_FLAGS "${GAZEBO_CXX_FLAGS}")

add_library(cryobot_physics_plugin SHARED
    src/cryobot_physics_plugin.cpp
)
target_include_directories(cryobot_physics_plugin PRIVATE include)
target_compile_options(cryobot_physics_plugin PRIVATE -Wall -Wextra -Werror)
target_link_libraries(cryobot_physics_plugin ${GAZEBO_LIBRARIES})

install(TARGETS cryobot_physics_plugin DESTINATION lib)
