//==============================================================================
// PROJECT:         zqloader
// FILE:            test.cpp
// DESCRIPTION:     Various tests
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#include "datablock.h"
#include <cstdint>         // uint16_t
#include <filesystem>       // std::filesystem
#include <iostream>
#include <fstream>
#include "turboblock.h"
#include "byte_tools.h"
#include <random>


template <class TIterator>
uint16_t Crc16(TIterator p_begin, TIterator p_end)
{
    uint16_t shiftreg = 0xffff;
    uint16_t polynomial = 0x1021;

    for (auto it = p_begin; it != p_end; it++)
    {
        auto b = *it;
        shiftreg = shiftreg ^ (uint16_t(b) << 8);

        for (int i = 0; i < 8; i++)
        {
            bool do_xor = shiftreg & 0x8000;
            shiftreg <<= 1;
            if (do_xor)
            {
                shiftreg ^= polynomial;
            }
        }
    }
    return shiftreg;
}
uint16_t Crc16(const std::vector<std::byte>& p_data)
{
    return Crc16(p_data.begin(), p_data.end());
}



namespace fs = std::filesystem;


template<class TEngine = std::minstd_rand>
auto& GetSeededRandomEngine()
{
    static std::random_device seed;
    static TEngine gen(seed());
    return gen;
}



// Eg Dice: Random(1,6)
template<class TInt1, class TInt2,
    typename std::enable_if<std::is_integral<TInt1>::value, int>::type = 0,
    typename std::enable_if<std::is_integral<TInt2>::value, int>::type = 0>
TInt1 Random(TInt1 p_min, TInt2 p_max)
{
    std::uniform_int_distribution distrib(p_min, TInt1(p_max));
    return distrib(GetSeededRandomEngine());
}

uint16_t Test(TurboBlocks& p_blocks, fs::path p_filename)
{
#if 1
    // "C:\Projects\Visual Studio\Projects\zqloader\vscode\qloader_test.bin"
    std::cout << "Reading binary file " << p_filename << std::endl;
    DataBlock block;
    block.LoadFromFile(p_filename);
    auto filelen = block.size();

    block.push_back(0_byte);        // make space for length filled in later
    block.push_back(0_byte);        // ,,

    block.push_back(0x10_byte);     // poly
    block.push_back(0x21_byte);     // poly

    for (int n = 0; n < 20; n++)
    {
        block.push_back(10_byte);
    }


    for (int n = 0; n < 254; n++)
    {
        block.push_back(std::byte(n));
        //block.push_back(10_byte);
    }
    for (int n = 0; n < 254; n++)
    {
        block.push_back(std::byte(n));
        //block.push_back(10_byte);
    }
    for (int n = 0; n < 254; n++)
    {
        block.push_back(std::byte(n));
        //block.push_back(10_byte);
    }

    for (int n = 0; n < 20; n++)
    {
        block.push_back(10_byte);
    }

    for (int n = 0; n < 2000; n++)
    {
        block.push_back(0_byte);
        block.push_back(0_byte);
        block.push_back(0_byte);
        block.push_back(0_byte);
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(1_byte);
        block.push_back(1_byte);
        block.push_back(1_byte);
        block.push_back(1_byte);
    }


    auto start = 0xffff - block.size() + 1;
    //    auto start = 32768;
    auto len = uint16_t(block.size()) - uint16_t(filelen) - 4;
    std::cout << "Length of test data = " << std::dec << len << " (crc_code_size = " << filelen << ")" << std::endl;
    std::cout << "Start of test data = " << std::dec << (start + 4 + filelen) << std::endl;
    block[filelen] = std::byte((len & 0xff00) >> 8);       // fill in length
    block[filelen + 1] = std::byte(len & 0xff);

    auto crc = Crc16(block.begin() + filelen + 4, block.end());     // excluding length and poly
    //    p_blocks.CopyLoaderToScreen();
    p_blocks.AddDataBlock(std::move(block), uint16_t(start));
    std::cout << "=> CRC of test data = " << std::dec << crc << "; 0x" << std::hex << crc << std::dec << std::endl;
    return uint16_t(start);
#endif
#if 0
    DataBlock block;
    for (int n = 0; block.size() < 4 * 1024; n++)
    {
        //  block.push_back(0_byte);
        //  block.push_back(0_byte);
        for (int i = 0; i < 32; i++)
            block.push_back(255_byte);
        for (int i = 0; i < 32; i++)
            block.push_back(0_byte);
        for (int i = 0; i < 32; i++)
            block.push_back(0b11001010_byte);
        for (int i = 0; i < 32; i++)
            block.push_back(0b01010101_byte);
        //  block.push_back(0b11001100_byte);
    }
    std::cout << "Size = " << block.size() << std::endl;
    //  TurboBlocks tblocks(p_loader_length, CompressionType::none);
    p_blocks.AddDataBlock(std::move(block), uint16_t(16384));
    //  p_blocks.MoveToLoader(spectrumloader);
#endif
#if 0
    TonePulser tone;
    tone.SetPattern(80, 80).SetLength(20s).MoveToLoader(spectrumloader);
#endif
#if 0
    {
        PausePulser pause;
        pause.SetLength(100ms).
            SetEdge(Edge::zero).
            MoveToLoader(spectrumloader);
        spectrumloader.Run();

        std::cout << "Edge = " << spectrumloader.GetLastEdge() << std::endl;
    }
    {
        SpectrumLoader spectrumloader;
        TonePulser tone;
        tone.SetPattern(700, 700).
            //            SetLength(500ms).
            SetLength(1).
            MoveToLoader(spectrumloader);
        spectrumloader.Run();
        std::cout << "Edge = " << spectrumloader.GetLastEdge() << std::endl;
    }
    {
        SpectrumLoader spectrumloader;
        TonePulser tone;
        tone.SetLength(1).   // 000ms).
            SetPattern(700, 700, 700).SetLength(1).
            MoveToLoader(spectrumloader);
        spectrumloader.Run();

        std::cout << "Edge = " << spectrumloader.GetLastEdge() << std::endl;
    }
#endif
}
