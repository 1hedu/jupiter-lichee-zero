/* Compatibility shim — minimal libvgm snddef.h used by vendored Nuked-OPN2.
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LICHEEEMU_NUKED_OPN2_SNDDEF_H
#define LICHEEEMU_NUKED_OPN2_SNDDEF_H

#include "../stdtype.h"

typedef INT32 DEV_SMPL;

typedef struct _device_data {
    void *chipInf;
} DEV_DATA;

#endif
