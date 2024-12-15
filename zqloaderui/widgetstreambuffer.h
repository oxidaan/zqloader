//==============================================================================
// PROJECT:         zqloader (ui)
// FILE:            widgetstreambuffer.h
// DESCRIPTION:     A std::streambuffer that streams to a QTextEdit.
//                  Thread safe, also handles switching to qt UI thread.
// 
// Copyright (c) 2024 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================



#pragma once
#include <QObject>
#include <streambuf>
#include <mutex>
#include <QTextEdit>

/// A std::streambuf that streams to QTextEdit widget. 
/// Idea came from ... Copilot...
/// Then made thread safe, also handles switching to qt UI thread.
class TextEditStreamBuffer : public QObject,
                             public std::streambuf
{
    Q_OBJECT
public:
    TextEditStreamBuffer(QTextEdit* p_output) : m_output(p_output) 
    {
        connect(this, &TextEditStreamBuffer::signalBufferFull, this, [this] 
        {
            // This runs in QT's ui thread
            QString buffer;
            {
                std::lock_guard	lock(m_mutex);
                std::swap(buffer, m_buffer);        // get and empty buffer
            }
            m_output->insertPlainText(buffer);
            m_output->moveCursor(QTextCursor::End);
            
        });
    }

protected:
    virtual int overflow(int c) override
    {
        if (c != EOF)
        {
            std::lock_guard	lock(m_mutex);
            m_buffer += (char)c;
        }
        if(char(c) == '\n' || c == EOF)
        {
            emit signalBufferFull();     // Swap to QT's ui thread
        }
        return c;
    }
signals:
    void signalBufferFull();
private:
    std::mutex m_mutex;
    QTextEdit* m_output;
    QString m_buffer;
};