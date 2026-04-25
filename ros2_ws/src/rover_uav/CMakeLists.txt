cmake_minimum_required(VERSION 3.20)
project(rover_uav)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(rover_common REQUIRED)

# ── FlightCtrlNode component library ─────────────────────────────────────────
add_library(flight_ctrl_node_component SHARED src/flight_ctrl_node.cpp)
target_include_directories(flight_ctrl_node_component PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_options(flight_ctrl_node_component PRIVATE -Wall -Wextra -Werror -std=c++17)
ament_target_dependencies(flight_ctrl_node_component
    rclcpp
    rclcpp_lifecycle
    rclcpp_components
    sensor_msgs
    nav_msgs
    lifecycle_msgs
    rover_common
)
# rover_common mixes rosidl modern targets with old-style _INCLUDE_DIRS; the
# parent install include dir is not added by ament_target_dependencies when the
# modern target path is taken — add it explicitly so rover_common/*.hpp resolves.
target_include_directories(flight_ctrl_node_component PUBLIC ${rover_common_INCLUDE_DIRS})
target_link_libraries(flight_ctrl_node_component ${rover_common_LIBRARIES})

rclcpp_components_register_node(flight_ctrl_node_component
    PLUGIN "rover_uav::FlightCtrlNode"
    EXECUTABLE flight_ctrl_node
)

install(TARGETS flight_ctrl_node_component
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY include/ DESTINATION include)

# ── Tests ─────────────────────────────────────────────────────────────────────
if(BUILD_TESTING)
    find_package(ament_cmake_gtest REQUIRED)

    ament_add_gtest(test_rover_uav test/test_rover_uav.cpp)
    target_link_libraries(test_rover_uav flight_ctrl_node_component ${rover_common_LIBRARIES})
    ament_target_dependencies(test_rover_uav
        rclcpp
        rclcpp_lifecycle
        lifecycle_msgs
        rover_common
    )
    target_include_directories(test_rover_uav PUBLIC ${rover_common_INCLUDE_DIRS})
endif()

ament_package()
