// ==============================================================================
// PROJECT:         zqloader
// FILE:            tzxwriter.h
// DESCRIPTION:     Definition of class TzxWriter.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once

#include <vector>
#include "pulsers.h"


class TzxWriter
{
public:
    using PulserPtr = std::unique_ptr<Pulser>;
    using Pulsers   = std::vector<PulserPtr>;

public:
    ///  Writer
    void WriteTzxFile(const Pulsers &p_pulsers, std::ostream& p_stream );
private:
    template <class T>
    void TryWriteAsTzxBlock(const Pulser *p, std::ostream& p_stream)
    {
        if(dynamic_cast<const T *>(p))
        {
            WriteAsTzxBlock(*dynamic_cast<const T *>(p), p_stream);
        }
    }

    void WriteAsTzxBlock(const TonePulser &p_pluser, std::ostream &p_stream) const;
    void WriteAsTzxBlock(const PausePulser &p_pluser, std::ostream &p_stream) const;
    void WriteAsTzxBlock(const DataPulser &p_pluser, std::ostream &p_stream) const;
    static bool PulserIsLeader(const Pulser &p_pulser);
    static bool PulserIsSync(const Pulser &p_pulser);
};