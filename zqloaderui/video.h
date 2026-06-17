//==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            video.h
// DESCRIPTION:     QMediaPlayer/QCamera etc wrapper
// 
// Copyright (c) 2026 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================




#pragma once



#include <memory>
#include <string>

#include <QObject>
class QImage;


/// Video player
/// Plays froms stream, file or camera hardware.
/// Signals FrameReceived; get last frame at GetImage.
class Video : public QObject
{
Q_OBJECT
public:
    Video();

    ~Video();

    /// Play from file, stream or hardware camera
    bool Play(const std::string &p_url);

    /// Stop playing
    Video &Stop();


    bool IsPlaying() const;

    /// Get last frame as QImage
    QImage GetImage() const;
signals:
    // A new frame is present.
    void FrameReceived();
private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};


