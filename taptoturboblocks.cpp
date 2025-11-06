// ==============================================================================
// PROJECT:         zqloader
// FILE:            taptoturboblocks.cpp
// DESCRIPTION:     Implementation of class TapToTurboBlocks.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================


#include "taptoturboblocks.h"
#include <iostream>
#include "byte_tools.h"

/// call back
/// Handle loaded/incoming data block:
/// When header: check name; store as last header.
/// When data: based on last header header:
///     When basic: try get addresses like RANDOMIZE USR XXXXX
///     When code: Add to list of blocks to (turbo) load
/// p_zxfilename: when given check if name matches before accepting header.
bool TapToTurboBlocks::HandleTapBlock(DataBlock p_block, const std::string &p_zxfilename)
{
    using namespace spectrum;
    bool done          = false;
    TapeBlockType type = TapeBlockType(p_block[0]);
    // kick of type and checksum
    DataBlock block    = p_block.size() >= 2 ? DataBlock(p_block.begin() + 1, p_block.end() - 1) : std::move(p_block);
    if (type == TapeBlockType::header)
    {
        // In tzx files sometimes header is 18 bytes instead of 17
        if (block.size() != sizeof(TapeHeader) && block.size() != sizeof(TapeHeader) + 1)
        {
            throw std::runtime_error("Expecting header length to be " + std::to_string(sizeof(TapeHeader)) + ", but is : " + std::to_string(block.size()));
        }
        m_last_header = *reinterpret_cast<TapeHeader*>(block.data());
        std::string name(m_last_header.m_filename, 10);    // zx file name from tap/header
        if ((name == p_zxfilename || p_zxfilename == "") || m_headercnt >= 1)
        {
            m_last_block = type;
            m_headercnt++;
        }
        std::cout << "Spectrum tape header: " << m_last_header.m_type << ": ";
        std::cout << "'" << name << "'";
        std::cout << " Start address: " << m_last_header.GetStartAddress();
        std::cout << " Length: " << m_last_header.m_length << std::endl;
    }
    else if (type == TapeBlockType::data)
    {
        if (m_last_block != TapeBlockType::header && m_headercnt >= 1)
        {
            throw std::runtime_error("Found headerless, can not handle (can't know were it should go to)");
        }
        if (m_last_header.m_type == TapeHeader::Type::code ||
            m_last_header.m_type == TapeHeader::Type::screen)
        {
            auto start_adr = (m_loadcodes.size() > m_codecount) ?
                             m_loadcodes[m_codecount] : // taking address from last unused LOAD "" CODE XXXXX as found in basic
                             m_last_header.GetStartAddress();

            m_tblocks.AddMemoryBlock({std::move(block), start_adr});
            m_codecount++;
        }
        else if (m_last_header.m_type == TapeHeader::Type::basic_program)
        {
            if (m_codecount == 0)
            {
                auto usr = TryFindUsr(block);
                if (usr)
                {
                    std::cout << "Found USR " << usr << " in BASIC." << std::endl;
                    m_usr = usr;
                }
                auto clear = TryFindClear(block);
                if (clear)
                {
                    std::cout << "Found CLEAR " << clear << " in BASIC" << std::endl;
                    m_clear = clear;
                }

                m_loadcodes = TryFindLoadCode(block);
                for (auto code : m_loadcodes)
                {
                    std::cout << "Found LOAD \"\" CODE " << code << " in BASIC" << std::endl;
                }
            }
            else
            {
                done = true;        // Found basic after code. We are done.
            }
        }
        m_last_block = type;
    }
    return done;
}



// read a number from basic either as VAL "XXXXX" or a 2 byte int.
// 0 when failed/not found.
// (note when it is truly 0 makes no sence like RANDOMIZE USR 0, CLEAR 0, LOAD "" CODE 0)
inline uint16_t TapToTurboBlocks::TryReadNumberFromBasic(const DataBlock& p_basic_block, int p_cnt)
{
    std::string valstring;
    auto c = p_basic_block[p_cnt];
    if (p_cnt < p_basic_block.size() - 1 &&
        c == 0xB0_byte &&                                      //  VAL
        p_basic_block[p_cnt + 1] == std::byte('"'))            //  VAL "
    {
        // VAL "XXXX"
        p_cnt += 2;     // skip VAL "
        c      = p_basic_block[p_cnt];
        while (c != std::byte('"') && p_cnt < p_basic_block.size())
        {
            valstring += char(c);
            p_cnt++;
            c          = p_basic_block[p_cnt];
        }
        return uint16_t(std::atoi(valstring.c_str()));     // done
    }
    else if (p_cnt < p_basic_block.size() - 5 &&
             c == 0x0E_byte &&
             char(p_basic_block[p_cnt + 1]) == 0 &&
             char(p_basic_block[p_cnt + 2]) == 0 &&
             char(p_basic_block[p_cnt + 5]) == 0)
    {
        // https://retrocomputing.stackexchange.com/questions/5932/why-does-this-basic-program-declare-variables-for-the-numbers-0-to-4
        // 0x0E is kind of int flag at ZX basic. Followed by two zero's.
        // [0x0E] [0] [0] [LSB] [MSB] [0]
        // [RANDOMIZE USR] XXXXX       stored as int.
        p_cnt += 3;
        return *reinterpret_cast<const uint16_t*>(&p_basic_block[p_cnt]);
    }
    return 0;
}



inline std::vector<uint16_t> TapToTurboBlocks::TryFindInBasic(const DataBlock& p_basic_block, const CheckFun &p_check_fun)
{
    int seen = 0;       // When >0 recently saw pattern to search for
    std::vector<uint16_t> retval;
    for (int cnt = 0; cnt < p_basic_block.size(); cnt++)
    {
        if (seen)
        {
            auto val = TryReadNumberFromBasic(p_basic_block, cnt);
            seen--;
            if (val >= 16384)  // && val < 65536)
            {
                retval.push_back(val);
                seen = 0;
            }
        }

        auto check = p_check_fun(p_basic_block, cnt);
        if (check == 1)
        {
            seen = 8;       // allow some chars searching for number after code(eg the value stored as ASCII)
        }
        if(check > 1)
        {
            retval.push_back(uint16_t(check));
        }

    }
    return retval;
}



// Try to find (first) USR start address in given BASIC block
// eg RANDOMIZE USR XXXXX
// or RANDOMIZE USR VAL "XXXXX"
inline uint16_t TapToTurboBlocks::TryFindUsr(const DataBlock& p_basic_block)
{
    auto values = TryFindInBasic(p_basic_block, [](const DataBlock& p_basic_block, int cnt)
    {
        bool b = cnt > 1 &&
            p_basic_block[cnt] == 0xC0_byte && (  // USR
                p_basic_block[cnt - 1] == 0xf9_byte ||     // RANDOMIZE USR
                p_basic_block[cnt - 1] == 0xf5_byte ||     // PRINT USR
                p_basic_block[cnt - 1] == std::byte('=')); // (LET x ) = USR
        return int(b);
    });
    return values.size() ? values.front() : 0;
}



// Try to find (first) CLEAR XXXXX address in given BASIC block
// eg CLEAR XXXXX
// or CLEAR VAL "XXXXX"
inline uint16_t TapToTurboBlocks::TryFindClear(const DataBlock& p_basic_block)
{
    auto values = TryFindInBasic(p_basic_block, [](const DataBlock& p_basic_block, int cnt)
    {
        return int(p_basic_block[cnt] == 0xFD_byte); // CLEAR
    });
    return values.size() ? values.front() : 0;
}



// Try to find all LOAD "" CODE XXXXX addresses in given BASIC block
inline std::vector<uint16_t> TapToTurboBlocks::TryFindLoadCode(const DataBlock& p_basic_block)
{
    return TryFindInBasic(p_basic_block, [](const DataBlock& p_basic_block, int cnt)
    {
        if( cnt > 0 &&
            p_basic_block[cnt] == 0xAF_byte &&        // CODE
            p_basic_block[cnt - 1] == std::byte('"')) // [LOAD "]" CODE
        {
            return 1;
        }
        if(cnt > 0 &&
            p_basic_block[cnt] == 0xAA_byte && // SCREEN$
            p_basic_block[cnt - 1] == std::byte('"'))
        {
            return 16384; // so SCREEN$
        }
        return 0;
    });
}
