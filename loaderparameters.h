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
    double                      wanted_zero_cyclii = loader_defaults::wanted_zero_cyclii;
    int                         zero_max = loader_defaults::zero_max;            // aka ZERO_MAX sees a 'one' when waited more than this number of cycli at wait for edge
    double                      wanted_one_cyclii = loader_defaults::wanted_one_cyclii;
    int                         bit_loop_max = loader_defaults::bit_loop_max;        // aka ONE_MAX, the wait for edge loop counter until timeout

    int                         zero_duration = loader_defaults::zero_duration;
    int                         one_duration = loader_defaults::one_duration;

    int                         end_of_byte_delay = loader_defaults::end_of_byte_delay;

    int                         io_init_value = loader_defaults::io_init_value;       // aka ONE_MAX, the wait for edge loop counter until timeout
    int                         io_xor_value = loader_defaults::io_xor_value;        // aka ZERO_MAX sees a 'one' when waited more than this number of cycli at wait for edge

    uint32_t                    sample_rate = loader_defaults::sample_rate;         // device default
    bool                        catch_up = loader_defaults::catch_up;;

    CompressionType             compression_type = loader_defaults::compression_type;
    int                         decompression_speed = loader_defaults::decompression_speed; // kb/second time spectrum needsto decompress before sending next block
    std::chrono::milliseconds   initial_wait = loader_defaults::initial_wait;        // pause after loading ZQLoader itself, give basic some time.

    int                         spectrum_clock = spectrum::spectrum_clock;

    uint16_t                    when_done_call_usr = 0;      // 0 is automatic
    bool                        when_done_return_to_basic = false;

    uint16_t                    loader_copy_start = 0;         // start of free space were our loader can be copied to, begins with stack, then Control code copied from basic
    bool                        skip_pilots = false;

public:
    std::string GetDebugString()
    {
        std::stringstream ss;
        ss << "Duration of Zero/One: " << zero_duration << '/' << one_duration <<
            "; Max zero: " << zero_max <<
            "; End of byte: " << end_of_byte_delay <<
            "; Decompr. speed (kb/s): " << decompression_speed <<
            "; Init. wait: " << initial_wait.count();
        return ss.str();
    }

    // Write/serialize.
    std::string  ToString() 
    {
        std::stringstream ss;
    #define WRITE_CMD(what) \
                ss << #what << ' ' <<what << '\n'
        WRITE_CMD(zero_duration);
        WRITE_CMD(one_duration);
        WRITE_CMD(zero_max);
        WRITE_CMD(end_of_byte_delay);
        WRITE_CMD(decompression_speed);
        WRITE_CMD(bit_loop_max);
        ss << "initial_wait " << initial_wait.count() << '\n';
    #undef WRITE_CMD
        return ss.str();
    }


        // Read/serialize.
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
            READ_CMD(zero_duration);
            READ_CMD(one_duration);
            READ_CMD(zero_max);
            READ_CMD(end_of_byte_delay);
            READ_CMD(decompression_speed);
            READ_CMD(bit_loop_max);
            if (command == "initial_wait")
            {
                int init_wait;
                ss2 >> init_wait;
                initial_wait = std::chrono::milliseconds(init_wait);
            }
        }
    #undef READ_CMD
    }
};

