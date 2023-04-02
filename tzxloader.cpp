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
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

/// TZX block types
enum class TzxBlockType : uint8_t
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
        std::cout << id << std::endl;
        switch (id)
        {
        case TzxBlockType::StandardSpeedDataBlock:
            done = HandleTapBlock(p_stream, p_zxfilename, 0x4 - 2);
            break;
        case TzxBlockType::TurboSpeedDataBlock:
            done = HandleTapBlock(p_stream, p_zxfilename, 0x12 - 2);
            break;
        case TzxBlockType::Puretone:
            p_stream.ignore(4);
            break;
        case TzxBlockType::PulseSequence:
        {
            auto len = LoadBinary<std::uint8_t>(p_stream);
            p_stream.ignore(2 * len);
            break;  
        }
        case TzxBlockType::PureDataBlock:
            done = HandleTapBlock(p_stream, p_zxfilename, 0x0A - 2);
            break;
        case TzxBlockType::DirectRecordingBlock:
        {
            p_stream.ignore(5);
            auto len1 = LoadBinary<std::uint8_t>(p_stream);     // bit of weird 24 bit length
            auto len2 = LoadBinary<std::uint8_t>(p_stream);
            auto len3 = LoadBinary<std::uint8_t>(p_stream);
            p_stream.ignore(0xffff * len1 + 0xff * len2 + len3);
            break;
        }
        case TzxBlockType::CSWRecordingBlock:
        {
            auto len = LoadBinary<std::uint32_t>(p_stream);
            p_stream.ignore(len);
            break;
        }
        case TzxBlockType::GeneralizedDataBlock:
        {
            auto len = LoadBinary<std::uint32_t>(p_stream);
            p_stream.ignore(len);
            break;
        }
        case TzxBlockType::PauseOrStopthetapecommand:
            p_stream.ignore(2);
            break;
        case TzxBlockType::GroupStart:
        {
            auto len = LoadBinary<std::uint8_t>(p_stream);
            p_stream.ignore(len);
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
            auto len = LoadBinary<std::uint16_t>(p_stream);
            p_stream.ignore(len * 2);
            break;
        }
        case TzxBlockType::Returnfromsequence:
        {
            break;
        }
        case TzxBlockType::Selectblock:
        {
            auto len = LoadBinary<std::uint16_t>(p_stream);
            p_stream.ignore(len * 2);
            break;
        }
        case TzxBlockType::Stopthetapeifin48Kmode:
            p_stream.ignore(4);
            break;
        case TzxBlockType::Setsignallevel:
            p_stream.ignore(5);
            break;
        case TzxBlockType::Textdescription:
        {
            auto len = LoadBinary<std::uint8_t>(p_stream);
            p_stream.ignore(len);
            break;
        }
        case TzxBlockType::Messageblock:
        {
            p_stream.ignore(1);
            auto len = LoadBinary<std::uint8_t>(p_stream);
            p_stream.ignore(len);
            break;
        }
        case TzxBlockType::Archiveinfo:
        {
            auto len = LoadBinary<std::uint16_t>(p_stream);
            p_stream.ignore(len);
            break;
        }
        case TzxBlockType::Hardwaretype:
        {
            auto len = LoadBinary<std::uint8_t>(p_stream);
            p_stream.ignore(len * 3);
            break;
        }
        case TzxBlockType::Custominfoblock:
        {
            p_stream.ignore(0x10);
            auto len = LoadBinary<std::uint16_t>(p_stream);
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
    }
    return *this;
}

bool TzxLoader::HandleTapBlock(std::istream& p_stream, std::string p_zxfilename, size_t p_ignore)
{
    // [2 byte len] - [spectrum data incl. checksum]
    TapLoader taploader;
    p_stream.ignore(p_ignore);     // Pause after this block (ms.) {1000}
    auto block = taploader.LoadTapBlock(p_stream);
    if (m_OnHandleTapBlock)
    {
        return m_OnHandleTapBlock(std::move(block), p_zxfilename);
    }
    return false;
}

/// Make conversion from enum tags to string somewhat simpler
#define ENUM_TAG(prefix, tag) case prefix::tag: p_stream << #tag;break;

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
