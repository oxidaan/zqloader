// ==============================================================================
// PROJECT:         zqloader
// FILE:            turboblock.h
// DESCRIPTION:     Definition of class TurboBlock
//
// Copyright (c) 2025 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once

#include "datablock.h"
#include "compressor.h"
#include "types.h"          // CompressionType
#include "loader_defaults.h"
#include "pulsers.h"
#include <string>

///
/// Our turbo block as used with zqloader.z80asm.
/// Has header and data.
///
class TurboBlock
{

public:
    // What to do after a block was loaded, stored at m_usr_start_address
    enum class AfterBlock : uint16_t
    {
        LoadNext        = 0x1 << 8,       // go on to next block (H=1 at z80)
        CopyLoader      = 0x2 << 8,       // copy z80 loader code, then go on to next block (H=2 at z80)
        ReturnToBasic   = 0x3 << 8,       // return to basic (H=3 at z80)
        BankSwitch      = 0x4 << 8,       // zx spectrum 128 bank switch command (H=4)
        // all other values are like RANDOMIZE USR xxxxx so start MC there (and this was last block)
    };

private:
#pragma pack(push, 1)
    struct Header
    {
        uint16_t          m_length;                                    // 0-1 length of data to receive including checksum byte
        uint16_t          m_load_address;                              // 2-3 where data will be stored initially during load (w/o header) when 0 load_address_at_basic
        uint16_t          m_dest_address;                              // 4-5 destination to copy or decompress to, when 0 do not copy/decompress
                                                                       // (stays at load address)
        CompressionType   m_compression_type;                          // 6 Type of compression-see enum
        uint8_t           m_checksum;                                  // 7 checksum
        union
        {
            uint16_t   m_usr_start_address{};
            AfterBlock m_after_block; // 8-9
        };
        // When LoadNext: more blocks follow. This is the default.
        // When CopyLoader is 'copy loader' command, more blocks follow.
        // When ReturnToBasic end & return to basic, do not start MC.
        // Else start MC code here as USR. Then this must be last block.

        union
        {
            uint16_t   m_clear_address = 0;     // 10-11 CLEAR (SP address)
            uint16_t   m_bank_to_switch;        // 10-11 bank to switch to (Zx Spectrum 128)
        };

        uint8_t    m_code_for_most;         // the value that occurs least, will be used to trigger RLE for 'most'
        uint16_t   m_decompress_counter;
        uint8_t    m_code_for_multiples;    // the value that occurs 2nd least, will be used to trigger RLE for triples
#ifdef DO_COMRESS_PAIRS
        uint8_t    m_code_for_pairs;        // the value that occurs 3rd least, will be used to trigger RLE for doubles
        uint8_t    m_value_for_pairs;       // the value that occurs most as isolated doubles in the block (typically 0)
#endif
        uint8_t    m_value_for_most;        // 12 RLE byte most (compression data)



    };
    friend std::ostream& operator <<(std::ostream& p_stream, const Header& p_header);

#pragma pack(pop)
#ifdef DO_COMRESS_PAIRS
    static_assert(sizeof(Header) == 19, "Check size of Header");
#else
    static_assert(sizeof(Header) == 17, "Check size of Header");
#endif


public:

    // only for std::unique_ptr
    TurboBlock(const TurboBlock&) = delete;

    TurboBlock(TurboBlock&&)      = default;

    ~TurboBlock()                 = default;

    TurboBlock();

    /// Set given data as payload at this TurboBlock. Try to compress.
    /// At header sets:
    /// m_length,
    /// m_compression_type,
    /// RLE meta data
    TurboBlock& SetData(const DataBlock& p_data, CompressionType p_compression_type = loader_defaults::compression_type);

public:



    /// Used to check against HEADER_LEN
    static size_t GetHeaderSize() 
    {
        return sizeof(Header);
    }

    /// Set address as found in CLEAR (BASIC), zqloader might use this to set Stack Pointer.
    TurboBlock& SetClearAddress(uint16_t p_address)
    {
        GetHeader().m_clear_address = p_address;
        return *this;
    }


    /// Set address where data is initially loaded.
    /// When 0 will take dest address (unless SetOverwritesLoader was called)
    TurboBlock& SetLoadAddress(uint16_t p_address)
    {
        GetHeader().m_load_address = p_address;
        return *this;
    }


    /// Set address where data will be copied / decompressed to after loading.
    /// (so final location).
    /// When 0 do not copy/decompress; block stays at load address.
    TurboBlock& SetDestAddress(uint16_t p_address)
    {
        GetHeader().m_dest_address = p_address;
        return *this;
    }

    /// Set machine code start address to be executed after loading this block.
    /// (This makes the current block the last block)
    /// As if RANDOMIZE USR xxxxx thus starting machine code.
    /// Can also be AfterBlock::ReturnToBasic
    /// shares union with SetAfterBlockDo
    TurboBlock& SetUsrStartAddress(uint16_t p_address)
    {
        GetHeader().m_usr_start_address = p_address;
        return *this;
    }

    /// Set bank nr for NEXT block (to switch after loading this block) (128 K)
    /// Calls  SetAfterBlockDo(AfterBlock::BankSwitch);
    TurboBlock& SwitchBankTo(int p_bank)
    {
        if(p_bank >= 0)
        {
            GetHeader().m_bank_to_switch = uint16_t(p_bank);
            SetAfterBlockDo(AfterBlock::BankSwitch);
        }
        return *this;
    }

    /// Indicate what to do after block was loaded:
    /// AfterBlock::LoadNext: load next block (this is the default)
    /// AfterBlock::CopyLoader: the loader code need to be copied (to make space) after loading this block, then load next.
    /// AfterBlock::ReturnToBasic: Return to basic (so this is last block)
    /// AfterBlock::BanckSwitch: Switch to bank as given at SetBank, then load next.
    /// shares union with SetUsrStartAddress
    TurboBlock& SetAfterBlockDo(AfterBlock p_what)
    {
        if(GetHeader().m_after_block != AfterBlock::LoadNext)
        {
            throw std::runtime_error("After block command already set to " + std::to_string(int(GetHeader().m_after_block)));
        }
        GetHeader().m_after_block = p_what;
        return *this;
    }


    TurboBlock& SetSkipPilot(bool p_to_what)
    {
        m_skip_pilot = p_to_what;
        return *this;
    }

    AfterBlock GetAfterBlockDo() const
    {
        return GetHeader().m_after_block;
    }
    
    /// Move this TurboBlock (as pulsers) to given loader (eg SpectrumLoader).
    /// Give it leader+sync as used by zqloader.z80asm.
    /// Move all to given loader eg SpectrumLoader.
    /// p_pause_before this is important when ZX Spectrum needs some time to decompress.
    /// Give this an estimate how long it takes to handle previous block.
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader, std::chrono::milliseconds p_pause_before, int p_zero_duration, int p_one_duration, int p_end_of_byte_delay)
    {
        Check();

        if(p_pause_before > 0ms)
        {
            PausePulser(p_loader.GetTstateDuration()).SetLength(p_pause_before).MoveToLoader(p_loader);           // pause before
        }
        TonePulser(p_loader.GetTstateDuration()).SetPattern(500, 500).SetLength(m_skip_pilot ? 20ms : 200ms).MoveToLoader(p_loader);    // leader; best to have even number of edges
        TonePulser(p_loader.GetTstateDuration()).SetPattern(250, 499).SetLength(1).MoveToLoader(p_loader);        // sync + 499=minisync!



        DataBlock header(m_data.begin(), m_data.begin() + sizeof(Header));      // split (eg for minisync)
        DataBlock payload(m_data.begin() + sizeof(Header), m_data.end());

        MoveToLoader(p_loader, std::move(header), p_zero_duration, p_one_duration, p_end_of_byte_delay);     // header
        if (payload.size() != 0)
        {
            TonePulser(p_loader.GetTstateDuration()).SetPattern(501).SetLength(1).MoveToLoader(p_loader);       //  501=minisync!
           // PausePulser(p_loader.GetTstateDuration()).SetLength(500).SetEdge(Edge::toggle).MoveToLoader(p_loader); //  minisync!
            MoveToLoader(p_loader, std::move(payload), p_zero_duration, p_one_duration, p_end_of_byte_delay);     // data
        }
    }



   // After loading a compressed block ZX spectrum needs some time to
   // decompress before it can accept next block. Will wait this long after sending block.
    std::chrono::milliseconds EstimateHowLongSpectrumWillTakeToDecompress(int p_decompression_speed) const;


    TurboBlock& DebugDump(int p_max = 0) const;


private:

    // See https://wikiti.brandonw.net/index.php?title=Z80_Optimization#Looping_with_16_bit_counter
    // 'Looping with 16 bit counter'
    // For decompression.
    static uint16_t Adjust16bitCounterForUseWithDjnz(uint16_t p_counter);


    // Get (final) destination address
    uint16_t GetDestAddress() const
    {
        return GetHeader().m_dest_address == 0 ? GetHeader().m_load_address : GetHeader().m_dest_address;
    }




    //  Move given DataBlock (as pulsers) to loader (eg SpectrumLoader)
    //  PausePulser(minisync) + DataPulser
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader, DataBlock p_block, int p_zero_duration, int p_one_duration, int p_end_of_byte_delay)
    {
        // already done! PausePulser(p_loader.GetTstateDuration()).SetLength(500).SetEdge(Edge::toggle).MoveToLoader(p_loader); // extra minisync before

        DataPulser(p_loader.GetTstateDuration())                      // data
            .SetZeroPattern(p_zero_duration)                              // works with ONE_MAX 12 ZERO_MAX 4
            .SetOnePattern(p_one_duration)
            .SetEndOfByteDelay(p_end_of_byte_delay)
            .SetData(std::move(p_block))
            .MoveToLoader(p_loader);
    }


    Header& GetHeader()
    {
        return *(reinterpret_cast<Header*>(m_data.data()));
    }
    const Header& GetHeader() const
    {
        return *(reinterpret_cast<const Header*>(m_data.data()));
    }

        


    //  Do some checks, throws when not ok.
    void Check() const;


    // Determine best compression, and return given data compressed
    // using that algorithm.
    // TODO now only CompressionType::rle remains. Function not really usefull.
    // Although it can also decide to use no compression eg for very small blocks (CompressionType::none)
    // p_tries: when > 0 indicates must be able to use inline decompression.
    // This not always succeeds depending on choosen RLE paramters. The retry max this time.
    DataBlock TryCompress(const DataBlock& p_data, CompressionType &p_compression_type, Compressor<DataBlock>::RLE_Meta& out_rle_meta, uint16_t &out_decompress_counter, int p_tries);


    // Calculate a simple one-byte checksum over given data.
    // including header and the length fields.
    static uint8_t CalculateChecksum(const DataBlock &p_data);

private:

    size_t      m_data_size{};               // size of (uncompressed/final) data exl. header. Note: Spectrum does not need this.
    bool        m_skip_pilot = false;        // has pilot tone + sync
    DataBlock   m_data;                      // the data as send to Spectrum, starts with header
}; // class TurboBlock


