# apps/orbiter_payload/CMakeLists.txt
#
# Build contract provided by sakura_add_cfs_app() (see _defs/cfs_app_template.cmake):
#   orbiter_payload              OBJECT library (FSW source)
#   orbiter_payload_test         CMocka unit-test executable
#   orbiter_payload_unit_tests   CTest entry
#   orbiter_payload_cppcheck     cppcheck development target
sakura_add_cfs_app(orbiter_payload)
