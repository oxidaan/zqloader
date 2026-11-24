// ==============================================================================
// PROJECT:         zqloader
// FILE:            loaderparameters.h
// DESCRIPTION:     Stores all loader parameters, can save/load them
//
// Copyright (c) 2026 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================


#pragma once

#include "types.h"           // CompressionType
#include <chrono>
#include "loader_defaults.h"

struct LoaderParameters
{
    int                         zero_max = loader_defaults::zero_max;                // aka ZERO_MAX sees a 'one' when waited more than this number of cycli at wait for edge
    int                         bit_loop_max = loader_defaults::bit_loop_max;        // aka ONE_MAX, the wait for edge loop counter until timeout

    int                         zero_duration = loader_defaults::zero_duration;
    int                         one_duration = loader_defaults::one_duration;
    int                         end_of_byte_delay = loader_defaults::end_of_byte_delay;

    int                         io_init_value = loader_defaults::io_init_value;    
    int                         io_xor_value = loader_defaults::io_xor_value;      

    uint32_t                    sample_rate = loader_defaults::sample_rate;         // device default
    bool                        catch_up = loader_defaults::catch_up;

    CompressionType             compression_type = loader_defaults::compression_type;
    int                         decompression_speed = loader_defaults::decompression_speed; // kb/second time spectrum needsto decompress before sending next block
    std::chrono::milliseconds   initial_wait = loader_defaults::initial_wait;        // pause after loading ZQLoader itself, give basic some time.

    int                         spectrum_clock = spectrum::spectrum_clock;

    uint16_t                    when_done_call_usr = 0;      // 0 is automatic
    bool                        when_done_return_to_basic = false;

    uint16_t                    loader_copy_start = 0;         // start of free space were our loader can be copied to, begins with stack, then Control code copied from basic
    bool                        skip_pilots = false;

public:
    std::string GetDebugString() const
    {
        std::stringstream ss;
        ss << "Duration of Zero/One: " << zero_duration << '/' << one_duration <<
            "; Max zero: " << zero_max <<
            "; End of byte: " << end_of_byte_delay <<
            "; Decompr. speed (kb/s): " << decompression_speed <<
            "; Init. wait: " << initial_wait.count() <<
            "; Samplerate: " << sample_rate <<
            "; Catch up: " << catch_up;
        return ss.str();
    }

    // Write/serialize.
    std::string  ToString()  const
    {
        std::stringstream ss;
    #define WRITE_CMD(what) \
                ss << #what << ' ' << what << '\n'
        WRITE_CMD(zero_max);
        WRITE_CMD(bit_loop_max);
        WRITE_CMD(zero_duration);
        WRITE_CMD(one_duration);
        WRITE_CMD(end_of_byte_delay);
        WRITE_CMD(io_init_value);
        WRITE_CMD(io_xor_value);
        WRITE_CMD(sample_rate);
        WRITE_CMD(catch_up);
        ss << "compression_type " << int(compression_type) << '\n';
        WRITE_CMD(decompression_speed);
        ss << "initial_wait " << initial_wait.count() << '\n';
        WRITE_CMD(spectrum_clock);
        WRITE_CMD(when_done_call_usr);
        WRITE_CMD(when_done_return_to_basic);
        WRITE_CMD(loader_copy_start);
        WRITE_CMD(skip_pilots);
    #undef WRITE_CMD
        return ss.str();
    }


    // Read/serialize.
    // Only changes found.actually read parameters
    void FromString(const std::string& p_string)
    {
    #define READ_CMD(what)  \
        if(command == #what)  \
        {   \
            ss2 >> what;  \
        }
        std::stringstream ss1(p_string);
        std::string line;
        while (std::getline(ss1, line))
        {
            std::stringstream ss2(line);
            std::string command;
            // read command
            ss2 >> command;
            READ_CMD(zero_max);
            READ_CMD(bit_loop_max);
            READ_CMD(zero_duration);
            READ_CMD(one_duration);
            READ_CMD(end_of_byte_delay);
            READ_CMD(io_init_value);
            READ_CMD(io_xor_value);
            READ_CMD(sample_rate);
            READ_CMD(catch_up);
            if (command == "compression_type")
            {
                int compr_type;
                ss2 >> compr_type;
                compression_type = CompressionType(compr_type);
            }

            READ_CMD(decompression_speed);
            if (command == "initial_wait")
            {
                int init_wait;
                ss2 >> init_wait;
                initial_wait = std::chrono::milliseconds(init_wait);
            }
            READ_CMD(spectrum_clock);
            READ_CMD(when_done_call_usr);
            READ_CMD(when_done_return_to_basic);
            READ_CMD(loader_copy_start);
            READ_CMD(skip_pilots);
                
        }
    #undef READ_CMD
    }
};

