// ==============================================================================
// PROJECT:         zqloader
// FILE:            z80snapshotloader.cpp
// DESCRIPTION:     Implementation of class SnapShotLoader.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#include "spectrum_consts.h" // SCREEN_23RD
#include "z80snapshot_loader.h"
#include "loadbinary.h"
#include "datablock.h"
#include "symbols.h"
#include "turboblocks.h"
#include "memoryblock.h"
#include "byte_tools.h"
#include "tools.h"
#include <fstream>
#include <filesystem>
#include <string>
#include <cstring>  // memcpy
/*
https://worldofspectrum.org/faq/reference/z80format.htm
https://worldofspectrum.net/zx-modules/fileformats/z80format.html
https://worldofspectrum.org/faq/reference/128kreference.htm
I cant find the term samram nowhere but here. Dunno what it is. Ignored it.
Changed page to bank in second column. Asume thats a mistake.
Added bank address column. Note the weird shift between bank 0-1-2.

        Page    In '48 mode     In '128 mode    
        ----------------------------------------
         0      48K rom         rom (basic)     
         1      Interface I, Disciple or Plus D 
         2      -               rom (reset)     
         3      -               bank 0          c000-ffff
         4      8000-bfff (b2)  bank 1          c000-ffff
         5      c000-ffff (b0)  bank 2          8000-bfff
         6      -               bank 3          c000-ffff
         7      -               bank 4          c000-ffff
         8      4000-7fff (b5)  bank 5          4000-7fff
         9      -               bank 6          c000-ffff
        10      -               bank 7          c000-ffff
        11      Multiface rom   Multiface rom   
*/

inline uint16_t PageToAddress(int p_page_num, bool p_48_mode)
{
    // page #                    3       4       5       6       7       8       9       10
    constexpr uint16_t a128[] = {0xC000, 0xC000, 0x8000, 0xC000, 0xC000, 0x4000, 0xC000, 0xC000};
    constexpr uint16_t a48[] =  {0x0,    0x8000, 0xC000, 0x0,    0x0,    0x4000, 0x0,    0x0   };
    if(p_page_num < 3 || p_page_num > 10 )
    {
        throw std::runtime_error("Page number " + std::to_string(p_page_num) + " is rom bank");
    }
    return p_48_mode ? a48[p_page_num - 3] : a128[p_page_num - 3];
}

inline int PageToBank(int p_page_num, bool p_48_mode)
{
    // page #                    3       4       5       6       7       8       9       10
    constexpr int b128[]      = {0,      1,      2,      3,      4,      5,      6,      7  };
    constexpr int b48[]       = {0,      2,      0,      0,      0,      5,      0,      0  };
    if(p_page_num < 3 || p_page_num > 10 )
    {
        throw std::runtime_error("Page number " + std::to_string(p_page_num) + " is rom bank");
    }
    return p_48_mode ?  b48[p_page_num - 3] : b128[p_page_num - 3];

}

inline bool GetIs48K(uint8_t p_byte34, bool p_is_v3)
{
    if(p_byte34 == 2)
    {
        throw std::runtime_error("SAMRAM not supported, I dont know what that is, sorry");
    }
    return p_byte34 == 0 || p_byte34 == 1 || (p_byte34 == 3 && p_is_v3 == true);
}

namespace fs = std::filesystem;

///  Load z80 or sna snapshot from given file.
SnapShotLoader& SnapShotLoader::Load(const fs::path &p_filename)
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
//            std::cout << "Loading file Z80 snapshot file: " << p_filename << std::endl;
            LoadZ80(fileread);
        }
        if(ToLower(p_filename.extension().string()) == ".sna")
        {
//            std::cout << "Loading file sna snapshot file: " << p_filename << std::endl;
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
SnapShotLoader& SnapShotLoader::LoadZ80(std::istream& p_stream)
{
    auto header = LoadBinary<Z80SnapShotHeader>(p_stream);
    // "Because of compatibility, if byte 12 is 255, it has to be regarded as being 1."
    if (header.flags_and_border == 255)
    {
        header.flags_and_border = 1;
    }
    // adjust the weird bit 8 for R-register
    header.R_reg = (header.R_reg & 0x7f) | ((header.flags_and_border & 0x1) << 8);
    if (header.PC_reg != 0)      // v1
    {
        MemoryBlock mem48k;
        mem48k.m_datablock.resize(48 * 1024);
        mem48k.m_address = spectrum::RAM_START;
        std::cout << "Z80 version 1 file" << std::endl;
        p_stream.read(reinterpret_cast<char*>(mem48k.m_datablock.data()), 48 * 1024);       // will normally read less than 48k
        mem48k.m_datablock = DeCompress(mem48k.m_datablock);
        if( mem48k.m_datablock.size() != 48 * 1024)
        {
            throw std::runtime_error("Size of uncompressed Z80 block should be 48K but is: " + std::to_string(mem48k.m_datablock.size()));
        }
        m_ram.push_back(std::move(mem48k));
        m_is_48K = true;
    }
    else        // v2 or v3
    {
        auto length_and_version = LoadBinary<uint16_t>(p_stream);
        // "The value of the word at position 30 is 23 for version 2 files, and 54 or 55 for version 3"
        if (length_and_version == 23)
        {
            std::cout << "Z80 version 2 file; ";
        }
        else if (length_and_version == 54 || length_and_version == 55)
        {
            std::cout << "Z80 version 3 file; ";
        }
        else
        {
            throw std::runtime_error("Invalid Length of additional header block (" + std::to_string(length_and_version) + ")");
        }
        DataBlock buf(length_and_version);
        p_stream.read(reinterpret_cast<char *>(buf.data()), length_and_version);
        Z80SnapShotHeader2 header2{};
        memcpy(&header2, buf.data(), length_and_version);

        header.PC_reg = header2.PC_reg;

        m_is_48K = GetIs48K(header2.hardware_mode, length_and_version != 23);
        m_current_bank =  m_is_48K ? -1 : int(header2.current_bank);
        if(m_is_48K)
        {
            std::cout << "48K snapshot." << std::endl;
        }
        else
        {
            std::cout << "128K snapshot. Current bank = " << (m_current_bank & 0x07) << std::endl;
        }

        MemoryBlock mem48k;
        mem48k.m_datablock.resize(48 * 1024);
        mem48k.m_address = spectrum::RAM_START;
        int cnt = 0;
        // Read 16K data blocks
        while (p_stream.peek() && p_stream.good())
        {
            cnt++;
            auto data_header = LoadBinary<Z80SnapShotDataHeader>(p_stream);
            MemoryBlock bank16k;
            // "If length=0xffff, data is 16384 bytes long and not compressed"
            if (data_header.length != 0xffff)
            {
                DataBlock cblock;
                cblock.resize(data_header.length);
                p_stream.read(reinterpret_cast<char*>(cblock.data()), data_header.length);
                bank16k.m_datablock = DeCompress(cblock);        // z80 decompression algo
            }
            else
            {
                bank16k.m_datablock.resize(16384);
                p_stream.read(reinterpret_cast<char*>(bank16k.m_datablock.data()), 16384);
            }
            if (bank16k.size() != 16384)
            {
                throw std::runtime_error("Error reading z80 file block size not correct must be 16384 but is: " + std::to_string(bank16k.size()) + ".");
            }
            bank16k.m_address = PageToAddress(data_header.page_num, m_is_48K);
            bank16k.m_bank = PageToBank(data_header.page_num, m_is_48K);
            // std::cout << int(data_header.page_num) << ' ' << bank16k.m_bank << ' ' << bank16k.m_address <<  ' ' << int(header2.current_bank) << std::endl;
            if(bank16k.m_bank == 2 || bank16k.m_bank == 5 ||  bank16k.m_bank == header2.current_bank)
            {
                // Copy these banks to the 48K datablock; will be the first.
                // Same as v1 snapshot. And SNA snapshot.
                std::copy(bank16k.m_datablock.begin(), bank16k.m_datablock.end(), mem48k.m_datablock.begin() +  bank16k.m_address - spectrum::RAM_START);
            }
            else
            {
                // Its a switchable bank.
                m_ram.push_back(std::move(bank16k));
            }
        }
        m_ram.push_front( std::move(mem48k));
        if(m_is_48K)
        {
            if(cnt != 3)
            {
                throw std::runtime_error("Error Expect 48K snapshot to contain 3 16K blocks, but has: " + std::to_string(cnt));
            }
        }


    }

    m_z80_snapshot_header = std::move(header);
    return *this;
}



/// Load sna snapshot from given stream.
/// https://worldofspectrum.org/faq/reference/formats.htm
SnapShotLoader& SnapShotLoader::LoadSna(std::istream& p_stream)
{
    SnaSnapshotShotHeader header = LoadBinary<SnaSnapshotShotHeader>(p_stream);

    MemoryBlock mem48k;
    mem48k.m_datablock.resize(48 * 1024);
    mem48k.m_address = spectrum::RAM_START;
    p_stream.read(reinterpret_cast<char*>(mem48k.m_datablock.data()), 48 * 1024);

    uint16_t pc;
    if(p_stream.peek() && p_stream.good())
    {
        // File is longer so its a 128k sna file.
        m_is_48K = false;
        pc = LoadBinary<uint16_t>(p_stream);
        m_current_bank = int(LoadBinary<uint8_t>(p_stream));       // = port 0x7ffd
        auto current_bank = m_current_bank & 0x07;
        std::cout << "128K SNA snapshot. Current bank = " << current_bank << std::endl;
        uint8_t trdos_rom_paged  = LoadBinary<uint8_t>(p_stream);
        (void)trdos_rom_paged;
        m_ram.push_back(std::move(mem48k));
        int banks[] = {0,1,3,4,6,7};        // all not duplicated banks
        for(int bank : banks)
        {
            if(bank != current_bank)        // and also skip current bank
            {
                std::cout << p_stream.tellg() << " Reading bank: " << bank << std::endl;
                MemoryBlock bank16k;
                bank16k.m_address = 0xc000;
                bank16k.m_bank = bank;
                bank16k.m_datablock.resize(16384);
                p_stream.read(reinterpret_cast<char*>(bank16k.m_datablock.data()), 16384);
                m_ram.push_back(std::move(bank16k));
            }
        }
    }
    else
    {
        std::cout << "48K SNA snapshot." << std::endl;
        // 'pop pc'
        pc = uint16_t(mem48k.m_datablock[header.SP_reg - spectrum::RAM_START]) + 256 * uint16_t(mem48k.m_datablock[header.SP_reg + 1 - spectrum::RAM_START]);
        header.SP_reg += 2;
        m_ram.push_back(std::move(mem48k));
    }
    // sna snapshot header -> z80 snapshot header
    m_z80_snapshot_header.A_reg            = (header.AF_reg >> 8) & 0xff;
    m_z80_snapshot_header.F_reg            = header.AF_reg & 0xff;
    m_z80_snapshot_header.BC_reg           = header.BC_reg;
    m_z80_snapshot_header.HL_reg           = header.HL_reg;
    m_z80_snapshot_header.PC_reg           = pc;
    m_z80_snapshot_header.SP_reg           = header.SP_reg;
    m_z80_snapshot_header.I_reg            = header.I_reg;
    m_z80_snapshot_header.R_reg            = header.R_reg;
    m_z80_snapshot_header.flags_and_border = header.border;
    m_z80_snapshot_header.DE_reg           = header.DE_reg;
    m_z80_snapshot_header.BCa_reg          = header.BCa_reg;
    m_z80_snapshot_header.DEa_reg          = header.DEa_reg;
    m_z80_snapshot_header.HLa_reg          = header.HLa_reg;
    m_z80_snapshot_header.Aa_reg           = (header.AFa_reg >> 8) & 0xff;
    m_z80_snapshot_header.Fa_reg           = header.AFa_reg & 0xff;
    m_z80_snapshot_header.IY_reg           = header.IY_reg;
    m_z80_snapshot_header.IX_reg           = header.IX_reg;
    m_z80_snapshot_header.ei_di            = header.iff2;
    m_z80_snapshot_header.iff2             = 0;
    m_z80_snapshot_header.flags_and_imode  = header.imode;


    if(p_stream.fail())
    {
        throw std::runtime_error("Error reading SNA file, file is shorter than expected");
    }
    p_stream.peek();
    if(p_stream.good())
    {
        throw std::runtime_error("Error reading SNA file, file is longer than expected");
    }
    return *this;
}



/// Write given text to given attribute block
/// For fun write a attribute block -> out_attr
bool WriteTextToAttr(DataBlock &out_attr, const std::string &p_text, std::byte p_color, bool p_center, int p_col)
{
    static const std::string font = 1 + &*R"(
A    B    C    D    E   F   G    H    I J    K    L   M     N    O    P    Q    R    S    T   U    V     W     X     Y   Z    ! ?    -    . ,  +     #
 XX  XXX   XX  XXX  XXX XXX  XXX X  X X    X X  X X   XXXX  XXX   XX  XXX   XX  XXX   XXX XXX X  X X   X X X X X   X X X XXXX X  XX              X   #
X  X  X X X  X  X X X   X   X  X X  X X    X X X  X   X X X X  X X  X X  X X  X X  X X     X  X  X X   X X X X  X X  X X    X X X  X             X   #
XXXX  XX  X     X X XXX XX  X  X XXXX X    X XX   X   X X X X  X X  X X  X X  X XXX   XX   X  X  X X   X X X X   X   XXX   X  X    X XXXX      XXXXX #
X  X  X X X     X X X   X   XXXX X  X X    X X X  X   X X X X  X X  X XXX  X  X X X     X  X  X  X  X X  X X X   X    X   X   X   X              X   #
X  X  X X X  X  X X X   X      X X  X X X  X X  X X   X X X X  X X  X X     XX  X  X    X  X  X  X  X X  X X X  X X   X  X                   X   X   #
X  X XXX   XX  XXX  XXX X   XXXX X  X X  XX  X  X XXX X X X X  X  XX  X      XX X  X XXX   X   XX    X    X X  X   X  X  XXXX X   X       X X        #
    )";
    auto font_col_len       = 1 + 1 + font.find('#') - font.find('A');     // column length +1 for \n
    auto GetStartOfLetter = [&](char p_letter)
                            {
                                for (int n = 0; n < font_col_len; n++)
                                {
                                    auto c = font[n];
                                    if (c == p_letter)
                                    {
                                        return n;
                                    }
                                }
                                return 0; // error
                            };
    auto GetWidthForLetter = [&](char p_letter)
                            {
                                int first = -1;
                                for (int n = 0; n < font_col_len; n++)
                                {
                                    auto c = font[n];
                                    if(c == p_letter)
                                    {
                                        first = n;
                                    }
                                    else if(c != ' ' && first >= 0)
                                    {
                                        return n - first;       // eg A is 5 (not 4)
                                    }
                                }
                                return 0; // error
                            };
    auto GetStartForLetterRow = [&](char p_letter, int p_row)
                                {
                                    return GetStartOfLetter(p_letter) + p_row * font_col_len;
                                };


    int col = p_col;
    // determine text width for centering
    if(p_center)
    {
        int width = 0;
        for (const auto& c : p_text)
        {
            width += GetWidthForLetter(c) + (width != 0);
        }

        col = width <= 32 ? (32 - width) / 2 : 0;       // left position
    }
    bool is_empty = true;
    for (const auto& c : p_text)
    {
        // int idx = (row + 1) * 32;
        if (c == ' ')       // space
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
            auto width = GetWidthForLetter(c);
            for (int row = 1; row <= 6; row++)
            {
                auto start = GetStartForLetterRow(c, row);
                for(int i = 0; i <width; i++)
                {
                    if((col + i >= 0) && (col + i < 32))
                    {
                        bool set = font[start + i] == 'X';
                        if (set)
                        {
                            out_attr[row * 32 + col + i] |= color;
                            is_empty = false;
                        }
                    }
                }
            }
            col += width;
        }
        if (col >= 32)
        {
            break;
        }
    }
    return is_empty;
}



// Get emtpy space (zero's of given length) in given block
inline uint16_t GetEmptySpaceLocation(const DataBlock &p_block, uint16_t p_len, uint16_t p_offset = 0)
{
    int cnt = 0;
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
void SnapShotLoader::MoveToTurboBlocks(TurboBlocks& p_turbo_blocks, uint16_t p_new_loader_location, bool p_write_fun_attribs)
{
    const auto &symbols = p_turbo_blocks.GetSymbols();              // alias
    MemoryBlocks all_blocks =  GetRam();
    bool first = true;
    for(auto &block: all_blocks)
    {
        if(first)
        {
            DataBlock &first_block  = block.m_datablock;                               // the first block

            uint16_t z80_snapshot_offset = uint16_t(block.GetStartAddress());                                    // 48k z80 snapshot starts here (offset)
            if(p_new_loader_location == 0)
            {
                uint16_t len_needed = p_turbo_blocks.GetLoaderCodeLength(true); // space needed for our loader code
                p_new_loader_location = GetEmptySpaceLocation(first_block, len_needed, 6 * 1024 + 768); // location for our loader code
                if(p_new_loader_location == 0)                                                            // no empty space found, use last 3rd of screen
                {
                    p_new_loader_location = spectrum::SCREEN_23RD;
                    std::cout << "Not enough empty space found in snapshot. Will copy loader code to screen. (" << spectrum::SCREEN_23RD << "; length = " << len_needed << ")" << std::endl;
                }
                else
                {
                    p_new_loader_location += z80_snapshot_offset;       // adjust offset within (48k) snapshot
                    std::cout << "Found empty space to copy loader code to: " << p_new_loader_location << ". (length = " << len_needed << ")" << std::endl;
                }
            }
            uint16_t register_code_start = p_new_loader_location +
                                           symbols.GetSymbol("STACK_SIZE") +
                                           symbols.GetSymbol("ASM_CONTROL_CODE_LEN") +
                                           symbols.GetSymbol("ASM_UPPER_LEN");

            Z80SnapShotHeaderToSnapShotRegs(symbols);       // fill register values into ->m_reg_block
            // copy m_reg_block directly into 48k data block
            // without the int cast below crashes (msvc)? Dunno why?
            // or put brackets around it. Else iterator is temporary out of range which asserts.
            std::copy(m_reg_block.begin(), m_reg_block.end(), first_block.begin() + (register_code_start - z80_snapshot_offset));

            DataBlock screenblock(first_block.begin(), first_block.begin() + spectrum::SCREEN_SIZE);      // split
            DataBlock payload(first_block.begin() + spectrum::SCREEN_SIZE, first_block.end());

            if(p_write_fun_attribs)
            {
                // For fun write a attribute block -> text_attr (last 3rd)
                // at the bottom 1/3rd of screen were our loader is.
                // We also wont see the loader code then.
                DataBlock text_attr;
                text_attr.resize(256);
                WriteTextToAttr(text_attr, ToUpper(m_name), 0_byte, true, 0);    // 0_byte: random colors
                std::copy(text_attr.begin(), text_attr.end(), screenblock.begin() + (spectrum::ATTR_23RD - z80_snapshot_offset));

            }
            p_turbo_blocks.AddMemoryBlock({std::move(screenblock), spectrum::SCREEN_START});                     // screen
            p_turbo_blocks.SetLoaderCopyTarget(p_new_loader_location);
            p_turbo_blocks.AddMemoryBlock({std::move(payload), spectrum::SCREEN_START + spectrum::SCREEN_SIZE}); // rest
            // This puts startaddress at where registers are restored, thus starting the snapshot.
            m_usr = register_code_start;
            first = false;
        }
        else
        {
            p_turbo_blocks.AddMemoryBlock(std::move(block));   
        }
    }
}



// Fill in register data (as read from z80 file) to register block (eg snapshotregs.bin)
// test with zqloader_test2.z80asm
inline void SnapShotLoader::Z80SnapShotHeaderToSnapShotRegs(const Symbols& p_symbols)
{
    p_symbols.SetByte(m_reg_block, "flags_and_border", ( m_z80_snapshot_header.flags_and_border >> 1) & 0xb00000111);
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
    p_symbols.SetWord(m_reg_block, "AF_reg",  256 * m_z80_snapshot_header.A_reg + m_z80_snapshot_header.F_reg);
    p_symbols.SetWord(m_reg_block, "AFa_reg", 256 * m_z80_snapshot_header.Aa_reg + m_z80_snapshot_header.Fa_reg);
}



// Z80 decompress
// see https://worldofspectrum.org/faq/reference/z80format.htm
inline DataBlock SnapShotLoader::DeCompress(const DataBlock& p_block)
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
