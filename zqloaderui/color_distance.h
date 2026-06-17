//==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            color_distance.h
// DESCRIPTION:     Distance to mean helper function. Needs QRgb.
//
// Copyright (c) 2026 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#pragma once

#include <QColor>
#include <span>

// distance to mean, squared.
inline int ColorDistance(QRgb p_color1, QRgb p_color2)
{
    int r1 = qRed(p_color1);
    int g1 = qGreen(p_color1);
    int b1 = qBlue(p_color1);
    int r2 = qRed(p_color2);
    int g2 = qGreen(p_color2);
    int b2 = qBlue(p_color2);
    return ( r1 - r2 ) * ( r1 - r2 ) + ( g1 - g2 ) * ( g1 - g2 ) + ( b1 - b2 ) * ( b1 - b2 );
}



// For given color, find nearest color at given palette.
// p_size is size of p_palette.
// returns index at p_palette (0 is first) for nearest found color plus
// the distance (squared) to that color.
inline std::pair<int, int> GetNearestColor(const QRgb &p_color,
    std::span<const int>p_palette, std::span<const int> p_which_colors )
{
    int mindist{};
    int found_index{};
    bool first = true;
    for(int n = 0; n < p_palette.size(); n++)
    {
        // c++23 if(std::ranges::contains(p_which_colors, n))
        if(std::ranges::find(p_which_colors, n) != p_which_colors.end())
        {

            int dist = ColorDistance(p_color, p_palette[n]);

            if(( dist < mindist ) || first)
            {
                mindist     = dist;
                found_index = n;
            }
            first = false;
        }
    }
    return { mindist, found_index };
}

