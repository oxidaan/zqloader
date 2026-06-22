//==============================================================================
// PROJECT:         zqloader
// FILE:            spectrum_screen.h
// DESCRIPTION:     Definition of ZX spectrum Screen
//
// Copyright (c) 2026 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once



#include <cstddef>      // std::byte
#include <cstdint>
#include "spectrum_consts.h"
#include "datablock.h"

namespace spectrum
{


// spectrum::Screen
class Screen
{
public:
    // spectrum::Screen::AttributeColor
    enum class AttributeColor : uint8_t
    {
        black,          blue,        red,        magenta,        green,       cyan,        yellow,        white,
    };


    // spectrum::Screen::palette
    static constexpr int palette[] =
    {
    // black     blue      red       magenta   green     cyan      yellow    white
       0x000000, 0x0000d8, 0xd80000, 0xd800d8, 0x00d800, 0x00d8d8, 0xd8d800, 0xd8d8d8,     // normal
       0x000000, 0x0000ff, 0xff0000, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff      // bright
    };


    // spectrum::Screen::Attr
    // ZX Spectrum color attribute
    // https://www.overtakenbyevents.com/lets-talk-about-the-zx-specrum-screen-layout/#:~:text=Each%20block%20of%208x8%20pixels%20has%20a%20single,if%20set%20indicates%20the%20colours%20are%20rendered%20bright.
    union Attr
    {
        struct
        {
            AttributeColor  ink: 3;
            AttributeColor  paper: 3;
            uint8_t         bright: 1;
            uint8_t         flash : 1;
        } attr;
        std::byte   byte;
    };
    static_assert(sizeof( Attr ) == 1);

    // spectrum::Screen::AttrPaperToColor

    // For given attribute return paper color as rgb value.
    static auto AttrPaperToRgbColor(const Attr &p_attr)
    {
        return palette[int(p_attr.attr.paper) + 8 * p_attr.attr.bright];
    }
    // For given attribute return ink color as rgb value.
    static auto AttrInkToRgbColor(const Attr &p_attr)
    {
        return palette[int(p_attr.attr.ink) + 8 * p_attr.attr.bright];
    }


public:

    Screen()
    {
        m_datablock.resize(spectrum::SCREEN_SIZE);
    }
    Screen(Screen &&) = default;
    Screen &operator = (Screen &&) = default;


    /// Set pixel. Codes the infamous spectrum screen layout.
    void SetPixel(int x, int y, bool p_value)
    {
        // 0  1  0  y7 y6  y2 y1 y0    y5 y4 y3 x7 x6 x5 x4 x3
        auto address = (( y & 0b11000000 ) << 5 ) + (( y & 0b00000111 ) << 8 ) + (( y & 0b00111000 ) << 2 ) + (( x & 0b11111000 ) >> 3 );
        auto bit     = 1 << ( x & 0b00000111 );
        int  value   = int(m_datablock[address]);
        value                = p_value ? value | bit : value & ~bit;
        m_datablock[address] = std::byte(value);
    }



    /// Get pixel. Codes the infamous spectrum screen layout.
    bool GetPixel(int x, int y) const
    {
        // 0  1  0  y7 y6  y2 y1 y0    y5 y4 y3 x7 x6 x5 x4 x3
        auto address = (( y & 0b11000000 ) << 5 ) + (( y & 0b00000111 ) << 8 ) + (( y & 0b00111000 ) << 2 ) + (( x & 0b11111000 ) >> 3 );
        auto bit     = 1 << ( x & 0b00000111 );
        int  value   = int(m_datablock[address]);
        return value & bit;
    }



    /// Coordinates are attribute coordinates (32x24)
    void SetAttribute(int attr_x, int attr_y, spectrum::Screen::Attr p_attr)
    {
        m_datablock[spectrum::SCREEN_PIXEL_SIZE + attr_y * 32 + attr_x] = p_attr.byte;
    }



    /// Coordinates are attribute coordinates (32x24)
    spectrum::Screen::Attr GetAttribute(int attr_x, int attr_y) const
    {
        spectrum::Screen::Attr a;
        a.byte = m_datablock[spectrum::SCREEN_PIXEL_SIZE + attr_y * 32 + attr_x];
        return a;
    }



    const DataBlock &GetDataBlock() const
    {
        return m_datablock;
    }

private:

    DataBlock   m_datablock;
};


}

