//==============================================================================
// PROJECT:         zqloader
// FILE:            spectrum_consts.h
// DESCRIPTION:     Some ZX spectrum consts
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once

#include <cstdint>      // uint16_t
#include "types.h"      // Doublesec

namespace spectrum
{

static const uint16_t SCREEN_START = 16384;
static const uint16_t ROM_LENGTH = 16384;
static const uint16_t SCREEN_23RD = SCREEN_START + 4 * 1024;
static const uint16_t SCREEN_END = SCREEN_START + 6 * 1024;
static const uint16_t ATTR_BEGIN = SCREEN_END;
static const uint16_t ATTR_23RD = ATTR_BEGIN + 512;
static const uint16_t SCREEN_SIZE = 6 * 1024 + 768;

static const uint16_t PROG = 23755;      // *usual* start of basic. s/a PRINT PEEK(23635) + 256 * PEEK(23636)


static const int g_spectrum_clock = 3500000;
static const Doublesec g_tstate_dur = 1s / double(g_spectrum_clock); // Tstate duration as std::duration in seconds


// https://worldofspectrum.org/faq/reference/48kreference.htm
static const int g_tstate_leader = 2168;
static const int g_tstate_sync1 = 667;
static const int g_tstate_sync2 = 735;
static const int g_tstate_zero = 855;
static const int g_tstate_one = 1710;
// To speed up even ROM loading a bit:
static const int g_tstate_quick_zero = 700; // note 'quick_one' is taken as 2x this so 1400


}


