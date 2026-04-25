# apps/sim_adapter/CMakeLists.txt
#
# Build contract provided by sakura_add_cfs_app() (see _defs/cfs_app_template.cmake):
#   sim_adapter              OBJECT library (FSW source)
#   sim_adapter_test         CMocka unit-test executable
#   sim_adapter_unit_tests   CTest entry
#   sim_adapter_cppcheck     cppcheck development target
#
# CFS_FLIGHT_BUILD guard: the entire sim_adapter implementation is excluded from
# flight builds by #ifndef CFS_FLIGHT_BUILD in sim_adapter.c / sim_adapter.h.
sakura_add_cfs_app(sim_adapter)
