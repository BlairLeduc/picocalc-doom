# picocalc-doom — one source tree, two targets (design.md §5, §15):
#   - the UF2 for the PicoCalc with a Pimoroni Pico Plus 2 (default)
#   - a host test build, with the system compiler and no Pico SDK, behind
#     -DPICOCALC_DOOM_HOST=ON, run under CTest.
#
# Upstream's CMakeLists.txt is not used (§5.4): the build compiles a
# patched copy of upstream, made at configure time (cmake/upstream.cmake),
# from the lists in cmake/upstream-sources.cmake.

cmake_minimum_required(VERSION 3.18)

option(PICOCALC_DOOM_HOST "Build the host tests, with no Pico SDK" OFF)

set(PICOCALC_DOOM_WARNINGS -Wall -Wextra -Werror)

if (PICOCALC_DOOM_HOST)
    project(picocalc_doom_host C)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)

    enable_testing()
    add_subdirectory(test/host)

    # The host engine (§15, M2): the patched upstream built for the
    # workstation, a tool run by scripts/host_engine.sh. Needs the
    # submodule; CI builds it so a patch that breaks it is seen.
    option(PICOCALC_DOOM_HOST_ENGINE "Build doom_host, the host engine" ON)
    if (PICOCALC_DOOM_HOST_ENGINE)
        include(cmake/upstream.cmake)
        picocalc_doom_patch_upstream(${CMAKE_CURRENT_SOURCE_DIR})
        set(PICOCALC_DOOM_UPSTREAM ${CMAKE_CURRENT_SOURCE_DIR}/frank-doom)
        add_subdirectory(test/host/engine)
    endif()
else()
    # One image, for the Plus 2; it runs on a Plus 2 W too (D2, §11.2).
    if (NOT DEFINED PICO_BOARD)
        set(PICO_BOARD pimoroni_pico_plus2_rp2350)
    endif()

    include(cmake/pico_sdk_import.cmake)

    project(picocalc_doom C CXX ASM)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)

    pico_sdk_init()

    include(cmake/upstream.cmake)
    picocalc_doom_patch_upstream(${CMAKE_CURRENT_SOURCE_DIR})
    set(PICOCALC_DOOM_UPSTREAM ${CMAKE_CURRENT_SOURCE_DIR}/frank-doom)

    # The firmware's version, git describe --always --dirty, for the
    # banner (§13.1). A target that always runs, so it is taken at build
    # time; the header only changes when the text does.
    set(PICOCALC_DOOM_GENERATED ${CMAKE_BINARY_DIR}/generated)
    add_custom_target(picocalc_doom_version ALL
        COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                -DOUT=${PICOCALC_DOOM_GENERATED}/picocalc_doom_version.h
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake
        BYPRODUCTS ${PICOCALC_DOOM_GENERATED}/picocalc_doom_version.h
        COMMENT "picocalc-doom: version from git describe"
        VERBATIM)

    # PICOCALC_DOOM_UART=OFF is the build that ships: no stdio, GP2/GP3
    # free, upstream quiet. ON, the default, is the development build
    # (§14.2): stdio into the log ring, which core 1 drains to UART0
    # (picocalc/log.c).
    option(PICOCALC_DOOM_UART "stdio on UART0, GP2/GP3 (development)" ON)
    # ON draws M1's corner-coded test pattern at boot (picocalc/core1.c),
    # until Doom's first frame; OFF, the panel stays black until then.
    option(PICOCALC_DOOM_TEST_PATTERN "draw the test pattern at boot" OFF)
    # Doom arguments fixed at build time (§14.1), before any other, such
    # as "-iwad doom.wad -timedemo demo1". Each in its own build directory.
    set(PICOCALC_DOOM_ARGS "" CACHE STRING "Doom arguments built in, for scripted runs")
    # ON logs each tic's frame hash ("fh TIC CRC", §15, M5) for
    # scripts/host_engine.sh compare. Costs core 0 a pass over each frame.
    option(PICOCALC_DOOM_FRAME_HASHES "log a frame hash a tic (development)" OFF)
    # With the hashes, the frame drawn at this tic is logged too, as hex
    # rows ("fd ROW HEX"), to set beside the host engine's; -1, none.
    set(PICOCALC_DOOM_FRAME_DUMP_TIC -1 CACHE STRING "tic whose frame a hash build logs, -1 none")
    if (PICOCALC_DOOM_FRAME_HASHES AND NOT PICOCALC_DOOM_UART)
        message(FATAL_ERROR "PICOCALC_DOOM_FRAME_HASHES logs to the UART: it needs PICOCALC_DOOM_UART")
    endif()
    # Where Doom draws (§7.3): A, upstream's I_VideoBuffer in the zone, in
    # the PSRAM; B, a 76,800-byte buffer in SRAM, the gate's first lever,
    # which M5 kept (44 frames a second against A's 28). A stays as the
    # control.
    set(PICOCALC_DOOM_VIDEO_BUF B CACHE STRING "where Doom draws: A (zone, PSRAM) or B (SRAM)")
    set_property(CACHE PICOCALC_DOOM_VIDEO_BUF PROPERTY STRINGS A B)
    if (NOT PICOCALC_DOOM_VIDEO_BUF MATCHES "^[AB]$")
        message(FATAL_ERROR "PICOCALC_DOOM_VIDEO_BUF is A or B, not ${PICOCALC_DOOM_VIDEO_BUF}")
    endif()
    # The gate's second lever (§16 M5): COLORMAP, 8,704 bytes, in SRAM.
    # M5 kept it: The Ultimate Doom's demo 4 needed it to reach 35 fps.
    option(PICOCALC_DOOM_COLORMAPS_SRAM "the light tables in SRAM" ON)
    # The third: the column and span drawers in SRAM (cmake/sram_sections.cmake).
    option(PICOCALC_DOOM_DRAWERS_SRAM "Doom's column and span drawers in SRAM" OFF)
    # §8.3's first audio lever (M6): upstream's effects mixer and EMU8950's
    # slot renderer and buffer loop in SRAM, out of the XIP cache Doom's
    # renderer needs.
    option(PICOCALC_DOOM_MIX_SRAM "the mixer and OPL renderer in SRAM" ON)
    # The second: upstream's own Thumb renderer for EMU8950
    # (src/opl/slot_render_pico.S, written for the RP2040), in place of its C.
    # The audio lever M6 kept: EMU8950 rendered on core 1, its register
    # writes queued by core 0's sequencer (picocalc/opl_core1.c, §8.3).
    option(PICOCALC_DOOM_OPL_CORE1 "the OPL emulator on core 1" ON)
    option(PICOCALC_DOOM_OPL_ASM "the OPL emulator's assembly renderer" OFF)
    # The flash at 300 MHz (§11.1, §11.4): picocalc-quest's qualified
    # divider 4, RX delay 4, 75 MHz. 0 is the bootrom's timing doubled.
    set(PICOCALC_DOOM_FLASH_DIV 4 CACHE STRING "flash QMI divider at 300 MHz, 0 the bootrom's doubled")
    set(PICOCALC_DOOM_FLASH_RXDELAY 4 CACHE STRING "flash QMI RX delay with FLASH_DIV")
    # The scratch build that forces the audio's late path (§16 M6, EL §6.4):
    # core 0's interrupts masked this many ms every five seconds; 0, never.
    # The audio's gain over upstream's mix, which peaks near 1,500 of
    # 32,768 (§8.1, M6); saturated, not wrapped.
    set(PICOCALC_DOOM_AUDIO_GAIN 8 CACHE STRING "audio gain over upstream's mix, 1-16")
    set(PICOCALC_DOOM_AUDIO_STALL 0 CACHE STRING "ms core 0's interrupts are masked every 5 s (scratch)")

    # Upstream's version, for its own logs: version.txt is "MAJOR MINOR",
    # joined with a dot, as upstream does.
    file(READ ${PICOCALC_DOOM_UPSTREAM}/version.txt fd_version)
    string(STRIP "${fd_version}" fd_version)
    string(REGEX REPLACE "[ \t]+" "." fd_version "${fd_version}")

    include(cmake/upstream-sources.cmake)
    include(cmake/engine-definitions.cmake)
    set(PICOCALC_DOOM_UPSTREAM_SOURCES)
    foreach(list ${FD_ALL_SOURCE_LISTS})
        list(APPEND PICOCALC_DOOM_UPSTREAM_SOURCES ${${list}})
    endforeach()
    list(TRANSFORM PICOCALC_DOOM_UPSTREAM_SOURCES PREPEND ${PICOCALC_DOOM_UPSTREAM}/)

    # The port's own code (§5.3), held to our warnings; upstream's is
    # built as it comes.
    set(PICOCALC_DOOM_SOURCES
        picocalc/audio_shim.c
        picocalc/audio_stage.c
        picocalc/board.c
        picocalc/clock.c
        picocalc/core1.c
        picocalc/dg_picocalc.c
        picocalc/doom_files.c
        picocalc/exit.c
        picocalc/font8x10.c
        picocalc/frame_hash.c
        picocalc/framepool.c
        picocalc/input_shim.c
        picocalc/kbd.c
        picocalc/keymap.c
        picocalc/lcd.c
        picocalc/log.c
        picocalc/main.c
        picocalc/opl_core1.c
        picocalc/page.c
        picocalc/psram_check.c
        picocalc/render_sram.c
        picocalc/settings.c
        picocalc/shell_args.c
        picocalc/southbridge.c
        picocalc/status.c
        picocalc/video_shim.c
    )
    set_source_files_properties(${PICOCALC_DOOM_SOURCES} PROPERTIES
        COMPILE_OPTIONS "${PICOCALC_DOOM_WARNINGS}")
    # The audio refill path's loops stay loops: GCC would otherwise turn
    # them into calls to the library's memcpy and memset, in flash (§8.2).
    set_property(SOURCE picocalc/audio_shim.c picocalc/audio_stage.c APPEND
        PROPERTY COMPILE_OPTIONS -fno-tree-loop-distribute-patterns)

    # Upstream's s_sound.c traces every sound and song in a dozen printf
    # lines, and wi_stuff.c every frame of the intermission: more than
    # UART0 carries. The log ring dropped 38,414 lines in M5's first play
    # session, then 12,750 in one second at E1M1's intermission. Their
    # printf is quiet even in the development build, as upstream's
    # FRANK_DOOM_QUIET makes it in the release (frank_doom_quiet_stdio.h);
    # the options come after the target's definitions, so -U then -D
    # override them for these files.
    set_source_files_properties(
        ${PICOCALC_DOOM_UPSTREAM}/src/doomgeneric/doomgeneric/s_sound.c
        ${PICOCALC_DOOM_UPSTREAM}/src/doomgeneric/doomgeneric/wi_stuff.c
        PROPERTIES COMPILE_OPTIONS "-UFRANK_DOOM_QUIET;-DFRANK_DOOM_QUIET=1")

    # Doom's configuration files read and written on the card (§13.3): the
    # patch compiles m_config.c's file code back in with this, and its
    # stdio calls go to FatFs (picocalc/config_stdio.h).
    set_property(SOURCE ${PICOCALC_DOOM_UPSTREAM}/src/doomgeneric/doomgeneric/m_config.c
        APPEND PROPERTY COMPILE_DEFINITIONS PICOCALC_DOOM_CONFIG_FILES=1)

    add_executable(picocalc-doom
        ${PICOCALC_DOOM_SOURCES}
        ${PICOCALC_DOOM_UPSTREAM_SOURCES}
    )
    add_dependencies(picocalc-doom picocalc_doom_version)

    # The third lever (§16 M5): upstream's drawers moved to SRAM after they
    # are compiled, by renaming their sections, with no patch (§5.3).
    if (PICOCALC_DOOM_DRAWERS_SRAM)
        set(sram_funcs
            R_DrawColumn R_DrawColumnLow
            R_DrawFuzzColumn R_DrawFuzzColumnLow
            R_DrawTranslatedColumn R_DrawTranslatedColumnLow
            R_DrawSpan R_DrawSpanLow
            R_DrawMaskedColumn)
        # Commas, not semicolons, through make and the shell.
        string(REPLACE ";" "," sram_funcs_arg "${sram_funcs}")
        add_custom_command(TARGET picocalc-doom PRE_LINK
            COMMAND ${CMAKE_COMMAND}
                -DOBJDIR=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/picocalc-doom.dir
                -DOBJCOPY=${CMAKE_OBJCOPY}
                -DFILES=r_draw.c,r_things.c
                -DFUNCS=${sram_funcs_arg}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sram_sections.cmake
            COMMENT "picocalc-doom: Doom's drawers to SRAM"
            VERBATIM)
    endif()

    if (PICOCALC_DOOM_MIX_SRAM)
        add_custom_command(TARGET picocalc-doom PRE_LINK
            COMMAND ${CMAKE_COMMAND}
                -DOBJDIR=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/picocalc-doom.dir
                -DOBJCOPY=${CMAKE_OBJCOPY}
                -DFILES=slot_render.cpp
                -DFUNCS=ALL
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sram_sections.cmake
            COMMAND ${CMAKE_COMMAND}
                -DOBJDIR=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/picocalc-doom.dir
                -DOBJCOPY=${CMAKE_OBJCOPY}
                -DFILES=emu8950.c
                -DFUNCS=OPL_calc_buffer_linear,OPL_calc_buffer_stereo
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sram_sections.cmake
            COMMAND ${CMAKE_COMMAND}
                -DOBJDIR=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/picocalc-doom.dir
                -DOBJCOPY=${CMAKE_OBJCOPY}
                -DFILES=i_picosound.c
                -DFUNCS=I_Pico_UpdateSound,decompress_buffer,adpcm_decode_block_s8
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sram_sections.cmake
            COMMENT "picocalc-doom: the mixer and OPL renderer to SRAM"
            VERBATIM)
    endif()

    # The audio refill path (§6.2, §8.2): upstream's fill callback, which
    # the PWM's interrupt calls, moved to SRAM, and its object's memcpy and
    # memset sent to the shim's, in SRAM, which also count underruns
    # (picocalc/audio_shim.c). Then, after the link, every function the
    # interrupt reaches is checked to be in SRAM.
    add_custom_command(TARGET picocalc-doom PRE_LINK
        COMMAND ${CMAKE_COMMAND}
            -DOBJDIR=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/picocalc-doom.dir
            -DOBJCOPY=${CMAKE_OBJCOPY}
            -DFILES=i_picosound.c
            -DFUNCS=i2s_fill_audio
            -DSYMS=memcpy=picocalc_audio_memcpy,memset=picocalc_audio_memset
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sram_sections.cmake
        COMMENT "picocalc-doom: the audio callback to SRAM"
        VERBATIM)
    add_custom_command(TARGET picocalc-doom POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DELF=$<TARGET_FILE:picocalc-doom>
            -DOBJDUMP=${CMAKE_OBJDUMP}
            -DFUNCS=audio_irq,i2s_fill_audio$<$<BOOL:${PICOCALC_DOOM_OPL_CORE1}>:,gated_fill>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_sram.cmake
        COMMENT "picocalc-doom: the audio refill path is in SRAM"
        VERBATIM)

    # Upstream's include paths; src/pico has audio.h, the API
    # picocalc/audio_shim.c implements.
    target_include_directories(picocalc-doom PRIVATE
        picocalc
        ${PICOCALC_DOOM_GENERATED}
        ${PICOCALC_DOOM_UPSTREAM}/src
        ${PICOCALC_DOOM_UPSTREAM}/src/doomgeneric/doomgeneric
        ${PICOCALC_DOOM_UPSTREAM}/src/pico
        ${PICOCALC_DOOM_UPSTREAM}/src/fatfs
        ${PICOCALC_DOOM_UPSTREAM}/src/opl
        ${PICOCALC_DOOM_UPSTREAM}/drivers
        ${PICOCALC_DOOM_UPSTREAM}/drivers/sdcard
    )

    # Upstream's definitions, less its board variant, clock and voltage,
    # and HDMI and PS/2 settings (§5.4).
    target_compile_definitions(picocalc-doom PRIVATE
        PSRAM_MAX_FREQ_MHZ=100              # §11.3
        FRANK_DOOM_VERSION="${fd_version}"
        FRANK_DOOM_QUIET=$<NOT:$<BOOL:${PICOCALC_DOOM_UART}>>

        ${PICOCALC_DOOM_ENGINE_DEFINITIONS}
        USE_OPL_MUSIC=1
        USE_EMU8950_OPL=1
        EMU8950_NO_WAVE_TABLE_MAP=1
        EMU8950_NO_TLL=1
        EMU8950_NO_FLOAT=1
        EMU8950_NO_TIMER=1
        EMU8950_NO_TEST_FLAG=1
        EMU8950_SIMPLER_NOISE=1
        EMU8950_SHORT_NOISE_UPDATE_CHECK=1
        EMU8950_LINEAR_SKIP=1
        EMU8950_LINEAR_END_OF_NOTE_OPTIMIZATION=1
        EMU8950_NO_PERCUSSION_MODE=1
        EMU8950_LINEAR=1
        EMU8950_ASM=$<BOOL:${PICOCALC_DOOM_OPL_ASM}>
        EMU8950_SLOT_RENDER=1
        EMU8950_NO_RATECONV=1
        PICO_ON_DEVICE=1

        # Upstream's sound module names its I2S pins in a log line and the
        # config it hands i2s_init(); the shim ignores them and drives the
        # PicoCalc's PWM on GP26/27 (§8).
        PICO_AUDIO_I2S_DATA_PIN=26
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=27

        # The card on hardware spi0 at 25 MHz (§10.1).
        SDCARD_PIN_SPI0_MISO=16
        SDCARD_PIN_SPI0_CS=17
        SDCARD_PIN_SPI0_SCK=18
        SDCARD_PIN_SPI0_MOSI=19
        SDCARD_CLK_FAST=25000000
        # get_fattime() from the port, not the driver's (§13.2).
        SDCARD_EXTERNAL_FATTIME=1

        # clk_peri follows clk_sys, for the LCD's SPI divider (§11.1).
        PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK=1
        # UART0 at 115200 8-N-1 on GP2/GP3, as the shell's (D5).
        PICOCALC_DOOM_UART=$<BOOL:${PICOCALC_DOOM_UART}>
        PICOCALC_DOOM_TEST_PATTERN=$<BOOL:${PICOCALC_DOOM_TEST_PATTERN}>
        PICOCALC_DOOM_ARGS="${PICOCALC_DOOM_ARGS}"
        PICOCALC_DOOM_FRAME_HASHES=$<BOOL:${PICOCALC_DOOM_FRAME_HASHES}>
        PICOCALC_DOOM_FRAME_DUMP_TIC=${PICOCALC_DOOM_FRAME_DUMP_TIC}
        PICOCALC_DOOM_VIDEO_BUF_B=$<STREQUAL:${PICOCALC_DOOM_VIDEO_BUF},B>
        PICOCALC_DOOM_COLORMAPS_SRAM=$<BOOL:${PICOCALC_DOOM_COLORMAPS_SRAM}>
        PICOCALC_DOOM_FLASH_DIV=${PICOCALC_DOOM_FLASH_DIV}
        PICOCALC_DOOM_FLASH_RXDELAY=${PICOCALC_DOOM_FLASH_RXDELAY}
        PICOCALC_DOOM_AUDIO_STALL=${PICOCALC_DOOM_AUDIO_STALL}
        PICOCALC_DOOM_AUDIO_GAIN=${PICOCALC_DOOM_AUDIO_GAIN}
        PICOCALC_DOOM_OPL_CORE1=$<BOOL:${PICOCALC_DOOM_OPL_CORE1}>
        PICO_DEFAULT_UART=0
        PICO_DEFAULT_UART_TX_PIN=2
        PICO_DEFAULT_UART_RX_PIN=3
    )

    target_link_libraries(picocalc-doom
        pico_stdlib
        pico_multicore
        pico_unique_id
        hardware_adc
        hardware_clocks
        hardware_dma
        hardware_flash
        hardware_i2c
        hardware_irq
        hardware_interp
        hardware_pio
        hardware_powman     # the shell's clock (§13.2)
        hardware_pwm
        hardware_spi
        hardware_xip_cache  # the PSRAM's round trip (§11.3)
    )

    # As upstream: Doom's stdio file calls go to FatFs
    # (src/doomgeneric_fatfs/stdio_fatfs.c).
    target_link_options(picocalc-doom PRIVATE
        -Wl,--wrap=fopen
        -Wl,--wrap=fclose
        -Wl,--wrap=fread
        -Wl,--wrap=fgetc
        -Wl,--wrap=fwrite
        -Wl,--wrap=fseek
        -Wl,--wrap=ftell
        -Wl,--wrap=remove
        -Wl,--wrap=rename
        # Timed, not changed: the copy of strategy A (picocalc/video_shim.c)
        # and each WAD's open and directory (picocalc/dg_picocalc.c).
        -Wl,--wrap=I_FinishUpdate
        -Wl,--wrap=W_AddFile
        # The demo's name kept past D_DoomMain() (picocalc/dg_picocalc.c).
        -Wl,--wrap=G_DeferedPlayDemo
        -Wl,--wrap=G_TimeDemo
        # A save directory for each game (picocalc/doom_files.c, §10.2).
        -Wl,--wrap=M_GetSaveGameDir
        # A save's write, timed (picocalc/doom_files.c, M8).
        -Wl,--wrap=P_TempSaveGameFile
        -Wl,--wrap=R_FillBackScreen
        # FatFs's buffers kept from malloc()'s panic (picocalc/doom_files.c).
        -Wl,--wrap=ff_memalloc
        # Mixing's share (picocalc/audio_shim.c, §8.3).
        -Wl,--wrap=I_UpdateSound
        # The OPL on core 1 (picocalc/opl_core1.c, §8.3).
        $<$<BOOL:${PICOCALC_DOOM_OPL_CORE1}>:-Wl,--wrap=I_PicoSoundSetMusicGenerator>
        $<$<BOOL:${PICOCALC_DOOM_OPL_CORE1}>:-Wl,--wrap=OPL_writeReg>
        $<$<BOOL:${PICOCALC_DOOM_OPL_CORE1}>:-Wl,--wrap=OPL_calc_buffer_stereo>
        $<$<STREQUAL:${PICOCALC_DOOM_VIDEO_BUF},B>:-Wl,--wrap=I_InitGraphics>
        $<$<BOOL:${PICOCALC_DOOM_COLORMAPS_SRAM}>:-Wl,--wrap=R_Init>
    )

    # stdio is the log ring's driver (picocalc/log.c), not the SDK's UART.
    pico_enable_stdio_uart(picocalc-doom 0)
    pico_enable_stdio_usb(picocalc-doom 0)

    pico_add_extra_outputs(picocalc-doom)

    # The image's size on every build (§12, §15).
    get_filename_component(PICOCALC_DOOM_TOOLCHAIN_BIN ${CMAKE_C_COMPILER} DIRECTORY)
    find_program(PICOCALC_DOOM_SIZE arm-none-eabi-size HINTS ${PICOCALC_DOOM_TOOLCHAIN_BIN} REQUIRED)
    add_custom_command(TARGET picocalc-doom POST_BUILD
        COMMAND ${PICOCALC_DOOM_SIZE} $<TARGET_FILE:picocalc-doom>
        VERBATIM)
endif()
