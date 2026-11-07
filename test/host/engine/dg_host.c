/*
 * picocalc-doom — the host engine's headless platform (design.md §15, M2).
 *
 * The patched upstream engine built for the workstation, with this file in
 * place of the device's dg_picocalc.c and shims: no window, no input, a null
 * sound module, upstream's own stdio file code instead of the FatFs glue,
 * and the device's zone size. It is a tool, run by scripts/host_engine.sh,
 * not a CTest: it needs WADs, which never go in the repository.
 *
 * Doom's own arguments work as they do anywhere (-iwad, -file, -timedemo,
 * -warp, -skill). Doom ignores arguments it does not know, so the tool's
 * own are read from the same list:
 *
 *   -hosthashes FILE  write one frame hash a tic (frame_hash.h) to FILE
 *   -hosttics N       run N tics of a level, report, and exit
 *   -hostzone MB      the zone's size; the device's 3 MB by default (§12.2)
 *   -hostzonefill B   fill the zone with byte B before Doom uses it
 *   -hostzoneoffset N move the zone's base N bytes (a multiple of 16)
 *                     The last two change what a read of memory Doom never
 *                     wrote, or of a zone block's header, returns: a frame
 *                     whose hash changes with them depends on such a read
 *                     and cannot match the device's (scripts/host_engine.sh)
 *   -hostclock        a virtual clock: time passes only when Doom sleeps,
 *                     so -hosttics runs as fast as the host can draw
 *   -hostshot TIC FILE  write the last frame drawn at game tic TIC to FILE,
 *                     a PPM, to look at; -1 for the last frame of the run
 *
 * Results are lines on stdout beginning "host:", for the script to read
 * among Doom's own messages.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Doom's headers first: doomtype.h declares false and true itself. */
#include "doomgeneric.h"
#include "doomtype.h"
#include "doomstat.h"
#include "d_player.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "w_wad.h"
#include "z_zone.h"

#include "frame_hash.h"
#include "psram_allocator.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern int prndindex;
extern boolean palette_changed;

/* The device's lumpinfo_t: name[8], four 4-byte pointers and ints. The
 * host's is larger (8-byte pointers); the device's is what §12.1 budgets. */
#define DEVICE_LUMPINFO_BYTES 28

static FILE *hash_file;
static frame_hash_tic_t hash_held;
static unsigned long hash_tics, hash_frames;

static int shot_tic = -2;          /* -hostshot; -2 for none */
static const char *shot_file;
static uint8_t shot_rgb[256 * 3];

static int run_tics;            /* -hosttics; 0 runs until Doom stops */
static int level_start = -1;    /* gametic at the first frame of the level */
static int virtual_clock;
static uint32_t virtual_ms;

/* The zone, sampled each frame, and the WAD's reads, counted. */
static int zone_size;
static int zone_low_free = -1;      /* least free or purgeable, bytes */
static unsigned long reads, read_bytes;             /* all of them */
static unsigned long play_reads, play_bytes;        /* once a level is drawn */
static int in_play;

static uint64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

static uint64_t start_us;

/* Upstream's w_file_stdc.c is compiled with fread renamed to this
 * (CMakeLists.txt): every read of a WAD, which on the device is a read of
 * the card (§10.1). Reads once a level's first frame is drawn are the ones
 * in play: a lump the zone purged and needs again, or one the level's
 * precache did not read (Doom skips the precache in a demo). */
size_t host_fread(void *ptr, size_t size, size_t nitems, FILE *stream)
{
    size_t n = fread(ptr, size, nitems, stream);
    reads++;
    read_bytes += n * size;
    if (in_play) {
        play_reads++;
        play_bytes += n * size;
    }
    return n;
}

static void sample_zone(void)
{
    int free_bytes = Z_FreeMemory();
    if (zone_low_free < 0 || free_bytes < zone_low_free) {
        zone_low_free = free_bytes;
    }
}

static const char *map_name(void)
{
    static char name[16];
    if (gamemode == commercial) {
        snprintf(name, sizeof name, "MAP%02d", gamemap);
    } else {
        snprintf(name, sizeof name, "E%dM%d", gameepisode, gamemap);
    }
    return name;
}

static void palette_rgb(uint8_t *rgb)
{
    for (int c = 0; c < 256; c++) {
        rgb[3 * c + 0] = colors[c].r;
        rgb[3 * c + 1] = colors[c].g;
        rgb[3 * c + 2] = colors[c].b;
    }
}

static void write_shot(void)
{
    FILE *f = fopen(shot_file, "wb");
    if (f == NULL) {
        perror(shot_file);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", DOOMGENERIC_RESX, DOOMGENERIC_RESY);
    for (int i = 0; i < DOOMGENERIC_RESX * DOOMGENERIC_RESY; i++) {
        fwrite(&shot_rgb[3 * DG_ScreenBuffer[i]], 1, 3, f);
    }
    fclose(f);
}

static void report(const char *why)
{
    frame_hash_tic_t done;
    if (shot_tic == -1) {
        palette_rgb(shot_rgb);
        write_shot();
    }
    if (hash_file != NULL) {
        if (frame_hash_flush(&hash_held, &done)) {
            fprintf(hash_file, "%d %08x\n", done.tic, (unsigned)done.crc);
            hash_tics++;
        }
        fclose(hash_file);
        hash_file = NULL;
    }

    double secs = (double)(now_us() - start_us) / 1e6;
    player_t *p = &players[consoleplayer];
    printf("host: end %s\n", why);
    printf("host: state gametic %d map %s skill %d gamestate %d prnd %d\n",
           gametic, map_name(), gameskill, gamestate, prndindex);
    if (p->mo != NULL) {
        printf("host: player health %d armor %d x %d y %d z %d angle %08x"
               " kills %d items %d secrets %d\n",
               p->health, p->armorpoints, p->mo->x, p->mo->y, p->mo->z,
               (unsigned)p->mo->angle, p->killcount, p->itemcount,
               p->secretcount);
    }
    printf("host: zone size %d low_free %d peak_used %d\n",
           zone_size, zone_low_free, zone_size - zone_low_free);
    printf("host: reads all %lu bytes %lu play %lu play_bytes %lu\n",
           reads, read_bytes, play_reads, play_bytes);
    printf("host: wad lumps %u directory %u\n",
           numlumps, numlumps * DEVICE_LUMPINFO_BYTES);
    if (hash_tics) {
        printf("host: hashes tics %lu frames %lu\n", hash_tics, hash_frames);
    }
    printf("host: time %.2f s\n", secs);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    start_us = now_us();

    /* Doom's settings are its defaults on every run: upstream reads
     * default.cfg from the working directory, so the tool names an empty
     * one unless told otherwise. */
    char **args = calloc((size_t)argc + 5, sizeof *args);
    if (args == NULL) {
        return 2;
    }
    memcpy(args, argv, (size_t)argc * sizeof *args);
    int n = argc;
    myargc = argc;
    myargv = argv;
    if (!M_CheckParm("-config")) {
        args[n++] = "-config";
        args[n++] = "/dev/null";
    }
    if (!M_CheckParm("-extraconfig")) {
        args[n++] = "-extraconfig";
        args[n++] = "/dev/null";
    }
    argc = n;
    argv = args;
    myargc = argc;
    myargv = argv;

    int i = M_CheckParmWithArgs("-hosthashes", 1);
    if (i) {
        hash_file = fopen(argv[i + 1], "w");
        if (hash_file == NULL) {
            perror(argv[i + 1]);
            return 2;
        }
    }
    i = M_CheckParmWithArgs("-hosttics", 1);
    if (i) {
        run_tics = atoi(argv[i + 1]);
    }
    virtual_clock = M_CheckParm("-hostclock") != 0;
    i = M_CheckParmWithArgs("-hostshot", 2);
    if (i) {
        shot_tic = atoi(argv[i + 1]);
        shot_file = argv[i + 2];
    }

    doomgeneric_Create(argc, argv);
    for (;;) {
        doomgeneric_Tick();
    }
}

void DG_Init(void)
{
    DG_ScreenBuffer = calloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY, sizeof(pixel_t));
    if (DG_ScreenBuffer == NULL) {
        fprintf(stderr, "host: out of memory\n");
        exit(2);
    }
}

void DG_DrawFrame(void)
{
    palette_changed = false;
    sample_zone();
    if (gamestate == GS_LEVEL) {
        in_play = 1;
    }

    if (shot_tic >= 0 && gametic == shot_tic) {
        palette_rgb(shot_rgb);
        write_shot();
    }

    if (hash_file != NULL) {
        uint8_t rgb[256 * 3];
        palette_rgb(rgb);
        uint32_t crc = frame_hash_frame(DG_ScreenBuffer,
                                        DOOMGENERIC_RESX * DOOMGENERIC_RESY, rgb);
        frame_hash_tic_t done;
        if (frame_hash_note(&hash_held, gametic, crc, &done)) {
            fprintf(hash_file, "%d %08x\n", done.tic, (unsigned)done.crc);
            hash_tics++;
        }
        hash_frames++;
    }

    if (run_tics && gamestate == GS_LEVEL) {
        if (level_start < 0) {
            level_start = gametic;
        } else if (gametic - level_start >= run_tics) {
            report("tics");
            exit(0);
        }
    }
}

void DG_SleepMs(uint32_t ms)
{
    if (virtual_clock) {
        virtual_ms += ms;
        return;
    }
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

uint32_t DG_GetTicksMs(void)
{
    if (virtual_clock) {
        return virtual_ms;
    }
    return (uint32_t)((now_us() - start_us) / 1000u);
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    (void)pressed;
    (void)key;
    return 0;
}

void DG_SetWindowTitle(const char *title)
{
    (void)title;
}

/* A null sound module (§15): Init fails, so the engine plays nothing and
 * calls nothing else of it. The music module is always taken, so every
 * entry does nothing. */

static boolean null_sound_init(boolean use_sfx_prefix)
{
    (void)use_sfx_prefix;
    return false;
}

static void null_void(void)
{
}

static int null_sfx_lump(sfxinfo_t *sfx)
{
    (void)sfx;
    return 0;
}

static void null_params(int channel, int vol, int sep)
{
    (void)channel;
    (void)vol;
    (void)sep;
}

static int null_start(sfxinfo_t *sfx, int channel, int vol, int sep, int pitch)
{
    (void)sfx;
    (void)vol;
    (void)sep;
    (void)pitch;
    return channel;
}

static void null_stop(int channel)
{
    (void)channel;
}

static boolean null_playing(int channel)
{
    (void)channel;
    return false;
}

static void null_cache(sfxinfo_t *sounds, int num_sounds)
{
    (void)sounds;
    (void)num_sounds;
}

sound_module_t DG_sound_module = {
    NULL, 0,
    null_sound_init, null_void, null_sfx_lump, null_void, null_params,
    null_start, null_stop, null_playing, null_cache,
};

static boolean null_music_init(void)
{
    return true;
}

static void null_volume(int volume)
{
    (void)volume;
}

static void *null_register(void *data, int len)
{
    (void)data;
    (void)len;
    return NULL;
}

static void null_handle(void *handle)
{
    (void)handle;
}

static void null_play(void *handle, boolean looping)
{
    (void)handle;
    (void)looping;
}

static boolean null_music_playing(void)
{
    return false;
}

music_module_t DG_music_module = {
    NULL, 0,
    null_music_init, null_void, null_volume, null_void, null_void,
    null_register, null_handle, null_play, null_void, null_music_playing,
    null_void,
};

/* Doom's system calls, as the device's (picocalc/dg_picocalc.c), less the
 * hardware. */

void I_Error(char *error, ...)
{
    char msg[512];
    va_list ap;
    va_start(ap, error);
    vsnprintf(msg, sizeof msg, error, ap);
    va_end(ap);
    fflush(stdout);
    fprintf(stderr, "%s\n", msg);

    /* -timedemo ends with I_Error("timed ..."): the demo played to its
     * end, which is success. */
    if (strncmp(msg, "timed ", 6) == 0) {
        printf("host: %s\n", msg);
        report("timedemo");
        exit(0);
    }
    printf("host: error %s\n", msg);
    report("error");
    exit(1);
}

void *I_Realloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (size != 0 && p == NULL) {
        I_Error("I_Realloc: failed on reallocation of %zu bytes", size);
    }
    return p;
}

/* The device's PSRAM allocator, which the patched w_wad.c takes the WAD
 * directory from (design.md §12.1): on the host, the C heap. */
void *psram_malloc(size_t size)
{
    return malloc(size);
}

void psram_free(void *ptr)
{
    free(ptr);
}

void I_Quit(void)
{
    report("quit");
    exit(0);
}

byte *I_ZoneBase(int *size)
{
    int mb = 3;     /* upstream's, the device's (§12.2) */
    int i = M_CheckParmWithArgs("-hostzone", 1);
    if (i) {
        mb = atoi(myargv[i + 1]);
    }
    int offset = 0;
    i = M_CheckParmWithArgs("-hostzoneoffset", 1);
    if (i) {
        offset = atoi(myargv[i + 1]) & ~15;
    }
    *size = mb * 1024 * 1024;
    zone_size = *size;
    byte *p = malloc((size_t)*size + (size_t)offset);
    if (p == NULL) {
        fprintf(stderr, "host: no %d MB for the zone\n", mb);
        exit(2);
    }
    p += offset;
    i = M_CheckParmWithArgs("-hostzonefill", 1);
    memset(p, i ? (int)strtol(myargv[i + 1], NULL, 0) : 0, (size_t)*size);
    return p;
}

void I_AtExit(void (*func)(void), boolean run_on_error)
{
    (void)func;
    (void)run_on_error;
}

void I_PrintBanner(char *msg)
{
    printf("%s\n", msg);
}

void I_PrintDivider(void)
{
    printf("------------------------------------------------\n");
}

void I_PrintStartupBanner(char *gamedescription)
{
    I_PrintDivider();
    printf("%s\n", gamedescription);
    I_PrintDivider();
}

boolean I_ConsoleStdout(void)
{
    return true;
}

void I_InitGraphics(void);
void I_InitTimer(void);

void I_Init(void)
{
    I_InitTimer();
    I_InitGraphics();
}

void I_InitJoystick(void)
{
}

void I_BindJoystickVariables(void)
{
}

void I_Tactile(int on, int off, int total)
{
    (void)on;
    (void)off;
    (void)total;
}

boolean I_GetMemoryValue(unsigned int offset, void *value, int size)
{
    (void)offset;
    (void)value;
    (void)size;
    return false;
}
