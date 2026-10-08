//==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            zxvideo.h
// DESCRIPTION:     Video fun!
// 
// Copyright (c) 2026 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================



#pragma once



#include <memory>
#include <vector>
#include <set>
#include <QWidget>
#include <filesystem>
#include "spectrum_screen.h"

class QImage;
class QPaintEvent;


struct AlgorithmParameters
{
    bool m_use_floyd_steinberg = true;
    bool m_use_distance_to_black_white = true;      // floyd steinberg uses grayscale image
    bool m_use_simple_count = false;                // for color distance count nearest color directly or take distance into account.
    bool m_use_dark_and_light = true;               // when calculating attributes use a light and dark color from subsets below

    std::set<int> m_dark_colors  = spectrum::screen::spectrum_dark_colors;
    std::set<int> m_light_colors = spectrum::screen::spectrum_light_colors;
};

class ZxImage : public QWidget
{
Q_OBJECT
public:
    ZxImage(QWidget *parent = nullptr);
    ~ZxImage();

    ZxImage& SetDirectory(const std::filesystem::path &p_path);

    const spectrum::screen::Screen GetLastLoadedScreen();

    void paintEvent(QPaintEvent* event) override;

    ZxImage& SetAlgorithmParameters(AlgorithmParameters p_how);

    AlgorithmParameters GetAlgorithmParameters() const;


private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};


