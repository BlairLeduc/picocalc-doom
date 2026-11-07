/* exit.c — leaving (design.md §13.3, D14).
 *
 * Nothing may be left half-written on the card, since neither way out
 * waits for it: FatFs is unmounted first, which writes back what it
 * holds of the FAT. Before the reboot to the shell, core 1 parks between
 * southbridge transactions (§6.2). From picocalc-quest (D8).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "exit.h"

#include <stdio.h>

#include "hardware/watchdog.h"
#include "pico/stdlib.h"

#include "core1.h"
#include "ff.h"
#include "page.h"
#include "picocalc_shell.h"

/* The southbridge's least delay (hardware-notes.md §6). */
#define POWER_OFF_S 6u

/* How long "Back to the shell." stays up (§13.3). */
#define TO_SHELL_US 2000000u

typedef enum { EXIT_NONE, EXIT_SHELL, EXIT_POWER_OFF } exit_to_t;

static exit_to_t exit_to(void) {
    if (picocalc_shell_loader_present() && picocalc_shell_card_present()) return EXIT_SHELL;
    if (g_core1.sb_power_off) return EXIT_POWER_OFF;
    return EXIT_NONE;
}

const char *picocalc_exit_label(void) {
    switch (exit_to()) {
    case EXIT_SHELL:     return "Esc: back to the shell";
    case EXIT_POWER_OFF: return "Esc: switch off";
    default:             return NULL;
    }
}

static void unmount(void) {
    FRESULT r = f_unmount("");
    if (r != FR_OK) printf("exit: unmounting the card gave %d\n", (int)r);
}

void picocalc_exit(void) {
    uint32_t t0 = time_us_32();
    switch (exit_to()) {
    case EXIT_NONE:
        printf("exit: no shell to go back to, and the southbridge cannot power off\n");
        return;

    case EXIT_POWER_OFF:
        /* The southbridge first: if it refuses, the card is still
         * mounted and the page carries on. */
        if (!core1_power_off(POWER_OFF_S)) {
            printf("exit: the southbridge did not take the power off\n");
            return;
        }
        unmount();
        page_show(PAGE_POWER_OFF);
        printf("exit: power off in %u s, asked for %lu ms after the key\n", POWER_OFF_S,
               (unsigned long)((time_us_32() - t0) / 1000u));
        for (;;) tight_loop_contents();

    case EXIT_SHELL:
        unmount();
        page_show(PAGE_TO_SHELL);
        printf("exit: to the shell, %lu ms after the key\n",
               (unsigned long)((time_us_32() - t0) / 1000u));
        /* Time to read the page; the log's last line goes out meanwhile. */
        busy_wait_us_32(TO_SHELL_US);
        core1_park();
        picocalc_shell_exit();
        /* Only if the card came out in the last moment: there is no shell
         * to load, so start again, which shows the no-card page. */
        watchdog_reboot(0, 0, 0);
        for (;;) tight_loop_contents();
    }
}
