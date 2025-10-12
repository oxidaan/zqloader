// ==============================================================================
// PROJECT:         zqloader
// FILE:            loader_defaults.h
// DESCRIPTION:     Defaults for ZQ loader
//
// Copyright (c) 2024 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once

namespace loader_defaults
{
// T-states
constexpr int wait_for_edge_loop_duration  = 43;    // Depends only on z80 code. See zqloader.z80asm
constexpr int bit_loop_duration            = 91;    // Depends only on z80 code. see zqloader.z80asm
constexpr int byte_loop_loop_duration      = 155;   // Depends only on z80 code. see zqloader.z80asm

constexpr int wanted_zero_cyclii           = 0;
constexpr int wanted_one_cyclii            = 4;     // must be > zero_max

constexpr int bit_loop_max                 = 12;  // max #INs before timeout. when 0 dont set (so keep defaults at zqloader.z80asm) (old: 12)
constexpr int zero_max                     = 3;   // max #INs to see a zero.  when 0 dont set (so keep defaults at zqloader.z80asm) (old: 4->3) aka ZERO_MAX

constexpr int zero_duration                = bit_loop_duration + wanted_zero_cyclii * wait_for_edge_loop_duration;  // bit_loop ; 91;  // @@ see zqloader.asm (old: 118)
constexpr int one_duration                 = bit_loop_duration + ((zero_max + wanted_one_cyclii) * wait_for_edge_loop_duration)/2;// 241; //int(91 + 3.5 * 43); //250;  231 worked better with jsw3.z80!? (old: 293)
constexpr int end_of_byte_delay            = byte_loop_loop_duration - bit_loop_duration;  // 64;



constexpr int volume_left                  = 100;
constexpr int volume_right                 = 100;
constexpr int sample_rate                  = 0;                 // 0 is device default (eg 48000hz)

constexpr CompressionType compression_type = CompressionType::automatic;

inline int CalcZeroDuration(int p_wanted_zero_cyclii)
{
    return bit_loop_duration + p_wanted_zero_cyclii * wait_for_edge_loop_duration;
}
inline int CalcOneDuration(int p_zero_max, int p_wanted_one_cyclii)
{
    // average p_zero_max and p_wanted_one_cyclii
    // p_wanted_one_cyclii > p_zero_max
    return bit_loop_duration + ((p_zero_max + p_wanted_one_cyclii) * wait_for_edge_loop_duration) / 2;// 241; //int(91 + 3.5 * 43); //250;  231 worked better with jsw3.z80!? (old: 293)
}

}