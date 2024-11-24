//==============================================================================
// PROJECT:         zqloader
// FILE:            tools.cpp
// DESCRIPTION:     Random stuff, including random
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#include "tools.h"
#include <string>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif





/// Get value of given named commandine parameter. 
/// Return nullopt when not found.
std::optional<std::string> CommandLine::TryGetParameter(std::string_view p_command) const noexcept
{
    using namespace std::string_literals;

    for (int n = 1; n < m_args.size(); n++)
    {
        std::string arg = m_args[n];
        if ((arg == p_command || arg == ("--" + std::string(p_command))) && n < (m_args.size() - 1) )
        {
            std::string value =  m_args[n+1];
            if((value == "=" || value == ":") &&  n < (m_args.size() - 2))
            {
                value =  m_args[n+2];
            }
            return value;
        }
        else if (p_command.length() == 1 && arg.length() > 1 && arg[0] == '-' && arg[1] != '-')
        {
            // Commandline given single - followed by letters eg -abcd = flags: mere presence of letter indicates 'true'
            if (arg.find(p_command) != std::string::npos)
            {
                return "1";
            }
        }
        else
        {
            // parse it a bit eg cmd=value
            std::stringstream ss(arg);
            std::string s;
            ss >> iomanip::skip(" \n\r\t\0=:\""s);
            ss >> iomanip::readuntil(s, " \n\r\t\0=:\""s);
            if (s == p_command ||
                s == ("--" + std::string(p_command)) )
            {
                std::string value;
                ss >> iomanip::skip(" \n\r\t\0=:"s) >> iomanip::readuntil(value, "");    // read rest
                return value;
            }
        }
    }
    return std::nullopt;
}
