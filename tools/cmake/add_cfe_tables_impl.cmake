# tools/cmake/add_cfe_tables_impl.cmake — No-op table-tool implementation.
#
# cFE's arch_build.cmake includes this file and expects it to define
# add_cfe_tables().  None of the SAKURA-II apps use cFE binary tables, so
# the function body is intentionally empty.

function(add_cfe_tables APP_NAME)
    # No-op: this mission does not build cFE binary table files.
endfunction()
