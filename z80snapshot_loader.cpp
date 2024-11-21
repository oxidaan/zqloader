// ==============================================================================
// PROJECT:         zqloader
// FILE:            z80snapshotloader.cpp
// DESCRIPTION:     Implementation of class Z80SnapShotLoader.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#include "spectrum_consts.h" // SCREEN_23RD
#include "z80snapshot_loader.h"
#include "loadbinary.h"
#include "datablock.h"
#include "turboblocks.h"
#include "byte_tools.h"
#include "tools.h"
#include <fstream>
#include <filesystem>
#include <string>


void WriteTextToAttr(DataBlock& out_attr, const std::string& p_text1, std::byte p_color);
namespace fs = std::filesystem;

///  Load z80 or sna snapshot from given file.
Z80SnapShotLoader& Z80SnapShotLoader::Load(const fs::path &p_filename)
{
    std::ifstream fileread(p_filename, std::ios::binary);
    if (!fileread)
    {
        throw std::runtime_error("File " + p_filename.string() + " not found.");
    }
    try
    {
        if(ToLower(p_filename.extension().string()) == ".z80")
        {
            std::cout << "Loading file Z80 snapshot file: " << p_filename << std::endl;
            LoadZ80(fileread);
        }
        if(ToLower(p_filename.extension().string()) == ".sna")
        {
            std::cout << "Loading file sna snapshot file: " << p_filename << std::endl;
            LoadSna(fileread);
        }
    }
    catch(const std::exception &e)
    {
        // clarify
        throw std::runtime_error("Reading file: " + p_filename.string() + ": " + e.what());
    }
    m_name = p_filename.stem().string();
    //   MoveToLoader(p_loader);
    return *this;
}



///  Load z80 snapshot from given stream.
/// https://worldofspectrum.org/faq/reference/z80format.htm
Z80SnapShotLoader& Z80SnapShotLoader::LoadZ80(std::istream& p_stream)
{
    auto header = LoadBinary<Z80SnapShotHeader>(p_stream);
    // "Because of compatibility, if byte 12 is 255, it has to be regarded as being 1."
    if (header.flags_and_border == 255)
    {
        header.flags_and_border = 1;
    }
    // adjust the weird bit 8 for R-register
    header.R_reg = (header.R_reg & 0x7f) | ((header.flags_and_border & 0x1) << 8);
    m_mem48k.resize(48 * 1024);
    if (header.PC_reg == 0)      // v2 or v3
    {
        auto header2 = LoadBinary<Z80SnapShotHeader2>(p_stream);
        if (header2.length_and_version == 23)
        {
            std::cout << "Z80 version2 file" << std::endl;
        }
        if (header2.length_and_version == 54 ||
            header2.length_and_version == 55)
        {
            std::cout << "Z80 version3 file" << std::endl;
        }
        p_stream.ignore(header2.length_and_version - (sizeof(Z80SnapShotHeader2) - 2));
        header.PC_reg = header2.PC_reg;
        while (p_stream.peek() && p_stream.good())
        {
            auto data_header = LoadBinary<Z80SnapShotDataHeader>(p_stream);
            DataBlock block;
            if (data_header.length != 0xffff)
            {
                DataBlock cblock;
                cblock.resize(data_header.length);
                p_stream.read(reinterpret_cast<char*>(cblock.data()), data_header.length);
                block = DeCompress(cblock);        // z80 decompression algo
            }
            else
            {
                block.resize(16384);
                p_stream.read(reinterpret_cast<char*>(block.data()), 16384);
            }
            if (block.size() != 16384)
            {
                throw std::runtime_error("Error reading z80 file block size not correct must be 16384 but is: " + std::to_string(block.size()));
            }
            uint16_t offset;
            switch (data_header.page_num)
            {
            case 4:
                offset = 0x8000 - 16384;
                break;
            case 5:
                offset = 0xc000 - 16384;
                break;
            case 8:
                offset = 0x4000 - 16384;
                break;
            default:
                throw std::runtime_error("Error z80 pagenum is: " + std::to_string(data_header.page_num) + "; not a 48k snapshot?");
            }
            std::copy(block.begin(),
                      block.end(),
                      m_mem48k.begin() + offset);
        }
    }
    else
    {
        std::cout << "Z80 version1 file" << std::endl;
        p_stream.read(reinterpret_cast<char*>(m_mem48k.data()), 48 * 1024);       // will normally read less than 48k
        m_mem48k = DeCompress(m_mem48k);
        if(m_mem48k.size() != 48 * 1024)
        {
            throw std::runtime_error("Size of uncompressed Z80 block should be 48K but is: " + std::to_string(m_mem48k.size()));
        }
    }
    m_z80_snapshot_header = std::move(header);
    return *this;
}



/// Load sna snapshot from given stream.
/// https://worldofspectrum.org/faq/reference/formats.htm
Z80SnapShotLoader& Z80SnapShotLoader::LoadSna(std::istream& p_stream)
{
    SnaSnapshotShotHeader header = LoadBinary<SnaSnapshotShotHeader>(p_stream);
    m_mem48k.resize(48 * 1024);
    p_stream.read(reinterpret_cast<char*>(m_mem48k.data()), 48 * 1024);       

    static constexpr uint16_t snapshot_offset = 16384;                         // 48k z80 snapshot starts here (offset)
    uint16_t pc = uint16_t(m_mem48k[header.SP_reg - snapshot_offset]) + 256 * uint16_t(m_mem48k[header.SP_reg + 1 - snapshot_offset]);      // 'pop pc'
    header.SP_reg += 2;

    // sna snapshot header -> z80 snapshot header
    m_z80_snapshot_header.A_reg = (header.AF_reg >> 8) & 0xff;
    m_z80_snapshot_header.F_reg = header.AF_reg & 0xff;
    m_z80_snapshot_header.BC_reg = header.BC_reg;
    m_z80_snapshot_header.HL_reg = header.HL_reg;
    m_z80_snapshot_header.PC_reg = pc;
    m_z80_snapshot_header.SP_reg = header.SP_reg;
    m_z80_snapshot_header.I_reg = header.I_reg;
    m_z80_snapshot_header.R_reg = header.R_reg;
    m_z80_snapshot_header.flags_and_border = header.border;
    m_z80_snapshot_header.DE_reg = header.DE_reg;
    m_z80_snapshot_header.BCa_reg = header.BCa_reg;
    m_z80_snapshot_header.DEa_reg = header.DEa_reg;
    m_z80_snapshot_header.HLa_reg = header.HLa_reg;
    m_z80_snapshot_header.Aa_reg  = (header.AFa_reg >> 8) & 0xff;
    m_z80_snapshot_header.Fa_reg = header.AFa_reg & 0xff;
    m_z80_snapshot_header.IY_reg = header.IY_reg;
    m_z80_snapshot_header.IX_reg = header.IX_reg;
    m_z80_snapshot_header.ei_di = header.iff2;     
    m_z80_snapshot_header.iff2 = 0;      
    m_z80_snapshot_header.flags_and_imode = header.imode;   
    return *this;
}


/// Write given text to attribute block
/// For fun write a attribute block -> text_attr
/// at the bottom 1/3rd of screen were our loader is.
void WriteTextToAttr(DataBlock &out_attr, const std::string &p_text, std::byte p_color)
{
    static const std::string font = 1 + &*R"(
    @    @    @    @   @   @    @    @ @    @    @   @     @    @    @    @    @    @    @   @    @     @     @    @   @    @
#XX  XXX   XX  XXX  XXX XXX  XXX X  X X    X X  X X   XXXX  XXX   XX  XXX  XXX  XXX   XXX XXX X  X X   X X X X X  X X X XXXX 
X  X  X X X  X  X X X   X   X  X X  X X    X X X  X   X X X X  X X  X X  X X  X X  X X     X  X  X X   X X X X X  X X X    X 
XXXX  XX  X     X X XXX XX  X  X XXXX X    X XX   X   X X X X  X X  X X  X X  X XXX   XX   X  X  X X   X X X X  XX  XXX   X  
X  X  X X X     X X X   X   XXXX X  X X    X X X  X   X X X X  X X  X XXX  XXXX X X     X  X  X  X  X X  X X X X  X  X   X   
X  X  X X X  X  X X X   X      X X  X X X  X X  X X   X X X X  X X  X X       X X  X    X  X  X  X  X X  X X X X  X  X  X    
X  X XXX   XX  XXX  XXX X   XXXX X  X X  XX  X  X XXX X X X X  X  XX  X       X X  X XXX   X   XX    X    X X  X  X  X  XXXX 
    )";
    const auto font_col_len       = font.find('#');
    auto GetStartOfLetter = [&](char p_letter)
                            {
                                int lt = p_letter - 'A';
                                if (lt == 0)
                                {
                                    return 0;   // so A
                                }
                                for (int n = 0; n < font_col_len; n++)
                                {
                                    auto c = font[n];
                                    if (c == '@')
                                    {
                                        lt--;
                                        if (lt == 0)
                                        {
                                            return n + 1;
                                        }
                                    }
                                }
                                return 0; // error
                            };
    auto GetWidthForLetter = [&](char p_letter)
                             {
                                 int let  = p_letter - 'A';
                                 int prev = -1;
                                 for (int n = 0; n < font_col_len; n++)
                                 {
                                     auto c = font[n];
                                     if (c == '@')
                                     {
                                         if (let == 0)
                                         {
                                             return n - prev - 1;
                                         }
                                         prev = n;
                                         let--;
                                     }
                                 }
                                 return 0; // error
                             };
    auto GetStartForLetterRow = [&](char p_letter, int p_row)
                                {
                                    return GetStartOfLetter(p_letter) + p_row * font_col_len;
                                };


    // determine text width for centering
    int width = 0;
    for (const auto& c : p_text)
    {
        width += GetWidthForLetter(c) + (width != 0);
    }

    int col = width <= 32 ? (32 - width) / 2 : 0;
    for (const auto& c : p_text)
    {
        // int idx = (row + 1) * 32;
        if (c == ' ')
        {
            col += 4;
        }
        else
        {
            std::byte color;
            if (p_color == 0_byte)      // black makes no sense, choose random color then
            {
                auto colr = Random(1, 7);
                color = std::byte(colr | (colr << 3));  // ink en paper same
            }
            else
            {
                color = p_color;
            }
            for (int row = 1; row <= 6; row++)
            {
                auto start = GetStartForLetterRow(c, row);
                for(int i = 0; i < 1 + GetWidthForLetter(c) && col < 32; i++)
                {
                    bool set = font[start + i] == 'X';
                    if (set)
                    {
                        out_attr[row * 32 + col + i] |= color;
                    }

                }
            }
            col += 1 + GetWidthForLetter(c);
            if (col >= 32)
            {
                break;
            }
        }
    }
}



// Get emtpy space (zero's of given length) in given block
inline uint16_t GetEmptySpaceLocation(const DataBlock &p_block, uint16_t p_len, uint16_t p_offset = 0)
{
    int cnt         = 0;
    for(uint16_t n = p_offset; n < p_block.size(); n++)
    {
        auto b = p_block[n];
        if(b == 0_byte)
        {
            cnt++;
            if(cnt == p_len)
            {
                return n + 1 - p_len;
            }
        }
        else
        {
            cnt = 0;
        }
    }
    return 0;
}



/// Move this snapshot -as read from Z80 file- to given TurboBlocks.
void Z80SnapShotLoader::MoveToTurboBlocks(TurboBlocks& p_turbo_blocks, bool p_allways_use_screen, bool p_write_fun_attribs)
{
    const auto &symbols                = p_turbo_blocks.GetSymbols();   // alias
    auto snapshot_data                 = GetData();                     // the z80 snapshot, 48k

    static constexpr uint16_t z80_snapshot_offset = 16384;                         // 48k z80 snapshot starts here (offset)
    uint16_t len_needed                = p_turbo_blocks.GetLoaderCodeLength(true);    // space needed for our loader code
    uint16_t copy_me_target_location   = GetEmptySpaceLocation(snapshot_data, len_needed, 6 * 1024 + 768); // location for our loader code
    bool at_screen                     = false;
    if(p_allways_use_screen)
    {
        at_screen               = true;
        copy_me_target_location = spectrum::SCREEN_23RD;
        std::cout << "Will copy loader code to screen. (" << spectrum::SCREEN_23RD << "; length = " << len_needed << ")" << std::endl;
    }
    else if(copy_me_target_location == 0)      // no empty space found, use last 3rd of screen
    {
        at_screen               = true;
        copy_me_target_location = spectrum::SCREEN_23RD;
        std::cout << "Not enough empty space found in snapshot. Will copy loader code to screen. (" << spectrum::SCREEN_23RD << "; length = " << len_needed << ")" << std::endl;
    }
    else
    {
        copy_me_target_location += z80_snapshot_offset;       // adjust offset within (48k) snapshot
        std::cout << "Found empty space to copy loader code to: " << copy_me_target_location << ". (length = " << len_needed << ")" << std::endl;
    }
    uint16_t register_code_start = copy_me_target_location +
                                    symbols.GetSymbol("STACK_SIZE") + 
                                    symbols.GetSymbol("ASM_CONTROL_CODE_LEN") + 
                                    symbols.GetSymbol("ASM_UPPER_LEN");

    Z80SnapShotHeaderToSnapShotRegs(symbols);       // fill register values into ->m_reg_block
    // copy m_reg_block directly into 48k data block
    std::copy(m_reg_block.begin(), m_reg_block.end(), snapshot_data.begin() + register_code_start - z80_snapshot_offset);

    DataBlock screenblock(snapshot_data.begin(), snapshot_data.begin() + spectrum::SCREEN_SIZE);      // split
    DataBlock payload(snapshot_data.begin() + spectrum::SCREEN_SIZE, snapshot_data.end());

    if(at_screen && p_write_fun_attribs)
    {
        // For fun write a attribute block -> text_attr (last 3rd)
        // at the bottom 1/3rd of screen were our loader is.
        // We also wont see the loader code then.
        DataBlock text_attr;
        text_attr.resize(256);
        WriteTextToAttr(text_attr, ToUpper(m_name), 0_byte);    // 0_byte: random colors
        std::copy(text_attr.begin(), text_attr.end(), screenblock.begin() + spectrum::ATTR_23RD - z80_snapshot_offset);

    }
    p_turbo_blocks.AddDataBlock(std::move(screenblock), spectrum::SCREEN_START);       // screen
    p_turbo_blocks.SetLoaderCopyTarget(copy_me_target_location);
    p_turbo_blocks.AddDataBlock(std::move(payload), spectrum::SCREEN_START + spectrum::SCREEN_SIZE);           // rest
    // This puts startaddress at where registers are restored, thus starting the snapshot.
    m_usr = register_code_start;
}



// Fill in register data (as read from z80 file) to register block (eg snapshotregs.bin)
// test with zqloader_test2.z80asm
inline void Z80SnapShotLoader::Z80SnapShotHeaderToSnapShotRegs(const Symbols& p_symbols)
{
    p_symbols.SetByte(m_reg_block, "flags_and_border",( m_z80_snapshot_header.flags_and_border >> 1) & 0xb00000111);
    p_symbols.SetWord(m_reg_block, "BC_reg", m_z80_snapshot_header.BC_reg);
    p_symbols.SetWord(m_reg_block, "DE_reg", m_z80_snapshot_header.DE_reg);
    p_symbols.SetWord(m_reg_block, "HL_reg", m_z80_snapshot_header.HL_reg);
    p_symbols.SetWord(m_reg_block, "BCa_reg", m_z80_snapshot_header.BCa_reg);
    p_symbols.SetWord(m_reg_block, "DEa_reg", m_z80_snapshot_header.DEa_reg);
    p_symbols.SetWord(m_reg_block, "HLa_reg", m_z80_snapshot_header.HLa_reg);
    p_symbols.SetWord(m_reg_block, "IX_reg", m_z80_snapshot_header.IX_reg);
    p_symbols.SetWord(m_reg_block, "IY_reg", m_z80_snapshot_header.IY_reg);
    p_symbols.SetByte(m_reg_block, "R_reg", m_z80_snapshot_header.R_reg);
    p_symbols.SetByte(m_reg_block, "I_reg", m_z80_snapshot_header.I_reg);
    p_symbols.SetWord(m_reg_block, "imode",
                      (m_z80_snapshot_header.flags_and_imode & 0x3) == 2 ? 0x5EED :     // IM 2 // !! endianess swapped!! ;=(
                      (m_z80_snapshot_header.flags_and_imode & 0x3) == 1 ? 0x56ED :     // IM 1
                      (m_z80_snapshot_header.flags_and_imode & 0x3) == 0 ? 0x46ED : 0); // IM 0
    p_symbols.SetByte(m_reg_block, "ei_di", (m_z80_snapshot_header.ei_di == 0) ?
                      0xF3 :                                                            // DI
                      0xFB);                                                            // EI
    p_symbols.SetWord(m_reg_block, "SP_reg", m_z80_snapshot_header.SP_reg);
    p_symbols.SetWord(m_reg_block, "PC_reg", m_z80_snapshot_header.PC_reg);
    p_symbols.SetWord(m_reg_block, "AF_reg",  256 * m_z80_snapshot_header.A_reg  + m_z80_snapshot_header.F_reg);
    p_symbols.SetWord(m_reg_block, "AFa_reg", 256 * m_z80_snapshot_header.Aa_reg + m_z80_snapshot_header.Fa_reg);
}



// Z80 decompress
// see https://worldofspectrum.org/faq/reference/z80format.htm
inline DataBlock Z80SnapShotLoader::DeCompress(const DataBlock& p_block)
{
    DataBlock retval;
    auto len = p_block.size();
    std::byte prev{};

    for (int n = 0; n < len; n++)
    {
        if (n <= len - 4 &&
            p_block[n + 0] == 0x0_byte &&
            p_block[n + 1] == 0xed_byte &&
            p_block[n + 2] == 0xed_byte &&
            p_block[n + 3] == 0x0_byte)
        {
            break;
        }

        auto b = p_block[n];

        if (b == 0xed_byte && prev == 0xed_byte)
        {
            int xx  = int(p_block[++n]);        // naming as in https://worldofspectrum.org/faq/reference/z80format.htm
            auto yy = p_block[++n];
            for (int i = 0; i < xx; i++)
            {
                retval.push_back(yy);
            }
            prev = 0x0_byte;
        }
        else if (b != 0xed_byte && prev == 0xed_byte)
        {
            retval.push_back(prev);
            retval.push_back(b);
            prev = b;
        }
        else if (b != 0xed_byte)
        {
            retval.push_back(b);
            prev = b;
        }
        else
        {
            prev = b;
        }
    }
    return retval;
}