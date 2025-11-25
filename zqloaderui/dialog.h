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
class LineEdit;

class Dialog : public QDialog
{
    Q_OBJECT

public:
    enum class State
    {
        Idle,                       // doing nothing
        Playing,                    // busy (turbo) loading
        Tuning,                     // playing endless header tone
        Preloading,                 // preloading zqloader
        PreloadingFunAttribs,       // after preloading zqloader loading fun-attributes
        Cancelled                   // Playing was cancelled
    };
    Dialog(QWidget *parent = nullptr);
    ~Dialog() override;
private:
    void Go();
    void Load();
    void Save() const;
    bool Read(QSettings& p_settings, QObject *p_for_what);
    void Write(QSettings& p_settings, const QObject *p_for_what) const;
    void UpdateUI();

    void SetState(State p_state);
    void RestoreDefaults();
    void Check() const;
    void CalculateLoaderParametersFromSlider(int p_index);
    void CalculateLoaderParameters(double p_wanted_zero_cyclii, int p_zero_max, double p_wanted_one_cyclii, bool p_special_case = false );
    void closeEvent(QCloseEvent *event) override;
signals:
    void signalDone();
private:
    Ui::Dialog *ui;
    ZQLoader m_zqloader;
    State m_state;
};


