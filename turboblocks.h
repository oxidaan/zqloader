// ==============================================================================
// PROJECT:         zqloader
// FILE:            turboblock.cpp
// DESCRIPTION:     Defintion of class TurboBlocks
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once


#include "datablock.h"
#include "symbols.h"         // Symbols member
#include <memory>            // std::unique_ptr
#include <iostream>
#include "types.h"           // CompressionType
#include "loader_defaults.h"


class TurboBlock;
class SpectrumLoader;




/// This class can be seen as the c++ counterpart of the zqloader turboloader written in z80 assember.
/// Maintains a chain of 'TurboBlocks', those are datablocks that can be loaded by zqloader turbo loader.
/// Also maintains the symbols as generated by sjasmplus in an export file - see class Symbols.
/// Tasks for this class:
/// - To load a block that overlaps our Z80 loader code (zqloader.z80asm) (at upper memory)
///   at the (now unused, but equaly sized) loader code at BASIC REM statement. Also make sure it is last.
/// - Handle the situation when the loader at BASIC REM statement is overwritten.
/// - Handle situation when all is overwritten eg at snapshots (then move loader code to lower part of screen)
/// - Handle the USR start address for chain of blocks so zqloader.z80asm knows.
class TurboBlocks
{
public:

    // What to do after a block was loaded, stored at m_usr_start_address
    enum AfterBlock : uint16_t
    {
        LoadNext      = 256,        // go on to next block (H=1 at z80)
        CopyLoader    = 512,        // copy z80 loader code, then go on to next block (H=2 at z80)
        ReturnToBasic = 768,        // return to basic (H=3 at z80)
        // all other values are like RANDOMIZE USR xxxxx so start MC there (and this was last block)
    };

public:

    /// CTORs
    TurboBlocks() ;
    TurboBlocks(TurboBlocks &&);
    TurboBlocks(const TurboBlocks &) = delete;

    TurboBlocks & operator = (TurboBlocks &&);
    TurboBlocks & operator = (const TurboBlocks &) = delete;

    /// DTOR
    ~TurboBlocks();


    /// Load given file at normal speed, typically loads zqloader.tap.
    template <class TPath>
    TurboBlocks& AddZqLoader(const TPath& p_filename);

    ///  Is ZqLoader added (with AddZqLoader above)?
    bool IsZqLoaderAdded() const
    {
        return m_zqloader_code.size() != 0;
    }

    /// Get # turbo blocks added
    size_t size() const;

    /// Add given Datablock as Turboblock at given address.
    /// Check if zqloader (upper) is overlapped.
    /// Check if zqloader (lower, at basic) is overlapped.
    TurboBlocks& AddDataBlock(DataBlock&& p_block, uint16_t p_start_adr);


    /// Convenience. Add a Datablock as Turboblock at given symbol name address (see class Symbols).
    TurboBlocks& AddDataBlock(DataBlock&& p_block, const std::string& p_symbol)
    {
        return AddDataBlock(std::move(p_block), m_symbols.GetSymbol(p_symbol));
    }

    /// p_usr_address: when done loading all blocks end start machine code here as in RANDOMIZE USR xxxx
    /// p_clear_address: when done loading put stack pointer here, which is a bit like CLEAR xxxx
    TurboBlocks& Finalize(uint16_t p_usr_address, uint16_t p_clear_address = 0);

    /// Move all added turboblocks to SpectrumLoader as given at CTOR.
    /// Call after Finalize.
    void MoveToLoader(SpectrumLoader& p_spectrumloader);


    /// Set durations in T states for zero and one pulses.
    TurboBlocks& SetDurations(int p_zero_duration, int p_one_duration, int p_end_of_byte_delay);

    auto GetDurations() const
    {
        return std::tuple{m_zero_duration, m_one_duration, m_end_of_byte_delay};
    }


    /// Set this ZQLoader parameter
    TurboBlocks& SetBitLoopMax(int p_value)
    {
        m_bit_loop_max = p_value;
        return *this;
    }


    /// Set this ZQLoader parameter
    TurboBlocks& SetBitOneThreshold(int p_value)
    {
        m_bit_one_threshold = p_value;
        return *this;
    }


    /// Set compression type.
    TurboBlocks& SetCompressionType(CompressionType p_compression_type)
    {
        m_compression_type = p_compression_type;
        return *this;
    }




    /// Add just a header with a 'copy to screen' command (no data)
    /// Mainly for debugging!
    TurboBlocks& CopyLoaderToScreen(uint16_t p_value);

    /// Set start of free space to copy loader including space for sp.
    TurboBlocks& SetLoaderCopyTarget(uint16_t p_value )
    {
        m_loader_copy_start = p_value;
        return *this;
    }



    /// Convenience public read access to Symbols as loaded by CTOR.
    const Symbols& GetSymbols() const
    {
        return m_symbols;
    }


    // Length needed when loader code needs to be moved away from BASIC location
    uint16_t GetLoaderCodeLength(bool p_with_registers) const;
   
   template <class TPath>
    TurboBlocks&SetSymbolFilename(const TPath &p_symbol_file_name);
private:



    bool HandleZqLoaderTapBlock(DataBlock p_block);

    // Get corrected address at datablock as read from zqloader.tap
    uint16_t GetZqLoaderSymbolAddress(const char* p_name) const;


    // Convenience
    // Set a 8 bit byte or 16 bit word to the TAP block that contains ZQLoader Z80. To set parameters to ZQLoader.
    // p_block: block to modify data to, should be zqloader.tap.
    // p_name: symbol name as exported by sjasmplus, see Symbols.
    // p_value: new byte value.
    void SetDataToZqLoaderTap(const char* p_name, uint16_t p_value);
    void SetDataToZqLoaderTap(const char* p_name, std::byte p_value);


    // Add given datablock with given start address
    void AddTurboBlock(DataBlock&& p_block, uint16_t p_dest_address);

    // Just add given turboblock at end.
    void AddTurboBlock(TurboBlock&& p_block);

private:

    DataBlock                     m_zqloader_header;               // standard zx header for zqloader
    DataBlock                     m_zqloader_code;                 // block with entire code for zqloader
    std::vector<TurboBlock>       m_turbo_blocks;                  // turbo blocks to load
    std::unique_ptr<TurboBlock>   m_upper_block;                   // when a block is found that overlaps our loader (nullptr when not) must be loaded last
    uint16_t                      m_loader_copy_start = 0;         // start of free space were our loader can be copied to, begins with stack, then Control code copied from basic
    CompressionType               m_compression_type        = loader_defaults::compression_type;
    Symbols                       m_symbols;                       // named symbols as read from EXP file
    int                           m_zero_duration           = loader_defaults::zero_duration;
    int                           m_one_duration            = loader_defaults::one_duration;
    int                           m_end_of_byte_delay       = loader_defaults::end_of_byte_delay;
    int                           m_bit_loop_max            = loader_defaults::bit_loop_max;   
    int                           m_bit_one_threshold       = loader_defaults::bit_one_threshold;
}; // class TurboBlocks



// Do 2 blocks overlap?
template <class T1, class T2, class T3, class T4>
static bool Overlaps(T1 p_start, T2 p_end, T3 p_start2, T4 p_end2)
{
    return (int(p_end) > int(p_start2)) && (int(p_start) < int(p_end2));
}

