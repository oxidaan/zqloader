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
#include <iostream>

namespace fs = std::filesystem;



using enum spectrum::Screen::PaletteColor;
constexpr int spectrum_dark_colors[]  = { black, blue, red };
constexpr int spectrum_light_colors[] = { green, cyan, yellow, white, br_green, br_cyan, br_yellow, br_white};


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
        QRgb paper_color = spectrum::Screen::AttrPaperToColor(p_spectrum_attr); // paper=light
        QRgb ink_color   = spectrum::Screen::AttrInkToColor(p_spectrum_attr);   // ink=dark
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
        QImage image256x192gray = image256x192.convertToFormat(QImage::Format_Grayscale8);    // to gray
        auto formt = image256x192.format();
        if (formt != QImage::Format_RGB32)
        {
            image256x192 = image256x192.convertToFormat(QImage::Format_RGB32);
        }


        // 2) Floyd–Steinberg dithering on the gray scale image
        for(int y = 0; y < spectrum::SCREEN_HEIGHT; y++)
        {
            for(int x = 0; x < spectrum::SCREEN_WIDTH; x++)
            {
                QRgb oldcolor         = image256x192gray.pixel(x, y);
                QRgb black(0x000000);
                QRgb white(0xffffff);
                auto dist_to_black    = ColorDistance(oldcolor, black);
                auto dist_to_white    = ColorDistance(oldcolor, white);
                bool is_ink = dist_to_black < dist_to_white;
                QRgb newcolor = is_ink ? black: white;
                image256x192gray.setPixel(x, y, is_ink ? black: white);
                spectrum_screen.SetPixel(x, y, is_ink);

                int error   = qRed(oldcolor) - qRed(newcolor);      // gray scale r and g and b are same
                if(x < 255)
                    SetFloydSteinbergColor(image256x192gray, x + 1, y,     error, error, error, 7);
                if( x > 0 && y < 191)
                    SetFloydSteinbergColor(image256x192gray, x - 1, y + 1, error, error, error, 3);
                if(y < 191)
                    SetFloydSteinbergColor(image256x192gray, x,     y + 1, error, error, error, 5);
                if( x < 255 &&  y < 191)
                    SetFloydSteinbergColor(image256x192gray, x + 1, y + 1, error, error, error, 1);

            }
        }
        // 3) Determine color attribute on the original
        for(int attry = 0; attry < 24; attry++)
        {
            for(int attrx = 0; attrx < 32; attrx++)
            {
                spectrum_screen.SetAttribute(attrx, attry, GetAttributeForCell(image256x192, attrx, attry));
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
                auto color = bit ? spectrum::Screen::AttrInkToColor(attr) : spectrum::Screen::AttrPaperToColor(attr);
                image.setPixel(x, y, color);
            }
        }
        return image;
     }

    // p_attr_x,  p_attr_y are attribute (32x24) coordinates thus corresponding to top left pixel
    // (as QRgb) of an 8x8 cell at a QImage.
    // For each 8x8 (=64) pixels of that cell determine:
    // nearest spectrum color considered dark and nearest spectrum color considered light
    // and count them both. Lower distances to nearest color count heavier.
    // Then find the indexes for the most used spectrum color considerded dark and light,
    // return those as spectrum attibute: dark for ink and light for paper.
    // The used the light color also deteines bright flag.
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
                count_light[index_light]      ++;//= (( 256 * 256 * 3 ) - dist_light );
                count_dark [index_dark]       ++;//= (( 256 * 256 * 3 ) - dist_dark );
            }
        }
        auto FindMax = []( int p_found[])
                       {
                           int max   = 0;
                           int idx_found = 0;
                           for(int i = 0; i < 16; i++)
                           {
                               if(p_found[i] >= max)
                               {
                                   idx_found = i;
                                   max   = p_found[i];
                               }
                           }
                           return idx_found;
                       };
        spectrum::Screen::Attr attr;
        auto dark_color  = FindMax(count_dark);      // black, blue, red so 0, 1 or 2
        auto light_color = FindMax(count_light);     // so magenta(3) -- bright white (15)
        attr.attr.ink    = spectrum::Screen::AttributeColor(dark_color);
        attr.attr.paper  = spectrum::Screen::AttributeColor(light_color % 8);
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

