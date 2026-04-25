# apps/orbiter_comm/CMakeLists.txt
#
# Build contract provided by sakura_add_cfs_app() (see _defs/cfs_app_template.cmake):
#   orbiter_comm              OBJECT library (FSW source)
#   orbiter_comm_test         CMocka unit-test executable
#   orbiter_comm_unit_tests   CTest entry
#   orbiter_comm_cppcheck     cppcheck development target
sakura_add_cfs_app(orbiter_comm)
