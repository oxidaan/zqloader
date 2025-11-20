// ==============================================================================
// PROJECT:         zqloader
// FILE:            tzxloader.cpp
// DESCRIPTION:     Implementation of class TzxLoader.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

//#define DEBUG_TZX

#include "tzxloader.h"
#include "taploader.h"
#include "loadbinary.h"
#include "datablock.h"
#include "pulsers.h"
#include <fstream>
#include <filesystem>
#include <sstream>
namespace fs = std::filesystem;



inline void DebugLog(const char *p_str)
{
    (void)p_str;
#ifdef DEBUG_TZX
    std::cout << p_str;
#endif
}
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


TzxLoader& TzxLoader::Load(const fs::path &p_filename, const std::string &p_zxfilename)
{
    std::ifstream fileread(p_filename, std::ios::binary);
    if (!fileread)
    {
        throw std::runtime_error("File " + p_filename.string() + " not found.");
    }
    try
    {
        std::cout << "Loading file " << p_filename << std::endl;
        Read(fileread, p_zxfilename);
        //   MoveToLoader(p_loader);
    }
    catch(const std::exception &e)
    {
        // clarify
        throw std::runtime_error("Reading file: " + p_filename.string() + ": " + e.what());
    }
    return *this;
}



// http://k1.spdns.de/Develop/Projects/zasm/Info/TZX%20format.html
// https://worldofspectrum.net/TZXformat.html
TzxLoader& TzxLoader::Read(std::istream& p_stream, const std::string &p_zxfilename)
{
    std::string s = LoadBinary<std::string>(p_stream, 7);

    if (s != "ZXTape!")
    {
        throw std::runtime_error("Not a tzx file");
    }
    auto eof_marker = LoadBinary<std::byte>(p_stream);
    auto version1   = LoadBinary<char>(p_stream);
    auto version2   = LoadBinary<char>(p_stream);
    std::cout << "TZX file version: " << int(version1) << '.' << int(version2) << std::endl;
    bool done       = false;
    while (p_stream.peek() && p_stream.good() && !done)
    {
        TzxBlockType id = LoadBinary<TzxBlockType>(p_stream);
        if (std::byte(id) == eof_marker)
        {
            break;
        }
        std::cout << id;
        switch (id)
        {
        case TzxBlockType::StandardSpeedDataBlock:
        {
            p_stream.ignore(0x2);
            auto len = LoadBinary<WORD>(p_stream);
            std::cout << " length = " << len << std::endl;
            done = HandleTapBlock(p_stream, p_zxfilename, len);
            break;
        }
        case TzxBlockType::TurboSpeedDataBlock:
        {
            p_stream.ignore(0x0f );
            auto len1 = LoadBinary<BYTE>(p_stream);     // bit of weird 24 bit length
            auto len2 = LoadBinary<BYTE>(p_stream);
            auto len3 = LoadBinary<BYTE>(p_stream);
            auto len  = 0x10000 * len3 + 0x100 * len2 + len1;
            std::cout << " length = " << len << std::endl;
            done = HandleTapBlock(p_stream, p_zxfilename, len);
            break;
        }
        case TzxBlockType::Puretone:
        {
            auto len1 = LoadBinary<WORD>(p_stream);
            auto len2 = LoadBinary<WORD>(p_stream);
            (void)len2;
            std::cout << ' ' << int(len1) << " T states" << std::endl;
            break;
        }
        case TzxBlockType::PulseSequence:
        {
            auto len = LoadBinary<BYTE>(p_stream);
            std::cout << ' ' << int(len) << " pulses" << std::endl;
            p_stream.ignore(2 * len);
            break;
        }
        case TzxBlockType::PureDataBlock:
        {
            p_stream.ignore(0x07);
            int len1 = LoadBinary<BYTE>(p_stream);     // bit of weird 24 bit length
            int len2 = LoadBinary<BYTE>(p_stream);
            int len3 = LoadBinary<BYTE>(p_stream);
            int len  = 0x10000 * len3 + 0x100 * len2 + len1;
            std::cout << " length = " << len << std::endl;
            done = HandleTapBlock(p_stream, p_zxfilename, len);
            break;
        }
        case TzxBlockType::DirectRecordingBlock:
        {
            p_stream.ignore(5);
            auto len1 = LoadBinary<BYTE>(p_stream);     // bit of weird 24 bit length
            auto len2 = LoadBinary<BYTE>(p_stream);
            auto len3 = LoadBinary<BYTE>(p_stream);
            auto len  = 0x10000 * len3 + 0x100 * len2 + len1;
            std::cout << " length = " << len << std::endl;;
            p_stream.ignore(len);
            break;
        }
        case TzxBlockType::CSWRecordingBlock:
        {
            auto len = LoadBinary<DWORD>(p_stream);
            p_stream.ignore(len);
            break;
        }
        case TzxBlockType::GeneralizedDataBlock:
        {
            GeneralizedDataBlock generalized_data_block = LoadBinary<GeneralizedDataBlock>(p_stream);
            auto remaining_len                          = generalized_data_block.GetRemainingLength();
            p_stream.ignore(remaining_len);
            int len                                     = int(generalized_data_block.GetDataLength());
            std::cout << " length = " << len << std::endl;
            if(len)
            {
                HandleTapBlock(p_stream, p_zxfilename, len);
            }
            break;
        }
        case TzxBlockType::PauseOrStopthetapecommand:
        {
            auto dura = LoadBinary<WORD>(p_stream);
            if(dura == 0)
            {
                std::cout << ": Stop the tape" << std::endl;
            }
            else
            {
                std::cout << ": " << dura << "ms" << std::endl;
            }
            break;
        }
        case TzxBlockType::GroupStart:
        {
            auto len = LoadBinary<BYTE>(p_stream);
            auto s2  = LoadBinary<std::string>(p_stream, len);
            std::cout << ": " << s2  << std::endl;
            break;
        }
        case TzxBlockType::GroupEnd:
            break;
        case TzxBlockType::Jumptoblock:
            p_stream.ignore(2);
            break;
        case TzxBlockType::Loopstart:
            p_stream.ignore(2);
            break;
        case TzxBlockType::Loopend:
            break;
        case TzxBlockType::Callsequence:
        {
            auto len = LoadBinary<WORD>(p_stream);
            p_stream.ignore(len * 2);
            break;
        }
        case TzxBlockType::Returnfromsequence:
        {
            break;
        }
        case TzxBlockType::Selectblock:
        {
            auto len = LoadBinary<WORD>(p_stream);
            p_stream.ignore(len * 2);
            break;
        }
        case TzxBlockType::Stopthetapeifin48Kmode:
            p_stream.ignore(4);
            break;
        case TzxBlockType::Setsignallevel:
            p_stream.ignore(5);
            break;
        case TzxBlockType::Messageblock:
            p_stream.ignore(1);
           // no break
        case TzxBlockType::Textdescription:
        {
            auto len = LoadBinary<BYTE>(p_stream);
            auto txt = LoadBinary<std::string>(p_stream, len);
            std::cout << ": " << txt << std::endl;
            break;
        }
        case TzxBlockType::Archiveinfo:
        {
            auto len = LoadBinary<WORD>(p_stream);
            std::cout << " length = " << len << std::endl;
            p_stream.ignore(len);
            break;
        }
        case TzxBlockType::Hardwaretype:
        {
            auto len = LoadBinary<BYTE>(p_stream);
            p_stream.ignore(len * 3);
            break;
        }
        case TzxBlockType::Custominfoblock:
        {
            p_stream.ignore(0x10);
            auto len = LoadBinary<WORD>(p_stream);
            p_stream.ignore(len);
            break;
        }
        case TzxBlockType::Glueblock:
        {
            p_stream.ignore(9);
            break;
        }
        default:
            std::cout << "TODO: TZX block: " << id << std::endl;       // TODO!

        }
        //std::cout << std::endl;
    }
    std::cout << std::endl;
    return *this;
}



bool TzxLoader::HandleTapBlock(std::istream& p_stream, std::string p_zxfilename, int p_length)
{
    auto block = TapLoader::LoadTapBlock(p_stream, p_length);
    if (m_OnHandleTapBlock)
    {
        return m_OnHandleTapBlock(std::move(block), std::move(p_zxfilename));
    }
    return false;
}



/// Make conversion from enum tags to string somewhat simpler
#define ENUM_TAG(prefix, tag) case prefix::tag: \
            p_stream << #tag; break;

// Write type as text (debugging)
std::ostream& operator << (std::ostream& p_stream, TzxBlockType p_enum)
{
    switch (p_enum)
    {
    ENUM_TAG(TzxBlockType, StandardSpeedDataBlock);
    ENUM_TAG(TzxBlockType, TurboSpeedDataBlock);
    ENUM_TAG(TzxBlockType, Puretone);
    ENUM_TAG(TzxBlockType, PulseSequence);
    ENUM_TAG(TzxBlockType, PureDataBlock);
    ENUM_TAG(TzxBlockType, DirectRecordingBlock);
    ENUM_TAG(TzxBlockType, CSWRecordingBlock);
    ENUM_TAG(TzxBlockType, GeneralizedDataBlock);
    ENUM_TAG(TzxBlockType, PauseOrStopthetapecommand);
    ENUM_TAG(TzxBlockType, GroupStart);
    ENUM_TAG(TzxBlockType, GroupEnd);
    ENUM_TAG(TzxBlockType, Jumptoblock);
    ENUM_TAG(TzxBlockType, Loopstart);
    ENUM_TAG(TzxBlockType, Loopend);
    ENUM_TAG(TzxBlockType, Callsequence);
    ENUM_TAG(TzxBlockType, Returnfromsequence);
    ENUM_TAG(TzxBlockType, Selectblock);
    ENUM_TAG(TzxBlockType, Stopthetapeifin48Kmode);
    ENUM_TAG(TzxBlockType, Setsignallevel);
    ENUM_TAG(TzxBlockType, Textdescription);
    ENUM_TAG(TzxBlockType, Messageblock);
    ENUM_TAG(TzxBlockType, Archiveinfo);
    ENUM_TAG(TzxBlockType, Hardwaretype);
    ENUM_TAG(TzxBlockType, Custominfoblock);
    ENUM_TAG(TzxBlockType, Glueblock);
    default:
        p_stream << "Unknown: " << int(p_enum);

    }
    return p_stream;
}



void TonePulser::WriteAsTzxBlock(std::ostream &p_stream) const
{
    if(m_pattern.size() == 1)
    {
        std::cout << "Tone: Puretone patt=" << m_pattern[0] << " #pulses=" << m_max_pulses << " Dur=" << ((m_max_pulses * m_pattern[0]) * m_tstate_dur).count() << "sec" << std::endl;
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::Puretone);
        std::cout << "  ";
        WriteBinary<WORD>(p_stream, WORD(m_pattern[0]));
        WriteBinary<WORD>(p_stream, WORD(m_max_pulses));
        DebugLog(" [End Tone: Puretone]\n");
    }
    else if(m_pattern.size() == 2 && m_pattern[0] == m_pattern[1])
    {
        std::cout << "Tone: Puretone patt=" << m_pattern[0] << " #pulses=" << 2 * m_max_pulses << " Dur=" << ((2 * m_max_pulses * m_pattern[0]) * m_tstate_dur).count() << "sec" << std::endl;
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::Puretone);
        std::cout << "  ";
        WriteBinary<WORD>(p_stream, WORD(m_pattern[0]));
        WriteBinary<WORD>(p_stream, WORD(2 * m_max_pulses ));
        DebugLog(" [End Tone: Puretone]\n");
    }
    else
    {
        // need to write as GeneralizedDataBlock (instead of Puretone) because pulses have different length
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::GeneralizedDataBlock);
        GeneralizedDataBlock generalized_data_block{};
        generalized_data_block.pause        = 0;
        generalized_data_block.totp         = 1;        // one tone
        generalized_data_block.npp          = BYTE(m_pattern.size());
        generalized_data_block.asp          = 1;        // one symbol

        generalized_data_block.totd         = 0;        // no data
        generalized_data_block.npd          = 0;        // should not be used when totd is 0
        generalized_data_block.asd          = 2;        // should not be used when totd is 0 But 0 = 256. Must be power of 2.
        generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(0));
        std::cout << "Tone: GeneralizedDataBlock pattern-size=" << m_pattern.size() << " #pulses=" << m_max_pulses << " block_length=" << generalized_data_block.block_length << std::endl;
#ifdef DEBUG_TZX
        auto pos_start = p_stream.tellp();
#endif
        WriteBinary(p_stream, generalized_data_block);
        DebugLog(" | SymDef: ");
        // Symbol definitions 
        if( generalized_data_block.totp > 0)        // Note: is 1! This defines the one and only tone.
        {
            for(int n = 0; n < generalized_data_block.asp; n++)      // Note: there is one only!
            {
                GeneralizedDataBlock::SymDef symdef{};
                symdef.m_edge = Edge::toggle;
                WriteBinary(p_stream, symdef);
                for (int i = 0; i < generalized_data_block.npp; i++)
                {
                    WriteBinary(p_stream, WORD(m_pattern[i]));
                }
            }
            DebugLog(" | Prle: ");
            // Symbol sequence (which symbol + count)
            for (unsigned n = 0u; n < generalized_data_block.totp; n++)     // Note: there is one only!
            {
                GeneralizedDataBlock::Prle prle{};
                prle.m_symbol      = 0; // the one and only symbol
                prle.m_repetitions = WORD(m_max_pulses / generalized_data_block.npp);
                WriteBinary(p_stream, prle);
            }
        }

        if(generalized_data_block.totd > 0)     // Note: is 0. (no data)
        {
            // no data so no SYMDEF[ASD]/PRLE[TOTD]
        }
#ifdef DEBUG_TZX
        auto pos_end = p_stream.tellp();
        if(unsigned(pos_end - pos_start) != generalized_data_block.GetBlockLength(0) +  sizeof(generalized_data_block.block_length))
        {
            std::stringstream ss;
            ss << "TZX issues detected:" << pos_end << ' ' << pos_start << ' ' << pos_end - pos_start << ' ' << generalized_data_block.GetBlockLength(0) << ' ' << sizeof(generalized_data_block.block_length) << std::endl;
            throw std::runtime_error(ss.str());
        }
#endif
        DebugLog(" [End Tone: GeneralizedDataBlock]\n");
    }
}



void PausePulser::WriteAsTzxBlock(std::ostream& p_stream) const
{
    if(m_duration_in_tstates > 0xffff && m_edge == Edge::no_change)
    {
        auto len_in_ms = WORD((1000 * m_duration_in_tstates * m_tstate_dur).count());
        std::cout << "Pause: PauseOrStopthetapecommand " << len_in_ms << "ms; (TStates=" << m_duration_in_tstates << ")" << std::endl;
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::PauseOrStopthetapecommand);
        WriteBinary(p_stream, len_in_ms);
        DebugLog(" [End Pause: PauseOrStopthetapecommand]\n");
    }
    else
    {
        // need to write as GeneralizedDataBlock because PauseOrStopthetapecommand is not accurate
        // enough and might need edge at end of pause
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::GeneralizedDataBlock);
        GeneralizedDataBlock generalized_data_block{};
        generalized_data_block.pause        = 0;
        generalized_data_block.totp         = 2;    // pause, then the edge
        generalized_data_block.npp          = 1;
        generalized_data_block.asp          = 2;    // two symbols one for pause one for edge

        generalized_data_block.totd         = 0;
        generalized_data_block.npd          = 0;        // should not be used when totd is 0
        generalized_data_block.asd          = 2;        // should not be used when totd is 0 But 0 = 256. Must be power of 2.
        generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(0));
        std::cout << "Pause: GeneralizedDataBlock TStates=" << m_duration_in_tstates << " Egde= " << m_edge << std::endl;
#ifdef DEBUG_TZX
        auto pos_start = p_stream.tellp();
#endif
        WriteBinary(p_stream, generalized_data_block);

        if( generalized_data_block.totp > 0)        // Note: is 2! This defines the two symbols
        {
            DebugLog(" | Symdef#1: ");
            // first symdef for pause (need to wait first)
            GeneralizedDataBlock::SymDef symdef{};
            symdef.m_edge = Edge::no_change;
            WriteBinary(p_stream, symdef);
            WriteBinary(p_stream, WORD(m_duration_in_tstates));

            DebugLog(" | Symdef#2: ");
            // 2nd symdef for edge
            symdef.m_edge = m_edge;
            WriteBinary(p_stream, symdef);
            WriteBinary(p_stream, WORD(0)); // # is npp

            DebugLog(" | Prle#1: ");
            GeneralizedDataBlock::Prle prle{};
            prle.m_symbol      = 0; // first symbol
            prle.m_repetitions = 1;
            WriteBinary(p_stream, prle);

            DebugLog(" | Prle#2: ");
            prle.m_symbol      = 1; // 2nd symbol
            prle.m_repetitions = 1;
            WriteBinary(p_stream, prle);
        }
    
        if(generalized_data_block.totd > 0)     // Note: is 0. (no data)
        {
            // no data so no SYMDEF[ASD]/PRLE[TOTD]
        }
#ifdef DEBUG_TZX
        auto pos_end = p_stream.tellp();
        if(unsigned(pos_end - pos_start) != generalized_data_block.GetBlockLength(0) +  sizeof(generalized_data_block.block_length))
        {
            std::stringstream ss;
            ss << "TZX issues detected:" << pos_end << ' ' << pos_start << ' ' << pos_end - pos_start << ' ' << generalized_data_block.GetBlockLength(0) << ' ' << sizeof(generalized_data_block.block_length) << std::endl;
            throw std::runtime_error(ss.str());
        }
#endif
    }
    DebugLog(" [End Pause: GeneralizedDataBlock]\n");

}



void DataPulser::WriteAsTzxBlock(std::ostream& p_stream) const
{
    WriteBinary<TzxBlockType>(p_stream, TzxBlockType::GeneralizedDataBlock);

    GeneralizedDataBlock generalized_data_block{};
    generalized_data_block.pause        = 0;

    generalized_data_block.totp         = 0;        // no pilot/sync block here
    generalized_data_block.npp          = 0;        // not used when totp is 0
    generalized_data_block.asp          = 1;        // not used when totp is 0, but 0 is 256.

    generalized_data_block.totd         = DWORD(GetTotalSize() * 8);        // so # bits
    generalized_data_block.npd          = BYTE(std::max(m_zero_pattern.size(), m_one_pattern.size()));  // eg 2 for standard, 1 for turbo
    generalized_data_block.asd          = 2; // we have a pattern for 0 and for 1

    generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(GetTotalSize()));
    std::cout << "Data: GeneralizedDataBlock blocklen=" << generalized_data_block.GetBlockLength(GetTotalSize()) << " #bytes=" << GetTotalSize() << " totd=" << generalized_data_block.totd << " m_max_pulses=" << " block_length=" << generalized_data_block.block_length << std::endl;
#ifdef DEBUG_TZX
    auto pos_start = p_stream.tellp();
#endif
    WriteBinary(p_stream, generalized_data_block);
        
    if(generalized_data_block.totp > 0)      // Note: is 0. (no leader/sync))
    {
        // no pulses, so no SYMDEF[ASP]/PRLE[TOTP]
    }

    for(int n = 0; n < generalized_data_block.asd; n++)     // should be 2. Pattern for 0 and 1
    {
        DebugLog(" | SymDef: ");
        GeneralizedDataBlock::SymDef symdef{};
        symdef.m_edge = Edge::toggle;      // toggle
        WriteBinary(p_stream, symdef);
        const auto & pattern = (n == 0) ? m_zero_pattern : m_one_pattern;
        for (int i = 0; i < generalized_data_block.npd; i++)        // should be 1 for turbo blocks; 2 for standard speed.
        {
            WriteBinary(p_stream, (i < pattern.size()) ? WORD(pattern[i]) : WORD(0));
        }
    }

    DebugLog(" | Data: ");
    for(int i = 0; i < GetTotalSize(); i++)
    {
        WriteBinary(p_stream, GetByte(i));
    }
#ifdef DEBUG_TZX
    auto pos_end = p_stream.tellp();
    if(unsigned(pos_end - pos_start) != generalized_data_block.GetBlockLength(GetTotalSize()) +  sizeof(generalized_data_block.block_length))
    {
        std::stringstream ss;
        ss << "TZX issues detected:" << pos_end << ' ' << pos_start << ' ' << pos_end - pos_start << ' ' << generalized_data_block.GetBlockLength(0) << ' ' << sizeof(generalized_data_block.block_length) << std::endl;
        throw std::runtime_error(ss.str());
    }
#endif
    DebugLog(" [End Data: GeneralizedDataBlock]\n");
    
}
