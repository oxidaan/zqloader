#pragma once



#include <memory>
#include <vector>
#include <QWidget>
class QImage;
class QPaintEvent;

// ZX Spectrum color attribute
// https://www.overtakenbyevents.com/lets-talk-about-the-zx-specrum-screen-layout/#:~:text=Each%20block%20of%208x8%20pixels%20has%20a%20single,if%20set%20indicates%20the%20colours%20are%20rendered%20bright.

union ColorAttr
{
    struct
    {
        uint8_t ink: 3;
        uint8_t paper: 3;
        uint8_t bright: 1;
        uint8_t flash : 1;
    } attr;
    std::byte byte;
};
static_assert(sizeof(ColorAttr) == 1);

//using ColorAttr = std::byte;
using Attributes = std::vector<ColorAttr> ;

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


