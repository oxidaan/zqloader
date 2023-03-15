#include "tools.h"
#include <string>
#include <exception>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif






/// Get value of given named commandine parameter. 
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
