//==============================================================================
// PROJECT:         zqloader
// FILE:            tzx_types.h
// DESCRIPTION:     Types for tzx.
//                  http://k1.spdns.de/Develop/Projects/zasm/Info/TZX%20format.html
//                  https://worldofspectrum.net/TZXformat.html
// 
// Copyright (c) 2026 Daan Scherft [Oxidaan]
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
    friend std::ostream &operator << (std::ostream &p_stream, const TurboSpeedDataBlock &p_block) 
    {
        p_stream << "Data length = " << p_block.length[0] + 256 * p_block.length[1] + 65536 * p_block.length[2]
                 // << " pause = " << p_block.pause
                 << " length_of_pilot_pulse = " << p_block.length_of_pilot_pulse
                 << " length_of_pilot_tone = " << p_block.length_of_pilot_tone
                 << " length_of_sync_first_pulse = " << p_block.length_of_sync_first_pulse
                 << " length_of_sync_second_pulse = " << p_block.length_of_sync_second_pulse
                 << " length_of_zero_bit_pulse = " << p_block.length_of_zero_bit_pulse
                 << " length_of_one_bit_pulse = " << p_block.length_of_one_bit_pulse;
        return p_stream; 
    }
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
    BYTE    asp;            // Number of pilot/sync symbols in the alphabet table (0=256) (= #pulse-symdef-s), best have power of 2.

    DWORD   totd;           // Total number of symbols in data stream (can be 0)    (when asd==2 this is # bits in data)
    BYTE    npd;            // Maximum number of pulses per data symbol             (= #pulse_lengths at data-symdef)
    BYTE    asd;            // Number of data symbols in the alphabet table (0=256) (= #data-symdef-s) typical 2 when data is bits
                            // almost always 2. Must be a power of 2.

    struct SymDef           // Pilot / sync / data symbols ('alphabet') definition table #=asp
    {
        Edge   m_edge;      // Exactly same as our Edge enum type eg 0 is toggle
        // WORD pulse_lengths[];    // Size of this is #npp for pulse-symdef; #npd for data-symdef (written 'manually')
    };

    static_assert(sizeof(SymDef) == sizeof(BYTE), "Sizeof SymDef must be 1");

    // Pilot and sync data stream, # = totp
    struct Prle
    {
        BYTE   m_symbol;            // ref to pulse-symdef number so 0 - asp-1 (basicly: what pulse to use)
        WORD   m_repetitions;       // How often is the above pulse repeated
    };
    static_assert(sizeof(Prle) ==3, "Sizeof SymDef must be 3");

    // Get 'Block length' that is total length without 'without these four bytes'
    // Used when writing
    size_t GetBlockLength(size_t p_size_of_data) const
    {
        return sizeof(GeneralizedDataBlock) - sizeof(block_length) + GetRemainingLength(p_size_of_data);
    }

    int GetNB() const
    {
        // eg 2-> 1; 4->2 etc
        return static_cast<int>(std::ceil(std::log2(asd == 0 ? 256 : asd)));
    }

    // Get Data length calculated from read block
    // used when reading
    size_t GetDataLength() const
    {
        if(block_length == 0)
        {
            throw std::runtime_error("Need to read or set block_length first");
        }
        auto nb = static_cast<int>(std::ceil(std::log2(asd == 0 ? 256 : asd)));
        return static_cast<size_t>((nb * totd) / 8);
        //return block_length + sizeof(block_length) - sizeof(GeneralizedDataBlock) - GetRemainingLength();
    }

    // Get size of SYMDEF[ASP] + PRLE[TOTP] + SYMDEF[ASD] + p_size_of_data
    size_t GetRemainingLength(size_t p_size_of_data = 0) const
    {   
        
        auto sizeof_pilot =  totp > 0 ? (asp == 0 ? 256 : int(asp)) * (sizeof(SymDef) + sizeof(WORD) * npp) + totp * sizeof(Prle) 
                                    :0;
        auto sizeof_data  =  totd > 0 ? (asd == 0 ? 256 : int(asd)) * (sizeof(SymDef) + sizeof(WORD) * npd) + p_size_of_data
                                    :0;
        return sizeof_pilot + sizeof_data;


    }
    friend std::ostream &operator << (std::ostream &p_stream, const GeneralizedDataBlock &p_block) 
    {
        p_stream << "block_length = " << p_block.block_length
               //  << " pause = " << p_block.pause
                 << " totp = "  << p_block.totp 
                 << " npp = "   << int(p_block.npp)
                 << " asp = "   << int(p_block.asp) << (int(p_block.asp) == 0 ? "(=256)" : "")
                 << " totd = "  << p_block.totd 
                 << " npd = "   << int(p_block.npd)
                 << " asd = "   << int(p_block.asd) << (int(p_block.asd) == 0 ? "(=256)" : "");
        return p_stream; 
    }

};
static_assert(sizeof(TurboSpeedDataBlock) == 0x12);     // until and including ASD
#pragma pack(pop)

