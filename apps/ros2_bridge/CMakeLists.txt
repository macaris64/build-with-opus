# apps/ros2_bridge/CMakeLists.txt
#
# Build contract provided by sakura_add_cfs_app() (see _defs/cfs_app_template.cmake):
#   ros2_bridge              OBJECT library (FSW source)
#   ros2_bridge_test         CMocka unit-test executable
#   ros2_bridge_unit_tests   CTest entry
#   ros2_bridge_cppcheck     cppcheck development target
sakura_add_cfs_app(ros2_bridge)
