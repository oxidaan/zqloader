#include "zxvideo.h"
#include "video.h"
#include <QWidget>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QPainter>
#include <filesystem>
#include <QCamera>
#include <QMediaCaptureSession>
#include <mutex>
#include "byte_tools.h"

namespace fs = std::filesystem;

constexpr QRgb spectrumpalette_in[]=  {0x000000, 0x0000d8, 0xd80000, 0xd800d8, 0x00d800, 0x00d8d8, 0xd8d800, 0xd8d8d8,      // normal
                                       0x000000, 0x0000ff, 0xff0000, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff};     // bright
constexpr QRgb spectrumpalette_out[]= {0x000000, 0x0000d8, 0xd80000, 0xd800d8, 0x00d800, 0x00d8d8, 0xd8d800, 0xd8d8d8,      // normal
                                       0x000000, 0x0000ff, 0xff0000, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff};     // bright


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
                std::unique_lock lock(m_image_mutex);
                m_image = image;
            }
            m_this->update();       // -> paintEvent
        });
    }
 
    void paintEvent(QPaintEvent* )  
    {
        QPainter painter(m_this);
        if (!m_image.isNull()) 
        {
            auto w = m_image.width() * 4;
            auto h = m_image.height() * 8;
            painter.drawImage(0, 0, m_video.GetImage().scaled(w, h)); // draw the image at (0,0)
            painter.drawImage(w, 0, m_image.scaled(w, h)); // draw the image at (0,0)
            auto image_attr = AttrToImage(m_width_and_height, ImageToAttr(m_image));
            painter.drawImage(2*w, 0,  image_attr.scaled(w, h)); // draw the image at (w,0)
        }
    }

    // Runs in miniaudio thread
    Attributes GetAttributes() const
    {
        QImage image;
        {
            std::unique_lock lock(m_image_mutex);
            image = m_image;
        }
        if(!image.isNull())
        {
            return ImageToAttr(image);
        }
        return {};
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


    static QImage AttrToImage(WidthAndHeight p_width_and_height, Attributes p_attr)
    {
        auto [width, height] = GetWidthAndHeight(p_width_and_height);
        QImage image(width, height, QImage::Format_RGB32);
        int bytesPerLine = image.bytesPerLine();
        auto* data = image.bits();
        if(p_width_and_height == wh_32x48)
        {
            for (int y = 0; y < 48; y++) 
            {
                for (int x = 0; x < 32; x++) 
                {
                    auto attr = p_attr[(y / 2) * 32 + x];
                    auto bright = attr.attr.bright ? 8 : 0;
                    int color_nr =   (y % 2) ? attr.attr.ink : attr.attr.paper  ;
                    int idx = bright + color_nr;
                    QRgb* row = reinterpret_cast<QRgb*>(data + y     * bytesPerLine);
                    row[x] = spectrumpalette_out[idx];
                }
            }
        }
        else if(p_width_and_height == wh_64x24)
        {
            for (int y = 0; y < 24; y++) 
            {
                for (int x = 0; x < 64; x++)   
                {
                    auto attr = p_attr[y * 32 + (x/2)];
                    auto bright = attr.attr.bright ? 8 : 0;
                    int color_nr =   (x % 2) ? attr.attr.ink : attr.attr.paper;       // paper first
                    int idx = bright + color_nr;
                    QRgb* row = reinterpret_cast<QRgb*>(data + y     * bytesPerLine);
                    row[x] = spectrumpalette_out[idx];
                }
            }
        }
        return image;
    }

    static QImage NormalizeContrast(const QImage &p_image)
    {
        QImage img = p_image.convertToFormat(QImage::Format_RGB32);

        int w = img.width();
        int h = img.height();

        int minR = 255, minG = 255, minB = 255;
        int maxR = 0,   maxG = 0,   maxB = 0;

        // 1. Find min/max per channel
        for (int y = 0; y < h; y++) 
        {
            const QRgb *line = reinterpret_cast<const QRgb*>(img.scanLine(y));
            for (int x = 0; x < w; x++) 
            {
                int r = qRed(line[x]);
                int g = qGreen(line[x]);
                int b = qBlue(line[x]);
                if(r >10 && g >10  && b > 10)
                {
                    minR = std::min(minR, r);
                    minG = std::min(minG, g); 
                    minB = std::min(minB, b); 
                }
                if(r < 245 && g < 245 && b < 245)
                {
                    maxR = std::max(maxR, r);
                    maxG = std::max(maxG, g);
                    maxB = std::max(maxB, b);
                }
            }
        }

        int rangeR = maxR - minR;
        int rangeG = maxG - minG;
        int rangeB = maxB - minB;
        // Avoid division by zero
        rangeR = rangeR ? rangeR : 1;
        rangeG = rangeG ? rangeG : 1;
        rangeB = rangeB ? rangeB : 1;

        // 2. Stretch contrast
        for (int y = 0; y < h; y++) 
        {
            QRgb *line = reinterpret_cast<QRgb*>(img.scanLine(y));
            for (int x = 0; x < w; x++)
            {
                int r = qRed(line[x]);
                int g = qGreen(line[x]);
                int b = qBlue(line[x]);

                int nr = std::clamp((r - minR) * 255 / rangeR, 0, 255);
                int ng = std::clamp((g - minG) * 255 / rangeG, 0, 255);
                int nb = std::clamp((b - minB) * 255 / rangeB, 0, 255);

                line[x] = qRgb(nr, ng, nb);
            }
        }

        return img;
    }
    
    static Attributes ImageToAttr(QImage p_image)
    {
        auto formt = p_image.format();
        if (formt != QImage::Format_RGB32)
        {
            p_image = p_image.convertToFormat(QImage::Format_RGB32);
        }
        p_image = NormalizeContrast(p_image);
        int width = p_image.width();
        int height = p_image.height();
        const auto* data = p_image.bits();
        int bytesPerLine = p_image.bytesPerLine();
        Attributes attribs;

        if(height == 48)
        {
            for (int y = 0; y < height; y+=2) 
            {
                const QRgb* row1 = reinterpret_cast<const QRgb*>(data + y     * bytesPerLine);
                const QRgb* row2 = reinterpret_cast<const QRgb*>(data + (y+1) * bytesPerLine);
                for (int x = 0; x < width; ++x) 
                {
                    QRgb color_paper = row1[x];
                    QRgb color_ink = row2[x];
                    auto attr = GetSpectrumAttribute(color_paper, color_ink);
                    attribs.push_back(attr);
                }
            }
        }
        else
        {
            for (int y = 0; y < height; y++) 
            {
                const QRgb* row = reinterpret_cast<const QRgb*>(data + y     * bytesPerLine);
                for (int x = 0; x < width; x+=2) 
                {
                    QRgb color_paper = row[x];      // paper first
                    QRgb color_ink = row[x + 1];
                    auto attr = GetSpectrumAttribute(color_paper, color_ink);
                    attribs.push_back(attr);
                }
            }
        }
        return attribs;
    }

    static std::pair<int,int> GetNeareastSpectrumColor(QRgb p_color, const QRgb *p_palette, int p_size = 8)
    {
        int r1 = qRed(p_color);
        int g1 = qGreen(p_color);
        int b1 = qBlue(p_color);
        int mindist{};
        int found;
        for(int n = 0; n < p_size; n++)
        {
            int r2 = qRed(p_palette[n]);
            int g2 = qGreen(p_palette[n]);
            int b2 = qBlue(p_palette[n]);

            // distance to mean
            int dist = (r1-r2) * (r1-r2) + (g1-g2) * (g1-g2) + (b1-b2) * (b1-b2);

            if(r2 == g2 && g2 ==b2)       // black gray or white
            {
    //            dist = (dist * 7)/8;
            }

            if((dist < mindist) || n==0)
            {
                mindist = dist;
                found = n;
            }
        }
        return {mindist, found};
    }

    // Color attr:
    // F  B  P2 P1 P0 I2 I1 I0
    static ColorAttr GetSpectrumAttribute(QRgb p_color_paper, QRgb p_color_ink)
    {
        auto [mindist_norm_paper,   found_norm_paper]   = GetNeareastSpectrumColor(p_color_paper, spectrumpalette_in,     8);
        auto [mindist_bright_paper, found_bright_paper] = GetNeareastSpectrumColor(p_color_paper, spectrumpalette_in + 8, 8);
        auto [mindist_norm_ink,     found_norm_ink]     = GetNeareastSpectrumColor(p_color_ink,   spectrumpalette_in,     8);
        auto [mindist_bright_ink,   found_bright_ink]   = GetNeareastSpectrumColor(p_color_ink,   spectrumpalette_in + 8, 8);
        bool use_bright = (mindist_bright_ink + mindist_bright_paper) < (mindist_norm_ink + mindist_norm_paper);
        //std::uint8_t retval;        // can you do anything usefull with std::byte ;-(
        //retval = use_bright ? 0b01000000 : 0;       
        //retval |= (use_bright ? found_bright_paper : found_norm_paper) << 3;
        //retval |= (use_bright ? found_bright_ink : found_norm_ink);
        ColorAttr retval{};
        retval.attr.bright = use_bright;
        retval.attr.paper  = use_bright ? found_bright_paper : found_norm_paper;
        retval.attr.ink    = use_bright ? found_bright_ink : found_norm_ink;
        retval.attr.flash = 0;
        return retval;
    }

public:
    WidthAndHeight m_width_and_height = wh_64x24;
    Video m_video;
private:
    ZxVideo *m_this;
    QImage m_image;
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
    return m_pimpl->paintEvent(event);
}

Attributes ZxVideo::GetAttributes() const
{
    return m_pimpl->GetAttributes();
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