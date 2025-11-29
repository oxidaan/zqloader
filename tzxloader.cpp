// ==============================================================================
// PROJECT:         zqloader
// FILE:            tzxloader.cpp
// DESCRIPTION:     Implementation of class TzxLoader.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================


#include "tzxloader.h"
#include "tzx_types.h"
#include "taploader.h"
#include "loadbinary.h"
#include "datablock.h"
#include <fstream>
#include <filesystem>



namespace fs = std::filesystem;


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
            std::cout << " length = " << len << "; ignored/can not handle this block." << std::endl;
            p_stream.ignore(len);
            break;
        }
        case TzxBlockType::CSWRecordingBlock:
        {
            auto len = LoadBinary<DWORD>(p_stream);
            std::cout << " length = " << len << "; ignored/can not handle this block." << std::endl;
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
                std::cout << ": Stop the tape!!" << std::endl;
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
            std::cout << "; ignored/can not handle this block." << std::endl;
            p_stream.ignore(2);
            break;
        case TzxBlockType::Loopstart:
            std::cout << "; ignored/can not handle this block." << std::endl;
            p_stream.ignore(2);
            break;
        case TzxBlockType::Loopend:
            break;
        case TzxBlockType::Callsequence:
        {
            auto len = LoadBinary<WORD>(p_stream);
            std::cout << " length = " << len << "; ignored/can not handle this block." << std::endl;
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
            std::cout << " length = " << len << "; ignored/can not handle this block." << std::endl;
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
            p_stream.ignore(1);     // time
           [[fallthrough]];              // no break
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

