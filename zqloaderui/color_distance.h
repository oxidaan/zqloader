//==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            color_distance.h
// DESCRIPTION:     Distance to mean helper function. Needs QRgb. But not 
//                  spectrum specific.
//
// Copyright (c) 2026 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#pragma once

#include <QColor>
#include <span>

/// Distance to mean, squared.
inline int ColorDistanceRgb(QRgb p_color1, QRgb p_color2)
{
    int r1 = qRed(p_color1);
    int g1 = qGreen(p_color1);
    int b1 = qBlue(p_color1);
    int r2 = qRed(p_color2);
    int g2 = qGreen(p_color2);
    int b2 = qBlue(p_color2);
    return ( r1 - r2 ) * ( r1 - r2 ) + ( g1 - g2 ) * ( g1 - g2 ) + ( b1 - b2 ) * ( b1 - b2 );
}

inline int ColorDistanceHsv(QRgb p_color1, QRgb p_color2)
{
    auto h1 = QColor(p_color1).hue();
    auto h2 = QColor(p_color2).hue();
    auto s1 = QColor(p_color1).saturation();
    auto s2 = QColor(p_color2).saturation();
    auto v1 = QColor(p_color1).value();
    auto v2 = QColor(p_color2).value();
    return ( h1 - h2 ) * ( h1 - h2 ) + ( s1 - s2 ) * ( s1 - s2 ) + ( v1 - v2 ) * ( v1 - v2 );
}


inline bool IsAlmostGray(QRgb p_rgb, int p_threshold = 10)
{
    QColor color(p_rgb);

    // Saturation: 0 = gray, 255 = fully saturated
    return color.hsvSaturation() < p_threshold;
}


inline bool IsAlmostSkin(QRgb p_rgb)
{
    int r = qRed(p_rgb);
    int g = qGreen(p_rgb);
    int b = qBlue(p_rgb);

    // Convert to YCbCr (approximation)
    //int Y  =  0.299*r + 0.587*g + 0.114*b;
    int Cb = -0.1687*r - 0.3313*g + 0.5*b + 128;
    int Cr =  0.5*r - 0.4187*g - 0.0813*b + 128;

    // Typical skin-color range
    return (Cb >= 77 && Cb <= 127) &&
           (Cr >= 133 && Cr <= 173);
}


/// For given color, find nearest color at given palette.
/// returns index at p_palette (0 is first) for nearest found color plus
/// the distance (squared) to that color.
/// Can optionally limit acceptable colors in given pallete with colors in p_which_colors.
/// Eg to use filter out 'dark' and 'light' colors.
/// Generic - so not spectrum specific.
inline std::pair<int, int> GetNearestColor(const QRgb &p_color,
    std::span<const uint32_t>p_palette, std::span<const int> p_which_colors = {} )
{
    int mindist{};
    int found_index{};
    bool first = true;
    for(int n = 0; n < p_palette.size(); n++)
    {
        // c++23 if(std::ranges::contains(p_which_colors, n))
        if(p_which_colors.size() == 0 || std::ranges::find(p_which_colors, n) != p_which_colors.end())
        {

            int dist = ColorDistanceRgb(p_color, p_palette[n]);

            if(( dist < mindist ) || first)
            {
                mindist     = dist;
                found_index = n;
                first = false;
            }
        }
    }
    return { mindist, found_index };
}


inline std::pair<int, int> GetNearestColor(
    const QRgb& p_color,
    std::span<const uint32_t> p_palette,
    std::initializer_list<int> colors)
{
    return GetNearestColor(
        p_color,
        p_palette,
        std::span<const int>(colors.begin(), colors.size()));
}


