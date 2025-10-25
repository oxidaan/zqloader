// ==============================================================================
// PROJECT:         zqloader
// FILE:            turboblock.cpp
// DESCRIPTION:     Implementation of class TurboBlocks
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#include "turboblocks.h"
#include "taploader.h"
#include "pulsers.h"
#include "spectrum_consts.h"        // SCREEN_END
#include "spectrum_types.h"         // ZxBlockType
#include "compressor.h"
#include <filesystem>
#include "spectrum_loader.h"        // CalculateChecksum
#include <bitset>
namespace fs = std::filesystem;

using namespace std::placeholders;



#ifdef _MSC_VER
#pragma warning (disable: 4456) // declaration of '' hides previous local declaration
#endif


///
/// Our turbo block as used with zqloader.z80asm.
///
class TurboBlock
{
    friend class TurboBlocks;

public:

#pragma pack(push, 1)
    struct Header
    {
        uint16_t          m_length;                                    // 0-1 length of data to receive including checksum byte
        uint16_t          m_load_address;                              // 2-3 where data will be stored initially during load (w/o header) when 0 load_address_at_basic
        uint16_t          m_dest_address;                              // 4-5 destination to copy or decompress to, when 0 do not copy/decompress
                                                                       // (stays at load address)
        CompressionType   m_compression_type;                          // 6 Type of compression-see enum
        uint8_t           m_checksum;                                  // 7 checksum
        uint16_t          m_usr_start_address = TurboBlocks::LoadNext; // 8-9
        // When LoadNext: more blocks follow. This is the default.
        // When CopyLoader is 'copy loader' command, more blocks follow.
        // When ReturnToBasic end & return to basic, do not start MC.
        // Else start MC code here as USR. Then this must be last block.

        uint16_t   m_clear_address = 0;     // 10-11 CLEAR (SP address)

        uint8_t    m_code_for_most;         // the value that occurs least, will be used to trigger RLE for 'most'
        uint16_t   m_decompress_counter;
        uint8_t    m_code_for_multiples;    // the value that occurs 2nd least, will be used to trigger RLE for triples
#ifdef DO_COMRESS_PAIRS
        uint8_t    m_code_for_pairs;        // the value that occurs 3rd least, will be used to trigger RLE for doubles
        uint8_t    m_value_for_pairs;       // the value that occurs most as isolated doubles in the block (typically 0)
#endif
        uint8_t    m_value_for_most;        // 12 RLE byte most (compression data)



        friend std::ostream& operator <<(std::ostream& p_stream, const Header& p_header)
        {
            p_stream
                << "length = " << p_header.m_length << "\n"
                << "load_address = " << p_header.m_load_address << "\n"
                << "dest_address = " << p_header.m_dest_address << "\n"
                << "compression_type = " << p_header.m_compression_type
#ifdef DO_COMRESS_PAIRS
                << " (compressing pairs)"
#endif
                << "\nchecksum = " << int(p_header.m_checksum) << "\n"
                << "After block do: "
                << ((p_header.m_usr_start_address == TurboBlocks::LoadNext) ? "LoadNext" :
                (p_header.m_usr_start_address == TurboBlocks::CopyLoader) ? "CopyLoader" :
                (p_header.m_usr_start_address == TurboBlocks::ReturnToBasic) ? "ReturnToBasic" :
                "Start MC at : " + std::to_string(p_header.m_usr_start_address)) << "\n"
                << "CLEAR address/SP = " << p_header.m_clear_address << "\n"
                << std::hex << "m_code_for_most = " << int(p_header.m_code_for_most) << ' '
                << "m_decompress_counter = " << p_header.m_decompress_counter << ' '
                << "m_code_for_multiples = " << int(p_header.m_code_for_multiples) << ' '
#ifdef DO_COMRESS_PAIRS
                << "m_code_for_pairs = " << int(p_header.m_code_for_pairs) << ' '
                << "m_value_for_pairs = " << int(p_header.m_value_for_pairs) << ' '
#endif
                << "m_value_for_most = " << int(p_header.m_value_for_most) << std::dec << ' '
            ;

            return p_stream;
        }


    };

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

    TurboBlock()
    {
        m_data.resize(sizeof(Header));
        GetHeader().m_length            = 0;
        GetHeader().m_load_address      = 0;                     // where it initially loads payload, when 0 use load at basic buffer
        GetHeader().m_dest_address      = 0;                     // no copy so keep at m_load_address
        GetHeader().m_compression_type  = CompressionType::none;
        GetHeader().m_usr_start_address = TurboBlocks::LoadNext; // assume more follow
        GetHeader().m_clear_address     = 0;
        GetHeader().m_checksum          = 1;                     // checksum init val
    }

private:

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


    /// Set machine code start address OR value from enum AfterBlock.
    /// When LoadNext or CopyLoader more blocks follow.
    /// When ReturnToBasic (and thus <> 0) this is the last block
    ///     but no RANDOMIZE USR xxxxx is done (default), instead return to BASIC
    /// When CopyLoader copy loader to screen and continue with next block.
    /// all other values do RANDOMIZE USR xxxxx thus starting machine code.
    TurboBlock& SetUsrStartAddress(uint16_t p_address)
    {
        GetHeader().m_usr_start_address = p_address;
        return *this;
    }


    // Indicate a copy to screen is needed (after loading this block)
    // Eg loader code at Spectrum will be moved to indicated location - not necessairely the screen.
    // p_dest_address copy dest location typically at screen eg SCREEN23_RD + stack
    TurboBlock& SetCopyLoader()
    {
        GetHeader().m_usr_start_address = TurboBlocks::CopyLoader;
        return *this;
    }


    /// Set address as found in CLEAR (BASIC), zqloader might use this to set Stack Pointer.
    TurboBlock& SetClearAddress(uint16_t p_address)
    {
        GetHeader().m_clear_address = p_address;
        return *this;
    }


    // See https://wikiti.brandonw.net/index.php?title=Z80_Optimization#Looping_with_16_bit_counter
    // 'Looping with 16 bit counter'
    static uint16_t Adjust16bitCounterForUseWithDjnz(uint16_t p_counter)
    {
        uint8_t b        = p_counter & 0xff;        // lsb - used at djnz 'ld b, e'
        uint16_t counter = p_counter - 1;           // 'dec de'
        uint8_t a        = (counter & 0xff00) >> 8; // msb (we use a)
        a++;                                        // 'inc d'
        return ((uint16_t(a) << 8) & 0xff00) + uint16_t(b);
    }


    /// Set given data as payload at this TurboBlock. Try to compress.
    /// At header sets:
    /// m_length,
    /// m_compression_type,
    /// RLE meta data
    TurboBlock& SetData(const DataBlock& p_data, CompressionType p_compression_type = loader_defaults::compression_type)
    {
        m_data_size = p_data.size();
        Compressor<DataBlock>::RLE_Meta rle_meta{};
        // try inline decompression
        bool try_inline             = !m_overwrites_loader && GetHeader().m_load_address == 0 && GetHeader().m_dest_address != 0;
        uint16_t decompress_counter = 0;
        DataBlock compressed_data   = TryCompress(p_data, p_compression_type, rle_meta, decompress_counter, try_inline ? 5 : 0);
        // std::cout << "Compressed as: " << compression_type << " Uncompressed size: " << p_data.size() << "; Compressed size: " << compressed_data.size() << std::endl;
        if (try_inline && p_compression_type == CompressionType::rle)
        {
            SetLoadAddress(uint16_t(GetHeader().m_dest_address + p_data.size() - compressed_data.size()));
            //  std::cout << "Using inline decompression: Setting load address to " << GetHeader().m_load_address <<
            //      " (Dest Address = " << GetHeader().m_dest_address <<
            //      " Data size = " << p_data.size() <<
            //      " Compressed size = " << compressed_data.size() << ")" << std::endl;
        }
        const DataBlock* data;

        GetHeader().m_compression_type = p_compression_type;
        if (p_compression_type == CompressionType::rle)
        {
            GetHeader().m_code_for_most      = uint8_t(rle_meta.code_for_most);
            GetHeader().m_code_for_multiples = uint8_t(rle_meta.code_for_multiples);
            GetHeader().m_value_for_most     = uint8_t(rle_meta.value_for_most);

            decompress_counter               = Adjust16bitCounterForUseWithDjnz(decompress_counter);

            GetHeader().m_decompress_counter = decompress_counter;
#ifdef DO_COMRESS_PAIRS
            GetHeader().m_value_for_pairs    = uint8_t(rle_meta.value_for_pairs);
            GetHeader().m_code_for_pairs     = uint8_t(rle_meta.code_for_pairs);
#endif
            data                             = &compressed_data;
        }
        else // no compression
        {
            data = &p_data;
        }


        GetHeader().m_length   = uint16_t(data->size());
        GetHeader().m_checksum = uint8_t(CalculateChecksum(*data));
        // GetHeader().m_length = uint16_t(compressed_data.size() + 2);       // @DEBUG should give ERROR
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
        return GetHeader().m_length == 0;
    }


    // Get (final) destination address
    uint16_t GetDestAddress() const
    {
        return GetHeader().m_dest_address == 0 ? GetHeader().m_load_address : GetHeader().m_dest_address;
    }


    // Get (final) length
    uint16_t GetLength() const
    {
        return uint16_t(m_data_size);
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
        if(!ProbablyIsFunAttribute())   // avoid excessive logging
        {
            if(p_pause_before > 0ms)
            {
                std::cout << "Pause before = " << p_pause_before.count() << "ms" << std::endl;
            }
            DebugDump();
        }

        if(p_pause_before > 0ms)
        {
            PausePulser(p_loader.GetTstateDuration()).SetLength(p_pause_before).MoveToLoader(p_loader);                 // pause before
        }
        TonePulser(p_loader.GetTstateDuration()).SetPattern(500, 500).SetLength(200ms).MoveToLoader(p_loader);      // leader
//        TonePulser().SetPattern(500).SetLength(200ms).MoveToLoader(p_loader);         // (same) leader
        TonePulser(p_loader.GetTstateDuration()).SetPattern(250).SetLength(1).MoveToLoader(p_loader);             // sync
//        PausePulser().SetLength(250).SetEdge(Edge::toggle).MoveToLoader(p_loader);      // sync (same)

        const auto &data = GetData();
        DataBlock header(data.begin(), data.begin() + sizeof(Header));      // split (eg for minisync)
        DataBlock payload(data.begin() + sizeof(Header), data.end());

        MoveToLoader(p_loader, std::move(header), p_zero_duration, p_one_duration, p_end_of_byte_delay);     // header
        if (payload.size() != 0)
        {
            MoveToLoader(p_loader, std::move(payload), p_zero_duration, p_one_duration, p_end_of_byte_delay);    // data
        }
    }


    //  Move given DataBlock (as pulsers) to loader (eg SpectrumLoader)
    //  PausePulser(minisync) + DataPulser
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader, DataBlock p_block, int p_zero_duration, int p_one_duration, int p_end_of_byte_delay)
    {
        PausePulser(p_loader.GetTstateDuration()).SetLength(500).SetEdge(Edge::toggle).MoveToLoader(p_loader); // extra mini sync before

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


    // After loading a compressed block ZX spectrum needs some time to
    // decompress before it can accept next block. Will wait this long after sending block.
    std::chrono::milliseconds EstimateHowLongSpectrumWillTakeToDecompress() const
    {
        if (GetHeader().m_dest_address == 0)
        {
            return 10ms;        // nothing done. Yeah checksum check, end check etc. 10ms should be enough.
        }
        if (GetHeader().m_compression_type == CompressionType::rle)
        {
#ifdef DO_COMRESS_PAIRS
            return m_data_size * 100ms / 4000;      // eg 0.33 sec/10kb? Just tried.
#else
            return m_data_size * 100ms / 5000;      // eg 0.33 sec/10kb? Just tried.
#endif

        }
        return m_data_size * 100ms / 20000;      // LDIR eg 0.05 sec/10kb? whatever.
    }


    // Set a flag indicating this block overwrites our zqloader code at upper memory.
    // Need to load it elsewhere first, then copy to final location. Cannot load anything after that.
    // Adjusts loadaddress and dest address accordingly.
    TurboBlock& SetOverwritesLoader(uint16_t p_upper_start_offset, uint16_t p_loader_upper_length)
    {
        m_overwrites_loader = true;
        SetLoadAddress(p_upper_start_offset);     // zqloader.asm will put it at BASIC buffer (load_address_at_basic)
        SetDestAddress(0xffff - (p_loader_upper_length - 1));
        return *this;
    }


    // Get prepared data.
    const DataBlock &GetData() const
    {
        return m_data;
    }

    // To avoid extensive logging 
    bool ProbablyIsFunAttribute() const
    {
        return (m_data_size == 256 && GetDestAddress() == spectrum::ATTR_23RD) ||
               (m_data_size == 768 && GetDestAddress() == spectrum::ATTR_BEGIN);

    }


    TurboBlock& DebugDump(int p_max = 0) const
    {
        std::cout << GetHeader() << std::endl;
        auto dest = GetDestAddress();
        if (dest)
        {
            std::cout << "Orig. data length = " << m_data_size << "\n";
            std::cout << "Compr. data length = " << GetHeader().m_length << "\n";
            std::cout << "First byte written address = " << dest << "\n";
            std::cout << "Last byte written address = " << (dest + m_data_size - 1) << "\n";
        }
        std::cout << std::endl;
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
        if(p_max)
        {
            std::cout << std::endl;
        }
        return *const_cast<TurboBlock*>(this);
    }


    //  Do some checks, throws when not ok.
    void Check() const
    {
        if( GetHeader().m_length == 0 && GetHeader().m_dest_address == 0 && GetHeader().m_load_address == 0 && GetHeader().m_usr_start_address != TurboBlocks::AfterBlock::CopyLoader)
        {
            throw std::runtime_error("Useless empty block ");
        }
        if (GetHeader().m_dest_address == 0 && GetHeader().m_load_address == 0 && GetHeader().m_length != 0)
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
        if (GetHeader().m_load_address == 0 && !m_overwrites_loader && GetHeader().m_length != 0)
        {
            throw std::runtime_error("Can not determine address where data will be loaded to");
        }
    }


    // Determine best compression, and return given data compressed
    // using that algorithm.
    // TODO now only CompressionType::rle remains. Function not really usefull.
    // Although it can also decide to use no compression eg for very small blocks (CompressionType::none)
    // p_tries: when > 0 indicates must be able to use inline decompression.
    // This not always succeeds depending on choosen RLE paramters. The retry max this time.
    DataBlock TryCompress(const DataBlock& p_data, CompressionType &p_compression_type, Compressor<DataBlock>::RLE_Meta& out_rle_meta, uint16_t &out_decompress_counter, int p_tries)
    {
        if (p_compression_type == CompressionType::automatic)
        {
            // no compression when small
            // also no compression when m_dest_address is zero: will not run decompression.
            if (p_data.size() < 200 || GetHeader().m_dest_address == 0)
            {
                p_compression_type = CompressionType::none;
                return p_data.Clone();       // done
            }
        }
        if (p_compression_type == CompressionType::rle ||
            p_compression_type == CompressionType::automatic)
        {
            DataBlock compressed_data;
            Compressor<DataBlock> compressor;


            // Compress. Also to check size
            auto try_compressed_data = compressor.CompressInline(p_data, out_rle_meta, out_decompress_counter, p_tries);
            if (!try_compressed_data)
            {
                // failed to compress inline
                p_compression_type = CompressionType::none;
                return p_data.Clone();
            }
            compressed_data = std::move(*try_compressed_data);
            if ((p_compression_type == CompressionType::automatic || p_tries != 0) && compressed_data.size() >= p_data.size())
            {
                // When automatic and it is bigger after compression... Then dont.
                std::cout << "Failed to compress!" << std::endl;
                p_compression_type = CompressionType::none;
                return p_data.Clone();
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

            p_compression_type = CompressionType::rle;
            return compressed_data;
        }
        // So p_compression_type == CompressionType::none
        return p_data.Clone();       // done
    }


    // Calculate a simple one-byte checksum over given data.
    // including header and the length fields.
    static uint8_t CalculateChecksum(const DataBlock &p_data)
    {
        int8_t retval = 1;  // checksum init val
        // retval++;  // @DEBUG must give CHECKSUM ERROR
        for (const std::byte& b : p_data)
        {
            retval += int8_t(b);
            retval  = ((retval >> 7) & 0x1) | (retval << 1); // in z80 this is just rlca
        }
        return uint8_t(retval);
    }

private:

    size_t      m_data_size{};               // size of (uncompressed/final) data. Note: Spectrum does not need this.
    bool        m_overwrites_loader = false; // will this block overwrite our loader itself?
    DataBlock   m_data;                      // the data as send to Spectrum, starts with header
}; // class TurboBlock



TurboBlocks::TurboBlocks()                             = default;
TurboBlocks::TurboBlocks(TurboBlocks &&)               = default;
TurboBlocks & TurboBlocks::operator = (TurboBlocks &&) = default;


/// Take an export file name that will be used to load symbols.
template<>
TurboBlocks& TurboBlocks::SetSymbolFilename(const fs::path& p_symbol_file_name)
{
    m_symbols.ReadSymbols(p_symbol_file_name);
    if(m_symbols.GetSymbol("HEADER_LEN") != sizeof(TurboBlock::Header))
    {
        throw std::runtime_error("TurboBlock::Header length mismatch with zqloader.z80asm");
    }
    std::cout << "Z80 loader total length = " << m_symbols.GetSymbol("TOTAL_LEN") << "\n";

    return *this;
}



TurboBlocks::~TurboBlocks() = default;

/// Add given tap file at normal speed, must be zqloader.tap.
/// Patches zqloader.tap eg BIT_LOOP_MAX etc.
/// Also loads symbol file (has same base name).
template<>
TurboBlocks& TurboBlocks::AddZqLoader(const fs::path& p_filename)
{
    std::cout << "Processing zqloader file: " << p_filename << " (normal speed)" << std::endl;

    fs::path filename_exp = p_filename;
    filename_exp.replace_extension("exp");          // zqloader.exp (symbols)
    SetSymbolFilename(filename_exp);

    TapLoader loader;
    loader.SetOnHandleTapBlock([&](DataBlock p_block, std::string )
                               {
                                   return HandleZqLoaderTapBlock(std::move(p_block));
                               });
    loader.Load(p_filename, "");
    return *this;
}



/// Get # turbo blocks added
size_t TurboBlocks::size() const
{
    return m_turbo_blocks.size() + (m_upper_block != nullptr);
}

/// Add given Datablock as TurboBlock at given address.
/// Check if zqloader (upper) is overlapped.
/// Check if zqloader (lower, at basic) is overlapped.
TurboBlocks& TurboBlocks::AddDataBlock(DataBlock&& p_block, uint16_t p_start_adr)
{
    DataBlock data_block;
    auto end_adr            = p_start_adr + p_block.size();
    // Does it overwrite our loader (at upper regions)?
    auto loader_upper_start = m_symbols.GetSymbol("ASM_UPPER_START");
    auto loader_upper_len   = m_symbols.GetSymbol("ASM_UPPER_LEN");
    // overlaps loader code at upper memory?
    if (Overlaps(p_start_adr, end_adr, loader_upper_start, loader_upper_start + loader_upper_len))
    {
        if (m_upper_block)
        {
            throw std::runtime_error("Already found a block loading to loader-overlapped region, blocks overlap");
        }
        std::cout << "Block overlaps loader at upper memory region. Adding extra block..." << std::endl;
        int size_before = int(loader_upper_start) - int(p_start_adr);
        if (size_before > 0)
        {
            data_block = DataBlock(p_block.begin(), p_block.begin() + size_before);   // the lower part, truncate
            end_adr    = p_start_adr + size_before;
        }
        DataBlock block_upper(p_block.begin() + size_before, p_block.end());  // part overwriting loader *must* be loaded last


        // *Must* be last block since our loader will be overwritten
        // So keep, then append as last.
        m_upper_block = std::make_unique<TurboBlock>();
        // puts it at BASIC buffer
        m_upper_block->SetOverwritesLoader(m_symbols.GetSymbol("ASM_UPPER_START_OFFSET"), loader_upper_len);
        m_upper_block->SetData(block_upper, m_compression_type);
    }
    else
    {
        data_block = std::move(p_block);
    }

    // block overwrites our (not yet moved) loader code at the BASIC rem (CLEAR) statement
    // SCREEN_END + 768 + 256 is an estimate of PROG; (256 is printerbuffer)
    auto prog = spectrum::PROG;   // spectrum::SCREEN_END + 768 + 256;
    if ( m_loader_copy_start == 0 && Overlaps(p_start_adr, end_adr, prog, m_symbols.GetSymbol("CLEAR")))
    {
        // set m_loader_copy_start to copy loader to m_copy_me_start_location
        m_loader_copy_start = spectrum::SCREEN_23RD;       // default when not set otherwise
        std::cout << "Block overlaps loader at BASIC (=" << prog << ", " << m_symbols.GetSymbol("CLEAR") <<
            "). Will copy loader to screen at " <<
            m_loader_copy_start + m_symbols.GetSymbol("STACK_SIZE") <<
            " (loader will use block: start = " << m_loader_copy_start <<
            " end = " << m_loader_copy_start + GetLoaderCodeLength(false) - 1 << ")" << std::endl;
    }

    // block overwrites our loader copied loader code (after copy)
    // cut it in two pieces before and after thus leaving space
    // except when it is presumably a register block
    uint16_t copy_me_end_location = m_loader_copy_start + GetLoaderCodeLength(false);
    if (m_loader_copy_start != 0 &&
        Overlaps(p_start_adr, end_adr, m_loader_copy_start, copy_me_end_location))
    {
        std::cout << "Block overlaps the copied loader code. Will split in two." << std::endl;
        auto size_before = int(m_loader_copy_start) - int(p_start_adr);
        if (size_before > 0)
        {
            // data block before loader (eg at screen)
            DataBlock b4(data_block.begin(), data_block.begin() + size_before);
            AddTurboBlock(std::move(b4), p_start_adr);
        }
        auto size_after = int(end_adr) - int(copy_me_end_location);
        if (size_after > 0)
        {
            DataBlock after(data_block.begin() + data_block.size() - size_after, data_block.end());
            AddTurboBlock(std::move(after), copy_me_end_location);
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



/// Add just a header with a 'copy to screen' command (no data)
/// Mainly for debugging!
TurboBlocks& TurboBlocks::CopyLoaderToScreen(uint16_t p_value)
{
    SetLoaderCopyTarget(p_value);
    TurboBlock empty;
    m_turbo_blocks.push_back(std::move(empty));
    return *this;
}



// private
// Add given datablock as turbo block at given start address
// to end of list of turboblocks
void TurboBlocks::AddTurboBlock(DataBlock&& p_block, uint16_t p_dest_address)
{
    if(m_turbo_blocks.size() == 0)
    {
        // when first block overlaps loader at basic, add an empty block before it
        // will get SetCopyLoader (like CopyLoaderToScreen)
        if (Overlaps(p_dest_address, p_dest_address + p_block.size(), spectrum::PROG, m_symbols.GetSymbol("CLEAR")))
        {
            TurboBlock tblock;
            m_turbo_blocks.push_back(std::move(tblock));
        }
    }
    TurboBlock tblock;
    tblock.SetDestAddress(p_dest_address);
    tblock.SetData(std::move(p_block), m_compression_type);
    m_turbo_blocks.push_back(std::move(tblock));
}



// Just add given turboblock at end
void TurboBlocks::AddTurboBlock(TurboBlock&& p_block)
{
    m_turbo_blocks.push_back(std::move(p_block));
}


/// To be called after last block was added.
/// Make sure:
///     to add upperblock (overwriting loader) as last in the chain.
///     patch loader code.
///     first block will copy loader to alternative location.
/// To be called after adding last AddDataBlock, before MoveToLoader.
/// p_usr_address: when done loading all blocks end start machine code here as in RANDOMIZE USR xxxxx
///     (When 0 return to BASIC)
/// p_clear_address: when done loading put stack pointer here, which is a bit like CLEAR xxxxx
TurboBlocks &TurboBlocks::Finalize(uint16_t p_usr_address, uint16_t p_clear_address)
{

    if(m_loader_copy_start && !IsZqLoaderAdded())
    {
        std::cout << "ZQLoader already (pre) loaded or not present, cannot patch loader copy code, will use screen to move loader to." << std::endl;
    }
    if(m_loader_copy_start && IsZqLoaderAdded())
    {
        // loader (control code) will be copied to screen for example
        // patch zqloader itself
        if(p_usr_address == 0)
        {
            throw std::runtime_error("Loader will be moved, so will stack, but at end returns to BASIC, will almost certainly crash");
        }
        uint16_t copy_me_target_location = m_loader_copy_start + m_symbols.GetSymbol("STACK_SIZE");

        SetDataToZqLoaderTap("COPY_ME_SP", copy_me_target_location);        // before new copied block
        auto copy_me_source_location = m_symbols.GetSymbol("ASM_CONTROL_CODE_START");
        auto copy_me_length          = m_symbols.GetSymbol("ASM_CONTROL_CODE_LEN");
        bool overlaps  = Overlaps(copy_me_source_location, copy_me_source_location + copy_me_length, copy_me_target_location, copy_me_target_location + copy_me_length);
        bool copy_lddr = copy_me_target_location > copy_me_source_location;
        if(!copy_lddr)    
        {
            // when false: use LDIR, copies from low to high
            // when dest address is before source when overlapping
            //   -> xxxxxxxx ->
            //  xxxxxxxx         LDIR copy_lddr==false
            std::cout << "Copy loader backwards (but LDIR working forwards) from " << copy_me_source_location << " to " << copy_me_target_location << " length=" << copy_me_length << " last = " << copy_me_target_location + copy_me_length - 1 << (overlaps ? " (overlaps)" : "") << std::endl;
            SetDataToZqLoaderTap("COPY_ME_DEST", copy_me_target_location);
            SetDataToZqLoaderTap("COPY_ME_SOURCE_OFFSET", copy_me_source_location);
            SetDataToZqLoaderTap("COPY_ME_LDDR_OR_LDIR", 0xb0ed); // LDIR! Endianness swapped
        }
        else
        {    
            // when true: use LDDR, copies from high to low
            // when dest address is after source when overlapping
            //   <- xxxxxxxx <-
            //          xxxxxxxx LDDR copy_lddr==true
            std::cout << "Copy loader forwards (but LDDR working backwards) from " << copy_me_source_location << " to " << copy_me_target_location << " length=" << copy_me_length << " last = " << copy_me_target_location + copy_me_length - 1 << (overlaps ? " (overlaps)" : "") << std::endl;
            SetDataToZqLoaderTap("COPY_ME_DEST", copy_me_target_location + copy_me_length - 1);
            SetDataToZqLoaderTap("COPY_ME_SOURCE_OFFSET", copy_me_source_location + copy_me_length - 1);
            SetDataToZqLoaderTap("COPY_ME_LDDR_OR_LDIR", 0xb8ed); // LDDR! Endianness swapped
        }

        SetDataToZqLoaderTap("COPY_ME_END_JUMP", copy_me_target_location);
        std::cout << "\n";

    }

    if (m_upper_block)
    {
        if (m_loader_copy_start)
        {
            // adjust place to temporary store last block into (that overwrites loader at upper itself)
            m_upper_block->SetLoadAddress(m_loader_copy_start + m_symbols.GetSymbol("STACK_SIZE") + m_symbols.GetSymbol("ASM_CONTROL_CODE_LEN"));
        }
        // Add upperblock (when present) as last to list
        AddTurboBlock(std::move(*m_upper_block));
        m_upper_block = nullptr;
    }

    if (m_turbo_blocks.size() > 0)
    {
        // When indicated, after loading first block, loader will be copied.
        if (m_loader_copy_start)
        {
            m_turbo_blocks.front().SetCopyLoader();
        }
        // What should be done after last block
        m_turbo_blocks.back().SetUsrStartAddress(p_usr_address ? p_usr_address : TurboBlocks::ReturnToBasic);    // now is last block
        m_turbo_blocks.back().SetClearAddress(p_clear_address);
    }

    return *this;
}



/// Move all earlier added turboblocks inluding zqloader (as normal block)
/// to given SpectrumLoader.
/// no-op when there are no blocks.
void TurboBlocks::MoveToLoader(SpectrumLoader& p_spectrumloader)
{
    std::chrono::milliseconds pause_before = 0ms;
    if(IsZqLoaderAdded())        // else probably already preloaded
    {
        p_spectrumloader.AddLeaderPlusData(std::move(m_zqloader_header), spectrum::g_tstate_quick_zero, 1750ms);
        p_spectrumloader.AddLeaderPlusData(std::move(m_zqloader_code),   spectrum::g_tstate_quick_zero, 1500ms);
        p_spectrumloader.AddPause(100ms); // time needed to start our loader after loading itself (basic!)
    }

    for (auto& tblock : m_turbo_blocks)
    {
        auto next_pause = tblock.EstimateHowLongSpectrumWillTakeToDecompress(); // b4 because moved
        std::move(tblock).MoveToLoader(p_spectrumloader, pause_before, m_zero_duration, m_one_duration, m_end_of_byte_delay);
        pause_before = next_pause;
    }
    m_turbo_blocks.clear();
}



// Length needed when loader code needs to be moved away from BASIC location
inline uint16_t TurboBlocks::GetLoaderCodeLength(bool p_with_registers) const
{
    return m_symbols.GetSymbol("STACK_SIZE") +                                 // some space for stack
           m_symbols.GetSymbol("ASM_CONTROL_CODE_LEN") +                       // our code eg decompressor
           m_symbols.GetSymbol("ASM_UPPER_LEN") +                              // needed to reserve space when overwriting data at upper block
           (p_with_registers ? m_symbols.GetSymbol("REGISTER_CODE_LEN") : 0u); // only needed when loading z80 snapshot
}



// Handle the tap block for zqloader.tap - so our loader itself.
// patch certain parameters, then move to spectrumloader (normal speed)
inline bool TurboBlocks::HandleZqLoaderTapBlock(DataBlock p_block)
{
    using namespace spectrum;
    TapeBlockType type = TapeBlockType(p_block[0]);
    if (type == TapeBlockType::header)
    {
        // can here (like otla) change the name as zx spectrum sees it
        m_zqloader_header = std::move(p_block);
    }
    else if(type == TapeBlockType::data)
    {
        m_zqloader_code = std::move(p_block);
        SetDataToZqLoaderTap("BIT_LOOP_MAX", std::byte(m_bit_loop_max));
        SetDataToZqLoaderTap("BIT_ONE_THESHLD", std::byte(m_bit_loop_max  - m_zero_max));
        SetDataToZqLoaderTap("IO_INIT_VALUE", std::byte(m_io_init_value));
        SetDataToZqLoaderTap("IO_XOR_VALUE", std::byte(m_io_xor_value));
    }

    // m_spectrumloader.AddLeaderPlusData(std::move(p_block), g_tstate_quick_zero, 1750ms);
    return false;
}



// Get corrected address at datablock as read from zqloader.tap
inline uint16_t TurboBlocks::GetZqLoaderSymbolAddress(const char* p_name) const
{
    uint16_t adr             = 1 + m_symbols.GetSymbol(p_name); // + 1 because of start byte
    uint16_t asm_upper_start = m_symbols.GetSymbol("ASM_UPPER_START");
    if (adr >= asm_upper_start)
    {
        // correct when (symbol value) is in the upper regions
        // so after DISP (eg BIT_LOOP_MAX,BIT_ONE_THESHLD)
        // because at the loaded block containing zqloader the address is lower.
        adr -= asm_upper_start;
        uint16_t asm_upper_start_offset = m_symbols.GetSymbol("ASM_UPPER_START_OFFSET");
        adr += asm_upper_start_offset;
    }
    adr -= m_symbols.GetSymbol("TOTAL_START");
    return adr;
}



// private
inline void RecalculateChecksum(DataBlock& p_block)
{
    p_block[p_block.size() - 1] = 0_byte;       // Checksum, recalculate (needs to be zero b4 CalculateChecksum)
    p_block[p_block.size() - 1] = spectrum::CalculateChecksum(0_byte, p_block);
}



/// Convenience
/// Set a 8 bit byte or 16 bit word to the TAP block that contains ZQLoader Z80. To set parameters to ZQLoader.
/// p_block: block to modify data to, should be zqloader.tap.
/// p_name: symbol name as exported by sjasmplus, see Symbols.
/// p_value: new byte value.
inline void TurboBlocks::SetDataToZqLoaderTap( const char* p_name, uint16_t p_value)
{
    uint16_t adr = GetZqLoaderSymbolAddress(p_name);
    m_zqloader_code[adr]     = std::byte(p_value & 0xff);   // z80 is little endian
    m_zqloader_code[adr + 1] = std::byte((p_value >> 8) & 0xff);
    RecalculateChecksum(m_zqloader_code);
    std::cout << "Patching word '" << p_name << "' to: " << int(p_value) << " hex= " << std::hex << int(p_value) << std::dec << std::endl;
}



inline void TurboBlocks::SetDataToZqLoaderTap( const char* p_name, std::byte p_value)
{
    uint16_t adr = GetZqLoaderSymbolAddress(p_name);
    m_zqloader_code[adr] = p_value;
    RecalculateChecksum(m_zqloader_code);
    std::cout << "Patching byte '" << p_name << "' to: " << int(p_value) << " hex= " << std::hex << int(p_value) << std::dec << " bin= " << std::bitset<8>(int(p_value)) << std::endl;
}



/// Set durations in T states for zero and one pulses.
/// When 0 keep defaults.
TurboBlocks &TurboBlocks::SetDurations(int p_zero_duration, int p_one_duration, int p_end_of_byte_delay)
{
    if(p_zero_duration != 0)
    {
        m_zero_duration = p_zero_duration;
    }
    if (p_one_duration != 0)
    {
        m_one_duration = p_one_duration;
    }
    if (p_end_of_byte_delay != 0)       
    {
        m_end_of_byte_delay = p_end_of_byte_delay;
    }
    std::cout << "Around " << (1000ms / spectrum::g_tstate_dur) / ((m_zero_duration + m_one_duration) / 2) << " bps" << std::endl;
    return *this;
}

