cmake_minimum_required(VERSION 3.20)
project(rover_common)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(lifecycle_msgs REQUIRED)

# ── Message types ─────────────────────────────────────────────────────────────
rosidl_generate_interfaces(${PROJECT_NAME}
    "msg/Hk.msg"
    "msg/Tc.msg"
    "msg/LinkState.msg"
    "msg/SimFault.msg"
    DEPENDENCIES builtin_interfaces
)

# ── TmBridge library ──────────────────────────────────────────────────────────
add_library(tm_bridge SHARED src/tm_bridge.cpp)
target_include_directories(tm_bridge PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_options(tm_bridge PRIVATE -Wall -Wextra -Werror -std=c++17)
ament_target_dependencies(tm_bridge rclcpp rclcpp_lifecycle)

install(TARGETS tm_bridge
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY include/ DESTINATION include)

# ── Tests ─────────────────────────────────────────────────────────────────────
if(BUILD_TESTING)
    find_package(ament_cmake_gtest REQUIRED)

    ament_add_gtest(test_tm_bridge test/test_tm_bridge.cpp)
    target_link_libraries(test_tm_bridge tm_bridge)
    ament_target_dependencies(test_tm_bridge
        rclcpp
        rclcpp_lifecycle
        lifecycle_msgs
    )
endif()

# ── Exports ───────────────────────────────────────────────────────────────────
ament_export_include_directories(include)
ament_export_libraries(tm_bridge)
ament_export_dependencies(rclcpp rclcpp_lifecycle)

ament_package()
