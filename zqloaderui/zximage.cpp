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
#include "tools.h"

namespace fs = std::filesystem;





// color categories
using enum spectrum::screen::PaletteColor;
constexpr int spectrum_dark_colors[] = { black , blue, red };
constexpr int spectrum_light_colors[] = { green, cyan, yellow, white, br_green, br_cyan, br_yellow, br_white};
constexpr int spectrum_gray_colors[] = { black, white, br_black, br_white };


using Attributes = std::vector<spectrum::screen::Attr>;


struct AlgorithmParameters
{
    bool m_use_distance_to_black_white = true;
    bool m_use_floyd_steinberg = true;
    std::span<const int> m_dark_colors = spectrum_dark_colors;
    std::span<const int> m_light_colors = spectrum_light_colors;
};

class ZxImage::Impl
{

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
            auto w = 2 * spectrum::screen::SCREEN_WIDTH;
            auto h = 2 * spectrum::screen::SCREEN_HEIGHT;
//            painter.drawImage(0, 0, m_image.scaled(w, h, Qt::KeepAspectRatioByExpanding));              // draw the original image
            painter.drawImage(0, 0, CenterScale(m_image, w, h));              // draw the original image
            painter.drawImage(w, 0, SpectrumScreenToImage(m_screen).scaled(w, h));
        }
    }




    void SetDirectory(const fs::path &p_dir)
    {
        std::unique_lock lock(m_mutex);
        m_index = 0;
        m_filenames.clear();
        for (fs::path filename : fs::directory_iterator{p_dir})
        {
            if(ToLower(filename.extension().string()) == ".png" || ToLower(filename.extension().string()) == ".jpg")
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
    // Return spectrum screen that can be sent as data to Spectrum
    const spectrum::screen::Screen &LoadNext()
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
        AlgorithmParameters how;
        auto screen =  ImageToSpectrumScreen(image, how);

        {
            std::unique_lock lock(m_mutex);
            m_image = std::move(image);
            m_screen = std::move(screen);
        }
        m_this->update();       // -> paintEvent
        return m_screen;
    }





    /// Convert given QImage to spectrum screen
    /// Can run in miniaudio thread
    static spectrum::screen::Screen ImageToSpectrumScreen(const QImage &p_image, AlgorithmParameters p_how)
    {
        spectrum::screen::Screen spectrum_screen;
        // 1) Scale down to spectrum resolution (256x192)
        QImage image256x192 = CenterScale(p_image, spectrum::screen::SCREEN_WIDTH, spectrum::screen::SCREEN_HEIGHT);

        // 2) Determine color attribute using the scaled image
        for (int attry = 0; attry < 24; attry++)
        {
            for (int attrx = 0; attrx < 32; attrx++)
            {
                spectrum::screen::Attr attr = DetermineAttributeForCell(image256x192, attrx, attry, p_how);
                //attr.attr.ink = spectrum::Screen::AttributeColor::black;
                //attr.attr.paper = spectrum::Screen::AttributeColor::white;
                spectrum_screen.SetAttribute(attrx, attry, attr);
            }
        }


        QImage image256x192gray = image256x192.convertToFormat(QImage::Format_Grayscale8);    // to gray
        //image256x192gray = image256x192gray.convertToFormat(QImage::Format_RGB32);    // needed?
        auto formt = image256x192.format();
        if (formt != QImage::Format_RGB32)
        {
            image256x192 = image256x192.convertToFormat(QImage::Format_RGB32);
        }



        QImage &image = p_how.m_use_distance_to_black_white ? image256x192gray : image256x192;
        QRgb color_paper;
        QRgb color_ink;

        // Writing pixels. Optional with Floyd–Steinberg dithering
        for (int y = 0; y < spectrum::screen::SCREEN_HEIGHT; y++)
        {
            for (int x = 0; x < spectrum::screen::SCREEN_WIDTH; x++)
            {
                if(p_how.m_use_distance_to_black_white)
                {
                    // using distance to black/white
                    color_ink = 0x000000;
                    color_paper = 0xffffff;
                }
                else
                {
                    // using distance to (earlier determined) attribute colors
                    auto attr = spectrum_screen.GetAttribute(x/8, y/8);
                    color_ink = spectrum::screen::AttrInkToRgbColor(attr);
                    color_paper = spectrum::screen::AttrPaperToRgbColor(attr);
                }

                QRgb oldcolor = image.pixel(x, y);
                auto dist_to_ink   = ColorDistanceRgb(oldcolor, color_ink);
                auto dist_to_paper = ColorDistanceRgb(oldcolor, color_paper);
                bool is_ink = dist_to_ink < dist_to_paper;
                // not needed image.setPixel(x, y, newcolor);
                if(p_how.m_use_floyd_steinberg)
                {
                    QRgb newcolor = is_ink ? color_ink : color_paper;
                    SetFloydSteinbergColor4(image, x, y, oldcolor, newcolor);
                }
                spectrum_screen.SetPixel(x, y, is_ink);         // write to spectrum screen itself
            }
        }

        return spectrum_screen;
    }

    /// Convert given spectrum screen to a QImage.
    /// To show only - not sent to Spectrum with zqloader
    static QImage SpectrumScreenToImage(const spectrum::screen::Screen &p_image)
    {
        QImage image(spectrum::screen::SCREEN_WIDTH, spectrum::screen::SCREEN_HEIGHT, QImage::Format_RGB32);
        for(int y = 0; y < spectrum::screen::SCREEN_HEIGHT ; y++)
        {
            for(int x = 0; x < spectrum::screen::SCREEN_WIDTH ; x++)
            {
                bool bit = p_image.GetPixel(x, y);
                auto attr = p_image.GetAttribute(x / 8, y / 8);
                auto color = bit ? spectrum::screen::AttrInkToRgbColor(attr) : spectrum::screen::AttrPaperToRgbColor(attr);
                image.setPixel(x, y, color);
            }
        }
        return image;
    }

private:





    // p_attr_x,  p_attr_y are attribute (32x24) coordinates thus corresponding to top left pixel
    // (as QRgb) of an 8x8 cell at a QImage.
    // For each 8x8 (=64) pixels of that cell determine:
    // nearest spectrum color considered dark and nearest spectrum color considered light
    // and count them both. Lower distances to nearest color count heavier.
    // Then find the indexes for the most used spectrum color considered dark and light,
    // return those as spectrum attibute: dark for ink and light for paper.
    // The used the light color also determines bright flag.
    // (At spectrum bright flag has barely effect on dark colors).
    static spectrum::screen::Attr DetermineAttributeForCell(const QImage &p_image, int p_attr_x, int p_attr_y, AlgorithmParameters p_how)
    {
        int count_dark[16]{};
        int count_light[16]{};
        for(int y = 0; y < 8; y++)
        {
            for(int x = 0; x < 8; x++)
            {
                QRgb rgb = p_image.pixel(p_attr_x * 8 + x, p_attr_y * 8 + y);
                if(IsAlmostGray(rgb))
                {
                    auto [dist_dark,  index_dark]  = GetNearestColor(rgb, spectrum::screen::palette, {black});
                    auto [dist_light, index_light] = GetNearestColor(rgb, spectrum::screen::palette, {white, br_white});
                    count_dark [index_dark]       += (( 256 * 256 * 3 ) - dist_dark );
                    count_light[index_light]      += (( 256 * 256 * 3 ) - dist_light );
                }
                else
                {
                    auto [dist_dark,  index_dark]  = GetNearestColor(rgb, spectrum::screen::palette, p_how.m_dark_colors);
                    auto [dist_light, index_light] = GetNearestColor(rgb, spectrum::screen::palette, p_how.m_light_colors);
                    count_dark [index_dark]       += (( 256 * 256 * 3 ) - dist_dark );
                    count_light[index_light]      += (( 256 * 256 * 3 ) - dist_light );
                }
            }
        }

        auto FindMax = [](const int p_values[])
        {
            int idx_max    = 0;
            int idx_2ndmax = -1;

            for (int i = 1; i < 16; i++)
            {
                if (p_values[i] >= p_values[idx_max])
                {
                    idx_2ndmax = idx_max;
                    idx_max    = i;
                }
                else if (idx_2ndmax == -1 || p_values[i] > p_values[idx_2ndmax])
                {
                    idx_2ndmax = i;
                }
            }
            return std::pair<int, int>{idx_max, idx_2ndmax};
        };

        auto dark_color = FindMax(count_dark).first;     // black, blue, red so 0, 1 or 2
        auto light_color = FindMax(count_light).first;    // so magenta(3) -- bright white (15)
        spectrum::screen::Attr attr{};
        attr.attr.ink    = spectrum::screen::AttributeColor(dark_color % 8);
        attr.attr.paper  = spectrum::screen::AttributeColor(light_color % 8);
        attr.attr.bright = light_color > 8;
        return attr;
    }




    // scale centered, keep aspect ratio, crop sides.
    static QImage CenterScale(const QImage &p_image, int w, int h)
    {
        QImage scaled = p_image.scaled(
            w, h,
            Qt::KeepAspectRatioByExpanding,
            Qt::SmoothTransformation
        );

        int x = (scaled.width()  - w) / 2;
        int y = (scaled.height() - h) / 2;

        return scaled.copy(x, y, w, h);
    }

    // Set floyd-steinberg color
    static void SetFloydSteinbergColor(QImage &p_image, int x, int y, int p_red_error, int p_green_error, int p_blue_error, int p_factor)
    {
        QRgb sample = p_image.pixel(x, y);
        auto r     = qRed(sample)   + (p_red_error   * p_factor) / 16;
        auto g     = qGreen(sample) + (p_green_error * p_factor) / 16;
        auto b     = qBlue(sample)  + (p_blue_error  * p_factor) / 16;
        p_image.setPixel(x, y, qRgb(r, g, b));
    }

    static void SetFloydSteinbergColor4(QImage &p_image, int x, int y, unsigned int oldcolor, unsigned int newcolor)
    {
        int error_r = qRed(oldcolor) - qRed(newcolor);
        int error_g = qGreen(oldcolor) - qGreen(newcolor);
        int error_b = qBlue(oldcolor) - qBlue(newcolor);
        if (x < (p_image.width() - 1))
        {
            SetFloydSteinbergColor(p_image, x + 1, y,     error_r, error_g, error_b, 7);     // right
        }
        if (x > 0 && y < (p_image.height() - 1))
        {
            SetFloydSteinbergColor(p_image, x - 1, y + 1, error_r, error_g, error_b, 3); // left below
        }
        if (y < (p_image.height() - 1))
        {
            SetFloydSteinbergColor(p_image, x,     y + 1, error_r, error_g, error_b, 5);     // below
        }
        if (x < (p_image.width() - 1) && y <  (p_image.height() - 1))
        {
            SetFloydSteinbergColor(p_image, x + 1, y + 1, error_r, error_g, error_b, 1);     // below right
        }
    }

private:
    std::vector<fs::path> m_filenames;
    int m_index = 0;
    ZxImage * m_this;
    QImage    m_image;      // original image as loaded from file
    spectrum::screen::Screen m_screen;
    mutable std::mutex m_mutex;
}; // class ZxImage::Impl



ZxImage::ZxImage(QWidget *p_parent) :
    QWidget(p_parent),
    m_pimpl(new Impl(this) )
{}



ZxImage::~ZxImage() = default;


void ZxImage::paintEvent(QPaintEvent* event)
{
    m_pimpl->paintEvent(event);
}

void ZxImage::SetDirectory(const fs::path &p_dir)
{
    m_pimpl->SetDirectory(p_dir);
}

const spectrum::screen::Screen &ZxImage::LoadNext()
{
    return m_pimpl->LoadNext();
}

