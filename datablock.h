// ==============================================================================
// PROJECT:         zqloader
// FILE:            datablock.h
// DESCRIPTION:     Definition of struct DataBlock
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once


#include <vector>
#include <filesystem>


/// Used for all data block storages.
/// This is just a std::vector<std::byte> but I dont want copying made too easy.
/// So copying can only be done through Clone.
/// Moving is OK though.
/// Also added a LoadFromFile.
struct DataBlock : public std::vector<std::byte>
{
    using Base = std::vector<std::byte>;

private:

    // Copy CTOR only accessable by CLone
    DataBlock(const DataBlock& p_other) = default;

public:

    using Base::Base;

    ///  Move CTOR
    DataBlock(DataBlock&& p_other) = default;

    ///  Move assign
    DataBlock& operator =(DataBlock&& p_other) = default;

    ///  No CTOR copy from std::vector
    DataBlock(const Base& p_other) = delete;

    ///  Move CTOR from std::vector
    DataBlock(Base&& p_other) :
        Base(std::move(p_other))
    {}

    ///  Make Clone (deep) copy
    DataBlock Clone() const
    {
        return *this;
    }


};

///  Load (binary) from given file
DataBlock LoadFromFile(const std::filesystem::path &p_filename);

/// Save (binary) to given file
void SaveToFile(const DataBlock &p_to_save, const std::filesystem::path &p_filename);