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
#include <memory>           // std::unique_ptr


QT_BEGIN_NAMESPACE
namespace Ui
{
class Dialog;
}
QT_END_NAMESPACE


class Dialog : public QDialog
{
    Q_OBJECT

public:

    Dialog(QWidget *parent = nullptr);
    ~Dialog() override;
    void closeEvent(QCloseEvent* event) override;
signals:
    void signalDone();
private:
    Ui::Dialog *ui;
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};


