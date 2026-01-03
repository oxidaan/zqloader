// ==============================================================================
// PROJECT:         zqloader
// FILE:            samplesender.h
// DESCRIPTION:     Definition of class MemoryBlock.
//
// Copyright (c) 2025 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once

#include "datablock.h"
#include <list>
#include <algorithm>       // clamp

// Stores a DataBlock + (spectrum compatible) start (destination) address
// Also bank number.
struct MemoryBlock
{
    int GetStartAddress() const
    {
        return m_address;
    }
    int size() const
    {
        return uint16_t(m_datablock.size());
    }
    int GetEndAddress() const
    {
        return  m_address + int(m_datablock.size());
    }
    DataBlock m_datablock;
    int m_address{};
    int m_bank = -1;        // <0: no back set eg 48k ZX Spectrum
};

using MemoryBlocks = std::list<MemoryBlock>;



// Find next start or end address in all given blocks - starting at p_search_from.
// return address plus a counter giving the balance/difference between start and and address found *at that* location.
// counter 1: found single begin addres; -1 found single end addres.
// Return <0 for address when not found (p_search_from reached end)
// (when there is overlap)
inline std::pair<int, int> FindNextAddress(const MemoryBlocks &p_memory_blocks, int p_search_from)
{
    int lowest_adr = -1;
    int how_much = 0;       // balance counter
    auto UpdateLowest=[&](int adr, int to_add)
    {
            if( adr >= p_search_from)
            {
                // found an address less than lowest, or first
                if(adr < lowest_adr || lowest_adr < 0)
                {
                    lowest_adr = adr;
                    how_much = to_add;
                }
                else if(adr == lowest_adr)
                {
                    how_much += to_add;
                }
            }
    };
    for(const auto &block: p_memory_blocks)
    {
        if(block.m_bank < 0)
        {
            UpdateLowest(block.GetStartAddress(), 1);
            UpdateLowest(block.GetEndAddress(), -1);
        }
    }
    return {lowest_adr, how_much};
}


// Compact/simplifies given memory blocks.
// Combines directly adjacent memory blocks into one.
// Combines overlapping memory blocks, content of last has highest priority.
// Result is sorted form low address to high address.
inline MemoryBlocks Compact(MemoryBlocks p_memory_blocks)
{
    MemoryBlocks retval;
    DataBlock mem_combined;
    mem_combined.resize(64*1024);
    // dump in *loading* order at mem_combined thus taking care of overlaps.
    for(const auto &block: p_memory_blocks)
    {
        if(block.m_bank < 0)        // 128K bank switch block. See end.
        {
            mem_combined.resize(std::max(size_t(block.m_address + block.size()), mem_combined.size()));        // can only grow
            // cpy datablock->mem_combined
            std::copy( block.m_datablock.begin(), block.m_datablock.end(), mem_combined.begin() + block.m_address);
        }
    }
    
    int counter = 0;
    int search_from = 0;
    int found_start{};
    while(true)
    {
        auto [found_address, how_much] = FindNextAddress(p_memory_blocks, search_from);
        //std::cout << "how_much = " << how_much << " counter = " << counter << " newcounter = " << counter+how_much << " found_address = " << found_address << " search_from = " << search_from <<   std::endl;
        if(found_address < 0)
        {
            break;      // done
        }
        if(how_much > 0)        // found start address
        {
            if(counter == 0)
            {
                found_start = uint16_t(found_address);
            }
        }
        counter += how_much;
        if(how_much < 0)
        {
            if(counter < 0)
            {
                throw std::runtime_error("Found end address before start address");
            }
            else if(counter == 0)
            {
                MemoryBlock b;
                b.m_address = found_start;
                b.m_datablock.resize(found_address - found_start);
                //std::cout << "found_start = " << found_start << " found_address(end) = " << found_address << " len = " << found_address - found_start << std::endl;
                // cpy mem_combined -> m_datablock
                std::copy(mem_combined.begin() + found_start, mem_combined.begin() + found_address, b.m_datablock.begin());
                retval.push_back(std::move(b));
            }
        }
        search_from = found_address + 1;
    }
    // Add 128K banks at end.
    for(auto &block: p_memory_blocks)
    {
        if(block.m_bank >= 0)
        {
            retval.push_back(std::move(block));
        }
    }
    return retval;
}

// Split DataBlock in two
inline std::pair<DataBlock, DataBlock> SplitBlock(const DataBlock& p_data, size_t p_start)
{
    p_start = std::min(p_start, p_data.size());
    DataBlock first(p_data.begin(), p_data.begin() + p_start);
    DataBlock second(p_data.begin() + p_start, p_data.end());
    return {std::move(first), std::move(second)};
}

// Split DataBlock in three
inline std::tuple<DataBlock, DataBlock, DataBlock> SplitBlock3(const DataBlock& p_data, size_t p_start, size_t p_end)
{
    // Clamp indices to valid range
    p_start = std::min(p_start, p_data.size());
    p_end = std::min(p_end, p_data.size());

    DataBlock first(p_data.begin(), p_data.begin() + p_start);
    DataBlock second(p_data.begin() + p_start, p_data.begin() + p_end);
    DataBlock third(p_data.begin() + p_end, p_data.end());

    return {std::move(first), std::move(second), std::move(third)};
}

// Split MemoryBlock in two
inline std::pair<MemoryBlock, MemoryBlock> SplitBlock(const MemoryBlock& p_data, int p_start)
{
    // adjust, now from DataBlock start; also check.
    int start = p_start > p_data.GetStartAddress() ? p_start - p_data.GetStartAddress() : 0; 
    MemoryBlock first;
    MemoryBlock second;
    std::tie(first.m_datablock, second.m_datablock) = SplitBlock(p_data.m_datablock, start);
    first.m_address  = p_data.m_address;
    second.m_address = p_start;
    return {std::move(first), std::move(second)};
}


// Split MemoryBlock in three
inline std::tuple<MemoryBlock, MemoryBlock, MemoryBlock> SplitBlock3(const MemoryBlock& p_data, int p_start, int p_end)
{
    // adjust, now from DataBlock start; also check.
    int start = p_start > p_data.GetStartAddress() ? p_start - p_data.GetStartAddress() : 0;    
    int end   = p_end   > p_data.GetStartAddress() ? p_end   - p_data.GetStartAddress() : 0;
    MemoryBlock first;
    MemoryBlock second;
    MemoryBlock third;
    std::tie(first.m_datablock, second.m_datablock, third.m_datablock) = SplitBlock3(p_data.m_datablock, start, end);
    first.m_address  = p_data.m_address;
    second.m_address = p_start;
    third.m_address  = p_end;
    return {std::move(first), std::move(second), std::move(third)};
}



// Do 2 ranges overlap?
template <class T1, class T2, class T3, class T4>
static bool Overlaps(T1 start1, T2 end1, T3 start2, T4 end2)
{
    using C = std::common_type_t<T1, T2, T3, T4>;
    return (C(end1) > C(start2)) && (C(start1) < C(end2));
}

// Does 1 MemoryBlock overlap with region?
template <class T1, class T2>
static bool Overlaps(const MemoryBlock &p_block, T1 p_start2, T2 p_end2)
{
    return Overlaps(p_block.GetStartAddress(), p_block.GetEndAddress(),p_start2, p_end2 );
}

// Do 2 MemoryBlocks overlap?
inline bool Overlaps(const MemoryBlock &p_block1, const MemoryBlock &p_block2)
{
    return Overlaps(p_block1.GetStartAddress(), p_block1.GetEndAddress(),p_block2.GetStartAddress(), p_block2.GetEndAddress() );
}
