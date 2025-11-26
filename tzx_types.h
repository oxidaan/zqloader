//==============================================================================
// PROJECT:         zqloader
// FILE:            tzx_types.h
// DESCRIPTION:     Types for tzx.
//                  http://k1.spdns.de/Develop/Projects/zasm/Info/TZX%20format.html
//                  https://worldofspectrum.net/TZXformat.html
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once


#include "types.h"      // Edge



using BYTE   = uint8_t;
using WORD   = uint16_t;
using DWORD  = uint32_t;

/// TZX block types
enum class TzxBlockType : BYTE
{
    StandardSpeedDataBlock    = 0x10,
    TurboSpeedDataBlock       = 0x11,
    Puretone                  = 0x12,
    PulseSequence             = 0x13,
    PureDataBlock             = 0x14,
    DirectRecordingBlock      = 0x15,
    CSWRecordingBlock         = 0x18,
    GeneralizedDataBlock      = 0x19,
    PauseOrStopthetapecommand = 0x20,
    GroupStart                = 0x21,
    GroupEnd                  = 0x22,
    Jumptoblock               = 0x23,
    Loopstart                 = 0x24,
    Loopend                   = 0x25,
    Callsequence              = 0x26,
    Returnfromsequence        = 0x27,
    Selectblock               = 0x28,
    Stopthetapeifin48Kmode    = 0x2A,
    Setsignallevel            = 0x2B,
    Textdescription           = 0x30,
    Messageblock              = 0x31,
    Archiveinfo               = 0x32,
    Hardwaretype              = 0x33,
    Custominfoblock           = 0x35,
    Glueblock                 = 0x5A
};

std::ostream& operator << (std::ostream& p_stream, TzxBlockType p_enum);



#pragma pack(push, 1)
struct TurboSpeedDataBlock
{
    WORD length_of_pilot_pulse;
    WORD length_of_sync_first_pulse;
    WORD length_of_sync_second_pulse;
    WORD length_of_zero_bit_pulse;
    WORD length_of_one_bit_pulse;
    WORD length_of_pilot_tone;
    BYTE used_bits_last_byte;
    WORD pause;
    BYTE length[3];
};
static_assert(sizeof (TurboSpeedDataBlock) == 0x12);
#pragma pack(pop)



// http://k1.spdns.de/Develop/Projects/zasm/Info/TZX%20format.html
// https://worldofspectrum.net/TZXformat.html
#pragma pack(push, 1)
struct GeneralizedDataBlock
{
    DWORD   block_length;   // Block length (without these four bytes)
    WORD    pause;

    DWORD   totp;           // Total number of symbols in pilot / sync block(can be 0) (eg 2 for a leader, then a sync)
    BYTE    npp;            // Maximum number of pulses per pilot/sync symbol (= #pulse_lengths at pulse-symdef)
    BYTE    asp;            // Total number of pilot/sync symbols in the alphabet table (0=256) (= #pulse-symdef-s)

    DWORD   totd;           // Total number of symbols in data stream (can be 0)    (when asd==2 this is # bits in data)
    BYTE    npd;            // Maximum number of pulses per data symbol             (= #pulse_lengths at data-symdef)
    BYTE    asd;            // Number of data symbols in the alphabet table (0=256) (= #data-symdef-s) typical 2 when data is bits
                            // almost always 2. Must be a power of 2.

    struct SymDef           // Pilot / sync / data symbols ('alphabet') definition table #=asp
    {
        Edge   m_edge;      // exactly same as our Edge enum type eg 0 is toggle
        // WORD pulse_lengths[];    // Size of this is #npp for pulse-symdef; #npd for data-symdef (written 'manually')
    };

    static_assert(sizeof(SymDef) == sizeof(BYTE), "Sizeof SymDef must be 1");

    // Pilot and sync data stream, # = totp
    struct Prle
    {
        BYTE   m_symbol;            // ref to pulse-symdef number so 0 - asp-1 (basicly: what pulse to use)
        WORD   m_repetitions;       // How often is the above pulse repeated
    };

    size_t GetBlockLength(size_t p_size_of_data) const
    {
        return sizeof(GeneralizedDataBlock) - sizeof(block_length) + GetRemainingLength(p_size_of_data);
    }


    size_t GetDataLength() const
    {
        if(block_length == 0)
        {
            throw std::runtime_error("Need to read or set block_length first");
        }
        //     length+data-4 + 4                    -  length w/o data
        return block_length + sizeof(block_length) - sizeof(GeneralizedDataBlock) - GetRemainingLength();
    }


    size_t GetRemainingLength(size_t p_size_of_data = 0) const
    {   
        
        auto sizeof_leader =  totp > 0 ? (asp == 0 ? 256 : int(asp)) * (sizeof(SymDef) + sizeof(WORD) * npp) +
                                    totp *(sizeof(BYTE) + sizeof(WORD)) 
                                    :0;
        auto sizeof_data   =  totd > 0 ? (asd == 0 ? 256 : int(asd)) * (sizeof(BYTE) + sizeof(WORD) * npd) +
                                    p_size_of_data 
                                    :0;
        return sizeof_leader + sizeof_data;


    }
    std::ostream &operator << (std::ostream &p_stream) const
    {
        p_stream << "block_length = " << block_length
                 << "pause = " << pause
                 << "totp = "  << totp 
                 << "npp = "   << npp 
                 << "asp = "   << asp 
                 << "totd = "  << totd 
                 << "npd = "   << npd 
                 << "asd = "   << asd << std::endl;
        return p_stream; 
    }

};
#pragma pack(pop)

