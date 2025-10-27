//==============================================================================
// PROJECT:         zqloader
// FILE:            spectrum_types.h
// DESCRIPTION:     Definition some ZX spectrum specific types.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once


#include <cstdint>  // uint16_t
#include <iosfwd>   // forward for std::ostream
#include "spectrum_consts.h"
#include <cstddef>  // std::byte stuff

namespace spectrum
{
// Tape BlockType (header or data)
enum class TapeBlockType : unsigned char
{
    header = 0x0,           // ZX spectrum header
    data = 0xff,            // ZX spectrum data
    raw = 0x1,              // eg header and checksum as read from tap file.
    unknown = 0x2,
    error = 0x3,            // eg eof
};



// ZX Spectrum tape header
#pragma pack(push, 1)
struct TapeHeader
{
    // TapeHeaderHeader::Type
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
            return PROG;       // Afaik typical start of basic (maybe should read PROG)
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
static_assert(sizeof(TapeHeader) == 17, "Sizeof ZxHeader must be 17");


/// Calculate a ZX Spectrum standard tap block checksum
/// Is always at end of block.
template <class TData>
std::byte CalculateChecksum(std::byte p_init_val, const TData& p_data)
{
    std::byte retval = p_init_val;
    for (const std::byte& b : p_data)
    {
        retval ^= b;
    }
    return retval;
}

}

std::ostream& operator << (std::ostream& p_stream, spectrum::TapeBlockType p_enum);
std::ostream& operator << (std::ostream& p_stream, spectrum::TapeHeader::Type p_enum);



