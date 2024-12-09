//==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            dial.h
// DESCRIPTION:     A (Q)Dial that stops at max/min - does not wrap around.
// 
// Copyright (c) 2024 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#pragma once

#include <QMouseEvent>
#include <QDial>

///
/// A Dial that stops at max/min - does not wrap around
///
class Dial : public QDial
{
    Q_OBJECT
public:
    using QDial::QDial;
    //virtual ~Dial() {};

protected:
    void sliderChange(QAbstractSlider::SliderChange change) override
    {
        if(m_prev_value >= maximum() && value() < (maximum() + minimum()) / 2)
        {
            setValue(m_prev_value);
            return;
        }

        if(m_prev_value <= minimum() && value() > (maximum() + minimum()) / 2)
        {
            setValue(m_prev_value);
            return;
        }
        m_prev_value = value();
        QDial::sliderChange(change);
    }

private:
    int m_prev_value = 0;

};



