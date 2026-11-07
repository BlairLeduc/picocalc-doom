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

# The key map: every MCU code, retranslated releases, the Shift trap, the
# bindings against the chords the MCU swallows (§9.2, §9.3).
add_executable(test_keymap test_keymap.c ${PROJECT_SOURCE_DIR}/picocalc/keymap.c)
target_include_directories(test_keymap PRIVATE ${PROJECT_SOURCE_DIR}/picocalc)
target_compile_definitions(test_keymap PRIVATE PICOCALC_DOOM_HOST=1)
target_compile_options(test_keymap PRIVATE ${PICOCALC_DOOM_WARNINGS})
add_test(NAME test_keymap COMMAND test_keymap)

# The audio shim's staging and conversion: the period, every 16-bit sample
# to a compare value against a model, slots in and halves out across every
# boundary, underruns as silence (§8.1, §8.2).
add_executable(test_audio_stage test_audio_stage.c ${PROJECT_SOURCE_DIR}/picocalc/audio_stage.c)
target_include_directories(test_audio_stage PRIVATE ${PROJECT_SOURCE_DIR}/picocalc)
target_compile_definitions(test_audio_stage PRIVATE PICOCALC_DOOM_HOST=1)
target_compile_options(test_audio_stage PRIVATE ${PICOCALC_DOOM_WARNINGS})
target_link_libraries(test_audio_stage PRIVATE m)
add_test(NAME test_audio_stage COMMAND test_audio_stage)

# picocalc.ini: every setting, bad lines named and the rest applied, and
# the run key against the key map's bindings (§10.3, §9.2).
add_executable(test_settings test_settings.c ${PROJECT_SOURCE_DIR}/picocalc/settings.c
    ${PROJECT_SOURCE_DIR}/picocalc/keymap.c)
target_include_directories(test_settings PRIVATE ${PROJECT_SOURCE_DIR}/picocalc)
target_compile_definitions(test_settings PRIVATE PICOCALC_DOOM_HOST=1)
target_compile_options(test_settings PRIVATE ${PICOCALC_DOOM_WARNINGS})
add_test(NAME test_settings COMMAND test_settings)
