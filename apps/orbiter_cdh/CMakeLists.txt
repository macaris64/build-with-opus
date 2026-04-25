# apps/orbiter_cdh/CMakeLists.txt
#
# Build contract provided by sakura_add_cfs_app() (see _defs/cfs_app_template.cmake):
#   orbiter_cdh              OBJECT library (FSW source)
#   orbiter_cdh_test         CMocka unit-test executable
#   orbiter_cdh_unit_tests   CTest entry
#   orbiter_cdh_cppcheck     cppcheck development target
sakura_add_cfs_app(orbiter_cdh)
