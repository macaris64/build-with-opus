cmake_minimum_required(VERSION 3.20)
project(rover_teleop)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)

add_library(teleop_node_component SHARED
    src/teleop_node.cpp
)
target_include_directories(teleop_node_component PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_options(teleop_node_component PRIVATE -Wall -Wextra -Werror)
ament_target_dependencies(teleop_node_component
    rclcpp
    rclcpp_lifecycle
    rclcpp_components
    geometry_msgs
    lifecycle_msgs
)
rclcpp_components_register_node(teleop_node_component
    PLUGIN "rover_teleop::TeleopNode"
    EXECUTABLE teleop_node
)

install(TARGETS teleop_node_component
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY include/ DESTINATION include)

# ── Tests ──────────────────────────────────────────────────────────────────────
if(BUILD_TESTING)
    find_package(ament_cmake_gtest REQUIRED)

    ament_add_gtest(test_teleop test/test_teleop.cpp)
    target_link_libraries(test_teleop teleop_node_component)
    ament_target_dependencies(test_teleop
        rclcpp
        rclcpp_lifecycle
        lifecycle_msgs
    )
endif()

ament_package()
