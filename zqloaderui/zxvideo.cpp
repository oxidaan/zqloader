//==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            zxvideo.cpp
// DESCRIPTION:     Video fun!
// 
// Copyright (c) 2026 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#include "zxvideo.h"
#include "video.h"
#include <QWidget>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QPainter>
#include <QCamera>
#include <QMediaCaptureSession>
#include <mutex>
#include "color_distance.h"
#include "byte_tools.h"

namespace fs = std::filesystem;


class ZxVideo::Impl
{
    friend class Video;

public:
    Impl(ZxVideo *p_this) :
        m_this(p_this)
    {
        connect(&m_video, &Video::FrameReceived, [this]
        {
            {
                auto [width, height] = GetWidthAndHeight(m_width_and_height);
                QImage image =  m_video.GetImage().scaled(width, height,
                                 Qt::IgnoreAspectRatio,
                                 Qt::FastTransformation);
                auto spectrum_image = ImageToAttr(image);
                m_scaled_image = std::move(image);
                std::unique_lock lock(m_image_mutex);
                m_spectrum_image = std::move(spectrum_image);
            }
            m_this->update();       // -> paintEvent
        });
    }
 
    void paintEvent(QPaintEvent* )  
    {
        QPainter painter(m_this);
        if (!m_scaled_image.isNull())
        {
            auto w = m_width_and_height == wh_64x24 ? m_scaled_image.width() * 4  : m_scaled_image.width() * 8;
            auto h = m_width_and_height == wh_32x48 ? m_scaled_image.height() * 4 : m_scaled_image.height() * 8;
            painter.drawImage(0, 0, m_video.GetImage().scaled(w, h));       // draw the original image
            painter.drawImage(w + 4, 0, m_scaled_image.scaled(w, h));              // draw the image at spectrum resolution (enlarged)
            auto image_attr = AttrToImage(m_width_and_height, m_spectrum_image);
            painter.drawImage(2 * w + 8, 0,  image_attr.scaled(w, h));      // draw the spectrum image at (w,0)
        }
    }

    // Runs in miniaudio thread
    Attributes GetAttributes() const
    {
        std::unique_lock lock(m_image_mutex);
        return m_spectrum_image.Clone();
    }

    DataBlock GetPattern() const
    {
        DataBlock block;
        if (m_width_and_height == WidthAndHeight::wh_32x48)
        {
            for(int i = 0; i < 3; i++)
            {
                for (int n = 0; n < (4 * 32 * 8); n++)
                {
                    block.push_back(0x0_byte);      // paper
                }
                for (int n = 0; n < (4 * 32 * 8); n++)
                {
                    block.push_back(0xff_byte);     // ink
                }
            }
        }
        else
        {
            for (int n = 0; n < spectrum::screen::SCREEN_PIXEL_SIZE; n++)
            {
                block.push_back(0x0f_byte);
            }
        }
        return block;
    }


private:
    static std::pair<int, int> GetWidthAndHeight(WidthAndHeight p_width_and_height)
    {
        switch(p_width_and_height)
        {
        case wh_32x24:
            return {32, 24};
        case wh_64x24:
            return {64, 24};
        case wh_32x48:
            return {32, 48};
        }
        throw std::runtime_error("Invalid WidthAndHeight enum");
    }

    // To show only (from paintEvent); not sent to ZX Spectrum.
    static QImage AttrToImage(WidthAndHeight p_width_and_height, const Attributes &p_attr)
    {
        auto [width, height] = GetWidthAndHeight(p_width_and_height);
        QImage image(width, height, QImage::Format_RGB32);
        auto bytesPerLine = image.bytesPerLine();
        auto* data = image.bits();
        if(p_width_and_height == wh_32x48)
        {
            for (int y = 0; y < 48; y++) 
            {
                for (int x = 0; x < 32; x++) 
                {
                    spectrum::screen::Attr attr{};
                    attr.byte = p_attr[(y / 2) * 32 + x];
                    QRgb* row = reinterpret_cast<QRgb*>(data + y * bytesPerLine);
                    auto color = (y % 2) ? spectrum::screen::AttrInkToRgbColor(attr) : spectrum::screen::AttrPaperToRgbColor(attr);
                    row[x] = color;
                }
            }
        }
        else if(p_width_and_height == wh_64x24)
        {
            for (int y = 0; y < 24; y++) 
            {
                for (int x = 0; x < 64; x++)   
                {
                    spectrum::screen::Attr attr{};
                    attr.byte = p_attr[y * 32 + (x/2)];
                    QRgb* row = reinterpret_cast<QRgb*>(data + y * bytesPerLine);
                    auto color = (x % 2) ? spectrum::screen::AttrInkToRgbColor(attr) : spectrum::screen::AttrPaperToRgbColor(attr);
                    row[x] = color;
                }
            }
        }
        else 
        {
            for (int y = 0; y < 24; y++)
            {
                for (int x = 0; x < 32; x++)
                {
                    spectrum::screen::Attr attr{};
                    attr.byte = p_attr[y * 32 + x ];
                    QRgb* row = reinterpret_cast<QRgb*>(data + y * bytesPerLine);
                    auto color = spectrum::screen::AttrInkToRgbColor(attr);
                    row[x] = color;
                }
            }
        }
        return image;
    }
  
    
    // Get zx spectrum attribute array for image, can be sent to ZX Spectrum.
    // given image must be 32x48 or 64x24 or 32x24
    static Attributes ImageToAttr(QImage p_image)
    {
        auto formt = p_image.format();
        if (formt != QImage::Format_RGB32)
        {
            p_image = p_image.convertToFormat(QImage::Format_RGB32);
        }
        int width = p_image.width();
        int height = p_image.height();
        const auto* data = p_image.bits();
        auto bytesPerLine = p_image.bytesPerLine();
        Attributes attribs;

        if(height == 48)
        {
            for (int y = 0; y < height; y+=2) 
            {
                const QRgb* row0 = reinterpret_cast<const QRgb*>(data + y     * bytesPerLine);
                const QRgb* row1 = reinterpret_cast<const QRgb*>(data + (y+1) * bytesPerLine);
                for (int x = 0; x < width; x++) 
                {
                    QRgb color_paper = row0[x];     // y = 0,2 4,...
                    QRgb color_ink = row1[x];       // y = 1,3,5,...
                    auto attr = GetSpectrumAttribute(color_paper, color_ink);
                    attribs.push_back(attr.byte);
                }
            }
        }
        else if (width == 64)
        {
            for (int y = 0; y < height; y++) 
            {
                const QRgb* row = reinterpret_cast<const QRgb*>(data + y     * bytesPerLine);
                for (int x = 0; x < width; x+=2) 
                {
                    QRgb color_paper = row[x];      // paper first
                    QRgb color_ink = row[x + 1];
                    auto attr = GetSpectrumAttribute(color_paper, color_ink);
                    attribs.push_back(attr.byte);
                }
            }
        }
        else
        {
            for (int y = 0; y < height; y++)
            {
                const QRgb* row = reinterpret_cast<const QRgb*>(data + y * bytesPerLine);
                for (int x = 0; x < width; x++)
                {
                    QRgb color = row[x];      
                    auto attr = GetSpectrumAttribute(color, color);
                    attribs.push_back(attr.byte);
                }
            }
        }
        return attribs;
    }

    // Get a spectrum attribute from the two given rgb values.
    // Color attr:
    // F  B  P2 P1 P0 I2 I1 I0
    static spectrum::screen::Attr GetSpectrumAttribute(QRgb p_color_paper, QRgb p_color_ink)
    {

        using enum spectrum::screen::PaletteColor;
        std::initializer_list<int> spectrum_normal_colors = { black, blue, red, magenta, green, cyan, yellow, white };
        std::initializer_list<int> spectrum_bright_colors = { br_black, br_blue, br_red, br_magenta, br_green, br_cyan, br_yellow, br_white };


        auto [mindist_norm_paper,   found_norm_paper]   = GetNearestColor(p_color_paper, spectrum::screen::palette,  spectrum_normal_colors);
        auto [mindist_bright_paper, found_bright_paper] = GetNearestColor(p_color_paper, spectrum::screen::palette,  spectrum_bright_colors);
        auto [mindist_norm_ink,     found_norm_ink]     = GetNearestColor(p_color_ink,   spectrum::screen::palette,  spectrum_normal_colors);
        auto [mindist_bright_ink,   found_bright_ink]   = GetNearestColor(p_color_ink,   spectrum::screen::palette,  spectrum_bright_colors);
        bool use_bright = (mindist_bright_ink + mindist_bright_paper) < (mindist_norm_ink + mindist_norm_paper);
        //std::uint8_t retval;        // can you do anything usefull with std::byte ;-(
        //retval = use_bright ? 0b01000000 : 0;       
        //retval |= (use_bright ? found_bright_paper : found_norm_paper) << 3;
        //retval |= (use_bright ? found_bright_ink : found_norm_ink);
        spectrum::screen::Attr retval{};
        retval.attr.bright = use_bright;
        retval.attr.paper  = static_cast<spectrum::screen::Attr::Color>(use_bright ? found_bright_paper - 8 : found_norm_paper);
        retval.attr.ink    = static_cast<spectrum::screen::Attr::Color>(use_bright ? found_bright_ink - 8 : found_norm_ink);
        retval.attr.flash = 0;
        return retval;
    }

public:
    WidthAndHeight m_width_and_height = wh_32x48;// wh_64x24;
    Video m_video;
private:
    ZxVideo *m_this;               // back pointer
    QImage m_scaled_image;         // image resulution is 64x24 or 32x48 depending on m_width_and_height
    Attributes m_spectrum_image;
    mutable std::mutex m_image_mutex;
};



ZxVideo::ZxVideo(QWidget *p_parent) :
    QWidget(p_parent),
    m_pimpl( new Impl(this) )
{
}



ZxVideo::~ZxVideo() = default;


void ZxVideo::paintEvent(QPaintEvent* event)
{
    m_pimpl->paintEvent(event);
}

Attributes ZxVideo::GetAttributes() const
{
    return m_pimpl->GetAttributes();
}

DataBlock ZxVideo::GetPattern() const
{
    return m_pimpl->GetPattern();
}



ZxVideo &ZxVideo::SetWidthAndHeight(WidthAndHeight p_width_and_height)
{
    m_pimpl->m_width_and_height = p_width_and_height;
    return *this;
}

ZxVideo &ZxVideo::Play(const std::string &p_url)
{
    m_pimpl->m_video.Play(p_url);
    return *this;
}

ZxVideo &ZxVideo::Stop()
{
    m_pimpl->m_video.Stop();
    return *this;
}

bool ZxVideo::IsPlaying() const
{
    return m_pimpl->m_video.IsPlaying();
}