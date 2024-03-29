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
    auto command_line = GetCmdLine(1);
    std::stringstream ss(command_line);

    while (ss && !ss.eof())
    {
        std::string s;
        ss >> iomanip::readuntil(s, " \n\r\t\0=:"s);
        ss >> iomanip::skip(" \n\r\t\0=:"s);
        if (s == p_command ||
            s == ("--" + std::string(p_command)) )
        {
            std::string value;
            ss >> iomanip::quoted(value, '\0');     // do not handle escapes
            return value;
        }
        if (p_command.length() == 1 && s.length() > 1 && s[0] == '-' && s[1] != '-')
        {
            // Commandline given single - followed by letters eg -abcd = flags: mere presence of letter indicates 'true'
            if (s.find(p_command) != std::string::npos)
            {
                return "1";
            }
        }
    }
    return std::nullopt;
}


std::string CommandLine::GetCmdLine(int p_first) const noexcept
{
    std::string s;
    for (int n = p_first; n < m_args.size(); n++)
    {
        if (!s.empty())
        {
            s += " ";
        }
        s += m_args[n];
    }
    return s;
}
