//==============================================================================
// PROJECT:         zqloader
// FILE:            z80snapshotloader.h
// DESCRIPTION:     Definition of class SnapShotLoader.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once

#include <string>
#include "datablock.h"
#include <cstdint>
#include <iosfwd>


class TurboBlocks;
class Symbols;


/// Loads .Z80 or .sna snapshot files.
/// See https://worldofspectrum.org/faq/reference/z80format.htm
class SnapShotLoader
{
#pragma pack(push, 1)
    // z80 snapshot header https://worldofspectrum.org/faq/reference/z80format.htm
    struct Z80SnapShotHeader
    {
        uint8_t  A_reg;
        uint8_t  F_reg;
        uint16_t BC_reg;        // BC register pair(LSB, i.e.C, first)
        uint16_t HL_reg;
        uint16_t PC_reg;
        uint16_t SP_reg;
        uint8_t  I_reg;
        uint8_t  R_reg;
        uint8_t  flags_and_border;
        uint16_t DE_reg;
        uint16_t BCa_reg;
        uint16_t DEa_reg;
        uint16_t HLa_reg;
        uint8_t  Aa_reg;
        uint8_t  Fa_reg;
        uint16_t IY_reg;    // IY first!
        uint16_t IX_reg;
        uint8_t  ei_di;     // Interrupt flipflop, 0=DI, otherwise EI
        uint8_t  iff2;      // (not particularly important...)
        uint8_t  flags_and_imode;       // Interrupt mode (0, 1 or 2)
    };

    // z80 v2 snapshot header
    struct Z80SnapShotHeader2
    {
        uint16_t  length_and_version;
        uint16_t PC_reg;
        uint8_t  hardware_mode;
        uint8_t  out_mode;
        uint8_t  out_mode_intf1;
        uint8_t  simulator_flags;
        uint8_t  soundchip_reg_number;
        uint8_t  soundchip_registers[16];
    };

    // z80 v3 snapshot header
    struct Z80SnapShotHeader3
    {
        uint16_t  low_tstate_counter;
        uint8_t  high_tstate_counter;
        uint8_t  reserved_for_spectator;
        uint8_t  mgt_rom_paged;
        uint8_t  multiface_rom_paged;
        uint8_t  rom_is_ram1;
        uint8_t  rom_is_ram2;
        uint8_t  keyboard_joystick_mappings1[10];
        uint8_t  keyboard_joystick_mappings2[10];
        uint8_t  mgt_type;
        uint8_t  inhibit_button_status;
        uint8_t  inhibit_flag_rom;
        //uint8_t  last_out_0x1ffd;
    };
    struct Z80SnapShotDataHeader
    {
        uint16_t length;
        uint8_t page_num;
    };
#pragma pack(pop)

    static_assert(sizeof(Z80SnapShotHeader) == 30, "Sizeof Z80SnapShotHeader must be 30");
    static_assert(sizeof(Z80SnapShotHeader2) == 25, "Sizeof Z80SnapShotHeader2 must be 23");
    static_assert(sizeof(Z80SnapShotHeader) + sizeof(Z80SnapShotHeader2) + sizeof(Z80SnapShotHeader3) == 86, "Sizeof Z80SnapShotHeaders must be 86");

#pragma pack(push, 1)
    // https://worldofspectrum.org/faq/reference/formats.htm
    struct SnaSnapshotShotHeader
    {
        uint8_t  I_reg;
        uint16_t HLa_reg;
        uint16_t DEa_reg;
        uint16_t BCa_reg;
        uint16_t AFa_reg;
        uint16_t HL_reg;
        uint16_t DE_reg;
        uint16_t BC_reg;        
        uint16_t IY_reg;    
        uint16_t IX_reg;
        uint8_t  iff2;           // Interrupt flipflop, bit 2 0=DI, otherwise EI 
        uint8_t  R_reg;
        uint16_t  AF_reg;
        uint16_t  SP_reg;
        uint8_t imode;       // Interrupt mode (0, 1 or 2)
        uint8_t  border;
    };
#pragma pack(pop)
    static_assert(sizeof(SnaSnapshotShotHeader) == 27, "Sizeof SnaSnapshotShotHeader must be 27");

public:
    SnapShotLoader()
    {}

    ///  Load Z80 or sna snapshot file from given filename.
    template <typename TPath>
    SnapShotLoader& Load(const TPath &p_filename);

    ///  Load Z80 snapshot file from given stream.
    SnapShotLoader& LoadZ80(std::istream& p_stream);
    ///  Load sna snapshot file from given stream.
    SnapShotLoader& LoadSna(std::istream& p_stream);

    /// The given datablock should be a sjasmplus generated block with
    /// some Z80 code to set all registers.
    /// Typically would be snapshotregs.bin.
    /// Together with Symbols will replace data there with actual snapshot register values.
    SnapShotLoader& SetRegBlock(DataBlock p_snapshot_regs_block)
    {
        m_reg_block = std::move(p_snapshot_regs_block);
        return *this;
    }

    /// Move data as loaded from Z80 snapshot file to given TurboBlocks.
    /// Will replace register data as read by this class (from z80 snapshot) in the block as 
    /// given in SetRegBlock, using the symbols as known by TurboBlocks.
    void MoveToTurboBlocks(TurboBlocks& p_turbo_blocks, uint16_t p_new_loader_location,  bool p_write_fun_attribs);

    /// This is the start addres at register load code (snapshotregs.bin)
    /// When jumping to that address will load all registers, including PC, 
    /// thus activating the snapshot.
    uint16_t GetUsrAddress() const
    {
        return m_usr;
    }

    ///  Get entire (48kb) snapshot but excluding registers.
    DataBlock GetData()
    {
        return std::move(m_mem48k);
    }



private:
    void Z80SnapShotHeaderToSnapShotRegs(const Symbols& p_symbols);

    // Z80 decompress 
    // see https://worldofspectrum.org/faq/reference/z80format.htm
    DataBlock DeCompress(const DataBlock& p_block);
private:
    Z80SnapShotHeader m_z80_snapshot_header;
    DataBlock m_mem48k;         // snapshot data (as read from z80 file)
    DataBlock m_reg_block;      // Typically would be snapshotregs.bin as created by sjasmplus.
    uint16_t m_usr = 0;
    std::string m_name;
};

bool WriteTextToAttr(DataBlock& out_attr, const std::string& p_text1, std::byte p_color, bool p_center = true, int p_col = 0);
