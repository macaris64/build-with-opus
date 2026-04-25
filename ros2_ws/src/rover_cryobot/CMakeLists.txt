cmake_minimum_required(VERSION 3.20)
project(rover_cryobot)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(std_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(rover_common REQUIRED)

# ── HdlcLite library (pure-C++ tether framing, no ROS deps) ──────────────────
add_library(hdlc_lite SHARED src/hdlc_lite.cpp)
target_include_directories(hdlc_lite PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_options(hdlc_lite PRIVATE -Wall -Wextra -Werror -std=c++17)

install(TARGETS hdlc_lite
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib/${PROJECT_NAME}
)

# ── DrillCtrlNode + TetherClient component library ───────────────────────────
add_library(rover_cryobot_component SHARED
    src/tether_client.cpp
    src/drill_ctrl_node.cpp
)
target_include_directories(rover_cryobot_component PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_options(rover_cryobot_component PRIVATE -Wall -Wextra -Werror -std=c++17)
target_link_libraries(rover_cryobot_component
    hdlc_lite
    ${rover_common_LIBRARIES}
)
ament_target_dependencies(rover_cryobot_component
    rclcpp
    rclcpp_lifecycle
    rclcpp_components
    std_msgs
    lifecycle_msgs
    rover_common
)
# rover_common mixes rosidl modern targets with old-style _INCLUDE_DIRS; the
# parent install include dir is not added by ament_target_dependencies when the
# modern target path is taken — add it explicitly so rover_common/*.hpp resolves.
target_include_directories(rover_cryobot_component PUBLIC ${rover_common_INCLUDE_DIRS})
# Wire rosidl-generated message typesupport (rover_common::msg::Hk).
rosidl_target_interfaces(rover_cryobot_component rover_common "rosidl_typesupport_cpp")

rclcpp_components_register_node(rover_cryobot_component
    PLUGIN "rover_cryobot::DrillCtrlNode"
    EXECUTABLE drill_ctrl_node
)

install(TARGETS rover_cryobot_component
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY include/ DESTINATION include)

# ── Tests ─────────────────────────────────────────────────────────────────────
if(BUILD_TESTING)
    find_package(ament_cmake_gtest REQUIRED)

    ament_add_gtest(test_rover_cryobot test/test_rover_cryobot.cpp)
    target_link_libraries(test_rover_cryobot
        rover_cryobot_component
        hdlc_lite
        ${rover_common_LIBRARIES}
    )
    ament_target_dependencies(test_rover_cryobot
        rclcpp
        rclcpp_lifecycle
        lifecycle_msgs
        rover_common
    )
    target_include_directories(test_rover_cryobot PUBLIC ${rover_common_INCLUDE_DIRS})
endif()

ament_export_include_directories(include)
ament_export_libraries(hdlc_lite rover_cryobot_component)
ament_export_dependencies(rover_common rclcpp rclcpp_lifecycle rclcpp_components std_msgs lifecycle_msgs)

ament_package()
