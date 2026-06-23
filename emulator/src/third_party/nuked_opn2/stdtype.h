/* Compatibility shim — provides libvgm's (U)INTxx typedefs from <stdint.h>
 * so vendored Nuked-OPN2 (libvgm flavor) compiles unchanged inside QEMU.
 * Part of licheeEmu. SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LICHEEEMU_NUKED_OPN2_STDTYPE_H
#define LICHEEEMU_NUKED_OPN2_STDTYPE_H

#include <stdint.h>

typedef uint8_t  UINT8;
typedef  int8_t   INT8;
typedef uint16_t UINT16;
typedef  int16_t  INT16;
typedef uint32_t UINT32;
typedef  int32_t  INT32;
typedef uint64_t UINT64;
typedef  int64_t  INT64;

#endif
