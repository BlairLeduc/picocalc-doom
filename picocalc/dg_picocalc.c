/*
 * picocalc-doom — the doomgeneric platform and Doom's I_* system calls
 * (design.md §5.3), in place of upstream's src/doomgeneric_rp2350.c, which
 * is not compiled.
 *
 * Upstream's behaviour less HDMI, PS/2 and USB. main() has started and
 * checked the PSRAM by the time DG_Init() runs (§11.3, M1). DG_Init()
 * sets up the frame pool (§7.2) and checks the card, with a page for each
 * failure (§13.1); DG_StartScreen() starts the IWAD, from the WAD list
 * when there is more than one (§13.1, D13); I_Error() shows its page and
 * starts again on a key. The at-exit list and Doom's Quit to the shell
 * (§13.3) replace the marked parts in M8.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Doom's headers first: doomtype.h declares boolean's false and true as
 * enumerators, which <stdbool.h>'s macros would turn into numbers. */
#include "doomgeneric.h"
#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"
#include "w_wad.h"

#include "pico/stdlib.h"

#include "core1.h"
#include "exit.h"
#include "ff.h"
#include "frank_doom_log.h"
#include "input.h"
#include "doomkeys.h"
#include "page.h"
#include "picocalc_shell.h"
#include "psram_allocator.h"
#include "video.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern void stdio_fatfs_init(void);

/* Doom arguments fixed at build time (§14.1), such as
 * "-iwad doom.wad -timedemo demo1": they come before anything else, so a
 * scripted run knows what it booted with. */
#ifndef PICOCALC_DOOM_ARGS
#define PICOCALC_DOOM_ARGS ""
#endif

/* The card's state must hold this long to count: the slot is
 * mechanical (hardware-notes.md §7.1). */
#define CARD_SETTLE_US 500000u

/* The volume Doom's files are on; upstream's name, which its FatFs glue
 * does not use, kept so a link against either sees one definition. */
FATFS fs;

/* A failure before Doom starts, other than the card's: logged, and a
 * page naming it, while core 1 keeps the heartbeat. Not panic(): with the
 * probe attached its breakpoint halts core 0, and the timer with it
 * (hardware-notes.md §2.7). */
static void stop(const char *why)
{
    g_core0_stack = core0_stack_used();
    FRANK_DOOM_LOG("picocalc-doom: %s; stopped\n", why);
    page_show_message(PAGE_STOPPED, why);
    for (;;) {
        tight_loop_contents();
    }
}

/* Wait until the card has been `in` (or out) for CARD_SETTLE_US. */
static void wait_for_card(bool in)
{
    uint32_t since = time_us_32();
    for (;;) {
        if (picocalc_shell_card_present() != in) {
            since = time_us_32();
        } else if (time_us_32() - since >= CARD_SETTLE_US) {
            return;
        }
        busy_wait_us_32(10000);
    }
}

/* Show the page, and start again once a card is in that was not before:
 * after an insertion for an empty slot, after the card comes out and
 * goes back in otherwise (§13.1). */
static void card_failed(page_t p)
{
    page_show(p);
    if (p != PAGE_NO_CARD) {
        wait_for_card(false);
    }
    wait_for_card(true);
    FRANK_DOOM_LOG("picocalc-doom: a card went in; starting again\n");
    picocalc_restart();
}

/* The card, with its timing and a page for each failure (§10.1, §13.1). */
static void check_card(void)
{
    if (!picocalc_shell_card_present()) {
        FRANK_DOOM_LOG("  card         : the slot is empty\n");
        card_failed(PAGE_NO_CARD);
    }
    uint32_t t0 = time_us_32();
    FRESULT r = f_mount(&fs, "", 1);
    uint32_t us = time_us_32() - t0;
    if (r != FR_OK) {
        FRANK_DOOM_LOG("  card         : did not mount (%d), after %lu ms\n", (int)r,
                       (unsigned long)(us / 1000u));
        card_failed(PAGE_CARD_FAILED);
    }
    FRANK_DOOM_LOG("  card         : mounted in %lu ms, %s\n", (unsigned long)(us / 1000u),
                   fs.fs_type == FS_EXFAT ? "exFAT" : fs.fs_type == FS_FAT32 ? "FAT32"
                   : "FAT12/16");
    FILINFO fi;
    if (f_stat("/doom", &fi) != FR_OK || !(fi.fattrib & AM_DIR)) {
        FRANK_DOOM_LOG("  card         : no /doom\n");
        card_failed(PAGE_NO_DOOM);
    }
    (void)us;
}

/* main()'s last call. Here, not in main.c: a file that includes Doom's
 * headers has printf compiled out in the quiet build (doomtype.h includes
 * upstream's frank_doom_quiet_stdio.h), and main.c's banner is printf. */
void picocalc_doom_run(void)
{
    static char *argv[] = { "doom", NULL };
    /* DG_StartScreen() replaces myargv with the IWAD's and the build's. */
    doomgeneric_Create(1, argv);
    for (;;) {
        doomgeneric_Tick();
    }
}

void DG_Init(void)
{
    /* The PSRAM was started and checked by main() (§11.3). */
    psram_set_sram_mode(0);

    /* The frame pool, in the PSRAM's scratch region (§7.2): its first
     * buffer is DG_ScreenBuffer. */
    video_init();

    check_card();
    f_chdir("/doom");
    stdio_fatfs_init();
}

/* The IWADs of Doom's own table (d_iwad.c) that this engine plays: Doom's
 * games, not Heretic's, Hexen's or Strife's (§1). In the WAD list's
 * order, each with its entry there (§13.1), at most PAGE_LIST_COLS. */
static const struct {
    const char *file;
    const char *entry;
} s_iwads[] = {
    { "doom.wad",      "DOOM (doom.wad)" },
    { "doom2.wad",     "DOOM II: Hell on Earth (doom2.wad)" },
    { "plutonia.wad",  "Final DOOM: Plutonia (plutonia.wad)" },
    { "tnt.wad",       "Final DOOM: TNT Evilution (tnt.wad)" },
    { "doom1.wad",     "DOOM shareware (doom1.wad)" },
    { "freedoom1.wad", "Freedoom: Phase 1 (freedoom1.wad)" },
    { "freedoom2.wad", "Freedoom: Phase 2 (freedoom2.wad)" },
    { "freedm.wad",    "FreeDM (freedm.wad)" },
    { "chex.wad",      "Chex Quest (chex.wad)" },
    { "hacx.wad",      "HACX (hacx.wad)" },
};
#define IWADS (sizeof s_iwads / sizeof s_iwads[0])
_Static_assert(IWADS <= PAGE_LIST_MAX, "every IWAD fits the list");

#define MAX_ARGS 32

/* Split s at spaces into args[n...], in place; returns the new n. */
static int split_args(char *s, char **args, int n)
{
    while (*s != '\0' && n < MAX_ARGS) {
        while (*s == ' ') {
            *s++ = '\0';
        }
        if (*s == '\0') {
            break;
        }
        args[n++] = s;
        while (*s != '\0' && *s != ' ') {
            s++;
        }
    }
    return n;
}

/* Core 0, before Doom: the WAD list (D13), until Enter chooses one of
 * the `n` found or Esc leaves (§13.3). Returns the index chosen. */
static unsigned choose_iwad(const unsigned *found, unsigned n)
{
    const char *entries[PAGE_LIST_MAX];
    for (unsigned i = 0; i < n; i++) {
        entries[i] = s_iwads[found[i]].entry;
    }
    unsigned sel = 0;
    const char *esc = picocalc_exit_label();
    page_show_list(entries, n, sel, esc);
    FRANK_DOOM_LOG("  iwad         : the WAD list, %u to choose from; %s\n", n,
                   esc ? esc : "Esc goes nowhere");
    for (;;) {
        int key = input_next_press();
        if (key < 0) {
            sleep_ms(5);
            continue;
        }
        if (key == KEY_UPARROW || key == KEY_DOWNARROW) {
            sel = (sel + (key == KEY_UPARROW ? n - 1u : 1u)) % n;
            page_list_select(sel);
        } else if (key == KEY_ENTER) {
            break;
        } else if (key == KEY_ESCAPE) {
            FRANK_DOOM_LOG("  iwad         : Esc in the WAD list\n");
            picocalc_exit();   /* returns only if it cannot leave */
        }
    }
    /* Enter's repeats are dropped and its release reaches Doom as an up,
     * which does nothing: the key map keeps it held until then. */
    page_show(PAGE_NONE);
    return found[sel];
}

/* Which IWAD to start (§13.1, D13): the build's -iwad if it names one;
 * the only one when the card has one; otherwise the one chosen from the
 * WAD list. M7 adds picocalc.ini's iwad. */
void DG_StartScreen(void)
{
    static char build_args[] = PICOCALC_DOOM_ARGS;
    static char *args[MAX_ARGS + 1];
    unsigned found[IWADS], n_found = 0;
    int n = 0;

    for (unsigned i = 0; i < IWADS; i++) {
        FILINFO fi;
        if (f_stat(s_iwads[i].file, &fi) == FR_OK && !(fi.fattrib & AM_DIR)) {
            FRANK_DOOM_LOG("  iwad         : %s, %lu bytes\n", s_iwads[i].file,
                           (unsigned long)fi.fsize);
            found[n_found++] = i;
        }
    }
    if (n_found == 0) {
        FRANK_DOOM_LOG("  iwad         : none in /doom\n");
        card_failed(PAGE_NO_IWAD);
    }

    args[n++] = "doom";
    int build_first = n;
    n = split_args(build_args, args, n);
    boolean build_iwad = false;
    for (int i = build_first; i < n; i++) {
        if (strcmp(args[i], "-iwad") == 0) {
            build_iwad = true;
        }
    }
    if (!build_iwad && n + 2 <= MAX_ARGS) {
        unsigned chosen = n_found == 1 ? found[0] : choose_iwad(found, n_found);
        args[n++] = "-iwad";
        args[n++] = (char *)s_iwads[chosen].file;
        FRANK_DOOM_LOG("  iwad         : starting %s, %s\n", s_iwads[chosen].file,
                       n_found == 1 ? "the only one" : "chosen from the list");
    }
    args[n] = NULL;

    myargc = n;
    myargv = args;
    for (int i = 1; i < n; i++) {
        FRANK_DOOM_LOG("%s%s", i == 1 ? "  arguments    : " : " ", args[i]);
    }
    FRANK_DOOM_LOG("\n");
}

/* W_AddFile() wrapped at link time (--wrap): each WAD's open and
 * directory read, timed, and the directory's size (§12.1, M3). */
wad_file_t *__real_W_AddFile(char *filename);

wad_file_t *__wrap_W_AddFile(char *filename)
{
    unsigned int before = numlumps;
    uint32_t t0 = time_us_32();
    wad_file_t *w = __real_W_AddFile(filename);
    uint32_t us = time_us_32() - t0;
    FRANK_DOOM_LOG("picocalc-doom: W_AddFile(%s): %u lumps in %lu ms; the directory %u lumps, "
                   "%lu bytes\n", filename, numlumps - before, (unsigned long)(us / 1000u),
                   numlumps, (unsigned long)numlumps * sizeof(lumpinfo_t));
    (void)before;
    (void)us;
    return w;
}

/* Core 0's idle time, for the heartbeat's busy share (§14.3). */
void DG_SleepMs(uint32_t ms)
{
    uint32_t t0 = time_us_32();
    sleep_ms(ms);
    g_core0_sleep_us += time_us_32() - t0;
    g_core0_stack = core0_stack_used();
}

uint32_t DG_GetTicksMs(void)
{
    return to_ms_since_boot(get_absolute_time());
}

void DG_SetWindowTitle(const char *title)
{
    (void)title;
}

/* Doom's system calls. */

void I_Error(char *error, ...)
{
    /* The page names it, and a key starts again, through the restart
     * that parks core 1 (§13.1). A -timedemo run ends here too, with its
     * result. */
    static char message[PAGE_MESSAGE_MAX];
    g_core0_stack = core0_stack_used();
    va_list ap;
    va_start(ap, error);
    vsnprintf(message, sizeof message, error, ap);
    va_end(ap);
    video_hash_flush();
    FRANK_DOOM_LOG("I_Error: %s\n", message);
    page_show_message(PAGE_DOOM_ERROR, message);
    /* Keys queued when Doom stopped are not the answer, and a key held
     * then repeats as a repeat, not a press: only a press from here on. */
    while (input_next_press() >= 0) {
    }
    while (input_next_press() < 0) {
        sleep_ms(5);
    }
    FRANK_DOOM_LOG("picocalc-doom: a key on the error page; starting again\n");
    picocalc_restart();
}

void *I_Realloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (size != 0 && p == NULL) {
        I_Error("I_Realloc: failed on reallocation of %zu bytes", size);
    }
    return p;
}

void I_Quit(void)
{
    /* M8 runs the at-exit list and leaves to the shell or powers off
     * (§13.3). Upstream rebooted; M0 stops, since a reset with a
     * southbridge transaction in flight is fatal to the keyboard once
     * core 1 polls it (§6.2). */
    for (;;) {
        tight_loop_contents();
    }
}

byte *I_ZoneBase(int *size)
{
    /* Upstream's: 3 MB of PSRAM, or 2 MB if that fails (§12.2). */
    *size = 3 * 1024 * 1024;
    void *p = psram_malloc(*size);
    if (!p) {
        *size = 2 * 1024 * 1024;
        p = psram_malloc(*size);
    }
    if (!p) {
        stop("There is no room in the PSRAM for Doom's zone.");
    }
    return p;
}

void I_AtExit(void (*func)(void), boolean run_on_error)
{
    /* M8 keeps these and I_Quit() runs them (§13.3). */
    (void)func;
    (void)run_on_error;
}

void I_PrintBanner(char *msg)
{
    FRANK_DOOM_LOG("%s\n", msg);
    (void)msg;  /* unused when FRANK_DOOM_QUIET compiles the log out */
}

void I_PrintDivider(void)
{
    FRANK_DOOM_LOG("------------------------------------------------\n");
}

void I_PrintStartupBanner(char *gamedescription)
{
    I_PrintDivider();
    FRANK_DOOM_LOG("%s\n", gamedescription);
    (void)gamedescription;
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
