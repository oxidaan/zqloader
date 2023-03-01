// ==============================================================================
// PROJECT:         zqloader
// FILE:            loadbinary.h
// DESCRIPTION:     Definition of class Event.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once
#include <future>

///
/// Kind of windows style event.
///
class Event
{
public:

    Event()
    {}



    void Reset()
    {
        m_promise = std::promise<bool>();      // move assign so (re) init
        m_was_set = false;
        m_was_get = false;
    }



    void Signal()
    {
        if (!m_was_set)
        {
            m_was_set = true;
            m_promise.set_value(true);     // this is a promise
        }
    }



    void wait()
    {
        if (!m_was_get)
        {
            m_was_get = true;
            std::future<bool> fut = m_promise.get_future();
            fut.wait();                    // this waits for the promise m_done to be written to
        }
    }



    auto wait_for(std::chrono::milliseconds p_timeout)
    {
        if (!m_was_get)
        {
            m_was_get = true;
            std::future<bool> fut = m_promise.get_future();
            return fut.wait_for(p_timeout);                    // this waits for the promise m_done to be written to
        }
        return std::future_status::ready;
    }

private:

    bool                 m_was_set = false;
    bool                 m_was_get = false;
    std::promise<bool>   m_promise;

};
