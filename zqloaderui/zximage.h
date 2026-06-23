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
#include <QWidget>
#include <filesystem>
#include "spectrum_screen.h"

class QImage;
class QPaintEvent;


class ZxImage : public QWidget
{
Q_OBJECT
public:
    ZxImage(QWidget *parent = nullptr);
    ~ZxImage();

    void SetDirectory(const std::filesystem::path &p_path);

    const spectrum::screen::Screen &LoadNext();

    void paintEvent(QPaintEvent* event) override;


private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};


