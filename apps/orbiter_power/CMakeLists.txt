# apps/orbiter_power/CMakeLists.txt
#
# Build contract provided by sakura_add_cfs_app() (see _defs/cfs_app_template.cmake):
#   orbiter_power              OBJECT library (FSW source)
#   orbiter_power_test         CMocka unit-test executable
#   orbiter_power_unit_tests   CTest entry
#   orbiter_power_cppcheck     cppcheck development target
sakura_add_cfs_app(orbiter_power)
