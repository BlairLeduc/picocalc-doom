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
    # (§14.2). M0 uses the SDK's UART stdio; M1 replaces it with the log
    # ring.
    option(PICOCALC_DOOM_UART "stdio on UART0, GP2/GP3 (development)" ON)

    # Upstream's version, for its own logs: version.txt is "MAJOR MINOR",
    # joined with a dot, as upstream does.
    file(READ ${PICOCALC_DOOM_UPSTREAM}/version.txt fd_version)
    string(STRIP "${fd_version}" fd_version)
    string(REGEX REPLACE "[ \t]+" "." fd_version "${fd_version}")

    include(cmake/upstream-sources.cmake)
    set(PICOCALC_DOOM_UPSTREAM_SOURCES)
    foreach(list ${FD_ALL_SOURCE_LISTS})
        list(APPEND PICOCALC_DOOM_UPSTREAM_SOURCES ${${list}})
    endforeach()
    list(TRANSFORM PICOCALC_DOOM_UPSTREAM_SOURCES PREPEND ${PICOCALC_DOOM_UPSTREAM}/)

    # The port's own code (§5.3), held to our warnings; upstream's is
    # built as it comes.
    set(PICOCALC_DOOM_SOURCES
        picocalc/audio_shim.c
        picocalc/dg_picocalc.c
        picocalc/input_shim.c
        picocalc/main.c
        picocalc/video_shim.c
    )
    set_source_files_properties(${PICOCALC_DOOM_SOURCES} PROPERTIES
        COMPILE_OPTIONS "${PICOCALC_DOOM_WARNINGS}")

    add_executable(picocalc-doom
        ${PICOCALC_DOOM_SOURCES}
        ${PICOCALC_DOOM_UPSTREAM_SOURCES}
    )
    add_dependencies(picocalc-doom picocalc_doom_version)

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

        CMAP256
        DOOMGENERIC_RESX=320
        DOOMGENERIC_RESY=240
        FEATURE_SOUND
        USE_PICO_SOUND
        USE_OPL_MUSIC=1
        USE_EMU8950_OPL=1
        NUM_SOUND_CHANNELS=16
        NO_USE_LIBSAMPLERATE
        NO_USE_TIMIDITY
        NO_USE_GUS
        NO_USE_MUSIC_PACKS
        USE_DIRECT_MIDI_LUMP=0
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
        EMU8950_ASM=0
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
        PICO_DEFAULT_UART=0
        PICO_DEFAULT_UART_TX_PIN=2
        PICO_DEFAULT_UART_RX_PIN=3
    )

    target_link_libraries(picocalc-doom
        pico_stdlib
        pico_multicore
        hardware_clocks
        hardware_dma
        hardware_irq
        hardware_interp
        hardware_pio
        hardware_pwm
        hardware_spi
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
    )

    pico_enable_stdio_uart(picocalc-doom $<BOOL:${PICOCALC_DOOM_UART}>)
    pico_enable_stdio_usb(picocalc-doom 0)

    pico_add_extra_outputs(picocalc-doom)

    # The image's size on every build (§12, §15).
    get_filename_component(PICOCALC_DOOM_TOOLCHAIN_BIN ${CMAKE_C_COMPILER} DIRECTORY)
    find_program(PICOCALC_DOOM_SIZE arm-none-eabi-size HINTS ${PICOCALC_DOOM_TOOLCHAIN_BIN} REQUIRED)
    add_custom_command(TARGET picocalc-doom POST_BUILD
        COMMAND ${PICOCALC_DOOM_SIZE} $<TARGET_FILE:picocalc-doom>
        VERBATIM)
endif()
