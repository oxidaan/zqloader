// ==============================================================================
// PROJECT:         zqloader
// FILE:            compressor.h
// DESCRIPTION:     Definition of class Compressor.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================


#pragma once

// When set compresses isolated pairs (2 sequential bytes of value 'value_for_pairs') into one byte 'code_for_pairs'
// spares around 5% or a couple of hunderd bytes on average for a game.
// I hoped this would be more because of 16x16 bit sprites might have this often, but:
// That size improvement is largely, if not entirely, undone because the decompression is now slower,
// also zqloader.tap (which is loaded at normal speed!) becomes larger.
// When changing also change same at file zqloader.z80asm!
#ifdef DO_COMRESS_PAIRS
#pragma message("Compiling with compressing algorithm: compress pairs")    
#endif

#include <optional>         // std::optional
#include <iostream>         // cout
#include <algorithm>        // std::sort
#include <vector>
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
/// 'code_for_most' and 'code_for_triples' itself are stored 2x - but are asumed 'rare'.
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
        TData   code_for_most;      // the value that occurs least, will be used as escape code for 'most'
        TData   code_for_multiples; // the value that occurs 2nd least, will be used as escape code for 'multiples'
        TData   value_for_most;     // the value that occurs most in the block (typically 0) ('most')
        TData   value_for_pairs;    // the value that occurs most as isolated pairs in the block ('pairs')
        TData   code_for_pairs;     // the value that occurs 3rd least, will be used as escape code for 'pairs'

        friend std::ostream& operator <<(std::ostream& p_stream, const RLE_Meta& p_header)
        {
            p_stream 
                << "code_for_most = " << int(p_header.code_for_most)
                << " code_for_multiples = " << int(p_header.code_for_multiples)
                << " value_for_most = " << int(p_header.value_for_most)
#ifdef DO_COMRESS_PAIRS
                << " value_for_pairs = " << int(p_header.value_for_pairs)
                << " code_for_pairs = " << int(p_header.code_for_pairs) 
#endif
                ;
            return p_stream;
        }

    };

private:

    using Hist = std::vector< std::pair<int, TData> >;
public:



    /// Compress, RLE. Return compressed data.
    /// Return RLE meta data as output parameter - not written to compressed data.
    static DataBlock Compress(const DataBlock& in_buf, RLE_Meta& out_max_min, uint16_t &out_decompress_counter)
    {
        DataBlock compressed;
        out_max_min = DetermineCompressionRleValues(in_buf.begin(), in_buf.end());
        auto it = compressed.begin();
        out_decompress_counter = Compress(in_buf.begin(), in_buf.end(), compressed, it, out_max_min);
        return compressed;
    }



    /// Compress, RLE. Only succeeds when can be compressed inline, so at same memory block.
    /// Return compressed data as optional.
    /// Tries multiple meta data values for code_for_most etc to make sure it can be decompressed inline.
    /// Return RLE meta data as output parameter - not written to compressed data.
    static std::optional<DataBlock> CompressInline(const DataBlock& in_buf, RLE_Meta& out_max_min, uint16_t &out_decompress_counter, int p_max_tries)
    {
        for (int tr = 0; tr == 0 || tr < p_max_tries; tr++)
        {
            out_max_min = DetermineCompressionRleValues(in_buf.begin(), in_buf.end(), tr);
            DataBlock compressed;
            auto it = compressed.begin();
            out_decompress_counter = Compress(in_buf.begin(), in_buf.end(), compressed, it, out_max_min);
            if (p_max_tries == 0 || CanUseDecompressionInline(in_buf, compressed, out_max_min))
            {
//                                std::cout << "Compression attempt #" << (tr + 1) << " succeeded..." << std::endl;
                return compressed;
            }
            // std::cout << "Compression attempt #" << (tr + 1) << " Failed..." << std::endl;
        }
        std::cout << "Inline compression failed" << std::endl;
        return {};  // failed...
    }





    /// Decompress given block, return decompressed data.
    /// RLE meta data is given as parameters here.
    /// Only used for testing plus CanUseDecompressionInline.
    static DataBlock DeCompress(const DataBlock& p_compressed, const RLE_Meta& p_max_min)
    {
        return DeCompress(p_compressed.begin(), p_compressed.end(), p_max_min);
    }



    /// Decompress between given iterators, return decompressed data
    /// RLE meta data is given as parameters here.
    /// Only used for testing plus CanUseDecompressionInline.
    static DataBlock DeCompress(const_iterator p_begin, const_iterator p_end, const RLE_Meta& p_max_min)
    {
        DataBlock retval;
        DeCompress(p_begin, p_end, retval, retval.begin(), p_max_min);
        return retval;
    }

private:

    // Determine RLE mata data using suplied histogram.
    static RLE_Meta DetermineCompressionRleValues(const_iterator p_begin, const_iterator p_end, int p_try = 0) 
    {
        RLE_Meta retval;

        // Determine escape codes. These are the values that occur less
        {
            auto hist = GetSortedHistogram(p_begin, p_end);
            retval.code_for_most      = ( hist.begin() + p_try     )->second; // value that occurs less, will be used to code most value
            retval.code_for_multiples = ( hist.begin() + p_try + 1 )->second; // value that occurs 2nd less, will be used to code tripples
            retval.code_for_pairs     = ( hist.begin() + p_try + 2 )->second; // value that occurs 2nd less, will be used to code tripples
        }
 
        auto hist2 = GetSortedHistogram(p_begin, p_end, 2);
        retval.value_for_pairs = hist2.back().second; 

        // Get value that occurs most in sequences of at least 3 or more. -> value_for_most
        // will be compressed as [code_for_most][#count]
        {
            auto hist = GetHistogram(p_begin, p_end, 3, true);
            SortHist(hist);
            retval.value_for_most    = hist.back().second;                   // value that occurs most, will be coded with min1
        }
        // value_for_most is same as code_for_most
        // There is even no max, dont code it - or skip
        if (retval.value_for_most == retval.code_for_most || 
            retval.value_for_most == retval.code_for_multiples)
        {
            retval.value_for_most           = TData{ 0 };
            retval.code_for_most = TData{ 1 };
        }
        return retval;
    }


    static bool IsEscapeCode(const RLE_Meta &rle, TData val) 
    {
#ifdef DO_COMRESS_PAIRS
        return val == rle.code_for_most || val == rle.code_for_multiples || val == rle.code_for_pairs;
#else
        return val == rle.code_for_most || val == rle.code_for_multiples;
#endif
    }

    // Compress block between given iterators to given output iterator.
    // RLE meta data given here as parameter.
    static uint16_t Compress(const_iterator p_begin, const_iterator p_end, DataBlock& out_buf, iterator& out_it, const RLE_Meta& p_rle)
    {
        const auto& value_for_most   = p_rle.value_for_most;     // alias (the value that occurs most in the block (typically 0))
        const auto& code_for_most    = p_rle.code_for_most;
        const auto& code_for_multiples = p_rle.code_for_multiples;
#ifdef DO_COMRESS_PAIRS
        const auto& value_for_pairs= p_rle.value_for_pairs;
        const auto& code_for_pairs = p_rle.code_for_pairs;
#endif
        uint16_t decompress_counter = 0;
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

        int most_count = 0;
        int multiple_count = 0;
        TData prev{};
        // write-out (flush) the most_count count, taking care of count equals to the min values.
        // because a duplicate min value codes for the single min value
        // and a single 'most' value is just that value.
        auto WriteMost = [&]()
        {
#ifdef DO_COMRESS_PAIRS
            if(value_for_most == value_for_pairs && most_count == 2)
            {
                Write(code_for_pairs);
                decompress_counter++;
                most_count = 0;
            }
#endif
            // keep writing single 'most' values as long as maxcount equals code_for_most or code_for_triples 
            // or we have just one or two
            while (( IsEscapeCode(p_rle, TData(most_count)) || most_count <= 2) && most_count >  0 )
            {
                --most_count;
                Write(value_for_most);
                decompress_counter++;
            }
 
            if (most_count >  0 )
            {
                Write(code_for_most);
                decompress_counter++;
                Write(TData(most_count));
                most_count = 0;
            }
        };
        auto WritePairs = [&]()
        {
#ifdef DO_COMRESS_PAIRS
            if(prev == value_for_pairs && multiple_count==2)
            {
                Write(code_for_pairs);      // write 2x value_for_pairs as code_for_pairs
                decompress_counter++;
                multiple_count = 0;
            }
#endif
        };
        auto WriteMultiples = [&]()
        {
            // keep writing single prev values as long as maxcount equals code_for_most or code_for_triples 
            // or we have just one or two
            // when multiple_count==1 just write one (previous) value.
            while ((IsEscapeCode(p_rle, TData(multiple_count)) || multiple_count <= 3) && multiple_count > 0 )
            {
                --multiple_count;
                Write(prev);
                decompress_counter++;
            }
            if (multiple_count >  0 )
            {
                Write(code_for_multiples);      
                decompress_counter++;
                Write(prev);                // Note prev can not be code_for_triples
                Write(TData(multiple_count));           
                multiple_count = 0;
            }
        };

        for (it = p_begin; it < p_end; )
        {
            auto val = Read();
            if (val == value_for_most)                // compressed with [code_for_most] [#]
            {
                WritePairs();
                WriteMultiples();                   // so flush if present; ,making prev_count 0
                if (most_count == int(GetMax<TData>())) // flush when overflow
                {
                    WriteMost();                    // max_count now 0
                }
                ++most_count;
            }
            else if(!IsEscapeCode(p_rle, val))            
            {
                WriteMost();                           // so flush if present
                if(val == prev)                          // compressed with [code_for_multiples][val][#]
                {
                    if (multiple_count == int(GetMax<TData>()))     // flush when overflow
                    {                   
                        WriteMultiples();              // multiple_count now 0
                    }
                    ++multiple_count;
                }
                else
                {
                    WritePairs();
                    WriteMultiples();           // flush if present; making prev_count 0, also writes all 'normal' values
                    multiple_count = 1;
                }
            }
            else        // escape code, write twice
            {
                WriteMost();
                WritePairs();
                WriteMultiples();
                Write(val);
                Write(val);
                decompress_counter++;
            }


            prev = val;
        }
        WritePairs();
        WriteMost();        // at end flush remaining when present
        WriteMultiples();
        return decompress_counter;
    }



    // Decompress block between given iterators to given output iterator.
    // RLE meta data is given as parameters here.
    static void DeCompress(const_iterator p_begin, const_iterator p_end, DataBlock& out_buf, iterator out_it, const RLE_Meta& p_max_min)
    {
        const auto& most             = p_max_min.value_for_most; // alias
        const auto& code_for_most    = p_max_min.code_for_most;
        const auto& code_for_multiples = p_max_min.code_for_multiples;
#ifdef DO_COMRESS_PAIRS
        const auto& code_for_pairs = p_max_min.code_for_pairs;
        const auto& value_for_pairs = p_max_min.value_for_pairs;
#endif

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



        auto WriteMost = [&]()
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
        auto WriteMultiples = [&]()
        {
            auto val = Read();
            if (val == code_for_multiples) // 2nd seen?
            {
                at_end = Write(val);
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

#ifdef DO_COMRESS_PAIRS
        auto WritePairs = [&]()
        {

            if(it < p_end)
            {
                auto val = Read();
                if (val == code_for_pairs) // 2nd seen?
                {
                    at_end = Write(val);
                    return;
                }
                it--;
            }
            Write(value_for_pairs);
            Write(value_for_pairs);
        };
#endif
        for (it = p_begin; it < p_end && !at_end ;)
        {
            auto b = Read();
            if (b == code_for_most)// && it < p_end)
            {
                WriteMost();
            }
#ifdef DO_COMRESS_PAIRS
            else if (b == code_for_pairs)// && it < p_end)
            {
                WritePairs();
            }
#endif
            else if (b == code_for_multiples)// && it < p_end)
            {
                WriteMultiples();
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
    static bool CanUseDecompressionInline(const DataBlock& p_orig_data, const DataBlock& p_compressed_data, const Compressor<DataBlock>::RLE_Meta& p_rle_meta)
    {
        DataBlock decompressed_data;
        if(p_orig_data.size() > p_compressed_data.size())
        {
            auto sz = p_orig_data.size() - p_compressed_data.size();
            decompressed_data.resize(sz);
            // append compressed data
            decompressed_data.insert(decompressed_data.end(), p_compressed_data.begin(), p_compressed_data.end());
            DeCompress(decompressed_data.cbegin() + sz, decompressed_data.cend(), decompressed_data, decompressed_data.begin(), p_rle_meta);
            return decompressed_data == p_orig_data;
        }
        return false;
    }



    static TData Read(const_iterator& p_it)
    {
        auto ret = *p_it;
        p_it++;
        return ret;
    }



    // Write one data to the output buffer at given iterator location.
    // when p_append_mode append.
    static bool Write(std::vector<TData>& out_buf, iterator& out_it, TData p_byte, bool p_append_mode = true)
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






    static Hist GetHistogram(const_iterator p_begin, const_iterator p_end, int p_sequential = 0, bool p_greater_than = false)
    {
        // key=freq, value=byte
        Hist hist{};

        // Make entries for each byte freq zero
        for (int n = 0; n < ( 1 << ( 8 * sizeof( TData ))); n++)
        {
            hist.push_back({ 0, TData(n) });
        }
        int cnt = 1;
        int prev = int(*p_begin) + 1;       // so !first
        for (auto it = p_begin; it != p_end; it++)
        {
            if(p_sequential == 0)                               // do not check sequential same values
            {
                auto& pair = hist[int(*it)];
                pair.first++;           // freq++
            }
            else if(int(*it) != prev) 
            {
                if(!p_greater_than && cnt == p_sequential)             // found exactly sequential same values
                {
                    auto& pair = hist[prev];
                    pair.first+=cnt;          
                }
                else if( p_greater_than && cnt >= p_sequential )                // more than -p_sequential
                {
                    auto& pair = hist[prev];
                    pair.first+=cnt;          
                }
                cnt = 1;
            }
            else
            {
                cnt++;              //  #values same as previous sequential values
            }
            prev = int(*it);
        }
        return hist;
    }

    static void SortHist(Hist &p_hist)
    {
        // sort on freq (at pair compare first element first)
        auto Compare = [](typename Hist::value_type p1, typename Hist::value_type p2)
        {
            return p1.first == p2.first ? p1.second > p2.second : p1.first < p2.first;
        };
        std::sort(p_hist.begin(), p_hist.end(), Compare); 
    }

    static Hist GetSortedHistogram(const_iterator p_begin, const_iterator p_end, int p_sequential = 0, bool p_greater_than = false)
    {
        auto hist = GetHistogram(p_begin, p_end, p_sequential, p_greater_than);
        SortHist(hist);
        return hist;
    }

    // Write RLE meta data to the given iterator position, thus becoming part of the compressed data.
    static void WriteRleValues(DataBlock& out_buf, iterator& out_it, const RLE_Meta& p_max_min)
    {
        Write(out_buf, out_it, p_max_min.value_for_most);
        Write(out_buf, out_it, p_max_min.value_for_pairs);
        Write(out_buf, out_it, p_max_min.code_for_pairs);
        Write(out_buf, out_it, p_max_min.code_for_multiples);
        Write(out_buf, out_it, p_max_min.code_for_most);
    }



    // Write RLE meta data from given iterator position
    static RLE_Meta ReadRleValues(const_iterator& p_it)
    {
        RLE_Meta retval;
        retval.value_for_most = Read(p_it);
        retval.value_for_pairs = Read(p_it);
        retval.code_for_pairs = Read(p_it);
        retval.code_for_multiples = Read(p_it);
        retval.code_for_most = Read(p_it);
        return retval;
    }



}; // class Compressor
