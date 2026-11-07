# Host tests (design.md §15). C tests build the portable parts of
# picocalc/ with the system compiler and no Pico SDK, one binary per area;
# exit 77 is a skip. Each milestone adds the tests for what it brings (the
# frame pool, the key map, the audio staging, the settings).

# create_patch.sh then apply_patch.sh reproduces working/ exactly: on a
# made-up upstream, and on the real patch when the submodule is present.
foreach(mode synthetic repo)
    add_test(NAME test_patch_roundtrip_${mode}
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_patch_roundtrip.sh ${mode})
    set_tests_properties(test_patch_roundtrip_${mode} PROPERTIES SKIP_RETURN_CODE 77)
endforeach()

# The shell's time to calendar fields, and FAT's time stamp (§13.2).
add_executable(test_clock test_clock.c ${PROJECT_SOURCE_DIR}/picocalc/clock.c)
target_include_directories(test_clock PRIVATE ${PROJECT_SOURCE_DIR}/picocalc)
target_compile_definitions(test_clock PRIVATE PICOCALC_DOOM_HOST=1)
target_compile_options(test_clock PRIVATE ${PICOCALC_DOOM_WARNINGS})
add_test(NAME test_clock COMMAND test_clock)

# Frame hashes: the CRC-32 and the one-hash-a-tic rule (§15).
add_executable(test_frame_hash test_frame_hash.c ${PROJECT_SOURCE_DIR}/picocalc/frame_hash.c)
target_include_directories(test_frame_hash PRIVATE ${PROJECT_SOURCE_DIR}/picocalc)
target_compile_options(test_frame_hash PRIVATE ${PICOCALC_DOOM_WARNINGS})
add_test(NAME test_frame_hash COMMAND test_frame_hash)

# The frame pool, the margins rule and the row expander (§7.2).
add_executable(test_framepool test_framepool.c ${PROJECT_SOURCE_DIR}/picocalc/framepool.c)
target_include_directories(test_framepool PRIVATE ${PROJECT_SOURCE_DIR}/picocalc)
target_compile_definitions(test_framepool PRIVATE PICOCALC_DOOM_HOST=1)
target_compile_options(test_framepool PRIVATE ${PICOCALC_DOOM_WARNINGS})
add_test(NAME test_framepool COMMAND test_framepool)
