#pragma once



#include <memory>
#include <string>

#include <QObject>
class QImage;

class Video : public QObject
{
Q_OBJECT
public:
    Video();

    ~Video();

    bool Play(const std::string &p_url);

    Video &Stop();


    bool IsPlaying() const;

    QImage GetImage() const;
signals:
    void FrameReceived();
private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};


