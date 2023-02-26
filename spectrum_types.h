//==============================================================================
// PROJECT:         zqloader
// FILE:            spectrum_types.h
// DESCRIPTION:     Definition of class SpectrumLoader.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#pragma once


#include <cstdint>  // uint16_t
#include <iosfwd>   // forward for std::ostream

// ZX BlockType (header or data)
enum class ZxBlockType : unsigned char
{
    header = 0x0,           // spectrum header
    data = 0xff,            // spectrum data
    raw = 0x1,              // eg header and checksum as read from tap file.
    unknown = 0x2,
    error = 0x3,            // eg eof
};
std::ostream& operator << (std::ostream& p_stream, ZxBlockType p_enum);



// ZX Spectrum header
#pragma pack(push, 1)
struct ZxHeader
{
    // ZxHeader::Type
    enum class Type : unsigned char
    {
        basic_program = 0,
        array_numbers = 1,
        array_text = 2,
        code = 3,
        screen = 4
    };

    uint16_t GetStartAddress() const
    {
        if (m_type == Type::code)
        {
            return m_start_address;
        }
        else if (m_type == Type::basic_program)
        {
            return 23755;       // Afaik typical start of basic (maybe should read PROG)
        }
        else
        {
            return 0;
        }
    }


    Type m_type;            // type see above
    char m_filename[10];    // seems obvious
    uint16_t m_length;      // total length of block to load (when basic this includes variables)
    union
    {
        uint16_t m_basic_program_start_line;    // auto start line when basic_program
        uint16_t m_start_address;               // start address when code. (Can also hold variable name)
    };
    uint16_t m_basic_program_length;        // basic program length after which start of variables follows
};
#pragma pack(pop)
static_assert(sizeof(ZxHeader) == 17, "Sizeof ZxHeader must be 17");

std::ostream& operator << (std::ostream& p_stream, ZxHeader::Type p_enum);




