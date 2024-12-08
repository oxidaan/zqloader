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
#include "turboblocks.h"
#include "byte_tools.h"
#include "tools.h"
#include "compressor.h"
#include "z80snapshot_loader.h"     // only for WriteTextToAttr

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

void DumpBlock(const DataBlock &p_block)
{

    for (int n = 0; n < p_block.size(); n++)
    {
        if (n % 16 == 0)
        {
            std::cout << std::endl;
        }
        else
        {
            std::cout << ", ";
        }
        std::cout << std::hex << "0x" << int(p_block[n]);
    }
    std::cout << std::dec;
}


void TestCompressor()
{
    Compressor<DataBlock> c;
    Compressor<DataBlock>::RLE_Meta rle;
    DataBlock block;
    for(int n = 0; n < 8; n++)
    {
        block.push_back(std::byte(0));
    }
     block.push_back(std::byte(1));
      block.push_back(std::byte(0));
        block.push_back(std::byte(1));
        block.push_back(std::byte(1));
    for(int n = 0; n < 4; n++)
    {
        block.push_back(std::byte(2));
    }
    for(int n = 0; n < 10; n++)
    {
        block.push_back(std::byte(0));
        block.push_back(std::byte(0));
        block.push_back(std::byte(3));
    }

    uint16_t decompress_counter;
    DataBlock compressed = c.Compress(block, rle, decompress_counter);
    DataBlock decompressed = c.DeCompress(compressed, rle);
    std::cout << "Orginal size = " << block.size()  << ' ' << 
                "Compressed size = " << compressed.size() << ' ' << 
                ((decompressed == block) ? "OK" : "NOK") << std::endl;
    std::cout << rle << std::endl;
    DumpBlock(compressed);
    std::cout << std::endl;
    DumpBlock(decompressed);
    std::cout << std::endl;
}

// returns USR (MC code start) address
//  "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader_test.bin"
uint16_t Test(TurboBlocks& p_blocks, fs::path p_filename)
{
#if 0
    // Copy loader to screen, then use that to load an attribute block
    {
//        p_blocks.CopyLoaderToScreen(16384);
        p_blocks.CopyLoaderToScreen(23700);
        //DataBlock empty;
        {
            DataBlock text_attr;
            text_attr.resize(2000);
            WriteTextToAttr(text_attr, ToUpper("HELLO WORLD"), 0_byte);    // 0_byte: random colors
            p_blocks.AddDataBlock(std::move(text_attr), 23690);
        }
        //p_blocks.AddDataBlock(std::move(empty),50000);
        // For fun write a attribute block -> text_attr (last 3rd)
        // at the bottom 1/3rd of screen were our loader is.
        // We also wont see the loader code then.
        for(int col = 0; col > -51; col--)
        {
            DataBlock text_attr;
            text_attr.resize(256);
            WriteTextToAttr(text_attr, ToUpper("HELLO WORLD"), 0_byte, false, col);    // 0_byte: random colors
            // copy fun attributes into 48k data block
            p_blocks.SetCompressionType(CompressionType::none);
            p_blocks.AddDataBlock(std::move(text_attr), 16384 + 6 * 1024 + 512);
        }
        return 0;
    }
#endif
#if 1
    // See zqloader_test.z80asm
    // p_blocks.CopyLoaderToScreen(16384);     // // can never work cannot print result (SP problems)

    //  "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader_test.bin"
    std::cout << "Reading binary file " << p_filename << std::endl;
    DataBlock block;
    block.LoadFromFile(p_filename);
    auto filelen = block.size();

    block.push_back(0_byte);        // make space for length filled in later, see below **
    block.push_back(0_byte);        // ,,

    block.push_back(0x10_byte);     // poly
    block.push_back(0x21_byte);     // poly

    auto codelen = block.size();
    

    // -----------------------------------------------------------------

    DataBlock fileblock1;
    fileblock1.LoadFromFile(fs::path("test2.bin"));
    block.insert(block.end(), fileblock1.begin(), fileblock1.end());       // append


    DataBlock fileblock2;
    fileblock2.LoadFromFile(fs::path("test2.bin"));
    block.insert(block.end(), fileblock2.begin(), fileblock2.end());       // append


/*
     // block.push_back(std::byte(200));
    for(int n = 0; n < 4; n ++)
    {
        for(int i = 0; i < 256; i++)
        {
            if(i != 100 && i != 200)
            {
                block.push_back(std::byte(i));
            }
        }
    }
    block.push_back(std::byte(100));
    block.push_back(std::byte(101));
    for(int n = 0; n < 100; n++)
    {
        block.push_back(0_byte);
    }
*/  
/*
    // Test data payload
    for(int n = 0; n < 16384; n++)
    {
        block.push_back(0_byte);
    }
    for(int n = 0; n < 16384; n++)
    {
        block.push_back(255_byte);
    }
*/
/*
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
        block.push_back(std::byte(Random(0, 255)));     // Note: causes different CRC each run!
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
*/


    // p_blocks.CopyLoaderToScreen(24023);      // can never work cannot print result (SP problems)
   // p_blocks.SetCompressionType(CompressionType::none);

/*
//    Test split, add in reverse order
    DataBlock block1(block.begin(), block.begin() + 5000);      
    DataBlock block2(block.begin() + 5000, block.begin() + 10000);
    DataBlock block3(block.begin() + 10000, block.end());


    p_blocks.AddDataBlock(std::move(block3), uint16_t(start+10000));
    p_blocks.AddDataBlock(std::move(block2), uint16_t(start+5000));
    p_blocks.AddDataBlock(std::move(block1), uint16_t(start));
*/


    //auto start = 16384;
    auto start = 0xffff - block.size() + 1;      // this is better test (overwrites loader @upper)...
    //auto start = 32768;                     // ...than this.
    //auto start = 25000;             // start of test data

    auto len = uint16_t(block.size()) - codelen;
    std::cout << "Length of test data = " << std::dec << len << " (crc_code_size = " << filelen << ")" << std::endl;
    std::cout << "Start of test data = " << std::dec << (start + codelen) << std::endl;
    block[filelen]     = std::byte((len & 0xff00) >> 8);       // fill in length **
    block[filelen + 1] = std::byte(len & 0xff);
    auto crc = Crc16(block.begin() + filelen + 4, block.end());     // excluding length and poly
    std::cout << "=> CRC of test data = " << std::dec << crc << "; 0x" << std::hex << crc << std::dec << std::endl;



    // split 'n mix
    DataBlock codeblock(block.begin(), block.begin() + codelen);      
    auto len1 =  fileblock1.size();
    DataBlock fblock1(block.begin() + codelen, block.begin() + codelen + len1);      
    auto len2 =  fileblock2.size();
    DataBlock fblock2(block.begin() + codelen + len1, block.begin() + codelen + len1 + len2);      

    p_blocks.AddDataBlock(std::move(codeblock), uint16_t(start));
    p_blocks.AddDataBlock(std::move(fblock2), uint16_t(start + codelen + len1));
    p_blocks.AddDataBlock(std::move(fblock1), uint16_t(start + codelen));
        //    p_blocks.AddDataBlock(std::move(block), uint16_t(start));

    return uint16_t(start);
#endif
#if 0
    (void)p_filename;
    p_blocks.SetCompressionType(CompressionType::none);
    // screen pattern
    DataBlock block;
    for (int n = 0; block.size() < 6 * 1024; n++)
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
    return 0;
#endif
#if 0
    TonePulser tone;
    tone.SetPattern(80, 80).SetLength(20s).MoveToLoader(m_spectrumloader);
#endif
#if 0
    {
        PausePulser pause;
        pause.SetLength(100ms).
            SetEdge(Edge::zero).
            MoveToLoader(m_spectrumloader);
        m_spectrumloader.Run();

        std::cout << "Edge = " << m_spectrumloader.GetLastEdge() << std::endl;
    }
    {
        SpectrumLoader m_spectrumloader;
        TonePulser tone;
        tone.SetPattern(700, 700).
            //            SetLength(500ms).
            SetLength(1).
            MoveToLoader(m_spectrumloader);
        m_spectrumloader.Run();
        std::cout << "Edge = " << m_spectrumloader.GetLastEdge() << std::endl;
    }
    {
        SpectrumLoader m_spectrumloader;
        TonePulser tone;
        tone.SetLength(1).   // 000ms).
            SetPattern(700, 700, 700).SetLength(1).
            MoveToLoader(m_spectrumloader);
        m_spectrumloader.Run();

        std::cout << "Edge = " << m_spectrumloader.GetLastEdge() << std::endl;
    }
#endif
}
