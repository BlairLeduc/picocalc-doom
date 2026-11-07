/* audio_shim.h — the PWM audio's counters, for the heartbeat (design.md
 * §8.3, §14.3). The output itself is upstream's i2s_* API (src/pico's
 * audio.h), implemented in audio_shim.c; this header is named apart.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_AUDIO_SHIM_H
#define PICOCALC_DOOM_AUDIO_SHIM_H

#include <stdint.h>

/* Kept on core 0, by the refill interrupt and by I_UpdateSound(); read
 * anywhere, a word at a time. Each word has one writer. */
typedef struct {
    volatile uint32_t halves;       /* halves refilled                          */
    volatile uint32_t late;         /* refills that found the deadline missed   */
    volatile uint32_t irq_us;       /* time in the refill interrupt, since boot */
    volatile uint32_t irq_max_us;   /* its longest run                          */
    volatile uint32_t played;       /* mix slots taken from upstream's ring     */
    volatile uint32_t underruns;    /* slots its ring was empty for, after the
                                       first was played (hardware-notes.md §5.8) */
    volatile uint32_t startup;      /* slots it was empty for before that       */
    volatile uint32_t music_late;   /* halves of silence because core 1 had not
                                       finished the next slot's music (§8.3)    */
    volatile uint32_t updates;      /* I_UpdateSound() calls                    */
    volatile uint32_t mix_us;       /* in I_UpdateSound(), less the interrupt's
                                       time inside it: mixing and the OPL       */
    volatile uint32_t mix_max_us;   /* its longest call                         */
    volatile uint32_t peak;         /* the largest |sample| played in the last
                                       heartbeat's second, of 32,768           */
    volatile uint32_t peak_seen;    /* the interrupt's copy of peak_ask        */
    volatile uint32_t pushes;       /* the push API's calls, which upstream's
                                       sound module never makes (§8.2)          */
} audio_stats_t;

extern audio_stats_t g_audio;

/* The heartbeat's request for a new peak: written by core 1 only, read
 * by the interrupt, which starts peak again when it changes. */
extern volatile uint32_t g_audio_peak_ask;

/* Frames in one of upstream's mix slots, as its sound module set them;
 * 0 before I_InitSound(). */
uint32_t audio_slot_frames(void);

#endif /* PICOCALC_DOOM_AUDIO_SHIM_H */
