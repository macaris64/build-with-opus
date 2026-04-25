cmake_minimum_required(VERSION 3.20)
project(fleet_monitor)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(rover_common REQUIRED)

# ── FleetMonitorNode component library ───────────────────────────────────────
add_library(fleet_monitor_component SHARED src/fleet_monitor_node.cpp)
target_include_directories(fleet_monitor_component PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_options(fleet_monitor_component PRIVATE -Wall -Wextra -Werror -std=c++17)
ament_target_dependencies(fleet_monitor_component
    rclcpp
    rclcpp_lifecycle
    rclcpp_components
    nav_msgs
    lifecycle_msgs
    rover_common
)
# rover_common mixes rosidl modern targets with old-style _INCLUDE_DIRS; add
# explicitly so rover_common/*.hpp and rover_common/msg/*.hpp both resolve.
target_include_directories(fleet_monitor_component PUBLIC ${rover_common_INCLUDE_DIRS})
target_link_libraries(fleet_monitor_component ${rover_common_LIBRARIES})
rosidl_target_interfaces(fleet_monitor_component rover_common "rosidl_typesupport_cpp")

rclcpp_components_register_node(fleet_monitor_component
    PLUGIN "fleet_monitor::FleetMonitorNode"
    EXECUTABLE fleet_monitor_node
)

install(TARGETS fleet_monitor_component
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY include/ DESTINATION include)

# ── Tests ─────────────────────────────────────────────────────────────────────
if(BUILD_TESTING)
    find_package(ament_cmake_gtest REQUIRED)

    ament_add_gtest(test_fleet_monitor test/test_fleet_monitor.cpp)
    target_link_libraries(test_fleet_monitor fleet_monitor_component ${rover_common_LIBRARIES})
    ament_target_dependencies(test_fleet_monitor
        rclcpp
        rclcpp_lifecycle
        lifecycle_msgs
        nav_msgs
        rover_common
    )
    target_include_directories(test_fleet_monitor PUBLIC
        ${rover_common_INCLUDE_DIRS}
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
    rosidl_target_interfaces(test_fleet_monitor rover_common "rosidl_typesupport_cpp")
endif()

ament_package()
