//==============================================================================
// PROJECT:         zqloader
// FILE:            tools.h
// DESCRIPTION:     Random stuff, including Random
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#pragma once
#include <string>
#include <iomanip>
#include <algorithm>        // transform
#include <sstream>
#include <optional>
#include <vector>
#include <random>


template <class TString>
std::string ToLower(TString p_string)
{
    std::string s = std::move(p_string);
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}

template <class TString>
std::string ToUpper(TString p_string)
{
    std::string s = std::move(p_string);
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return char(std::toupper(c)); });
    return s;
}


class CommandLine
{
public:
    ///  Initialize with argc and argv taken from main.
    CommandLine(int argc, char** argv) noexcept
    {
        std::string s;
        for (int n = 0; n < argc; n++)
        {
            m_args.push_back(std::string(argv[n]));
        }
    }

    /// Has it commandline parameters? Convenience.
    bool HasParameters() const noexcept
    {
        return m_args.size() > 1;
    }

    /// Get # parameters
    unsigned GetNumParameters() const noexcept
    {
        return unsigned(m_args.size() - 1);
    }

    /// Get last parameter
    std::string GetLastParameter() const noexcept
    {
        if (HasParameters())
        {
            return m_args.back();
        }
        return "";
    }

    /// Get nth parameter
    std::string GetParameter(int p_num) const noexcept
    {
        return m_args[p_num];
    }
    
    /// Get value of given named commandine parameter overload for const char *.
    /// Return given default when not found.
    std::string GetParameter(std::string_view p_param, const char* p_default) noexcept
    {
        auto opt = TryGetParameter<std::string>(p_param);
        if (opt.has_value())
        {
            return opt.value();
        }
        return p_default;
    }

    /// Get value of given named commandine parameter as TData.
    /// Return given default when not found.
    template <class TData>
    TData GetParameter(std::string_view p_param, TData p_default) noexcept
    {
        auto opt = TryGetParameter<TData>(p_param);
        if (opt.has_value())
        {
            return opt.value();
        }
        return p_default;
    }


    /// Get value of given named commandine parameter. 
    /// Return nullopt when not found.
    std::optional<std::string> TryGetParameter(std::string_view p_command) const noexcept;

    template <class TData>
    std::optional<TData> TryGetParameter(std::string_view p_command) const noexcept
    {
        auto opt = TryGetParameter(p_command);
        if (opt.has_value())
        {
            std::stringstream ss(opt.value());
            TData d;
            ss >> d;
            return d;
        }
        return std::nullopt;
    }

    ///  Convenience
    bool HasParameter(std::string_view p_param) const noexcept
    {
        return TryGetParameter(p_param).has_value();
    }

private:
    std::string GetCmdLine(int p_first = 0) const noexcept;

private:
    std::vector<std::string> m_args;
};


namespace iomanip
{

class skip
{
public:

    skip(std::string_view p_to_skip) :
        m_to_skip(p_to_skip)
    {}



    void Skip(std::istream& p_stream) const
    {
        if (p_stream.good())
        {
            do
            {
                char c = char(p_stream.peek());
                if (m_to_skip.find(c) == std::string::npos)
                {
                    break;
                }
                p_stream.ignore(1);
            } while (p_stream.good());
            p_stream.clear(p_stream.rdstate() & ~std::ios::failbit);
        }
    }

private:

    std::string_view   m_to_skip;
};


/// Skips stream reading as long as any of given characters encountered.
/// eg:
/// istream >> iomanip::skip("\n\r,; ");
inline std::istream& operator >>(std::istream& p_stream, const iomanip::skip& p_ignore)
{
    p_ignore.Skip(p_stream);
    return p_stream;
}


class readuntil
{
public:

    readuntil(std::string& p_string, std::string_view p_read_until) :
        m_string(p_string),
        m_read_until(p_read_until)
    {}



    void Read(std::istream& p_stream) const
    {
        if (p_stream.good())
        {
            m_string.clear();
            do
            {
                char c = char(p_stream.peek());
                if (m_read_until.find(c) != std::string::npos)
                {
                    break;
                }
                p_stream.ignore(1);
                if (!p_stream.fail())
                {
                    m_string += c;
                }
            } while (p_stream.good());
            p_stream.clear(p_stream.rdstate() & ~std::ios::failbit);
        }
    }

private:

    std::string&       m_string;
    std::string_view   m_read_until;
};



/// Reads into a target string intil one of given characters encountered.
/// eg:
/// istream >> iomanip::readuntil(target_string, "\n\r,; ");
inline std::istream& operator >>(std::istream& p_stream, const iomanip::readuntil& p_read_until)
{
    p_read_until.Read(p_stream);
    return p_stream;
}




class quoted
{
public:

    quoted(std::string& p_str, char p_escape = '\\') :
        m_read(p_str), m_escape(p_escape)
    {}


    void Read(std::istream& p_stream) const
    {
        p_stream >> skip(" \n\r\t");
        char c = char(p_stream.peek());
        if (c == '"' || c == '\'')
        {
            p_stream >> std::quoted(m_read, c, m_escape);
        }
        else
        {
            using namespace std::string_literals;
            p_stream >> readuntil(m_read, " \n\r\t\0"s);
        }
    }

private:
    std::string& m_read;
    char         m_escape;
};


/// As std::quoted but delim is autmatically determined eg "'" or '"', can be nothing as well.
/// eg:
/// istream >> iomanip::skip("\n\r,; ");
inline std::istream& operator >>(std::istream& p_stream, const quoted& p_quoted)
{
    p_quoted.Read(p_stream);
    return p_stream;
}

}       // iomanip



template<class TEngine = std::minstd_rand>
auto& GetSeededRandomEngine()
{
    static std::random_device seed;
    static TEngine gen(seed());
    return gen;
}



// Eg Dice: Random(1,6)
template<class TInt1, class TInt2,
    typename std::enable_if<std::is_integral<TInt1>::value, int>::type = 0,
    typename std::enable_if<std::is_integral<TInt2>::value, int>::type = 0>
TInt1 Random(TInt1 p_min, TInt2 p_max)
{
    std::uniform_int_distribution distrib(p_min, TInt1(p_max));
    return distrib(GetSeededRandomEngine());
}

