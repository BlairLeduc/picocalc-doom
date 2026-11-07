/* hardware/structs/qmi.h on the host: see hardware/gpio.h here.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef PICOCALC_DOOM_FAKE_QMI_H
#define PICOCALC_DOOM_FAKE_QMI_H

#include <stdint.h>

typedef struct {
    uint32_t atrans[8];
} qmi_hw_t;

extern qmi_hw_t fake_qmi;
#define qmi_hw (&fake_qmi)
#define QMI_ATRANS0_BASE_BITS 0x00000fffu

#endif
