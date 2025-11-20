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
#include "memoryblock.h"
#include <string>


/// call back
/// Handle loaded/incoming data block:
/// p_block includes type and checksum but not the length.
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

        auto header = *reinterpret_cast<TapeHeader*>(block.data());
        std::string name(header.m_filename, 10);    // zx file name from tap/header
        while (!name.empty() && name.back() == ' ')
        {
            name.pop_back();        // right trim spaces.
        }
        std::cout << "Spectrum tape header: " << header.m_type << ": ";
        std::cout << "'" << name << "'";
        std::cout << " Start address: " << header.GetStartAddress();
        std::cout << " Length: " << header.m_length << std::endl;

        if (m_last_block_type == TapeBlockType::header && m_headercnt >= 1)
        {
            std::cout << "<b>Warning found stray header (tap or tzx file possibly not correct)<\b>" << std::endl;
            m_headercnt = 0;
        }
        
        // matching; earlier or now
        if ((name == p_zxfilename || p_zxfilename == "") || m_headercnt >= 1)
        {
            m_last_header = std::move(header);
            m_headercnt++;
        }
    }
    else if (type == TapeBlockType::data)
    {
        std::cout << "  Data Block with payload length: " << block.size() <<" (" << m_last_header.m_type << ")" << std::endl; 
        if( m_headercnt >= 1)
        {
            // Else we didn't see a valid header yet. 
            if (m_last_block_type != TapeBlockType::header)
            {
                std::cout << "<b>Found headerless, can not handle (can't know were it should go to)</b>" << std::endl;
                m_headercnt++;
            }
            else if (m_last_header.m_type == TapeHeader::Type::basic_program)
            {
                if (m_codecount == 0)
                {
                    ParseBasic(block);
                }
                else
                {
                    done = true;        // Found basic after code. We are done.
                }
            }
            else if (m_last_header.m_type == TapeHeader::Type::code ||
                     m_last_header.m_type == TapeHeader::Type::screen)
            {
                if (m_codecount == 0 && !m_basic_was_parsed)    // try parse BASIC from code block eg meteorstorms (LOAD ""CODE)
                {
                    ParseBasic(block);
                }
                auto start_adr = (m_loadcodes.size() > m_codecount &&  m_loadcodes[m_codecount] != 0 ) ?
                                 m_loadcodes[m_codecount] :         // taking address from matching LOAD "" CODE XXXXX as found in basic
                                 m_last_header.GetStartAddress();

                m_tblocks.AddMemoryBlock({std::move(block), start_adr});
                m_codecount++;

            }
        }
        std::cout << std::endl;
    }
    m_last_block_type = type;
    return done;
}


inline void TapToTurboBlocks::ParseBasic(const DataBlock& p_basic_block)
{
    m_basic_was_parsed = true;
    auto usrs = TryFindUsr(p_basic_block);
    for (auto usr: usrs)
    {
        std::cout << "  Found USR " << usr << " in BASIC.\n";
    }
    if(usrs.size() != 1)
    {
        std::cout << "<b>Warning: Found " << usrs.size() << "x USR xxxx in BASIC. Code is probably protected and will not work!</b>" << std::endl;
    }
    else
    {
        m_usr = *usrs.begin();
    }

    auto clear = TryFindClear(p_basic_block);
    if (clear)
    {
        std::cout << "  Found CLEAR " << clear << " in BASIC" << std::endl;
        m_clear = clear;
    }

    m_loadcodes = TryFindLoadCode(p_basic_block);
    for (auto code : m_loadcodes)
    {
        std::cout << "  Found LOAD \"\" CODE " << (code ? std::to_string(code) : "") << " in BASIC" << std::endl;
    }
}

// read a number from basic either as VAL "XXXXX" or a 2 byte int.
// 0 when failed/not found.
// (note when it is truly 0 makes no sence like RANDOMIZE USR 0, CLEAR 0, LOAD "" CODE 0)
inline uint16_t TapToTurboBlocks::TryReadNumberFromBasic(const DataBlock& p_basic_block, int p_cnt, int p_max)
{
    std::string valstring;
    for(int cnt = p_cnt; cnt < p_cnt + p_max; cnt++)
    {
        auto c = p_basic_block[cnt];
        if (cnt < p_basic_block.size() - 2 &&
            c == 0xB0_byte &&                                      //  VAL
            p_basic_block[cnt + 1] == std::byte('"'))            //  VAL "
        {
            // VAL "XXXX"
            cnt += 2;     // skip VAL "
            c      = p_basic_block[cnt];
            while (c != std::byte('"') && cnt < p_basic_block.size())
            {
                valstring += char(c);
                cnt++;
                c          = p_basic_block[cnt];
            }
            return uint16_t(std::atoi(valstring.c_str()));     // done
        }
        else if (cnt < p_basic_block.size() - 5 &&
                 c == 0x0E_byte &&
                 char(p_basic_block[cnt + 1]) == 0 &&
                 char(p_basic_block[cnt + 2]) == 0 &&
                 char(p_basic_block[cnt + 5]) == 0)
        {
            // https://retrocomputing.stackexchange.com/questions/5932/why-does-this-basic-program-declare-variables-for-the-numbers-0-to-4
            // 0x0E is kind of int flag at ZX basic. Followed by two zero's.
            // [0x0E] [0] [0] [LSB] [MSB] [0]
            // [RANDOMIZE USR] XXXXX       stored as int.
            cnt += 3;
            return *reinterpret_cast<const uint16_t*>(&p_basic_block[cnt]);
        }
    }
    return 0;
}



inline std::vector<uint16_t> TapToTurboBlocks::TryFindInBasic(const DataBlock& p_basic_block, const CheckFun &p_check_fun)
{
    std::vector<uint16_t> retval;
    int first_marker_found = 0;     // eg LOAD in LOAD "blabla" CODE
    for (int cnt = 0; cnt < p_basic_block.size(); cnt++)
    {

        auto check = p_check_fun(p_basic_block, cnt);
        if (check == 1)
        {
            auto val = TryReadNumberFromBasic(p_basic_block, cnt, 8);
            if (val >= 16384)  // && val < 65536)
            {
                retval.push_back(val);
            }
            else if(first_marker_found > 0)
            {
                retval.push_back(0);
            }
        }
        else if(check == spectrum::SCREEN_START)       // eg SCREEN$
        {
            retval.push_back(uint16_t(check));
        }
        else if(check != 0)
        {
            first_marker_found = check;
        }
        if(first_marker_found > 0)
        {
            first_marker_found--;
        }

    }
    return retval;
}



// Try to find (first) USR start address in given BASIC block
// eg RANDOMIZE USR XXXXX
// or RANDOMIZE USR VAL "XXXXX"
inline std::vector<uint16_t> TapToTurboBlocks::TryFindUsr(const DataBlock& p_basic_block)
{
    auto values = TryFindInBasic(p_basic_block, [](const DataBlock& p_basic_block, int cnt)
    {
        bool b = cnt > 1 &&
            p_basic_block[cnt] == 0xC0_byte && (  // USR
                p_basic_block[cnt - 1] == 0xf9_byte ||     // RANDOMIZE USR
                p_basic_block[cnt - 1] == 0xf5_byte ||     // PRINT USR
                p_basic_block[cnt - 1] == 0xfa_byte ||     // IF USR        // scuba dive has this
                p_basic_block[cnt - 1] == std::byte('=')); // (LET x ) = USR
        return int(b);
    });
    return values;
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
            p_basic_block[cnt - 1] == std::byte('"')) // [LOAD "] " CODE
        {
            // 2 signals LOAD "" CODE. Only then a number is not mandatory
            return 1;// + int(p_basic_block[cnt - 2] == std::byte('"'));
        }
        if(p_basic_block[cnt] == 0xEF_byte)     // LOAD
        {
            return 16;
        }
        if(cnt > 0 &&
            p_basic_block[cnt] == 0xAA_byte && // SCREEN$
            p_basic_block[cnt - 1] == std::byte('"'))
        {
            return int(spectrum::SCREEN_START); // so SCREEN$
        }
        return 0;
    });
}
