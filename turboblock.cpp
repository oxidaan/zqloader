//==============================================================================
// PROJECT:         zqloader
// FILE:            turboblock.cpp
// DESCRIPTION:     Implementation of class TurboBlocks
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#include "turboblock.h"
#include "pulsers.h"
#include "compressor.h"
#include <filesystem>
namespace fs = std::filesystem;



#ifdef _MSC_VER
#pragma warning (disable: 4456) // declaration of '' hides previous local declaration
#endif


///
/// Our turbo block as used with qloader.z80asm.
/// 
class TurboBlock
{
    friend class TurboBlocks;
public:
    // What to do after loading a turboblock




#pragma pack(push, 1)
    struct Header
    {
        uint16_t m_length;                  // 0-1 length of data to receive including checksum byte, might be less than payload when rle_at_load
        uint16_t m_load_address;            // 2-3 where data will be stored initially during load (w/o header)
        uint16_t m_dest_address;            // 4-5 destination to copy or decompress to, when 0 do not copy/decompress 
        // (stays at load address)
        //     when compressed size of compressed data block. This is NOT the final/decompressed length.
        CompressionType m_compression_type; // 8 Type of compression-see enum
        uint8_t m_checksum;               // 9 RLE marker byte for load-RLE, when 0 do not use load-RLE
        uint16_t m_usr_start_address = TurboBlocks::LoadNext;   // 10-11 When 0: more blocks follow.
        // Else when>1 start mc code here as USR. Then this must be last block.
        // When 1 last block but do not do jp to usr.
        uint16_t m_clear_address = 0;       // 12-13 CLEAR and/or SP address
        uint8_t m_rle_most;                 // 14 RLE byte most
        uint8_t m_rle_min1;                 // 16 RLE byte min1
        uint8_t m_rle_min2;                 // 17 RLE byte min2
        uint8_t  m_copy_to_screen;          // when true it is a 'copy loader to screen' command

        friend std::ostream& operator <<(std::ostream& p_stream, const Header& p_header)
        {
            p_stream << "length = " << p_header.m_length << std::endl
                << "load_address = " << p_header.m_load_address << std::endl
                << "dest_address = " << p_header.m_dest_address << std::endl
                << "compression_type = " << p_header.m_compression_type << std::endl
                << "checksum = " << int(p_header.m_checksum) << std::endl
                << "usr_start_address = " << p_header.m_usr_start_address << std::endl
                << "m_clear_address = " << p_header.m_clear_address << std::endl
                << "m_rle_most = " << int(p_header.m_rle_most) << ' '
                << "m_rle_min1 = " << int(p_header.m_rle_min1) << ' '
                << "m_rle_min2 = " << int(p_header.m_rle_min2) << ' ' << std::endl
                << "m_copy_to_screen = " << int(p_header.m_copy_to_screen) << ' '
                //   << " is_last = " << int(p_header.m_is_last);
                ;

            return p_stream;
        }
    };
#pragma pack(pop)
    static_assert(sizeof(Header) == 16, "Check size of Header");
public:
    TurboBlock(const TurboBlock&) = delete;
    TurboBlock(TurboBlock&&) = default;
    TurboBlock()
    {
        m_data.resize(sizeof(Header));
        GetHeader().m_length = 0;
        GetHeader().m_load_address = 0;                 // where it initially loads payload, when 0 use load at basic buffer
        GetHeader().m_dest_address = 0;                 // no copy so keep at m_load_address
        GetHeader().m_compression_type = CompressionType::none;
        GetHeader().m_usr_start_address = TurboBlocks::ReturnToBasic;        // last block for now 
        GetHeader().m_clear_address = 0;
        GetHeader().m_checksum = 0;                    // MUST be zero at first so dont affect calc. itself
        GetHeader().m_copy_to_screen = false;
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
    /// When 0 do not copy/decompress, stays at load address.
    TurboBlock& SetDestAddress(uint16_t p_address)
    {
        GetHeader().m_dest_address = p_address;
        return *this;
    }


    /// When 0 more blocks follow.
    /// When <> 0 this is the last block:
    ///  When 1 (and thus <> 0) this is the last block but no RANDOMIZE USR xxxxx is done. (default)
    ///  When <> 0 and <> 1 this is last block, and will jump to this address as if RANDOMIZE USR xxxxx.
    TurboBlock& SetUsrStartAddress(uint16_t p_address)
    {
        GetHeader().m_usr_start_address = p_address;
        return *this;
    }

    /// Set address as found in CLEAR (BASIC), qloader might use this to set Stack Pointer.
    TurboBlock& SetClearAddress(uint16_t p_address)
    {
        GetHeader().m_clear_address = p_address;
        return *this;
    }



    /// Set given data as payload at this TurboBlock. Try to compress.
    /// At header sets:
    /// m_length,
    /// m_compression_type,
    /// RLE meta data
    TurboBlock& SetData(const DataBlock& p_data, CompressionType p_compression_type = CompressionType::automatic)
    {
        m_data_size = p_data.size();
        Compressor<DataBlock>::RLE_Meta rle_meta;
        // try inline decompression
        bool try_inline = !m_overwrites_loader && GetHeader().m_load_address == 0 && GetHeader().m_dest_address != 0;
        auto pair = TryCompress(p_data, p_compression_type, rle_meta, try_inline ? 5 : 0);
        CompressionType compression_type = pair.first;
        DataBlock&       compressed_data = pair.second;
        // std::cout << "Compressed as: " << compression_type << " Uncompressed size: " << p_data.size() << "; Compressed size: " << compressed_data.size() << std::endl;
        if (try_inline && compression_type == CompressionType::rle)
        {
            SetLoadAddress(uint16_t(GetHeader().m_dest_address + p_data.size() - compressed_data.size()));
            //  std::cout << "Using inline decompression: Setting load address to " << GetHeader().m_load_address <<
            //      " (Dest Address = " << GetHeader().m_dest_address <<
            //      " Data size = " << p_data.size() <<
            //      " Compressed size = " << compressed_data.size() << ")" << std::endl;
        }
        const DataBlock* data;
        if (compression_type == CompressionType::rle)
        {
            GetHeader().m_rle_most = uint8_t(rle_meta.most);
            GetHeader().m_rle_min1 = uint8_t(rle_meta.code_for_most);
            GetHeader().m_rle_min2 = uint8_t(rle_meta.code_for_triples);
            GetHeader().m_compression_type = compression_type;
            data = &compressed_data;
        }
        else // none
        {
            GetHeader().m_compression_type = compression_type;
            data = &p_data;
        }


        GetHeader().m_length = uint16_t(data->size());
        // GetHeader().m_length = uint16_t(compressed_data.size() + 2);       // @DEBUG should give ERROR
        // GetHeader().m_length = uint16_t(compressed_data.size() );       // @DEBUG should give CHECKSUM ERROR
        m_data.insert(m_data.end(), data->begin(), data->end());          // append given data at m_data (after header)




        if (!m_overwrites_loader && GetHeader().m_load_address == 0 && GetHeader().m_dest_address != 0)
        {
            // When only dest address given:
            // set load address to dest address, and dest address to 0 thus
            // loading immidiately at the correct location.
            SetLoadAddress(GetHeader().m_dest_address);
            SetDestAddress(0);
        }

        return *this;
    }

    bool IsEmpty() const
    {
        return  GetHeader().m_length == 0;
    }



    /// Move this TurboBlock to given loader (eg SpectrumLoader).
    /// Give it leader+sync as used by qloader.z80asm.
    /// Move all to given loader eg SpectrumLoader.
    /// p_pause_before this is important when ZX Spectrum needs some time to decompress.
    /// Give this an estimate how long it takes to handle previous block.
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader, std::chrono::milliseconds p_pause_before, int p_zero_duration, int p_one_duration)
    {
        std::cout << "Pause before = " << p_pause_before.count() << "ms" << std::endl;

        PausePulser().SetLength(p_pause_before).MoveToLoader(p_loader);                 // pause before
        TonePulser().SetPattern(600, 600).SetLength(200ms).MoveToLoader(p_loader);      // leader
        TonePulser().SetPattern(300).SetLength(1).MoveToLoader(p_loader);               // sync

        auto data = GetData();
        DataBlock header(data.begin(), data.begin() + sizeof(Header));      // split
        DataBlock payload(data.begin() + sizeof(Header), data.end());

        MoveToLoader(p_loader, std::move(header), p_zero_duration, p_one_duration);
        if (payload.size() != 0)
        {
            MoveToLoader(p_loader, std::move(payload), p_zero_duration, p_one_duration);
        }
    }

    TurboBlock& DebugDump(int p_max = 0) const
    {
        std::cout << GetHeader() << std::endl;
        std::cout << "Orig. data length = " << m_data_size << std::endl;
        std::cout << "Compr. data length = " << GetHeader().m_length << std::endl;
        auto dest = GetHeader().m_dest_address != 0 ? GetHeader().m_dest_address : GetHeader().m_load_address;
        std::cout << "First byte written address = " << dest << std::endl;
        std::cout << "Last byte written address = " << (dest + m_data_size - 1) << "\n" << std::endl;
        for (int n = 0; (n < m_data.size()) && (n < p_max); n++)
        {
            if (n % 32 == 0 || (n == sizeof(Header)))
            {
                if (n != 0)
                {
                    std::cout << std::endl;
                }
                if (n == sizeof(Header))
                {
                    std::cout << std::endl;
                }
                std::cout << " DB ";
            }
            else
            {
                std::cout << ", ";
            }
            std::cout << std::hex << "0x" << int(m_data[n]);
        }
        std::cout << std::endl;
        return *const_cast<TurboBlock*>(this);
    }
private:

    //  Move given DataBlock to loader (eg SpectrumLoader)
    //  PausePulser(minisync) + DataPulser
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader, DataBlock p_block, int p_zero_duration, int p_one_duration)
    {
        PausePulser().SetLength(500).SetEdge(Edge::toggle).MoveToLoader(p_loader);      // extra mini sync before

        DataPulser()        // data
            .SetBlockType(ZxBlockType::raw)
            //            .SetStartBitDuration(380)
            .SetZeroPattern(p_zero_duration)          // works with ONE_MAX 12 ONE_MIN 4
            .SetOnePattern(p_one_duration)
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

    // After loading a compressed block ZX spectrum needs some time to 
    // decompress before it can accept next block. WIll wait this long after sending block.
    std::chrono::milliseconds EstimateHowLongSpectrumWillTakeToDecompress() const
    {
        if (GetHeader().m_dest_address == 0)
        {
            return 10ms;        // nothing done. Yeah checksum check, end check etc. 10ms should be enough.
        }
        if (GetHeader().m_compression_type == CompressionType::rle)
        {
            return (m_data_size * 100ms / 3000);      // eg 0.33 sec/10kb? Just tried.
        }
        return (m_data_size * 100ms / 20000);      // LDIR eg 0.05 sec/10kb? whatever.
    }

    // Set a flag indicating this block overwrites our qloader code at upper memory.
    // Need to load it elsewhere first, then copy to final location. Cannot load anything after that.
    // Adjusts loadaddress and dest address accordingly.
    TurboBlock& SetOverwritesLoader(uint16_t p_loader_upper_length)
    {
        m_overwrites_loader = true;
        SetLoadAddress(0);     // qloader.asm will put it at BASIC buffer
        SetDestAddress(0xffff - (p_loader_upper_length - 1));
        return *this;
    }

    TurboBlock& SetCopyToScreen()
    {
        GetHeader().m_copy_to_screen = true;
        return *this;
    }
    // Move prepared data out of here. Append checksum.
    // Do some final checks and calculate checksum.
    // Makes data empty (moved away from)
    DataBlock GetData()
    {
        Check();
        GetHeader().m_checksum = CalculateChecksum();
        DebugDump(0);
        return std::move(m_data);
    }



    //  Do some checks, throws when not ok.
    void Check() const
    {
        if (GetHeader().m_dest_address == 0 && GetHeader().m_load_address == 0)
        {
            throw std::runtime_error("Both destination address to copy to and load address are zero");
        }
        if (GetHeader().m_dest_address == 0 && GetHeader().m_compression_type == CompressionType::rle)
        {
            throw std::runtime_error("Destination address to copy to after load is 0 while a RLE compression is set, will not decompress");
        }
        if (m_overwrites_loader && GetHeader().m_usr_start_address == TurboBlocks::LoadNext)
        {
            throw std::runtime_error("Block will overwrite our loader, but is not last");
        }
        if (GetHeader().m_load_address == 0 && !m_overwrites_loader)
        {
            throw std::runtime_error("Can not determine address where data will be loaded to");
        }
    }



    // Determine best compression, and return given data compressed  
    // using that algorithm.
    // TODO now only CompressionType::rle remains. Function not really usefull.
    // p_tries: when > 0 indicates must be able to use inline decompression.
    // This not always succeeds depending on choosen RLE paramters. The retry max this time.
    std::pair<CompressionType, DataBlock >
    TryCompress(const DataBlock& p_data, CompressionType p_compression_type, Compressor<DataBlock>::RLE_Meta& out_rle_meta, int p_tries)
    {
        if (p_compression_type == CompressionType::automatic)
        {
            // no compression when small
            // also no compression when m_dest_address is zero: will not run decompression.
            if (p_data.size() < 256 || GetHeader().m_dest_address == 0)
            {
                return { CompressionType::none, p_data.Clone() };       // done
            }
        }
        if (p_compression_type == CompressionType::rle ||
                p_compression_type == CompressionType::automatic)
        {
            DataBlock compressed_data;
            Compressor<DataBlock> compressor;


            // Compress. Also to check size
            auto try_compressed_data = compressor.Compress(p_data, out_rle_meta, p_tries);
            if (!try_compressed_data)
            {
                return { CompressionType::none, p_data.Clone() };
            }
            compressed_data = std::move(*try_compressed_data);
            // When automatic and it is bigger after compression... Then dont.
            if ((p_compression_type == CompressionType::automatic || p_tries != 0) && compressed_data.size() >= p_data.size())
            {
                return { CompressionType::none, p_data.Clone() };
            }


            // @DEBUG
            {
                Compressor<DataBlock> compressor;
                DataBlock decompressed_data = compressor.DeCompress(compressed_data, out_rle_meta);
                if (decompressed_data != p_data)
                {
                    throw std::runtime_error("Compression algorithm error!");
                }
            }
            // @DEBUG


            return { CompressionType::rle , std::move(compressed_data) };
        }
        return { p_compression_type, p_data.Clone() };
    }

    // Calculate a simple one-byte checksum over data.
    // including header and the length fields.
    uint8_t CalculateChecksum() const
    {
        int8_t retval = int8_t(sizeof(Header));
        //        int8_t retval = 1+ int8_t(sizeof(Header));  // @DEBUG must give CHECKSUM ERROR
        for (const std::byte& b : m_data)
        {
            retval += int8_t(b);
        }
        return uint8_t(-retval);
    }

private:
    size_t m_data_size;     // size of (uncompressed/final) data. Note: Spectrum does not need this.
    bool m_overwrites_loader = false;       // will this block overwrite our loader itself?
    DataBlock m_data;       // the data as send to Spectrum, starts with header
};








template<>
TurboBlocks::TurboBlocks(const fs::path &p_symbol_file_name) :
    m_symbols(p_symbol_file_name)
{
}


TurboBlocks::~TurboBlocks() = default;


/// Add given Datablock as TurboBlock at given address.
/// Check if qloader (upper) is overlapped.
/// Check if qloader (lower, at basic) is overlapped.
TurboBlocks& TurboBlocks::AddDataBlock(DataBlock&& p_block, uint16_t p_start_adr)
{
    auto end_adr = p_start_adr + p_block.size();
    // Does it overwrite our loader (at upper regions)?
    auto loader_upper_len = m_symbols.GetSymbol("ASM_UPPER_LEN");
    DataBlock data_block;
    if (Overlaps(p_start_adr, end_adr, 1 + 0xffff - loader_upper_len, 1 + 0xffff))
    {
        if (m_upper_block)
        {
            throw std::runtime_error("Already found a block loading to loader-overlapped region, blocks overlap");
        }
        std::cout << "Block Overlaps our loader! Adding extra block..." << std::endl;
        int size2 = int(loader_upper_len + end_adr - (1 + 0xffff));
        int size1 = int(p_block.size() - size2);
        if (size1 > 0)
        {
            data_block = DataBlock(p_block.begin(), p_block.begin() + size1);   // the lower part
            end_adr = p_start_adr + data_block.size();
        }
        DataBlock block_upper(p_block.begin() + size1, p_block.end());  // part overwriting loader *must* be loaded last


        // *Must* be last block since our loader will be overwritten
        // So keep, then append as last.
        m_upper_block = std::make_unique<TurboBlock>();
        m_upper_block->SetOverwritesLoader(loader_upper_len);        // puts it at BASIC buffer

        m_upper_block->SetData(block_upper, m_compression_type);
    }
    else
    {
        data_block = std::move(p_block);
    }

    // block overwrites our loader code at the BASIC rem (CLEAR) statement
    if (Overlaps(p_start_adr, end_adr, SCREEN_END + 768, m_symbols.GetSymbol("CLEAR")))
    {
        // add some signal to copy loader to screen at 16384+4*1024
        m_loader_at_screen = true;
    }

    // block overwrites our loader code at screen
    // cut it in two pieces before and after
    // except when it is presumably a register block
    if (m_loader_at_screen &&
        Overlaps(p_start_adr, end_adr, SCREEN_23RD, SCREEN_END))
    {
        auto size_before = (SCREEN_23RD - int(p_start_adr));
        if (size_before > 0)
        {
            // max 4k before loader at screen
            DataBlock screen_4k(data_block.begin(), data_block.begin() + size_before);
            AddTurboBlock(std::move(screen_4k), p_start_adr);
        }
        auto size_after = int(end_adr) - SCREEN_END;
        if (size_after > 0)
        {
            DataBlock after_screen(data_block.begin() + data_block.size() - size_after, data_block.end());
            AddTurboBlock(std::move(after_screen), SCREEN_END);
        }
        if (size_before <= 0 && size_after <= 0)
        {
            // asume this is a snapshot/register block
            AddTurboBlock(std::move(data_block), p_start_adr);
        }
    }
    else
    {
        AddTurboBlock(std::move(data_block), p_start_adr);
    }
    return *this;
}

// Add given datablock with given start address
void TurboBlocks::AddTurboBlock(DataBlock&& p_block, uint16_t p_dest_address)
{
    TurboBlock tblock;
    tblock.SetDestAddress(p_dest_address);
    tblock.SetData(std::move(p_block), m_compression_type);
    AddTurboBlock(std::move(tblock));
}

// Add given turboblock
void TurboBlocks::AddTurboBlock(TurboBlock&& p_block)
{
    if (m_turbo_blocks.size() > 0)
    {
        m_turbo_blocks.back().SetUsrStartAddress(TurboBlocks::LoadNext);    // set 0 at previous block: so more blocks follow
    }
    m_turbo_blocks.push_back(std::move(p_block));
}


void TurboBlocks::MoveToLoader(SpectrumLoader& p_loader, uint16_t p_usr_address, uint16_t p_clear_address)
{
    std::cout << std::endl;
    if (m_upper_block)
    {
        AddTurboBlock(std::move(*m_upper_block));        // add upperblock as last to list
    }
    if (m_turbo_blocks.size() > 0)
    {
        if (m_loader_at_screen)
        {
            m_turbo_blocks.front().SetCopyToScreen();
        }
        m_turbo_blocks.back().SetUsrStartAddress(p_usr_address);    // now is last block
        m_turbo_blocks.back().SetClearAddress(p_clear_address);
    }
    auto pause_before = 100ms;        // time needed to start our loader after loading itself (basic!)
    for (auto& tblock : m_turbo_blocks)
    {
        auto next_pause = tblock.EstimateHowLongSpectrumWillTakeToDecompress(); // b4 because moved
        std::move(tblock).MoveToLoader(p_loader, pause_before, m_zero_duration, m_one_duration);
        pause_before = next_pause;
    }
    m_turbo_blocks.clear();
}


TurboBlocks &TurboBlocks::SetDurations(int p_zero_duration, int p_one_duration)
{
    m_zero_duration = p_zero_duration;
    m_one_duration = p_one_duration;
    std::cout << "Around " << (1000ms / m_tstate_dur) / ((m_zero_duration + m_one_duration) / 2) << " bps" << std::endl;
    return *this;
}
