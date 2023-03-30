// ==============================================================================
// PROJECT:         zqloader
// FILE:            compressor.h
// DESCRIPTION:     Definition of class Compressor.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================


#pragma once


#include <optional>         // std::optional
#include <iostream>         // cout
#include <algorithm>        // std::sort
#include "byte_tools.h"


///
/// Compressor. Uses RLE compression:
/// In block to compress find:
/// most used (byte) value -> 'most'
/// and the two least used byte values. -> 'code_for_most' and 'code_for_triples'.
/// Then compress:
/// 2 or more sequential of 'most' (typically zero) is compressed by prefixing
/// 'code_for_most', then number of repeats (#repeats cannot be 'code_for_most')
/// (A single byte value 'most' is left untouched).
/// 3 or more sequential of any value are compressed by prefixing
/// 'code_for_triples'; then that byte value; then number of repeats (#repeats cannot be 'code_for_triples')
/// 'code_for_most' and 'code_for_triples' are stored 2x.
/// Larger blocks (eg repeat more than max value for TData) are just created as two blocks.
///
template <class TDataBlock>
class Compressor
{
public:

    using DataBlock      = TDataBlock;
    using TData          = typename TDataBlock::value_type;         // usually std::byte
    using iterator       = typename TDataBlock::iterator;
    using const_iterator = typename TDataBlock::const_iterator;
    struct RLE_Meta
    {
        TData   most;             // the value that occurs most
        TData   code_for_most;    // the value that occurs least, will be used to trigger RLE for max1
        TData   code_for_triples; // the value that occurs 2nd least, will be used to trigger RLE for triples
    };

private:

    using Hist = std::vector< std::pair<int, TData> >;

public:

    /// Compress, RLE. Return compressed data.
    /// RLE meta data is also written to compressed data.
    DataBlock Compress(const DataBlock& in_buf)
    {
        DataBlock compressed;
        auto max_min = DetermineCompressionRleValues(in_buf.begin(), in_buf.end());
        auto it      = compressed.begin();
        WriteRleValues(compressed, it, max_min);
        Compress(in_buf.begin(), in_buf.end(), compressed, it, max_min);
        return compressed;
    }



    /// Compress, RLE. Return compressed data.
    /// Return RLE meta data as output parameter - not written to compressed data.
    DataBlock Compress(const DataBlock& in_buf, RLE_Meta& out_max_min)
    {
        DataBlock compressed;
        out_max_min = DetermineCompressionRleValues(in_buf.begin(), in_buf.end());
        auto it = compressed.begin();
        Compress(in_buf.begin(), in_buf.end(), compressed, it, out_max_min);
        return compressed;
    }



    /// Compress, RLE. Only succeeds when can be compressed inline, so at same memory block.
    /// Return compressed data as optional.
    /// Tries multiple meta data values for min1, min2 to make sure it can be decompressed inline.
    /// Return RLE meta data as output parameter - not written to compressed data.
    std::optional<DataBlock> Compress(const DataBlock& in_buf, RLE_Meta& out_max_min, int p_max_tries)
    {
        if (p_max_tries == 0)
        {
            return Compress(in_buf, out_max_min);
        }

        Hist hist = GetSortedHistogram(in_buf.begin(), in_buf.end());
        for (int tr = 0; tr < p_max_tries; tr++)
        {
            out_max_min = DetermineCompressionRleValues(hist, tr);
            DataBlock compressed;
            auto it = compressed.begin();
            Compress(in_buf.begin(), in_buf.end(), compressed, it, out_max_min);
            if (CanUseDecompressionInline(in_buf, compressed, out_max_min))
            {
                //                std::cout << "Compression attempt #" << (tr + 1) << " succeeded..." << std::endl;
                return compressed;
            }
            // std::cout << "Compression attempt #" << (tr + 1) << " Failed..." << std::endl;
        }
        std::cout << "Inline compression failed" << std::endl;
        return {};  // failed...
    }



    /// Compress, RLE. Return compressed data.
    /// Use RLE on given fixed p_max1 value with here determined out_min1.
    /// For Load-RLE where p_max must be zero.
    DataBlock Compress(const DataBlock& in_buf, TData p_max1, TData& out_min1)
    {
        DataBlock compressed;
        auto max_min = DetermineCompressionRleValues(in_buf.begin(), in_buf.end());     // only for max_min.min1
        max_min.max1 = p_max1;
        out_min1     = max_min.min1;
        auto it = compressed.begin();
        Compress(in_buf.begin(), in_buf.end(), compressed, it, max_min);
        return compressed;
    }



    /// Decompress given block, return decompressed data.
    /// RLE meta data is read from compressed buffer.
    DataBlock DeCompress(const DataBlock& p_compressed)
    {
        return DeCompress(p_compressed.begin(), p_compressed.end());
    }



    /// Decompress between given iterators, return decompressed data
    /// RLE meta data is read from compressed buffer.
    DataBlock DeCompress(const_iterator p_begin, const_iterator p_end)
    {
        DataBlock retval;
        auto min_max = ReadRleValues(p_begin);
        DeCompress(p_begin, p_end, retval, retval.begin(), min_max);
        return retval;
    }



    /// Decompress given block, return decompressed data.
    /// RLE meta data is given as parameters here.
    DataBlock DeCompress(const DataBlock& p_compressed, const RLE_Meta& p_max_min)
    {
        return DeCompress(p_compressed.begin(), p_compressed.end(), p_max_min);
    }



    /// Decompress between given iterators, return decompressed data
    /// RLE meta data is given as parameters here.
    DataBlock DeCompress(const_iterator p_begin, const_iterator p_end, const RLE_Meta& p_max_min)
    {
        DataBlock retval;
        DeCompress(p_begin, p_end, retval, retval.begin(), p_max_min);
        return retval;
    }

private:

    /// Compress block between given iterators to given output iterator.
    /// RLE meta data given here as parameter.
    void Compress(const_iterator p_begin, const_iterator p_end, DataBlock& out_buf, iterator& out_it, const RLE_Meta& p_max_min)
    {
        const auto& most             = p_max_min.most; // alias
        const auto& code_for_most    = p_max_min.code_for_most;
        const auto& code_for_triples = p_max_min.code_for_triples;


        const_iterator it;
        bool append_mode = ( out_it == out_buf.end());
        auto Read        = [&]()
                           {
                               return Compressor::Read(it);
                           };
        auto Write = [&](TData p_byte)
                     {
                         Compressor::Write(out_buf, out_it, p_byte, append_mode);
                     };

        TData max_count{};
        TData prev_count{};
        TData prev{};
        // write-out (flush) the max count, taking care of count equals to the min values.
        // because a duplicate min value codes for the single min value
        // and a single max value is just that value.
        auto WriteMax = [&]()
        {
            // keep writing single max values as long as maxcount equals min1 or min2 or we have just one
            while (( max_count == code_for_most || max_count == code_for_triples || max_count == TData(1)) && max_count > TData{ 0 })
            {
                --max_count;
                Write(most);
            }
            if (max_count > TData{ 0 })
            {
                Write(code_for_most);
                Write(max_count);
                max_count = TData{ 0 };
            }
        };
        auto WriteTriples = [&]()
        {
            // keep writing single prev values as long as maxcount equals min1 or min2 or we have just one
            while (( prev_count == code_for_most || prev_count == code_for_triples || prev_count <= TData(2)) && prev_count > TData{ 0 })
            {
                --prev_count;
                Write(prev);
            }
            if (prev_count > TData{ 0 })
            {
                Write(code_for_triples);
                Write(prev);
                Write(prev_count);
                prev_count = TData{ 0 };
            }
        };

        for (it = p_begin; it < p_end; )
        {
            auto b = Read();
            if (b == most)
            {
                WriteTriples();                   // so flush 2nd max if there
                if (max_count == GetMax<TData>()) // flush when overflow
                {
                    WriteMax();                   // max1count now 0
                }
                ++max_count;
            }
            else if (b == prev && b != code_for_most && b != code_for_triples)
            {
                WriteMax();         // so flush 1st max if there
                if (prev_count == GetMax<TData>())
                {                   // flush when overflow
                    WriteTriples(); // max2count now 0
                }
                ++prev_count;
            }
            else
            {
                WriteMax();
                WriteTriples();
                if (b == code_for_most || b == code_for_triples)
                {
                    Write(b);       // write the 2x times
                }
                Write(b);
            }
            prev = b;
        }
        WriteMax();        // at end flush remaining when present
        WriteTriples();
    }



    /// Decompress block between given iterators to given output iterator.
    /// RLE meta data is given as parameters here.
    void DeCompress(const_iterator p_begin, const_iterator p_end, DataBlock& out_buf, iterator out_it, const RLE_Meta& p_max_min)
    {
        const auto& most             = p_max_min.most; // alias
        const auto& code_for_most    = p_max_min.code_for_most;
        const auto& code_for_triples = p_max_min.code_for_triples;

        const_iterator it;
        bool at_end      = false;

        bool append_mode = ( out_it == out_buf.end());
        auto Read        = [&]()
        {
            return Compressor::Read(it);
        };

        auto Write = [&](TData p_byte)
        {
            return Compressor::Write(out_buf, out_it, p_byte, append_mode);
        };



        auto WriteMax = [&]()
        {
            auto cnt = Read();
            if (cnt == code_for_most) // 2nd seen?
            {
                at_end = Write(code_for_most);
            }
            else
            {
                for (int n = 0; n < int(cnt) && !at_end; n++)
                {
                    at_end = Write(most);
                }
            }
        };
        auto WriteTriples = [&]()
        {
            auto val = Read();
            if (val == code_for_triples) // 2nd seen?
            {
                at_end = Write(code_for_triples);
            }
            else
            {
                auto cnt = Read();
                for (int n = 0; n < int(cnt) && !at_end; n++)
                {
                    at_end = Write(val);
                }
            }
        };

        for (it = p_begin; it < p_end && !at_end; )
        {
            auto b = Read();
            if (b == code_for_most && it < p_end)
            {
                WriteMax();
            }
            else if (b == code_for_triples && it < p_end)
            {
                WriteTriples();
            }
            else
            {
                at_end = Write(b);
            }
        }
    }



    // Check if can use de-compress in same memory location as compressed data.
    // Where compressed data is stored at end of block, being overwritten during decompression.
    // By just trying.
    bool CanUseDecompressionInline(const DataBlock& p_orig_data, const DataBlock& p_compressed_data, const Compressor<DataBlock>::RLE_Meta& p_rle_meta)
    {
        DataBlock decompressed_data;
        auto sz = p_orig_data.size() - p_compressed_data.size();
        decompressed_data.resize(sz);
        // append compressed data
        decompressed_data.insert(decompressed_data.end(), p_compressed_data.begin(), p_compressed_data.end());
        DeCompress(decompressed_data.cbegin() + sz, decompressed_data.cend(), decompressed_data, decompressed_data.begin(), p_rle_meta);
        return decompressed_data == p_orig_data;
    }



    TData Read(const_iterator& p_it)
    {
        auto ret = *p_it;
        p_it++;
        return ret;
    }



    // Write one data to the output buffer at given iterator location.
    // when given iterator is end() append.
    bool Write(std::vector<TData>& out_buf, iterator& out_it, TData p_byte, bool p_append_mode = true)
    {
        if (p_append_mode)
        {
            out_buf.push_back(p_byte);      // append mode
            out_it = out_buf.end();
            return false;
        }
        else if (out_it != out_buf.end())
        {
            *out_it = p_byte;
            out_it++;
            return false;
        }
        return true;
    }



    // Determine RLE mata data from buffer defined by given iterators.
    RLE_Meta DetermineCompressionRleValues(const_iterator p_begin, const_iterator p_end)
    {
        return DetermineCompressionRleValues(GetSortedHistogram(p_begin, p_end), 0);
    }



    Hist GetSortedHistogram(const_iterator p_begin, const_iterator p_end)
    {
        Hist hist{};
        // Make entries for each byte freq zero
        for (int n = 0; n < ( 1 << ( 8 * sizeof( TData ))); n++)
        {
            hist.push_back({ 0, TData(n) });
        }
        // Determine byte frequencies
        for (auto it = p_begin; it != p_end; it++)
        {
            auto& pair = hist[int(*it)];
            pair.first++;
        }

        auto Compare = [](typename Hist::value_type p1, typename Hist::value_type p2)
        {
            return p1.first == p2.first ? p1.second > p2.second : p1.first < p2.first;
        };
        std::sort(hist.begin(), hist.end(), Compare);    // on freq (at pair compare first element first)

        return hist;

    }



    // Determine RLE mata data using suplied histogram.
    RLE_Meta DetermineCompressionRleValues(const Hist& p_hist, int p_try)
    {
        TData max               = p_hist.back().second;                   // value that occurs most, will be coded with min1
        TData code_for_most     = ( p_hist.begin() + p_try )->second;     // value that occurs less, will be used to code most value
        TData code_for_tripples = ( p_hist.begin() + p_try + 1 )->second; // value that occurs 2nd less, will be used to code tripples

        // There is even no max, dont code it - or skip
        if (max == code_for_most || max == code_for_tripples)
        {
            max           = TData{ 0 };
            code_for_most = TData{ 1 };
        }
        return { max, code_for_most, code_for_tripples };

    }



    // Write RLE meta data to the given iterator position, thus becoming part of the compressed data.
    void WriteRleValues(DataBlock& out_buf, iterator& out_it, const RLE_Meta& p_max_min)
    {
        Write(out_buf, out_it, p_max_min.most);
        Write(out_buf, out_it, p_max_min.min1);
        Write(out_buf, out_it, p_max_min.min2);
    }



    // Write RLE meta data from given iterator position
    RLE_Meta ReadRleValues(const_iterator& p_it)
    {
        RLE_Meta retval;
        retval.most = Read(p_it);
        retval.min1 = Read(p_it);
        retval.min2 = Read(p_it);
        return retval;
    }



}; // class Compressor
