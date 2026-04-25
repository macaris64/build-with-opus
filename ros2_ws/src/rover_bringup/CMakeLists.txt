cmake_minimum_required(VERSION 3.20)
project(rover_bringup)

find_package(ament_cmake REQUIRED)

install(DIRECTORY launch config
    DESTINATION share/${PROJECT_NAME}
)

install(FILES launch/sim.launch.py
    DESTINATION share/${PROJECT_NAME}/launch
)

if(BUILD_TESTING)
    find_package(ament_cmake_pytest REQUIRED)
    ament_add_pytest_test(test_sim_launch
        test/test_sim_launch.py
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

ament_package()
