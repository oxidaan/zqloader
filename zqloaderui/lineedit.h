//==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            lineedit.h
// DESCRIPTION:     A (Q)LineEdit that signals signalFocusIn and signalFocusOut.
// 
// Copyright (c) 2024 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#pragma once

#include <QLineEdit>


/// A (Q)LineEdit that signals signalFocusIn and signalFocusOut.
class LineEdit : public QLineEdit
{
    Q_OBJECT
    using base = QLineEdit;
public:
    using QLineEdit::QLineEdit;
    void focusInEvent(QFocusEvent *e) override
    {
        emit signalFocusIn();
        base::focusInEvent(e);
    }
    void focusOutEvent(QFocusEvent *e) override
    {
        emit signalFocusOut();
        base::focusOutEvent(e);
    }
signals:
    void signalFocusIn();
    void signalFocusOut();
};
