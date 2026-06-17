// ==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            ZxImage.cpp
// DESCRIPTION:     Video fun
//
// Copyright (c) 2026 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================


#include "zximage.h"
#include "spectrum_consts.h"
#include "spectrum_screen.h"
#include <span>
#include "color_distance.h"
#include <mutex>
#include <QPainter>

namespace fs = std::filesystem;




constexpr int spectrum_dark_colors[]  = {0,1,2};        // black, blue, red
constexpr int spectrum_light_colors[] = {3,4,5,6,7,11,12,13,14,15};  // all others


using Attributes = std::vector<spectrum::Screen::Attr>;

class ZxImage::Impl
{
    friend class Video;

public:

    Impl(ZxImage *p_this) :
        m_this(p_this)
    {}



    void paintEvent(QPaintEvent*)
    {

        QPainter painter(m_this);
        std::unique_lock lock(m_mutex);
        if (!m_image.isNull())
        {
            auto w = 400;
            auto h = 300;
            painter.drawImage(0, 0, m_image.scaled(w, h));              // draw the original image
            painter.drawImage(w, 0, SpectrumScreenToImage(m_screen).scaled(w, h));
        }
    }

    void SetDirectory(const fs::path &p_dir)
    {
        std::unique_lock lock(m_mutex);
        m_index = 0;
        for (fs::path filename : fs::directory_iterator{p_dir})
        {
            if(filename.extension() == ".png" || filename.extension() == ".jpg" )
            {
                m_filenames.push_back(std::move(filename));
            }
        }
        if(m_filenames.size() == 0)
        {
            throw std::runtime_error("No image (.png/.jpg) filenames found at directory: " + p_dir.string());
        }
    }

    // Runs in miniaudio thread
    const spectrum::Screen &LoadNext()
    {
        fs::path image_filename;
        {
            std::unique_lock lock(m_mutex);
            image_filename = m_filenames[m_index];
            m_index++;
            if(m_index >= m_filenames.size())
            {
                m_index = 0;
            }
        }
        QImage image(QString::fromStdString(image_filename.string()));
        auto screen =  ImageToSpectrumScreen(image);

        {
            std::unique_lock lock(m_mutex);
            m_image = std::move(image);
            m_screen = std::move(screen);
        }
        m_this->update();       // -> paintEvent
        return m_screen;
    }

    static std::pair<int, QRgb> FindClosest(const QRgb &p_color, spectrum::Screen::Attr p_spectrum_attr)
    {
        QRgb paper_color = spectrum::Screen::palette[p_spectrum_attr.attr.paper + 8 * p_spectrum_attr.attr.bright]; // light
        QRgb ink_color   = spectrum::Screen::palette[p_spectrum_attr.attr.ink   + 8 * p_spectrum_attr.attr.bright]; // dark
        auto dp          = ColorDistance(p_color, paper_color);
        auto di          = ColorDistance(p_color, ink_color);
        return dp < di ? std::pair<int, QRgb> { 0, paper_color } : std::pair<int, QRgb> { 1, ink_color };
    }



    static void SetFloydSteinbergColor(QImage &p_image, int x, int y, int p_red_error, int p_green_error, int p_blue_error, int p_factor)
    {
        QRgb sample = p_image.pixel(x, y);
        auto r     = qRed(sample)   + (p_red_error   * p_factor) / 16;
        auto g     = qGreen(sample) + (p_green_error * p_factor) / 16;
        auto b     = qBlue(sample)  + (p_blue_error  * p_factor) / 16;
        p_image.setPixel(x, y, qRgb(r, g, b));
    }


    // Can run in miniaudio thread
    spectrum::Screen ImageToSpectrumScreen(const QImage &p_image)
    {
        spectrum::Screen spectrum_screen;
        // 1) Scale to 256x192
        QImage image256x192 = p_image.scaled(spectrum::SCREEN_WIDTH, spectrum::SCREEN_HEIGHT,
                                             Qt::IgnoreAspectRatio,
                                             Qt::FastTransformation);
        auto formt = image256x192.format();
        if (formt != QImage::Format_RGB32)
        {
            image256x192 = image256x192.convertToFormat(QImage::Format_RGB32);
        }
        // 2) Determine color attribute.
        for(int attry = 0; attry < 24; attry++)
        {
            for(int attrx = 0; attrx < 32; attrx++)
            {
                spectrum_screen.SetAttribute(attrx, attry, GetAttributeForCell(p_image, attrx, attry));
            }
        }
        // 3) Floyd–Steinberg dithering
        for(int y = 0; y < spectrum::SCREEN_HEIGHT; y++)
        {
            for(int x = 0; x < spectrum::SCREEN_WIDTH; x++)
            {
                spectrum::Screen::Attr attr_here = spectrum_screen.GetAttribute(x / 8, y / 8);
                QRgb oldcolor       = image256x192.pixel(x, y);
                auto [is_ink, newcolor] = FindClosest(oldcolor, attr_here);
                image256x192.setPixel(x, y, newcolor);
                spectrum_screen.SetPixel(x, y, is_ink);
#if 0   // TODO @DEBUG
                int red_error   = qRed(oldcolor) - qRed(newcolor);
                int green_error = qGreen(oldcolor) - qGreen(newcolor);
                int blue_error  = qBlue(oldcolor) - qBlue(newcolor);
                //auto attr_right       = spectrum_screen.GetAttribute((x+1) / 8, y     / 8);
                //auto attr_left_below  = spectrum_screen.GetAttribute((x-1) / 8, (y+1) / 8);
                //auto attr_below       = spectrum_screen.GetAttribute( x    / 8, (y+1) / 8);
                //auto attr_right_below = spectrum_screen.GetAttribute((x+1) / 8, (y+1) / 8);
                if(/*attr_here.byte == attr_right.byte && */x < 255)
                    SetFloydSteinbergColor(image256x192, x + 1, y,     red_error, green_error, blue_error, 7);
                if(/*attr_here.byte == attr_left_below.byte && x > 0 && */ y < 191)
                    SetFloydSteinbergColor(image256x192, x - 1, y + 1, red_error, green_error, blue_error, 3);
                if(/* attr_here.byte == attr_below.byte && */y < 191)
                    SetFloydSteinbergColor(image256x192, x,     y + 1, red_error, green_error, blue_error, 5);
                if(/* attr_here.byte == attr_right_below.byte && x < 255 && */ y < 191)
                    SetFloydSteinbergColor(image256x192, x + 1, y + 1, red_error, green_error, blue_error, 1);
#endif
            }
        }
        return spectrum_screen;
    }

     static QImage SpectrumScreenToImage(const spectrum::Screen &p_image)
     {
        QImage image(spectrum::SCREEN_WIDTH, spectrum::SCREEN_HEIGHT, QImage::Format_RGB32);
        for(int y = 0; y < spectrum::SCREEN_HEIGHT ; y++)
        {
            for(int x = 0; x < spectrum::SCREEN_WIDTH ; x++)
            {
                bool bit = p_image.GetPixel(x, y);
                auto attr = p_image.GetAttribute(x / 8, y / 8);
                int  coloridx = (bit ? attr.attr.ink : attr.attr.paper) + 8 *  attr.attr.bright;
                image.setPixel(x, y, spectrum::Screen::palette[coloridx]);
            }
        }
        return image;
     }

    // p_attr_x,  p_attr_y are attribute (32x24) coordinates thus corresponding to top left pixel
    // (as QRgb) of an 8x8 cell at a QImage.
    // For each 8x8 (=64) pixels of that cell determine:
    // nearest spectrum color considered dark and nearest spectrum color considered light and
    // count them both. Lower distances to nearest color count heavier.
    // Then find the indexes for the most used spectrum color considerded dark and light,
    // return those as spectrum attibute. Using the light colors also detemine bright flag.
    // (At spectrum bright flag has barely effect on dark colors).
    spectrum::Screen::Attr GetAttributeForCell(const QImage &p_image, int p_attr_x, int p_attr_y)
    {
        int count_light[16]{};
        int count_dark[16]{};
        for(int y = 0; y < 8; y++)
        {
            for(int x = 0; x < 8; x++)
            {
                QRgb rgb = p_image.pixel(p_attr_x * 8 + x, p_attr_y * 8 + y);
                auto [dist_light, index_light] = GetNearestColor(rgb, spectrum::Screen::palette, spectrum_light_colors);
                auto [dist_dark,  index_dark]  = GetNearestColor(rgb, spectrum::Screen::palette, spectrum_dark_colors);
                count_light[index_light]      ++;//= (( 256 * 256 * 3 ) - dist_light ); // TODO @DEBUG
                count_dark [index_dark]       ++;//= (( 256 * 256 * 3 ) - dist_dark );
            }
        }
        auto FindMax = []( int p_found[])
                       {
                           int max   = 0;
                           int found = 0;
                           for(int i = 0; i < 16; i++)
                           {
                               if(p_found[i] > max || max == 0)
                               {
                                   found = i;
                                   max   = p_found[i];
                               }
                           }
                           return found;
                       };
        spectrum::Screen::Attr attr;
        auto dark_color  = FindMax(count_dark);      // black, blue, red so 0, 1 or 2
        auto light_color = FindMax(count_light);     // so magenta(3) -- bright white (15)
        attr.attr.ink    = dark_color;
        attr.attr.paper  = light_color % 8;
        attr.attr.bright = light_color > 8;

        return attr;
    }

private:
    std::vector<fs::path> m_filenames;
    int m_index = 0;
    ZxImage * m_this;
    QImage    m_image;
    spectrum::Screen m_screen;
    mutable std::mutex m_mutex;
}; // class ZxImage::Impl



ZxImage::ZxImage(QWidget *p_parent) :
    QWidget(p_parent),
    m_pimpl(new Impl(this) )
{}



ZxImage::~ZxImage() = default;


void ZxImage::paintEvent(QPaintEvent* event)
{
    return m_pimpl->paintEvent(event);
}

void ZxImage::SetDirectory(const fs::path &p_dir)
{
    return m_pimpl->SetDirectory(p_dir);
}

const spectrum::Screen &ZxImage::LoadNext()
{
    return m_pimpl->LoadNext();
}

