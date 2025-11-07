// ==============================================================================
// PROJECT:         zqloader
// FILE:            turboblock.h
// DESCRIPTION:     Implementation of class TurboBlock
//
// Copyright (c) 2025 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#include "turboblock.h"

TurboBlock::TurboBlock()
{
    m_data.resize(sizeof(Header));
    GetHeader().m_length = 0;
    GetHeader().m_load_address = 0;                     // where it initially loads payload, when 0 use load at basic buffer
    GetHeader().m_dest_address = 0;                     // no copy so keep at m_load_address
    GetHeader().m_compression_type = CompressionType::none;
    GetHeader().m_after_block = AfterBlock::LoadNext; // assume more follow
    GetHeader().m_clear_address = 0;
    GetHeader().m_checksum = 1;                     // checksum init val
}


/// Set given data as payload at this TurboBlock. Try to compress.
/// At header sets:
/// m_length,
/// m_compression_type,
/// RLE meta data
TurboBlock& TurboBlock::SetData(const DataBlock& p_data, CompressionType p_compression_type)
{
    m_data_size = p_data.size();
    Compressor<DataBlock>::RLE_Meta rle_meta{};
    // try inline decompression
    bool try_inline = GetHeader().m_load_address == 0 && GetHeader().m_dest_address != 0;
    uint16_t decompress_counter = 0;
    DataBlock compressed_data = TryCompress(p_data, p_compression_type, rle_meta, decompress_counter, try_inline ? 5 : 0);
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
        GetHeader().m_code_for_most = uint8_t(rle_meta.code_for_most);
        GetHeader().m_code_for_multiples = uint8_t(rle_meta.code_for_multiples);
        GetHeader().m_value_for_most = uint8_t(rle_meta.value_for_most);

        decompress_counter = Adjust16bitCounterForUseWithDjnz(decompress_counter);

        GetHeader().m_decompress_counter = decompress_counter;
#ifdef DO_COMRESS_PAIRS
        GetHeader().m_value_for_pairs = uint8_t(rle_meta.value_for_pairs);
        GetHeader().m_code_for_pairs = uint8_t(rle_meta.code_for_pairs);
#endif
        data = &compressed_data;
    }
    else // no compression
    {
        data = &p_data;
    }


    GetHeader().m_length = uint16_t(data->size());
    GetHeader().m_checksum = CalculateChecksum(*data);
    // GetHeader().m_length = uint16_t(compressed_data.size() + 2);       // @DEBUG should give ERROR
    m_data.insert(m_data.end(), data->begin(), data->end());          // append given data at m_data (after header)


    if (GetHeader().m_load_address == 0 && GetHeader().m_dest_address != 0)
    {
        // When only dest address given:
        // set load address to dest address, and dest address to 0 thus
        // loading immidiately at the correct location.
        SetLoadAddress(GetHeader().m_dest_address);
        SetDestAddress(0);
    }

    return *this;
}


// After loading a compressed block ZX spectrum needs some time to
// decompress before it can accept next block. Will wait this long after sending block.
std::chrono::milliseconds TurboBlock::EstimateHowLongSpectrumWillTakeToDecompress(int p_decompression_speed) const
{
    if (GetHeader().m_dest_address == 0)
    {
        return 10ms;        // nothing done. Yeah checksum check, end check etc. 10ms should be enough.
    }
    if (GetHeader().m_compression_type == CompressionType::rle)
    {
        return 10ms + 1ms * ((m_data_size * 1000) / (1024 * p_decompression_speed));
    }
    // Uses ldir
    return 10ms + 1ms * ((m_data_size * 1000) / (1024 * loader_tstates::ldir_speed));
}

TurboBlock& TurboBlock::DebugDump(int p_max) const
{
    auto dest = GetDestAddress();
    if (m_data_size == spectrum::SCREEN_SIZE && dest == spectrum::SCREEN_START)
    {
        std::cout << "Screen: ";
    }
    std::cout << GetHeader() << std::endl;
    if (dest)
    {
        std::cout << "Orig. data length = " << m_data_size << "\n";
        std::cout << "Compr. data length = " << GetHeader().m_length << "\n";
        std::cout << "First byte written address = " << dest << "\n";
        std::cout << "Last byte written address = " << (dest + m_data_size - 1) << "\n";
    }
    std::cout << std::endl;
    // optionally dump some data
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
    if (p_max)
    {
        std::cout << std::dec << std::endl;
    }
    return *const_cast<TurboBlock*>(this);
}

// See https://wikiti.brandonw.net/index.php?title=Z80_Optimization#Looping_with_16_bit_counter
// 'Looping with 16 bit counter'
// For decompression.
inline uint16_t TurboBlock::Adjust16bitCounterForUseWithDjnz(uint16_t p_counter)
{
    uint8_t b = p_counter & 0xff;        // lsb - used at djnz 'ld b, e'
    uint16_t counter = p_counter - 1;           // 'dec de'
    uint8_t a = (counter & 0xff00) >> 8; // msb (we use a)
    a++;                                        // 'inc d'
    return ((uint16_t(a) << 8) & 0xff00) + uint16_t(b);
}

//  Do some checks, throws when not ok.
void TurboBlock::Check() const
{
    if (GetHeader().m_length == 0 && GetHeader().m_dest_address == 0 && GetHeader().m_load_address == 0 && GetHeader().m_after_block != AfterBlock::CopyLoader)
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
    if (GetHeader().m_load_address == 0 && GetHeader().m_length != 0)
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
inline DataBlock TurboBlock::TryCompress(const DataBlock& p_data, CompressionType& p_compression_type, Compressor<DataBlock>::RLE_Meta& out_rle_meta, uint16_t& out_decompress_counter, int p_tries)
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
        using Compressor = Compressor<DataBlock>;
        //Compressor<DataBlock> compressor;


        // Compress. Also to check size
        auto try_compressed_data = Compressor::CompressInline(p_data, out_rle_meta, out_decompress_counter, p_tries);
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
            DataBlock decompressed_data = Compressor::DeCompress(compressed_data, out_rle_meta);
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
inline uint8_t TurboBlock::CalculateChecksum(const DataBlock& p_data)
{
    int8_t retval = 1;  // checksum init val @checksum
    // retval++;  // @DEBUG must give CHECKSUM ERROR
    for (const std::byte& b : p_data)
    {
        retval += int8_t(b);
        retval = ((retval >> 7) & 0x1) | (retval << 1); // in z80 this is just rlca
    }
    return uint8_t(retval);
}


std::ostream& operator << (std::ostream& p_stream, const TurboBlock::Header& p_header)
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
        << ((p_header.m_after_block == TurboBlock::AfterBlock::LoadNext)      ? "LoadNext" :
            (p_header.m_after_block == TurboBlock::AfterBlock::CopyLoader)    ? "CopyLoader" :
            (p_header.m_after_block == TurboBlock::AfterBlock::ReturnToBasic) ? "ReturnToBasic" :
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