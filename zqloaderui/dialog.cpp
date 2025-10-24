//==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            dialog.cpp
// DESCRIPTION:     Implementation file for user inteface Dialog (QT/QDialog)
// 
// Copyright (c) 2024 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#include "dialog.h"
#include <streambuf>
#include <iostream>
#include <sstream>                  
#include <QFileDialog>
#include <QSettings>
#include <QTextEdit>
#include <QDesktopServices>
#include <QTimer>
#include "./ui_dialog.h"
#include <datablock.h>
#include <mutex>

#include "spectrum_consts.h"
#include "loader_defaults.h"        // consts with default settings for ZQLoader
#include "widgetstreambuffer.h"

#include <filesystem>
namespace fs = std::filesystem;


///  Yes!
void WriteFunText(ZQLoader &p_zq_loader, bool p_first)
{
    static int col = 32;
    constexpr auto text = "+++++ ZQ LOADER BY OXIDAAN +++++ .  .  .  .  . SELECT TURBO FILE NAME TO LOAD, THEN PRESS GO!  .  .  .  .";
    bool empty = false;
    if(p_first)     // first
    {
        // wipe screen
        DataBlock all_attr;
        all_attr.resize(768);
        p_zq_loader.SetCompressionType(CompressionType::automatic);
        p_zq_loader.AddDataBlock(std::move(all_attr), 16384 + 6 * 1024 );
    }
    else
    {
        DataBlock text_attr;
        text_attr.resize(256);
        empty = ZQLoader::WriteTextToAttr(text_attr, text, std::byte{}, false, col);    // 0_byte: random colors
        // copy fun attributes into 48k data block
        p_zq_loader.SetCompressionType(CompressionType::none);      // else ugly
        p_zq_loader.AddDataBlock(std::move(text_attr), 16384 + 6 * 1024 + 512);
    }
    col --;
    if(empty)
    {
        col = 31;       // repeat
    }
}

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::Dialog)
{
    m_zqloader.SetExeFilename(QCoreApplication::applicationDirPath().toStdString());


    ui->setupUi(this);
    QIcon icon(":/icon/zqloader.ico"); 
    setWindowIcon(icon);

    // redirect cout to textEditOutput
    ui->textEditOutput->moveCursor(QTextCursor::End);
    auto buffer = new TextEditStreamBuffer(ui->textEditOutput);
    std::streambuf* oldCoutBuffer = std::cout.rdbuf(buffer);
    (void)oldCoutBuffer;




    // connect/sync volume dials and edits for 2 volumes
    auto ConnectVolume = [this](Dial *p_dial, QLineEdit *p_edit)
    {
        connect(p_dial, &QDial::sliderMoved, [=]
        {
            p_edit->setText(QString::number(p_dial->value()));
        });
        connect(p_edit, &QLineEdit::textEdited, [=]
        {
            bool ok;
            auto v = p_edit->text().toInt(&ok);
            if(ok && v >= -100 && v <= 100)
            {
                p_dial->setValue(v);
            }
            if(ok)
            {
                p_edit->setText(QString::number(p_dial->value())); // read back, also limits value
            }
        });
        connect(p_edit, &QLineEdit::editingFinished, [=]
        {
            p_edit->setText(QString::number(p_dial->value())); // read back in case of error, also limits value
        });
        // Volume value directly to zqloader when dial changes
        connect(p_dial, &QDial::valueChanged, [=]
        {
            m_zqloader.SetVolume(ui->lineEditVolumeLeft->text().toInt(), ui->lineEditVolumeRight->text().toInt());
        });
 
    };
    ConnectVolume(ui->dialVolumeLeft,  ui->lineEditVolumeLeft);
    ConnectVolume(ui->dialVolumeRight, ui->lineEditVolumeRight);

    // common connect code for three browse buttons
    auto ConnectBrowse  = [this](QPushButton *p_button, QLineEdit *p_edit, QString p_text, QString p_filter, QFileDialog::FileMode p_mode)
    {
        connect(p_button, &QPushButton::pressed, [=]
        {
            QSettings settings("Oxidaan", "ZQLoader");
            QString dir = settings.value("dialog_directory", "").toString();    // remember last used dir.
            QFileDialog dialog(this);
            dialog.setDirectory(dir);
            dialog.setNameFilter(p_filter);
            dialog.setWindowTitle(p_text);
            dialog.setFileMode(p_mode);
            // Else hangs when (??)
            // preload->cancel preload, open this dialog -> cancel ->  preload->cancel preload -> open this dialog:
            // https://stackoverflow.com/questions/31983412/code-freezes-on-trying-to-open-qdialog
            // 9 years old issue still not solved???
            dialog.setOption(QFileDialog::DontUseNativeDialog, true);
            dialog.setAcceptMode(p_mode == QFileDialog::ExistingFiles ? QFileDialog::AcceptOpen :  QFileDialog::AcceptSave);
            if(dialog.exec() == QDialog::Accepted)
            {
                p_edit->setText(dialog.selectedFiles().first());
                QFileInfo fileInfo(dialog.selectedFiles().first()); 
                settings.setValue("dialog_directory", fileInfo.absolutePath());
            }
        });
    };

    ConnectBrowse(ui->pushButtonBrowseNormalFile, ui->lineEditNormalFile,
        "Give normal speed file to load",
        "Tap/tzx files (*.tap *.tzx);;All files (*.*)",
        QFileDialog::ExistingFiles);
    ConnectBrowse(ui->pushButtonBrowseTurboFile, ui->lineEditTurboFile,
        "Give turbo speed file to load",
        "All Spectrum files (*.tap *.tzx *.z80 *.sna);;Tap/tzx files (*.tap *.tzx);;SnapShot files (*.z80 *.sna)';;All files (*.*)",
        QFileDialog::ExistingFiles);
    ConnectBrowse(ui->pushButtonBrowseOutputFile, ui->lineEditOutputFile,
        "Give output file to create",
        "Wav files (*.wav);;Tzx files (*.tzx);;All files (*.*)",
        QFileDialog::AnyFile);


    // make hyperlinks work in about text
    connect(ui->labelAbout, &QLabel::linkActivated, [](const QString & link)
    {
        QDesktopServices::openUrl(link);
    });

    // make close button work would otherwise reject
    connect(ui->buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, [this]
    {
        Save();
        close();
    });
    // make cancel button work (dont save)
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, [this]
    {
        close();
    });
    connect(ui->buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, [this]
    {
        RestoreDefaults();
    });
    connect(ui->pushButtonCalculate, &QPushButton::pressed, [this]
    {
        auto wanted_zero_cyclii = ui->lineEditWantedZeroCyclii->text().toDouble();
        auto zero_max  = ui->lineEditZeroMax->text().toInt();
        auto wanted_one_cyclii = ui->lineEditWantedOneCyclii->text().toDouble();
        CalculateLoaderParameters(wanted_zero_cyclii, zero_max, wanted_one_cyclii);
    });
    connect(ui->pushButtonGo,&QPushButton::pressed, [this]
    {
        if( m_state == State::Playing || m_state == State::Tuning)
        {
            // Stop pressed (canceled) (pushButtonGo is now cancel)
            m_zqloader.Reset();
            SetState(m_state == State::Tuning ? State::Idle : State::Cancelled);
        }
        else
        {
            // go pressed
            Go();
        }
    });

    connect(ui->pushButtonPreLoad,&QPushButton::pressed, [this]
    {
        if(m_state == State::Preloading )     
        {
            // stop /cancel preload
            m_zqloader.Reset();
            SetState(State::Cancelled);
        }
        else if(m_state == State::PreloadingFunAttribs )     
        {
            // this will stop fun attribs, but keep preloaded
            //m_zqloader.Stop();      
            SetState(State::Idle);
        }
        else
        {
            // start preload
            m_zqloader.Reset();
            fs::path filename1 = ui->lineEditNormalFile->text().toStdString();
            m_zqloader.SetNormalFilename(filename1).SetPreload();       // should be zqloader or empty

            m_zqloader.SetSampleRate(ui->lineEditSampleRate->text().toInt());
            m_zqloader.SetVolume(ui->lineEditVolumeLeft->text().toInt(), ui->lineEditVolumeRight->text().toInt());

            std::cout << "Estimated duration: " << m_zqloader.GetEstimatedDuration().count() << "ms" << std::endl;
            m_zqloader.Start();
            SetState(State::Preloading);
        }
    });

    connect(ui->pushButtonTune,&QPushButton::pressed, [this]
    {
        if(m_zqloader.IsBusy())     
        {
            // stop tune
            m_zqloader.Reset();
            SetState(State::Idle);
        }
        else
        {
            // start tune
            m_zqloader.Reset();
            m_zqloader.SetSampleRate(ui->lineEditSampleRate->text().toInt());
            m_zqloader.SetVolume(ui->lineEditVolumeLeft->text().toInt(), ui->lineEditVolumeRight->text().toInt());
            m_zqloader.SetSpectrumClock(ui->lineEditClock->text().toInt());
            m_zqloader.PlayleaderTone();
            SetState(State::Tuning);
        }
    });

    connect(ui->horizontalSliderSpeed, &QSlider::valueChanged, [=](int value)
    {
        CalculateLoaderParametersFromSlider(value);
    });

    connect(ui->pushButtonClean,&QPushButton::pressed, [this]
    {
        ui->textEditOutput->clear();
        ui->pushButtonClean->setEnabled(false);
    });
    connect(ui->pushButtonTest,&QPushButton::pressed, [this]
    {
        m_zqloader.Test();
    });

    Load(); // settings

    m_zqloader.SetOnDone([this]
    {
        // runs in miniadio thread
        if(m_state == State::Preloading)
        {
            std::cout << "Preloading done! Select a turbo file and press Go!..." << std::endl;
            m_state = State::PreloadingFunAttribs;
            WriteFunText(m_zqloader, true);
        }
        else if( m_state == State::PreloadingFunAttribs)
        {
            // next fun attributes (scroll text)
            WriteFunText(m_zqloader, false);
            std::cout << '*' << std::flush;
        }
        else if(m_state != State::Idle)     // eg playing
        {
            emit signalDone();      // swap to ui thread ->
        }
    });
    connect(this, &Dialog::signalDone, this, [this] // this extra this is needed to go back to ui thread
    {
        if(!m_zqloader.IsBusy())        // double check
        {
            SetState(State::Idle);
            if(m_zqloader.GetTimeNeeded() != 0ms)
            {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(1) << std::chrono::duration<double>(m_zqloader.GetTimeNeeded()).count();
                ui->progressBar->setFormat("Done, took " + QString::fromStdString(ss.str()) + "s");
                ui->progressBar->setValue(100);
            }
            m_zqloader.WaitUntilDone(); // else tape loading error
            m_zqloader.Reset();         // do not keep preloaded state
        }
    });


    QTimer *timer = new QTimer(this);
    timer->setInterval(100ms);
    connect(timer, &QTimer::timeout, [this]
    {
        UpdateUI();
    });

    timer->start();

    // commandline file parameter given, start immidiately
    if(QCoreApplication::arguments().size() > 1)
    {
        std::cout << "Commandline parameter given: loading: " << QCoreApplication::arguments()[1].toStdString() << std::endl;
        ui->lineEditTurboFile->setText(QCoreApplication::arguments()[1]);
        // maybe not nice to have loading sounds unprepared: 
        // Go();
    }

    

}

Dialog::~Dialog()
{
    delete ui;
}


// Update ui (timer)
// most prominentlty the progressBar
inline void Dialog::UpdateUI()
{
    if(m_zqloader.IsBusy() && m_zqloader.GetEstimatedDuration() != 0ms)
    {
        auto perc = (100 * m_zqloader.GetCurrentTime())/m_zqloader.GetEstimatedDuration() ;
        ui->progressBar->setValue(perc);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << std::chrono::duration<double>(m_zqloader.GetCurrentTime()).count();
        ui->progressBar->setFormat(QString::fromStdString(ss.str()) + "s / %p%");
    }
}

// Update (button) enabled state and progressBar
// ui only.
inline void Dialog::SetState(State p_state)
{
    m_state = p_state;
    switch(p_state)
    {
    case State::Playing:
        ui->pushButtonGo->setText("Stop");
        ui->pushButtonPreLoad->setEnabled(false);
        ui->pushButtonTune->setEnabled(false);
        break;
    case State::Tuning:
        ui->pushButtonPreLoad->setEnabled(false);
        ui->pushButtonGo->setText("Stop");
        ui->pushButtonTune->setText("Stop");
        break;
    case State::Preloading:
    case State::PreloadingFunAttribs:
        ui->pushButtonGo->setText("Go!");
        ui->pushButtonPreLoad->setText("Stop");
        ui->pushButtonTune->setEnabled(false);
        break;
    case State::Cancelled:
        ui->progressBar->setFormat("Cancelled");
        // no break
    case State::Idle:
        ui->pushButtonPreLoad->setEnabled(true);    //!m_zqloader.IsPreLoaded());
        ui->pushButtonPreLoad->setText("PreLoad");
        ui->pushButtonGo->setText("Go!");
        ui->pushButtonTune->setText("Tune...");
        ui->pushButtonTune->setEnabled(true);
        break;
    }
}


inline void Dialog::RestoreDefaults()
{
    m_zqloader.Stop();
    SetState(State::Idle);
    ui->lineEditWantedZeroCyclii->setText(QString::number(loader_defaults::wanted_zero_cyclii));
    ui->lineEditZeroMax->setText(QString::number(loader_defaults::zero_max));
    ui->lineEditWantedOneCyclii->setText(QString::number(loader_defaults::wanted_one_cyclii));
    ui->lineEditBitLoopMax->setText(QString::number(loader_defaults::bit_loop_max));

    ui->lineEditZeroTStates->setText(QString::number(loader_defaults::zero_duration));
    ui->lineEditOneTStates->setText(QString::number(loader_defaults::one_duration));
    ui->lineEditEndOfByteDelay->setText(QString::number(loader_defaults::end_of_byte_delay));
        

    ui->comboBoxCompressionType->setCurrentIndex(int(loader_defaults::compression_type));
    ui->comboBoxLoaderLocation->setCurrentIndex(1);     // automatic.
    ui->lineEditLoaderAddress->setText(QString::number(spectrum::SCREEN_23RD));

    ui->lineEditSampleRate->setText(QString::number(m_zqloader.GetDeviceSampleRate()));
    ui->lineEditVolumeLeft->setText(QString::number(loader_defaults::volume_left));
    ui->dialVolumeLeft->setValue(loader_defaults::volume_left);
    ui->lineEditVolumeRight->setText(QString::number(loader_defaults::volume_right));
    ui->dialVolumeRight->setValue(loader_defaults::volume_right);

    ui->lineEditClock->setText(QString::number(spectrum::g_spectrum_clock));

    try
    {
        ui->lineEditNormalFile->setText(QString::fromStdString(m_zqloader.GetZqLoaderFile().string()));
    }
    catch(...)
    {
        // not found. Ignore for now. This is only RestoreDefaults.
    }
}



// 'Go' pressed.
inline void Dialog::Go()
{
    CheckLoaderParameters();        // might throw
    std::cout << "\n" << std::endl;
    if(m_state == State::PreloadingFunAttribs)
    {
        // update ui
        SetState(State::Idle);
    }

    // might break ongoing pre-load: 
    // m_zqloader.Reset();

    fs::path filename1 = ui->lineEditNormalFile->text().toStdString();
    fs::path filename2 = ui->lineEditTurboFile->text().toStdString();


    fs::path outputfilename = ui->lineEditOutputFile->text().toStdString();

    // When outputfile="path/to/filename" or -w or -wav given: 
    // Convert to wav file instead of outputting sound
    m_zqloader.SetOutputFilename(outputfilename, true);     // allow overwrite for now

    m_zqloader.SetBitLoopMax     (ui->lineEditBitLoopMax->text().toInt()).
               SetZeroMax        (ui->lineEditZeroMax->text().toInt()).
               SetDurations      (ui->lineEditZeroTStates->text().toInt(),
                                  ui->lineEditOneTStates->text().toInt(),
                                  ui->lineEditEndOfByteDelay->text().toInt());
    m_zqloader.SetSpectrumClock(ui->lineEditClock->text().toInt());
    m_zqloader.SetCompressionType(CompressionType(ui->comboBoxCompressionType->currentIndex()));                                    

    if(ui->comboBoxLoaderLocation->currentIndex() == 0)
    {
        m_zqloader.SetSnapshotLoaderLocation(ZQLoader::LoaderLocation::screen);
    }
    else if(ui->comboBoxLoaderLocation->currentIndex() == 1)
    {
        m_zqloader.SetSnapshotLoaderLocation(ZQLoader::LoaderLocation::automatic);
    }
    else
    {
        uint16_t adr = ui->lineEditLoaderAddress->text().toInt();
        m_zqloader.SetSnapshotLoaderLocation(adr);
    }
    m_zqloader.SetFunAttribs(ui->checkBoxFunAttribs->isChecked());

    m_zqloader.SetNormalFilename(filename1);
    m_zqloader.SetTurboFilename(filename2);

    m_zqloader.SetSampleRate(ui->lineEditSampleRate->text().toInt());
    m_zqloader.SetVolume(ui->lineEditVolumeLeft->text().toInt(), ui->lineEditVolumeRight->text().toInt());

    std::cout << "Estimated duration: " << m_zqloader.GetEstimatedDuration().count() << "ms" << std::endl;
    m_zqloader.Start();
    if(m_zqloader.IsBusy())     // else immidiately done eg writing wav file
    {
        SetState(State::Playing);
    }
    ui->pushButtonClean->setEnabled(true);
}

// Save all line edits + dialog position
inline void Dialog::Save() const
{
    QSettings settings("Oxidaan", "ZQLoader");
    settings.beginGroup(objectName());
    Write(settings, this);
    settings.endGroup();
}

// Save all line edits + dialog position to given settings
inline void Dialog::Write(QSettings& settings, const QObject *p_for_what) const
{
    {
        // save dialogs (that is only main dialog)
        auto for_what = dynamic_cast<const QDialog*>(p_for_what);
        if (for_what)
        {
            settings.setValue("pos", pos());
            settings.setValue("size", size());
        }
    }
    {
        // save QLineEdits
        auto for_what = dynamic_cast<const QLineEdit*>(p_for_what);
        if (for_what)
        {
            settings.setValue(for_what->objectName(), for_what->text());
        }
    }
    {
        // save QComboBox
        auto for_what = dynamic_cast<const QComboBox*>(p_for_what);
        if (for_what)
        {
            settings.setValue(for_what->objectName(), for_what->currentIndex());
        }
    }
    {
        // save slider
        auto for_what = dynamic_cast<const QSlider*>(p_for_what);
        if (for_what)
        {
            settings.setValue(for_what->objectName(), for_what->value());
        }
    }

    for(const QObject* child : p_for_what->children())
    {
        Write(settings, child);        // recusive
    }
}

// Load all line edits + dialog position
inline void Dialog::Load()
{
    QSettings settings("Oxidaan", "ZQLoader");
    settings.beginGroup(objectName());
    if(!Read(settings, this))
    {
        // Read failed (first time)
        RestoreDefaults();
    }
    settings.endGroup();

    // sync dials
    ui->dialVolumeLeft->setValue(ui->lineEditVolumeLeft->text().toInt());
    ui->dialVolumeRight->setValue(ui->lineEditVolumeRight->text().toInt());
}

// Read all line edits + dialog position from given settings
inline bool Dialog::Read(QSettings& settings, QObject *p_for_what)
{
    {
        // load dialogs (that is only main dialog)
        auto for_what = dynamic_cast<QDialog*>(p_for_what);
        if (for_what)
        {
            QVariant value = settings.value("pos");
            if (!value.isNull())
            {
                for_what->move(settings.value("pos").toPoint());
                for_what->resize(settings.value("size").toSize());
            }
            else
            {
                return false;
            }
        }
    }
    {
        // load QLineEdits
        auto for_what = dynamic_cast<QLineEdit*>(p_for_what);
        if (for_what)
        {
            for_what->setText(settings.value(for_what->objectName()).toString());
        }
    }
    {
        // load QComboBox
        auto for_what = dynamic_cast<QComboBox*>(p_for_what);
        if (for_what)
        {
            for_what->setCurrentIndex(settings.value(for_what->objectName()).toInt());
        }
    }
    {
        // load slider
        auto for_what = dynamic_cast<QSlider*>(p_for_what);
        if (for_what)
        {
            for_what->setValue(settings.value(for_what->objectName()).toInt());
        }
    }
    for(QObject* child: p_for_what->children())
    {
        Read(settings, child);        // recusive
    }
    return true;
}


// Convert Tstate value to # polling cycles at ZQloader z80 code.
// (currently not (yet) used anymore)
inline double TStateToCycle(int p_tstate)
{
   double retval = double(p_tstate -  loader_tstates::bit_loop_duration) / loader_tstates::wait_for_edge_loop_duration;
   return retval >= 0 ? retval : 0;
}
// Convert # polling cycles to Tstate value that will take that ZQloader z80 code.
inline int CycleToTstate(double p_cyclii)
{
    return int(loader_tstates::bit_loop_duration + p_cyclii * loader_tstates::wait_for_edge_loop_duration);
}

inline void Dialog::CalculateLoaderParametersFromSlider(int p_index )
{
    static const double interval[] = {1.5, 2.0, 3.0};
    int mod = sizeof(interval) / sizeof(interval[0]);
    double wanted_zero_cyclii= 0.0;
    int zero_max= 0;
    double wanted_one_cyclii = 0.0;
    for(int n = 0 ; n < p_index + 4; n++)
    {
        double v2 = interval[n % mod ];
        double v1 =( n / mod ) % (mod - 1) ? 2.0: 1.0;

        wanted_zero_cyclii = 1.0 + n/6;
        zero_max = wanted_zero_cyclii + v1;
        wanted_one_cyclii = double(zero_max) + v2;
    }
    CalculateLoaderParameters(wanted_zero_cyclii, zero_max, wanted_one_cyclii);
}

// Based on 'Wanted Zero Cycli'and 'Wanted One Cyclii' calculate
// 'Zero TStates' and 'One TStates' and put result in the dialog.
// Check validity first, throws when error.
inline void Dialog::CalculateLoaderParameters(double p_wanted_zero_cyclii, int p_zero_max, double p_wanted_one_cyclii )
{
    if(p_wanted_zero_cyclii < 1)
    {
        // Cannot be zero (one IN needed at least)
        p_wanted_zero_cyclii = 1;
    }
    if(p_zero_max <= p_wanted_zero_cyclii)
    {
        p_zero_max = p_wanted_zero_cyclii + 1;
        //throw std::runtime_error("'zero_max' must be bigger than 'Wanted zero cyclii' + 1 so bigger than: " + std::to_string(int(wanted_zero_cyclii) + 1));
    }
    if(p_wanted_one_cyclii <= p_zero_max)
    {
        p_wanted_one_cyclii = p_zero_max + 1;
//        throw std::runtime_error("'Wanted one Cycli' must be bigger than 'zero_max' (" + std::to_string(zero_max ) + ")");
    }
    int zero_tstates  = CycleToTstate(p_wanted_zero_cyclii - 1);
    int one_tstates   = CycleToTstate(p_wanted_one_cyclii - 1);
    ui->lineEditWantedZeroCyclii->setText(QString::number(p_wanted_zero_cyclii));
    ui->lineEditZeroMax->setText(QString::number(p_zero_max));
    ui->lineEditWantedOneCyclii->setText(QString::number(p_wanted_one_cyclii));
    ui->lineEditZeroTStates->setText(QString::number(zero_tstates));
    ui->lineEditOneTStates->setText(QString::number(one_tstates));

}


// Check if zero_max/one_tstates/zero_tstates loader parameters, as given in dialog, are valid.
// Throws when not valid.
inline void Dialog::CheckLoaderParameters() const
{
    auto zero_tstates = ui->lineEditZeroTStates->text().toInt();
    auto one_tstates = ui->lineEditOneTStates->text().toInt();
    auto zero_max  = ui->lineEditZeroMax->text().toInt();

    auto zero_maxtstates = CycleToTstate(zero_max);
    if(zero_max < 1)
    {
        throw std::runtime_error("'zero_max' can not be 0; must be bigger than - or equal to 1 (this is the maximum number of IN's without an edge to treat data as zero. At least one IN is always done.)");
    }
    if(one_tstates < zero_maxtstates )
    {
        throw std::runtime_error("'one_tstates' to small minimum is " + std::to_string(zero_maxtstates));
    }
    if(zero_tstates > zero_maxtstates )
    {
        throw std::runtime_error("'zero_tstates' to big maximum is " + std::to_string(zero_maxtstates));
    }

    if(one_tstates < (zero_tstates + loader_tstates::wait_for_edge_loop_duration))
    {
        throw std::runtime_error("'one_tstates' to small, minimum is " + std::to_string(zero_tstates + loader_tstates::wait_for_edge_loop_duration));
    }


}

