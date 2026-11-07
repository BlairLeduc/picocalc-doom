# upstream-sources.cmake — the upstream files the firmware compiles
# (design.md §5.4), as data only: no targets, so scripts/check_sources.sh
# can read it with cmake -P.
#
# Upstream gathers its engine with file(GLOB src/doomgeneric/doomgeneric/*.c),
# filters out every doomgeneric_*.c, every i_*.c, w_file_stdc.c and
# m_misc.c, then adds some back. These lists are the result, at the
# submodule's commit. After a submodule bump, check_sources.sh reports any
# file upstream added or removed; copy the change here. Paths are relative
# to the patched copy, frank-doom/.

# Upstream's glob after its filters.
set(FD_ENGINE_SOURCES
    src/doomgeneric/doomgeneric/am_map.c
    src/doomgeneric/doomgeneric/d_event.c
    src/doomgeneric/doomgeneric/d_items.c
    src/doomgeneric/doomgeneric/d_iwad.c
    src/doomgeneric/doomgeneric/d_loop.c
    src/doomgeneric/doomgeneric/d_main.c
    src/doomgeneric/doomgeneric/d_mode.c
    src/doomgeneric/doomgeneric/d_net.c
    src/doomgeneric/doomgeneric/doomdef.c
    src/doomgeneric/doomgeneric/doomgeneric.c
    src/doomgeneric/doomgeneric/doomstat.c
    src/doomgeneric/doomgeneric/dstrings.c
    src/doomgeneric/doomgeneric/dummy.c
    src/doomgeneric/doomgeneric/f_finale.c
    src/doomgeneric/doomgeneric/f_wipe.c
    src/doomgeneric/doomgeneric/g_game.c
    src/doomgeneric/doomgeneric/gusconf.c
    src/doomgeneric/doomgeneric/hu_lib.c
    src/doomgeneric/doomgeneric/hu_stuff.c
    src/doomgeneric/doomgeneric/icon.c
    src/doomgeneric/doomgeneric/info.c
    src/doomgeneric/doomgeneric/m_argv.c
    src/doomgeneric/doomgeneric/m_bbox.c
    src/doomgeneric/doomgeneric/m_cheat.c
    src/doomgeneric/doomgeneric/m_config.c
    src/doomgeneric/doomgeneric/m_controls.c
    src/doomgeneric/doomgeneric/m_fixed.c
    src/doomgeneric/doomgeneric/m_menu.c
    src/doomgeneric/doomgeneric/m_random.c
    src/doomgeneric/doomgeneric/memio.c
    src/doomgeneric/doomgeneric/mus2mid.c
    src/doomgeneric/doomgeneric/p_ceilng.c
    src/doomgeneric/doomgeneric/p_doors.c
    src/doomgeneric/doomgeneric/p_enemy.c
    src/doomgeneric/doomgeneric/p_floor.c
    src/doomgeneric/doomgeneric/p_inter.c
    src/doomgeneric/doomgeneric/p_lights.c
    src/doomgeneric/doomgeneric/p_map.c
    src/doomgeneric/doomgeneric/p_maputl.c
    src/doomgeneric/doomgeneric/p_mobj.c
    src/doomgeneric/doomgeneric/p_plats.c
    src/doomgeneric/doomgeneric/p_pspr.c
    src/doomgeneric/doomgeneric/p_saveg.c
    src/doomgeneric/doomgeneric/p_setup.c
    src/doomgeneric/doomgeneric/p_sight.c
    src/doomgeneric/doomgeneric/p_spec.c
    src/doomgeneric/doomgeneric/p_switch.c
    src/doomgeneric/doomgeneric/p_telept.c
    src/doomgeneric/doomgeneric/p_tick.c
    src/doomgeneric/doomgeneric/p_user.c
    src/doomgeneric/doomgeneric/r_bsp.c
    src/doomgeneric/doomgeneric/r_data.c
    src/doomgeneric/doomgeneric/r_draw.c
    src/doomgeneric/doomgeneric/r_main.c
    src/doomgeneric/doomgeneric/r_plane.c
    src/doomgeneric/doomgeneric/r_segs.c
    src/doomgeneric/doomgeneric/r_sky.c
    src/doomgeneric/doomgeneric/r_things.c
    src/doomgeneric/doomgeneric/s_sound.c
    src/doomgeneric/doomgeneric/sha1.c
    src/doomgeneric/doomgeneric/sounds.c
    src/doomgeneric/doomgeneric/st_lib.c
    src/doomgeneric/doomgeneric/st_stuff.c
    src/doomgeneric/doomgeneric/statdump.c
    src/doomgeneric/doomgeneric/tables.c
    src/doomgeneric/doomgeneric/v_video.c
    src/doomgeneric/doomgeneric/w_checksum.c
    src/doomgeneric/doomgeneric/w_file.c
    src/doomgeneric/doomgeneric/w_main.c
    src/doomgeneric/doomgeneric/w_wad.c
    src/doomgeneric/doomgeneric/z_zone.c
)

# What upstream adds back after the filters (its list(APPEND
# DOOMGENERIC_SOURCES ...)): the engine's own i_*.c it keeps, and its sound,
# OPL and MIDI code.
set(FD_ENGINE_ADDED_SOURCES
    src/doomgeneric/doomgeneric/i_video.c
    src/doomgeneric/doomgeneric/i_timer.c
    src/doomgeneric/doomgeneric/i_input.c
    src/doomgeneric/doomgeneric/i_sound.c
    src/doomgeneric/doomgeneric/i_endoom.c
    src/doomgeneric/doomgeneric/doomgeneric.c
    src/doomgeneric/doomgeneric/wi_stuff.c
    src/pico/i_picosound.c
    src/pico/i_oplmusic.c
    src/midifile.c
    src/opl/emu8950.c
    src/opl/emuadpcm.c
    src/opl/opl_api.c
    src/opl/opl_pico.c
    src/opl/slot_render.cpp
)

# Upstream's add_executable(frank-doom) sources, less its main.c, its
# platform (doomgeneric_rp2350.c) and its I2S driver (audio.c), which
# picocalc/ replaces (§5.3).
set(FD_MAIN_SOURCES
    src/doomgeneric_fatfs/w_file_fatfs.c
    src/doomgeneric_fatfs/m_misc_fatfs.c
    src/doomgeneric_fatfs/stdio_fatfs.c
    src/opl/slot_render_pico.S
)

# Upstream's drivers library, HDMI.c excepted.
set(FD_DRIVER_SOURCES
    drivers/psram_init.c
    drivers/psram_allocator.c
)

# Upstream's fatfs and sdcard libraries (src/fatfs/CMakeLists.txt,
# drivers/sdcard/sdcard.cmake), pio_spi.c excepted: the card is on
# hardware spi0 (§10.1).
set(FD_STORAGE_SOURCES
    src/fatfs/ff.c
    src/fatfs/ffunicode.c
    src/fatfs/ffsystem.c
    drivers/sdcard/sdcard.c
)

set(FD_ALL_SOURCE_LISTS
    FD_ENGINE_SOURCES
    FD_ENGINE_ADDED_SOURCES
    FD_MAIN_SOURCES
    FD_DRIVER_SOURCES
    FD_STORAGE_SOURCES
)

# Upstream builds these; the port does not (§3.3, §5.4).
set(FD_EXCLUDED_SOURCES
    src/main.c                          # picocalc/main.c
    src/doomgeneric_rp2350.c            # picocalc/dg_picocalc.c and the shims
    src/pico/audio.c                    # picocalc/audio_shim.c, PWM (§8)
    drivers/HDMI.c                      # the LCD (§7)
    drivers/sdcard/pio_spi.c            # hardware SPI
    drivers/ps2kbd/ps2kbd_mrmltr.cpp    # the southbridge keyboard (§9)
    drivers/ps2kbd/ps2kbd_wrapper.cpp
    drivers/ps2mouse/ps2mouse.c         # no mouse
    drivers/ps2mouse/ps2mouse_wrapper.c
)

# Files compiled here that upstream's build does not name. None yet.
set(FD_ADDED_SOURCES
)
