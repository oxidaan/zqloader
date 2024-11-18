//==============================================================================
// PROJECT:         zqloader
// FILE:            enumstreamer.cpp
// DESCRIPTION:     Definition of some functions to stream enum tags
//                  Mainly debugging.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================





#include "spectrum_types.h"
#include "turboblocks.h"
#include "types.h"

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

std::ostream& operator << (std::ostream& p_stream, spectrum::TapeBlockType p_enum)
{
    switch (p_enum)
    {
        ENUM_TAG(spectrum::TapeBlockType, header);
        ENUM_TAG(spectrum::TapeBlockType, data);
        ENUM_TAG(spectrum::TapeBlockType, raw);
        ENUM_TAG(spectrum::TapeBlockType, unknown);
        ENUM_TAG(spectrum::TapeBlockType, error);
    }
    return p_stream;
}

std::ostream& operator << (std::ostream& p_stream, spectrum::TapeHeader::Type p_enum)
{
    switch (p_enum)
    {
        ENUM_TAG(spectrum::TapeHeader::Type, basic_program);
        ENUM_TAG(spectrum::TapeHeader::Type, array_numbers);
        ENUM_TAG(spectrum::TapeHeader::Type, array_text);
        ENUM_TAG(spectrum::TapeHeader::Type, code);
        ENUM_TAG(spectrum::TapeHeader::Type, screen);
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

