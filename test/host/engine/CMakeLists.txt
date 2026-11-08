# The host engine, doom_host (design.md §15, M2): upstream's engine, as the
# firmware compiles it and patched the same way, with a headless platform
# (dg_host.c) in place of picocalc/'s, upstream's own w_file_stdc.c and
# m_misc.c in place of its FatFs glue, and a null sound module. No OPL,
# MIDI or sound mixer: the null module never reaches them.

include(${PROJECT_SOURCE_DIR}/cmake/upstream-sources.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/engine-definitions.cmake)

set(engine_dir ${PICOCALC_DOOM_UPSTREAM}/src/doomgeneric/doomgeneric)

# The firmware's engine files, less its sound and OPL code and the FatFs
# glue, plus upstream's stdio versions of the two files its filters drop.
set(host_upstream_sources ${FD_ENGINE_SOURCES})
foreach(f ${FD_ENGINE_ADDED_SOURCES})
    if (f MATCHES "^src/doomgeneric/doomgeneric/")
        list(APPEND host_upstream_sources ${f})
    endif()
endforeach()
list(REMOVE_DUPLICATES host_upstream_sources)
list(TRANSFORM host_upstream_sources PREPEND ${PICOCALC_DOOM_UPSTREAM}/)
list(APPEND host_upstream_sources
    ${engine_dir}/w_file_stdc.c
    ${engine_dir}/m_misc.c
)

# Upstream is built as it comes: its warnings are not ours.
set_source_files_properties(${host_upstream_sources} PROPERTIES COMPILE_OPTIONS "-w")

# Every read of a WAD goes through dg_host.c's counter: what the device
# would read from the card.
set_source_files_properties(${engine_dir}/w_file_stdc.c PROPERTIES
    COMPILE_DEFINITIONS "fread=host_fread")

# Z_Malloc is dg_host.c's, which can pad each block (-hostzonepad) and
# calls upstream's, renamed here, for the rest.
set_source_files_properties(${engine_dir}/z_zone.c PROPERTIES
    COMPILE_DEFINITIONS "Z_Malloc=host_Z_Malloc_upstream")

set(host_own_sources
    dg_host.c
    ${PROJECT_SOURCE_DIR}/picocalc/frame_hash.c
)
set_source_files_properties(${host_own_sources} PROPERTIES
    COMPILE_OPTIONS "${PICOCALC_DOOM_WARNINGS}")

add_executable(doom_host ${host_own_sources} ${host_upstream_sources})

target_include_directories(doom_host PRIVATE
    ${PROJECT_SOURCE_DIR}/picocalc
    ${engine_dir}
    ${PICOCALC_DOOM_UPSTREAM}/src
    ${PICOCALC_DOOM_UPSTREAM}/drivers    # psram_allocator.h, for w_wad.c
)

target_compile_definitions(doom_host PRIVATE
    ${PICOCALC_DOOM_ENGINE_DEFINITIONS}
    FRANK_DOOM_QUIET=0
)


# The committed frame-hash list still matches; skips without doom.wad.
add_test(NAME test_engine_demo1
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_engine_demo1.sh
            $<TARGET_FILE:doom_host> ${PROJECT_SOURCE_DIR})
set_tests_properties(test_engine_demo1 PROPERTIES SKIP_RETURN_CODE 77)

# Every screen size plays demo1 to its end with the zone intact (M9).
add_test(NAME test_engine_screens
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_engine_screens.sh
            $<TARGET_FILE:doom_host> ${PROJECT_SOURCE_DIR})
set_tests_properties(test_engine_screens PROPERTIES SKIP_RETURN_CODE 77)

# Doom uses POSIX and BSD calls (strdup, strcasecmp) and dg_host.c uses
# clock_gettime: GNU C, so glibc declares them, as Chocolate Doom builds.
set_target_properties(doom_host PROPERTIES C_EXTENSIONS ON)

# Doom's configuration files through the patched m_config.c, with the
# port's stdio for them (picocalc/config_stdio.h, §13.3): saved, loaded,
# a file written by hand, and the four bindings the file cannot hold.
add_executable(test_config_file test_config_file.c
    ${engine_dir}/m_config.c
    ${engine_dir}/m_controls.c
    ${engine_dir}/m_argv.c
    ${engine_dir}/m_misc.c
)
set_source_files_properties(test_config_file.c PROPERTIES
    COMPILE_OPTIONS "${PICOCALC_DOOM_WARNINGS}")
target_include_directories(test_config_file PRIVATE
    ${PROJECT_SOURCE_DIR}/picocalc
    ${engine_dir}
    ${PICOCALC_DOOM_UPSTREAM}/src
)
# For the target, not m_config.c's source property, which doom_host's
# m_config.c in this directory would share; only m_config.c reads it.
target_compile_definitions(test_config_file PRIVATE
    ${PICOCALC_DOOM_ENGINE_DEFINITIONS}
    FRANK_DOOM_QUIET=0
    PICOCALC_DOOM_CONFIG_FILES=1
)
set_target_properties(test_config_file PROPERTIES C_EXTENSIONS ON)
add_test(NAME test_config_file COMMAND test_config_file
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
