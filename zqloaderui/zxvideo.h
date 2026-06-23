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
#include "spectrum_screen.h"

class QImage;
class QPaintEvent;



//using ColorAttr = std::byte;
using Attributes = std::vector<spectrum::screen::Attr> ;

// By using horizontal or vertical blocks the spectrum can show these
// resulutions without color clash. ZQLoader only needs to send attribute blocks.
enum WidthAndHeight
{
    wh_32x24,
    wh_64x24,
    wh_32x48
};

class ZxVideo : public QWidget
{
Q_OBJECT
public:
    ZxVideo(QWidget *parent = nullptr);
    ~ZxVideo();

    void paintEvent(QPaintEvent* event) override;
    Attributes GetAttributes() const;
    ZxVideo &SetWidthAndHeight(WidthAndHeight p_width_and_height);

    ZxVideo &Play(const std::string &p_url);

    ZxVideo &Stop();

    bool IsPlaying() const;
signals:
    void FrameReceived();
private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};


