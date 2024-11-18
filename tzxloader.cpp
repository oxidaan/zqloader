//==============================================================================
// PROJECT:         zqloader
// FILE:            tzxloader.cpp
// DESCRIPTION:     Implementation of class TzxLoader.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#include "tzxloader.h"
#include "loadbinary.h"
#include "datablock.h"
#include "pulsers.h"
#include "spectrum_consts.h"        // g_tstate_dur
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

using BYTE = uint8_t;
using WORD = uint16_t;
using DWORD = uint32_t;

/// TZX block types
enum class TzxBlockType : BYTE
{
    StandardSpeedDataBlock = 0x10,
    TurboSpeedDataBlock = 0x11,
    Puretone = 0x12,
    PulseSequence = 0x13,
    PureDataBlock = 0x14,
    DirectRecordingBlock = 0x15,
    CSWRecordingBlock = 0x18,
    GeneralizedDataBlock = 0x19,
    PauseOrStopthetapecommand = 0x20,
    GroupStart = 0x21,
    GroupEnd = 0x22,
    Jumptoblock = 0x23,
    Loopstart = 0x24,
    Loopend = 0x25,
    Callsequence = 0x26,
    Returnfromsequence = 0x27,
    Selectblock = 0x28,
    Stopthetapeifin48Kmode = 0x2A,
    Setsignallevel = 0x2B,
    Textdescription = 0x30,
    Messageblock = 0x31,
    Archiveinfo = 0x32,
    Hardwaretype = 0x33,
    Custominfoblock = 0x35,
    Glueblock = 0x5A
};
std::ostream& operator << (std::ostream& p_stream, TzxBlockType p_enum);


// http://k1.spdns.de/Develop/Projects/zasm/Info/TZX%20format.html
// https://worldofspectrum.net/TZXformat.html
#pragma pack(push, 1)
struct GeneralizedDataBlock
{
    DWORD block_length;     // Block length (without these four bytes)
    WORD pause;

    DWORD totp;             // Total number of symbols in pilot / sync block(can be 0) (eg 2 for a leader, then a sync)
    BYTE npp;               // Maximum number of pulses per pilot/sync symbol (= #pulse_lengths at pulse-symdef)
    BYTE asp;               // Total number of pilot/sync symbols in the alphabet table (0=256) (= #pulse-symdef-s)

    DWORD totd;             // Total number of symbols in data stream (can be 0)    (when asd==2 this is # bits in data)
    BYTE npd;               // Maximum number of pulses per data symbol             (= #pulse_lengths at data-symdef)
    BYTE asd;               // Number of data symbols in the alphabet table (0=256) (= #data-symdef-s) typical 2 when data is bits

    struct SymDef           // Pilot / sync / data symbols ('alphabet') definition table #=asp
    {
        Edge m_edge;        // exactly same as our Edge enum type
        // WORD pulse_lengths[];    // Size of this is #npp for pulse-symdef; #npd for data-symdef
    };
    static_assert(sizeof(SymDef) == sizeof(BYTE), "Sizeof SymDef must be 1");

    // Pilot and sync data stream, # = totp
    struct Prle
    {
        BYTE m_symbol;              // ref to pulse-symdef number so 0 - asp-1 (basicly: what pulse to use)
        WORD m_repetitions;         // How ofte is the above pulse repeated
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
        return block_length  + sizeof(block_length) - sizeof(GeneralizedDataBlock) - GetRemainingLength() ;
    }
    
    size_t GetRemainingLength(size_t p_size_of_data = 0) const
    {
        return 
            (sizeof(SymDef) + npp * sizeof(WORD)) * asp +
            sizeof(Prle) * totp +
            (sizeof(SymDef) + npd * sizeof(WORD)) * asd +
            p_size_of_data;
    }

};
#pragma pack(pop)



TzxLoader& TzxLoader::Load(const fs::path &p_filename, std::string p_zxfilename)
{
    std::ifstream fileread(p_filename, std::ios::binary);
    if (!fileread)
    {
        throw std::runtime_error("File " + p_filename.string() + " not found.");
    }
    try
    {
        std::cout << "Loading file " << p_filename << std::endl;
        Load(fileread, p_zxfilename);
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
TzxLoader& TzxLoader::Load(std::istream& p_stream, std::string p_zxfilename)
{
    std::string s = LoadBinary<std::string>(p_stream, 7);

    if (s != "ZXTape!")
    {
        throw std::runtime_error("Not a tzx file");
    }
    auto eof_marker = LoadBinary<std::byte>(p_stream);
    auto version1 = LoadBinary<char>(p_stream);
    auto version2 = LoadBinary<char>(p_stream);
    std::cout << "TZX file version: " << int(version1) << '.' << int(version2) << std::endl;
    bool done = false;
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
            std::cout << " len = " << len << ' ';
            done = HandleTapBlock(p_stream, p_zxfilename, len);
            break;
        }
        case TzxBlockType::TurboSpeedDataBlock:
        {
            p_stream.ignore(0x0f );
            auto len1 = LoadBinary<BYTE>(p_stream);     // bit of weird 24 bit length
            auto len2 = LoadBinary<BYTE>(p_stream);
            auto len3 = LoadBinary<BYTE>(p_stream);
            auto len = 0x10000 * len3 + 0x100 * len2 + len1;
            std::cout << " len = " << len << ' ';
            done = HandleTapBlock(p_stream, p_zxfilename, len);
            break;
        }
        case TzxBlockType::Puretone:
        {
            auto len1 = LoadBinary<WORD>(p_stream);
            auto len2 = LoadBinary<WORD>(p_stream);
            (void)len2;
            std::cout << ' ' << int(len1) << " T states ";
            break;
        }
        case TzxBlockType::PulseSequence:
        {
            auto len = LoadBinary<BYTE>(p_stream);
            std::cout << ' ' << int(len) << " pulses ";
            p_stream.ignore(2 * len);
            break;  
        }
        case TzxBlockType::PureDataBlock:
        {
            p_stream.ignore(0x07);
            int len1 = LoadBinary<BYTE>(p_stream);     // bit of weird 24 bit length
            int len2 = LoadBinary<BYTE>(p_stream);
            int len3 = LoadBinary<BYTE>(p_stream);
            int len = 0x10000 * len3 + 0x100 * len2 + len1;
            std::cout << " len = " << len << ' ';
            done = HandleTapBlock(p_stream, p_zxfilename, len);
            break;
        }
        case TzxBlockType::DirectRecordingBlock:
        {
            p_stream.ignore(5);
            auto len1 = LoadBinary<BYTE>(p_stream);     // bit of weird 24 bit length
            auto len2 = LoadBinary<BYTE>(p_stream);
            auto len3 = LoadBinary<BYTE>(p_stream);
            auto len = 0x10000 * len3 + 0x100 * len2 + len1;
            std::cout << " len = " << len;
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
            auto remaining_len = generalized_data_block.GetRemainingLength();
            p_stream.ignore(remaining_len);
            int len = int(generalized_data_block.GetDataLength());
            std::cout << " len = " << len << ' ';
            if(len)
            {
                HandleTapBlock(p_stream, p_zxfilename, len);
            }
            break;
        }
        case TzxBlockType::PauseOrStopthetapecommand:
            p_stream.ignore(2);
            break;
        case TzxBlockType::GroupStart:
        {
            auto len = LoadBinary<BYTE>(p_stream);
            auto s2 = LoadBinary<std::string>(p_stream, len);
            std::cout << "  " << s2 << std::endl;
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
            std::cout << "  " << txt << std::endl;
            break;
        }
        case TzxBlockType::Archiveinfo:
        {
            auto len = LoadBinary<WORD>(p_stream);
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
            std::cout << "TODO: TZX:" << id << std::endl;       // TODO!

        }
        std::cout << std::endl;
    }
    return *this;
}

bool TzxLoader::HandleTapBlock(std::istream& p_stream, std::string p_zxfilename, int p_length)
{
    TapLoader taploader;
    auto block = taploader.LoadTapBlock(p_stream, p_length);
    if (m_OnHandleTapBlock)
    {
        return m_OnHandleTapBlock(std::move(block), p_zxfilename);
    }
    return false;
}

/// Make conversion from enum tags to string somewhat simpler
#define ENUM_TAG(prefix, tag) case prefix::tag: p_stream << #tag;break;

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
        std::cout << "TonePulser::WriteAsTzxBlock:Puretone " << m_pattern[0] << ' ' << ((m_max_pulses * m_pattern[0]) *spectrum::g_tstate_dur).count() << std::endl;
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::Puretone);
        WriteBinary<WORD>(p_stream, WORD(m_pattern[0]));
        WriteBinary<WORD>(p_stream, WORD(m_max_pulses));
    }
    else
    {
        // need to write as GeneralizedDataBlock (instead of Puretone) because pulses have different length
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::GeneralizedDataBlock);
        GeneralizedDataBlock generalized_data_block;
        generalized_data_block.pause = 0;
        generalized_data_block.totp = 1;
        generalized_data_block.npp = BYTE(m_pattern.size());
        generalized_data_block.asp = 1;

        generalized_data_block.totd = 0;
        generalized_data_block.npd = 0;
        generalized_data_block.asd = 0;
        generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(0));
        std::cout << "TonePulser::WriteAsTzxBlock:GeneralizedDataBlock pattern-size=" << m_pattern.size() << " m_max_pulses=" << m_max_pulses << " block_length=" << generalized_data_block.block_length << std::endl;
        WriteBinary(p_stream, generalized_data_block);
        // for(int n = 0; n < generalized_data_block.asp; n++)      // there is one only
        {
            GeneralizedDataBlock::SymDef symdef;
            symdef.m_edge = Edge::toggle;      
            WriteBinary(p_stream, symdef);
            for (int i = 0; i < generalized_data_block.npp; i++)
            {
                WriteBinary(p_stream, WORD(m_pattern[i]));
            }
        }
        //for (int n = 0; n < generalized_data_block.totp; n++)     // there is one only
        {
            GeneralizedDataBlock::Prle prle;
            prle.m_symbol = 0;      // the one and only symbol
            prle.m_repetitions = WORD(m_max_pulses / generalized_data_block.npp);
            WriteBinary(p_stream, prle);
        }
        // no data so no SYMDEF[ASD]/PRLE[TOTD]
    }

}

void PausePulser::WriteAsTzxBlock(std::ostream& p_stream) const
{

    if(m_duration_in_tstates > 0xffff && m_edge == Edge::no_change)
    {
        auto len_in_ms = WORD((1000 * m_duration_in_tstates * spectrum::g_tstate_dur).count());
        std::cout << "PausePulser::WriteAsTzxBlock:Pause " << len_in_ms << "ms" << std::endl;
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::PauseOrStopthetapecommand);
        WriteBinary(p_stream, len_in_ms);
    }
    else
    {
        // need to write as GeneralizedDataBlock because PauseOrStopthetapecommand is not accurate
        // enough and might need edge at end of pause
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::GeneralizedDataBlock);
        GeneralizedDataBlock generalized_data_block;
        generalized_data_block.pause = 0;
        generalized_data_block.totp = 2;            // pause, then the edge
        generalized_data_block.npp = 1;             
        generalized_data_block.asp = 2;             // two symbols one for pause one for edge

        generalized_data_block.totd = 0;
        generalized_data_block.npd = 0;
        generalized_data_block.asd = 0;
        generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(0));
        std::cout << "PausePulser::WriteAsTzxBlock:GeneralizedDataBlock" << std::endl;
        WriteBinary(p_stream, generalized_data_block);

        // first symdef for pause
        GeneralizedDataBlock::SymDef symdef;
        symdef.m_edge = Edge::no_change;
        WriteBinary(p_stream, symdef);
        WriteBinary(p_stream, WORD(m_duration_in_tstates));

        // 2nd symdef for edge
        symdef.m_edge = m_edge;
        WriteBinary(p_stream, symdef);
        WriteBinary(p_stream, WORD(0));

        GeneralizedDataBlock::Prle prle;
        prle.m_symbol = 0;      // first symbol
        prle.m_repetitions = 1;
        WriteBinary(p_stream, prle);

        prle.m_symbol = 1;      // 2nd symbol
        prle.m_repetitions = 1;
        WriteBinary(p_stream, prle);

    }
}

void DataPulser::WriteAsTzxBlock(std::ostream& p_stream) const
{

    WriteBinary<TzxBlockType>(p_stream, TzxBlockType::GeneralizedDataBlock);
    GeneralizedDataBlock generalized_data_block;
    generalized_data_block.pause = 0;

    generalized_data_block.totp = 0;
    generalized_data_block.npp = 0;
    generalized_data_block.asp = 0;

    generalized_data_block.totd = DWORD(GetTotalSize() * 8);
    generalized_data_block.npd = BYTE(std::max(m_zero_pattern.size(), m_one_pattern.size()));
    generalized_data_block.asd = 2;     // we have a pattern for 1 and for 0

    generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(GetTotalSize()));
    std::cout << "DataPulser::WriteAsTzxBlock:GeneralizedDataBlock #bytes=" << GetTotalSize() << " m_max_pulses=" << " block_length=" << generalized_data_block.block_length << std::endl;
    WriteBinary(p_stream, generalized_data_block);
    // no pulses, so no SYMDEF[ASP]/PRLE[TOTP]
    for(int n = 0; n < generalized_data_block.asd; n++)   
    {
        GeneralizedDataBlock::SymDef symdef;
        symdef.m_edge = Edge::toggle;      // toggle
        WriteBinary(p_stream, symdef);
        const auto & pattern = (n==0) ? m_zero_pattern : m_one_pattern;
        for (int i = 0; i < generalized_data_block.npd; i++)
        {
            WriteBinary(p_stream, (n < pattern.size()) ? WORD(pattern[n]) : WORD(0));
        }
    }

    for(int i = 0; i < GetTotalSize(); i++)
    {
        WriteBinary(p_stream, GetByte(i)); 
    }
}

