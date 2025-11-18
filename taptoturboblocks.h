// ==============================================================================
// PROJECT:         zqloader
// FILE:            taptoturboblocks.h
// DESCRIPTION:     Definition of class TapToTurboBlocks.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once


#include "datablock.h"
#include "turboblocks.h"
#include "spectrum_types.h"     // ZxHeader
#include <functional>

/// Loads one or more tap blocks.
/// Tries to read data from BASIC blocks (eg USR start address)
/// HandleTapBlock function feeds incoming tab blocks to the TurboBlocks as given at CTOR.
class TapToTurboBlocks
{
public:

    // CTOR taking TurboBlocks as sink.
    explicit TapToTurboBlocks(TurboBlocks& p_tblocks) :
        m_tblocks(p_tblocks)
    {}


    /// Handle incoming loaded (tap) data block.
    /// When header: check name; store as last header.
    /// When data: based on last header header:
    ///     When basic: try get addresses like RANDOMIZE USR XXXXX
    ///     When code: Add to list of blocks to (turbo) load as given at CTOR.
    /// p_zxfilename: when !empty check if name matches before accepting header.
    bool HandleTapBlock(DataBlock p_block, const std::string &p_zxfilename);

    /// Get MC start address as found in BASIC block as in RANDOMIZE USR xxxxx
    /// (As earlier found with TryFindUsr)
    uint16_t GetUsrAddress() const
    {
        return m_usr;
    }


    /// Get CLEAR address as found in BASIC block as in CLEAR xxxxx
    /// (As earlier found with TryFindClear)
    uint16_t GetClearAddress() const
    {
        return m_clear;
    }

    auto GetNumberLoadCode() const
    {
        return m_loadcodes.size();
    }

private:
    void ParseBasic(const DataBlock& p_basic_block);

    // Try to find (first) USR start address in given BASIC block
    // eg RANDOMIZE USR XXXXX
    // or RANDOMIZE USR VAL "XXXXX"
    static std::vector<uint16_t> TryFindUsr(const DataBlock& p_basic_block);

    // Try to find (first) CLEAR XXXXX address in given BASIC block
    // eg CLEAR XXXXX
    // or CLEAR VAL "XXXXX"
    static uint16_t TryFindClear(const DataBlock& p_basic_block);

    // Try to find all LOAD "" CODE XXXXX addresses in given BASIC block
    static std::vector<uint16_t>  TryFindLoadCode(const DataBlock& p_basic_block);


    // Try to find one ore more numbers in given BASIC block.
    // CheckFun/p_check_fun must return true when matching a certain pattern to search for (depending on what to search for)
    // then looks for the number that follows.
    // Used by TryFindUsr / TryFindClear / TryFindLoadCode.
    using CheckFun = std::function<int (const DataBlock&, int cnt)>;
    static std::vector<uint16_t> TryFindInBasic(const DataBlock& p_basic_block, const CheckFun &p_check_fun);


    // read a number from basic either as VAL "XXXXX" or a 2 byte int.
    // 0 when failed/not found.
    // (note when it is truly 0 makes no sense like RANDOMIZE USR 0, CLEAR 0, LOAD "" CODE 0)
    static uint16_t TryReadNumberFromBasic(const DataBlock& p_basic_block, int p_cnt, int p_max);

private:

    spectrum::TapeHeader      m_last_header{};
    spectrum::TapeBlockType   m_last_block_type = spectrum::TapeBlockType::unknown;      // not header or data
    int                       m_headercnt  = 0;
    int                       m_codecount  = 0;
    int                       m_basic_was_parsed = false;
    TurboBlocks&              m_tblocks;
    uint16_t                  m_usr        = 0;      // [RANDOMIZE] USX xxxxx found in BASIC
    uint16_t                  m_clear      = 0xff4a; // CLEAR xxxxx as found in BASIC. Default value here is SP found with debugger
    std::vector< uint16_t>    m_loadcodes;           // multiple LOAD "" CODE xxxx found in BASIC
};
