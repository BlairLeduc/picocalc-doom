# engine-definitions.cmake — the definitions that shape Doom's engine,
# shared by the firmware and the host engine (design.md §15), so the host
# runs the engine the device runs: the same frame, CMAP256, sound channels
# and sound path. Data only; the firmware adds its board's definitions to
# these in CMakeLists.txt.

set(PICOCALC_DOOM_ENGINE_DEFINITIONS
    CMAP256
    DOOMGENERIC_RESX=320
    DOOMGENERIC_RESY=240
    FEATURE_SOUND
    USE_PICO_SOUND
    NUM_SOUND_CHANNELS=16
    NO_USE_LIBSAMPLERATE
    NO_USE_TIMIDITY
    NO_USE_GUS
    NO_USE_MUSIC_PACKS
    USE_DIRECT_MIDI_LUMP=0
)
