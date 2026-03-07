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


/// More a namespace for tzx writer functions
class TzxWriter
{
public:
    using PulserPtr = std::unique_ptr<Pulser>;
    using Pulsers   = std::vector<PulserPtr>;

public:
    TzxWriter() = delete;
    ///  Writer
    static void WriteTzxFile(const Pulsers &p_pulsers, std::ostream& p_stream , Doublesec p_tstate_dur);

    static void WriteBegin(std::ostream& p_stream);
    static void WritePulsers(const Pulsers &p_pulsers, std::ostream& p_stream , Doublesec p_tstate_dur);

    static void WriteMetaData(std::ostream& p_stream , Doublesec p_tstate_dur);

private:
    static void WriteAsTzxBlock(const Pulser &p_pulser, std::ostream &p_stream, Doublesec p_tstate_dur) ;
    static void WriteAsTzxBlock(const TonePulser &p_pluser, std::ostream &p_stream, Doublesec p_tstate_dur) ;
    static void WriteAsTzxBlock(const PausePulser &p_pluser, std::ostream &p_stream, Doublesec p_tstate_dur) ;
    static void WriteAsTzxBlock(const DataPulser &p_pluser, std::ostream &p_stream, Doublesec p_tstate_dur) ;


    static void WriteInfo(std::ostream& p_stream, const std::string &p_text);

    static bool IsPulserLeader(const Pulser &p_pulser);
    static bool IsPulserSync(const Pulser &p_pulser);
    static bool IsPulserSpectrumData(const Pulser &p_pulser);
    static bool IsPulserTurboData(const Pulser &p_pulser);
    static bool IsZqLoaderTurboData(const Pulser &p_pulser);
    static void WriteAsStandardSpectrum (std::ostream& p_stream, const TonePulser &p_leader, const TonePulser &p_sync, const DataPulser &p_data);
    static void WriteAsTurboData        (std::ostream &p_stream, const TonePulser &p_leader, const TonePulser &p_sync, const DataPulser &p_data);
    static void WriteAsZqLoaderTurboData(std::ostream &p_stream, const TonePulser *p_leader, const TonePulser &p_sync, const DataPulser &p_data);
private:
};
