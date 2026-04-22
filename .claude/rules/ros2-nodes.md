---
paths:
  - "ros2_ws/**"
---

# ROS 2 Node Development Rules

- All nodes must subclass `rclcpp_lifecycle::LifecycleNode`; plain `rclcpp::Node` is banned
- Implement all five lifecycle callbacks: `on_configure`, `on_activate`, `on_deactivate`, `on_cleanup`, `on_shutdown`
- Subscription, timer, and service callbacks must not block — offload heavy work to a separate thread using a callback group
- QoS profiles must be defined as named constants at file scope; never inline `rclcpp::QoS(...)` at publisher or subscriber creation sites
- Log via `RCLCPP_INFO(get_logger(), ...)` only; `std::cout`, `printf`, and `fprintf` are banned
- Publishers and subscribers must be class members; no free-standing global publisher/subscriber variables
- Package names use `snake_case`; node names match the executable target name in `CMakeLists.txt`
- Every package must have a `colcon test` target: use `ament_add_gtest` or `launch_testing_ament_cmake`
- Do not spin a full DDS stack in unit tests — use `rclcpp::NodeOptions()` with `use_intra_process_comms` and a test executor
- All `rclcpp_lifecycle` lifecycle transitions must be tested; a configure → activate → deactivate → cleanup round trip is the minimum
