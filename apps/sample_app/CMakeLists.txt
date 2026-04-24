# apps/sample_app/CMakeLists.txt
#
# Build contract provided by sakura_add_cfs_app() (see _defs/cfs_app_template.cmake):
#   sample_app              OBJECT library (FSW source)
#   sample_app_test         CMocka unit-test executable
#   sample_app_unit_tests   CTest entry
#   sample_app_cppcheck     cppcheck development target
sakura_add_cfs_app(sample_app)
