//==============================================================================
// PROJECT:         zqloader
// FILE:            enumstreamer.cpp
// DESCRIPTION:     Definition of some functions to stream enum tags
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================




#include "samplesender.h"
#include "spectrum_types.h"
#include "tzxloader.h"
#include "compressor.h"
#include "turboblock.h"

#include <iostream>
/// Make conversion from enum tags to string somewhat simpler
#define ENUM_TAG(prefix, tag) case prefix::tag: p_stream << #tag;break;



std::ostream& operator << (std::ostream& p_stream, Edge p_enum)
{
    switch (p_enum)
    {
        ENUM_TAG(Edge, one);
        ENUM_TAG(Edge, zero);
        ENUM_TAG(Edge, no_change);
        ENUM_TAG(Edge, toggle);
    }
    return p_stream;
}

std::ostream& operator << (std::ostream& p_stream, ZxBlockType p_enum)
{
    switch (p_enum)
    {
        ENUM_TAG(ZxBlockType, header);
        ENUM_TAG(ZxBlockType, data);
        ENUM_TAG(ZxBlockType, raw);
        ENUM_TAG(ZxBlockType, unknown);
        ENUM_TAG(ZxBlockType, error);
    }
    return p_stream;
}

std::ostream& operator << (std::ostream& p_stream, ZxHeader::Type p_enum)
{
    switch (p_enum)
    {
        ENUM_TAG(ZxHeader::Type, basic_program);
        ENUM_TAG(ZxHeader::Type, array_numbers);
        ENUM_TAG(ZxHeader::Type, array_text);
        ENUM_TAG(ZxHeader::Type, code);
        ENUM_TAG(ZxHeader::Type, screen);
    }
    return p_stream;
}

std::ostream& operator << (std::ostream& p_stream, CompressionType p_enum)
{
    switch (p_enum)
    {
        ENUM_TAG(CompressionType, none);
        ENUM_TAG(CompressionType, rle);
        ENUM_TAG(CompressionType, automatic);
    }
    return p_stream;
}

