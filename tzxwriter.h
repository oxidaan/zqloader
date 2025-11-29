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
    TzxWriter() = default;
    ///  Writer
    void WriteTzxFile(const Pulsers &p_pulsers, std::ostream& p_stream , Doublesec p_tstate_dur);
    struct TzxBlockWriter : public PulserVisitor
    {
        TzxBlockWriter(std::ostream &p_stream, Doublesec p_tstate_dur) :
            m_stream(p_stream),
            m_tstate_dur(p_tstate_dur)
        {}
        void Visit(const TonePulser &p_pulser)  const  override
        {
            WriteAsTzxBlock(p_pulser, m_stream);
        }
        void Visit(const PausePulser &p_pulser)  const override
        {
            WriteAsTzxBlock(p_pulser, m_stream);
        }
        void Visit(const DataPulser &p_pulser)  const override
        {
            WriteAsTzxBlock(p_pulser, m_stream);
        }
    private:
        void WriteAsTzxBlock(const TonePulser &p_pluser, std::ostream &p_stream) const;
        void WriteAsTzxBlock(const PausePulser &p_pluser, std::ostream &p_stream) const;
        void WriteAsTzxBlock(const DataPulser &p_pluser, std::ostream &p_stream) const;
        std::ostream &m_stream;
        Doublesec m_tstate_dur;
    };
private:

    void WriteInfo(std::ostream& p_stream);

    static bool IsPulserLeader(const Pulser &p_pulser);
    static bool IsPulserSync(const Pulser &p_pulser);
    static bool IsPulserSpectrumData(const Pulser &p_pulser);
    static bool IsPulserTurboData(const Pulser &p_pulser);
    static bool IsZqLoaderTurboData(const Pulser &p_pulser);
    void WriteAsStandardSpectrum (std::ostream& p_stream, const TonePulser &p_leader, const TonePulser &p_sync, const DataPulser &p_data);
    void WriteAsTurboData        (std::ostream &p_stream, const TonePulser &p_leader, const TonePulser &p_sync, const DataPulser &p_data);
    void WriteAsZqLoaderTurboData(std::ostream &p_stream, const TonePulser *p_leader, const TonePulser &p_sync, const DataPulser &p_data);
private:
};
