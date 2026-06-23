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
#include <cstdint>      // uint8_t
#include "spectrum_consts.h"
#include "datablock.h"

namespace spectrum::screen
{
constexpr uint16_t SCREEN_WIDTH = 256;
constexpr uint16_t SCREEN_HEIGHT = 192;
constexpr uint16_t SCREEN_PIXEL_SIZE = (SCREEN_WIDTH * SCREEN_HEIGHT) / 8;
constexpr uint16_t ATTR_SIZE = (SCREEN_WIDTH * SCREEN_HEIGHT) / 64;
constexpr uint16_t SCREEN_SIZE = SCREEN_PIXEL_SIZE + ATTR_SIZE;
constexpr uint16_t SCREEN_23RD = SCREEN_START + 4 * 1024;
constexpr uint16_t SCREEN_END = SCREEN_START + SCREEN_PIXEL_SIZE;
constexpr uint16_t ATTR_BEGIN = SCREEN_END;
constexpr uint16_t ATTR_23RD = ATTR_BEGIN + 512;


/// Defines 8 spectrum color names.
enum class AttributeColor : uint8_t
{
    black,          blue,        red,        magenta,        green,       cyan,        yellow,        white,
};


/// Defines spectrum colors as RGB
/// Source https://lospec.com/palette-list/zx-spectrum
/// I doubt this ok. Especially bright blue is much darker at my ZX Spectrum.
/// But hey! I am colorblind.
static constexpr uint32_t palette[] =
{
// black     blue      red       magenta   green     cyan      yellow    white
   0x000000, 0x0000d8, 0xd80000, 0xd800d8, 0x00d800, 0x00d8d8, 0xd8d800, 0xd8d8d8,     // normal
   0x000000, 0x0000ff, 0xff0000, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff      // bright
};


/// ZX Spectrum color attribute
/// https://www.overtakenbyevents.com/lets-talk-about-the-zx-specrum-screen-layout/#:~:text=Each%20block%20of%208x8%20pixels%20has%20a%20single,if%20set%20indicates%20the%20colours%20are%20rendered%20bright.
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
inline auto AttrPaperToRgbColor(const Attr &p_attr)
{
    return palette[int(p_attr.attr.paper) + 8 * p_attr.attr.bright];
}
// For given attribute return ink color as rgb value.
inline auto AttrInkToRgbColor(const Attr &p_attr)
{
    return palette[int(p_attr.attr.ink) + 8 * p_attr.attr.bright];
}


/// spectrum::Screen
/// Used for image fun.
class Screen
{
public:

    Screen()
    {
        m_datablock.resize(SCREEN_SIZE);
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
    void SetAttribute(int attr_x, int attr_y, spectrum::screen::Attr p_attr)
    {
        m_datablock[SCREEN_PIXEL_SIZE + attr_y * 32 + attr_x] = p_attr.byte;
    }



    /// Coordinates are attribute coordinates (32x24)
    Attr GetAttribute(int attr_x, int attr_y) const
    {
        Attr a;
        a.byte = m_datablock[SCREEN_PIXEL_SIZE + attr_y * 32 + attr_x];
        return a;
    }


    /// Get as datablock, can be sent with ZQLoader.
    const DataBlock &GetDataBlock() const
    {
        return m_datablock;
    }

private:

    DataBlock   m_datablock;
};


}

