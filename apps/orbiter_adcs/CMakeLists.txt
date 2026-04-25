# apps/orbiter_adcs/CMakeLists.txt
#
# Build contract provided by sakura_add_cfs_app() (see _defs/cfs_app_template.cmake):
#   orbiter_adcs              OBJECT library (FSW source)
#   orbiter_adcs_test         CMocka unit-test executable
#   orbiter_adcs_unit_tests   CTest entry
#   orbiter_adcs_cppcheck     cppcheck development target
sakura_add_cfs_app(orbiter_adcs)
