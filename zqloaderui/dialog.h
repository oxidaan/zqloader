//==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            dialog.h
// DESCRIPTION:     Definition file for user inteface Dialog (QT/QDialog)
// 
// Copyright (c) 2024 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#pragma once

#include <QDialog>
#include <zqloader.h>


QT_BEGIN_NAMESPACE
namespace Ui
{
class Dialog;
}
QT_END_NAMESPACE

class QSettings;
class Dial;

class Dialog : public QDialog
{
    Q_OBJECT

public:
    enum class State
    {
        Idle,
        Playing,
        Tuning,
        Preloading,
        PreloadingFunAttribs,
        Cancelled
    };
    Dialog(QWidget *parent = nullptr);
    ~Dialog();
private:
    void Go();
    void Load();
    void Save() const;
    bool Read(QSettings& settings, QObject *p_parent);
    void Write(QSettings& settings, const QObject *p_parent) const;
    void UpdateUI();

    void SetState(State p_state);
    void RestoreDefaults();
    void CheckLoaderParameters() const;

signals:
    void signalDone();
private:
    Ui::Dialog *ui;
    ZQLoader m_zqloader;
    State m_state;
};


