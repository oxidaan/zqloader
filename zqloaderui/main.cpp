//==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            main.cpp
// DESCRIPTION:     Implementation of main() / qt dialog version.
// 
// Copyright (c) 2024 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#include "dialog.h"
#include <QApplication>
#include <QMessageBox>


/// I want to have std::exceptions to work.
class Application :
    public QApplication
{
public:
    using QApplication::QApplication;

    bool notify( QObject * receiver, QEvent * ev ) override
    {
        try
        {
            return QApplication::notify( receiver, ev );
        }
        catch(const std::exception &ex)
        {
            QMessageBox::warning( nullptr, "Error", ex.what() );
        }
        return false;
    }
};

int main(int argc, char *argv[])
{
    Application a(argc, argv);
    Dialog w;
    w.show();
    return a.exec();
}
