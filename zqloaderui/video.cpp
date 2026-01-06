#include "video.h"
#include <QWidget>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <filesystem>
#include <QCamera>
#include <QMediaCaptureSession>
#include <iostream>

namespace fs = std::filesystem;


class Video::Impl
{
    friend class Video;

public:

    Impl(Video *p_this) :
        m_this(p_this)
    {
        //m_media_player.setVideoSink(&m_video_sink);
        QObject::connect(&m_media_player, &QMediaPlayer::errorOccurred, [this](QMediaPlayer::Error e)
        {
            std::cout << "Mediaplayer Error:" << e << std::endl; 
        });
        QObject::connect(&m_media_player, &QMediaPlayer::mediaStatusChanged, [this](QMediaPlayer::MediaStatus s)
        {
            std::cout << "Mediaplayer Status:" << s<< std::endl; 
            if (s == QMediaPlayer::EndOfMedia && m_auto_repeat)
            {
                m_media_player.play();
            }
        });
        QObject::connect(&m_media_player, &QMediaPlayer::playbackStateChanged, [this](QMediaPlayer::PlaybackState s)
        {
            std::cout << "Mediaplayer State:" << s<< std::endl;  
        });
        QObject::connect(&m_video_sink, &QVideoSink::videoFrameChanged,[this](const QVideoFrame &p_frame)
        {
            m_frame_received = true;
            m_frame = p_frame;
            emit m_this->FrameReceived();
        }
        );
    }



    ~Impl()
    {}



    bool Play( const std::string &p_url )
    {
        Stop();
        if( p_url.find( "camera" ) == 0 )
        {
            m_session.setVideoSink(&m_video_sink);
            m_session.setCamera(&m_camera);
            m_camera.start();
            return true;
        }
        else if( p_url.find( "rtsp:" ) == 0 )
        {
            m_media_player.setVideoSink(&m_video_sink);
            m_media_player.setSource( QUrl( p_url.c_str() ) );
            m_media_player.play();
            return true;
        }
        else if(fs::exists(p_url) )
        {
            m_media_player.setVideoSink(&m_video_sink);
            m_media_player.setSource( QUrl::fromLocalFile(QString::fromStdString(p_url)) );
            m_media_player.play();
            return true;
        }
        else
        {
            throw std::runtime_error("File " + p_url + " not  found");
        }
    }



    void Stop()
    {
        m_frame_received = false;
        m_media_player.stop();
        m_camera.stop();
    }




    bool IsPlaying() const
    {
        return m_frame_received && m_media_player.playbackState() == QMediaPlayer::PlayingState;
    }

    QImage GetImage() const
    {
        if( m_frame_received )
        {
            return m_frame.toImage();
        }
        return QImage{};
    }






private:
    bool                    m_auto_repeat = true;
    Video *                 m_this;
    QMediaPlayer            m_media_player;     // used when playing stream or file
    QCamera                 m_camera;           // used when camera capture
    QMediaCaptureSession    m_session;          // used when camera capture
    QVideoSink              m_video_sink;
    QVideoFrame             m_frame;            // last frame received at m_video_sink
    bool                    m_frame_received = false;
};



Video::Video() :
    m_pimpl( new Impl(this) )
{
}



Video::~Video() = default;


bool Video::Play( const std::string &p_url )
{
    return m_pimpl->Play( p_url );
}





Video &Video::Stop()
{
    m_pimpl->Stop();
    return *this;
}



bool Video::IsPlaying() const
{
    return m_pimpl->IsPlaying();
}




QImage Video::GetImage() const
{
    return m_pimpl->GetImage();
}



