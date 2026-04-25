cmake_minimum_required(VERSION 3.20)
project(rover_common)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(rclcpp_components REQUIRED)
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

# ── TmBridge + UdpGateway library ────────────────────────────────────────────
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

# ── LinkStateBridge component library ────────────────────────────────────────
add_library(link_state_bridge_component SHARED src/link_state_bridge.cpp)
target_include_directories(link_state_bridge_component PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_options(link_state_bridge_component PRIVATE -Wall -Wextra -Werror -std=c++17)
ament_target_dependencies(link_state_bridge_component
    rclcpp
    rclcpp_lifecycle
    rclcpp_components
    lifecycle_msgs
    rover_common
)
target_include_directories(link_state_bridge_component PUBLIC ${rover_common_INCLUDE_DIRS})
target_link_libraries(link_state_bridge_component tm_bridge ${rover_common_LIBRARIES})
rosidl_target_interfaces(link_state_bridge_component ${PROJECT_NAME} "rosidl_typesupport_cpp")

rclcpp_components_register_node(link_state_bridge_component
    PLUGIN "rover_common::LinkStateBridge"
    EXECUTABLE link_state_bridge
)

install(TARGETS link_state_bridge_component
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

    ament_add_gtest(test_link_state_bridge test/test_link_state_bridge.cpp)
    target_link_libraries(test_link_state_bridge link_state_bridge_component ${rover_common_LIBRARIES})
    ament_target_dependencies(test_link_state_bridge
        rclcpp
        rclcpp_lifecycle
        lifecycle_msgs
        rover_common
    )
    target_include_directories(test_link_state_bridge PUBLIC ${rover_common_INCLUDE_DIRS})
endif()

# ── Exports ───────────────────────────────────────────────────────────────────
ament_export_include_directories(include)
ament_export_libraries(tm_bridge link_state_bridge_component)
ament_export_dependencies(rclcpp rclcpp_lifecycle rclcpp_components)

ament_package()
