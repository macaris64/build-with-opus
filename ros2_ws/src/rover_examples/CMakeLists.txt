cmake_minimum_required(VERSION 3.20)
project(rover_examples)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rover_common REQUIRED)
find_package(rover_cryobot REQUIRED)

# ── rover_examples library ────────────────────────────────────────────────────
add_library(rover_examples_lib SHARED
    src/hdlc_example.cpp
    src/tm_bridge_example.cpp
    src/tether_pipeline_example.cpp
)
target_include_directories(rover_examples_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${rover_common_INCLUDE_DIRS}
    ${rover_cryobot_INCLUDE_DIRS}
)
target_compile_options(rover_examples_lib PRIVATE -Wall -Wextra -Werror -std=c++17)
ament_target_dependencies(rover_examples_lib
    rover_common
    rover_cryobot
)
target_link_libraries(rover_examples_lib
    ${rover_common_LIBRARIES}
    ${rover_cryobot_LIBRARIES}
)

# ── Demo executable ───────────────────────────────────────────────────────────
add_executable(rover_examples_demo src/examples_main.cpp)
target_compile_options(rover_examples_demo PRIVATE -Wall -Wextra -Werror -std=c++17)
target_link_libraries(rover_examples_demo rover_examples_lib)
ament_target_dependencies(rover_examples_demo rclcpp rover_common rover_cryobot)
target_include_directories(rover_examples_demo PRIVATE
    ${rover_common_INCLUDE_DIRS}
    ${rover_cryobot_INCLUDE_DIRS}
)

install(TARGETS rover_examples_lib rover_examples_demo
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY include/ DESTINATION include)

# ── Tests ─────────────────────────────────────────────────────────────────────
if(BUILD_TESTING)
    find_package(ament_cmake_gtest REQUIRED)

    ament_add_gtest(test_examples test/test_examples.cpp)
    target_link_libraries(test_examples
        rover_examples_lib
        ${rover_common_LIBRARIES}
        ${rover_cryobot_LIBRARIES}
    )
    ament_target_dependencies(test_examples rover_common rover_cryobot)
    target_include_directories(test_examples PUBLIC
        ${rover_common_INCLUDE_DIRS}
        ${rover_cryobot_INCLUDE_DIRS}
    )
endif()

ament_package()
