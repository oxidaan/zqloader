//==============================================================================
// PROJECT:         zqloader
// FILE:            taptoturboblocks.h
// DESCRIPTION:     Definition of class TapToTurboBlocks.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once


#include "taploader.h"
#include "turboblock.h"
#include "spectrum_types.h"     // ZxHeader

/// Loads one or more tap blocks.
/// Tries to read data from BASIC blocks (eg USR start address)
/// HandleTapBlock function feeds incoming tab blocks to the TurboBlocks as given at CTOR.
class TapToTurboBlocks 
{
public:
    // CTOR taking TurboBlocks as sink.
    TapToTurboBlocks(TurboBlocks& p_tblocks) :
        m_tblocks(p_tblocks)
    {}


    /// Handle incoming loaded (tap) data block.
    /// When header: check name; store as last header.
    /// When data: based on last header header:
    ///     When basic: try get addresses like RANDOMIZE USR XXXXX
    ///     When code: Add to list of blocks to (turbo) load as given at CTOR.
    bool HandleTapBlock(DataBlock p_block, std::string p_zxfilename);

    /// Get MC start address as found in BASIC block as in RANDOMIZE USR xxxxx
    /// (As earlier found with TryFindUsr)
    uint16_t GetUsrAddress() const
    {
        return m_usr == 0 ? int(TurboBlocks::ReturnToBasic) : m_usr;
    }

    /// Get CLEAR address as found in BASIC block as in CLEAR xxxxx
    /// (As earlier found with TryFindClear)
    uint16_t GetClearAddress() const
    {
        return m_clear;
    }
private:

    // read a number from basic either as VAL "XXXXX" or a 2 byte int.
    // 0 when failed/not found. 
    // (note when it is truly 0 makes no sence like RANDOMIZE USR 0, CLEAR 0, LOAD "" CODE 0) 
    static uint16_t TryReadNumberFromBasic(const DataBlock& p_basic_block, int p_cnt);


    // Try to find one ore more numbers in given BASIC block.
    // p_check_fun must return true when matching a certain pattern to search for (depeding on what to search for)
    // then looks for the number that follows.
    // Used by TryFindUsr / TryFindClear / TryFindLoadCode.
    template <class TCheckFun>
    static std::vector<uint16_t> TryFindInBasic(const DataBlock& p_basic_block, TCheckFun p_check_fun)
    {
        int seen = 0;       // When >0 recently saw pattern to search for
        std::vector<uint16_t> retval;
        for (int cnt = 0; cnt < p_basic_block.size(); cnt++)
        {
            //   std::cout << std::hex << int(c) << ' ' << c << ' ';
            if (seen)
            {
                auto val = TryReadNumberFromBasic(p_basic_block, cnt);
                if (val >= 16384 )  // && val < 65536)
                {
                    retval.push_back(val);
                    seen = 0;
                }
                seen--;
            }

            if (p_check_fun(p_basic_block, cnt))
            {
                seen = 8;       // skip some chars searching for number after code(eg the value stored as ASCII)
            }
        }
        return retval;
    }


    // Try to find (first) USR start address in given BASIC block
    // eg RANDOMIZE USR XXXXX
    // or RANDOMIZE USR VAL "XXXXX"
    static uint16_t TryFindUsr(const DataBlock& p_basic_block);

    // Try to find (first) CLEAR XXXXX address in given BASIC block
    // eg CLEAR XXXXX
    // or CLEAR VAL "XXXXX"
    static uint16_t TryFindClear(const DataBlock& p_basic_block);

    // Try to find all LOAD "" CODE XXXXX addresses in given BASIC block
    static std::vector<uint16_t>  TryFindLoadCode(const DataBlock& p_basic_block);
private:
    ZxHeader m_last_header;
    ZxBlockType m_last_block = ZxBlockType::unknown;        // not header or data
    int m_headercnt = 0;
    int m_codecount = 0;
    TurboBlocks& m_tblocks;
    uint16_t m_usr = 0;                         // [RANDOMIZE] USX xxxxx found in BASIC
    uint16_t m_clear = 0;                       // CLEAR xxxxx found in BASIC
    std::vector< uint16_t> m_loadcodes;         // multiple LOAD "" CODE xxxx found in BASIC
};
