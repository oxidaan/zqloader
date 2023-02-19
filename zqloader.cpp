/*
MIT License

Copyright(c) 2023 Daan Scherft

Permission is hereby granted, free of charge, to any person obtaining a copy
of this softwareand associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright noticeand this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
// This project use the miniaudio sound library by David Read.


// g++ - O2 - std = c++17 - pthread zqloader.cpp - ldl - Iminiaudio


#if __cplusplus < 201703L
// At MSVC
// Properties -> C/C++ -> Language -> C++ Language Standard -> ISO c++17 Standard
// also set at C / C++ -> Command Line -> Additional options : /Zc:__cplusplus
// plus C / C++ -> Preprocessor -> Use standard comforming preprocessor
#error "Need c++17 or more"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6262)
#pragma warning(disable: 33005)
#pragma warning(disable: 6011)
#pragma warning(disable: 6385)
#pragma warning(disable: 26819)
#pragma warning(disable: 6386)
#pragma warning(disable: 5105)
#pragma warning(disable: 4100)
#endif

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#ifdef _MSC_VER
#pragma warning( pop )
#endif


#ifdef _MSC_VER
#pragma warning (disable: 4456) // declaration of '' hides previous local declaration
#pragma warning (disable: 4100) // unreferenced formal parameter
#pragma warning(disable: 26495) // Variable '' is uninitialized.Always initialize a member variable
#endif

#include <iostream>
#include <memory>
#include <chrono>
#include <vector>
#include <map>
#include <array>
#include <future>           // std::promise, std::future
#include <fstream>
#include <filesystem>       // std::filesystem;
#include <string>
#include <sstream>
#include <chrono>        
#include <cstddef>          // std::byte
#include <functional>       // std::bind
#include <optional>
#include <random>
#ifdef _WIN32
#include <conio.h>
#endif

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif




namespace fs = std::filesystem;
using namespace std::chrono_literals;




/// Used for all data block storages.
/// This is just a std::vector<std::byte> but I dont want copying made too easy.
/// So copying can only be done through Clone.
/// Moving is OK though.
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
    DataBlock& operator = (DataBlock&& p_other) = default;

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

DataBlock LoadFromFile(fs::path p_filename)
{
    std::ifstream fileread(p_filename, std::ios::binary | std::ios::ate);
    if (!fileread)
    {
        throw std::runtime_error("File " + p_filename.string() + " not found.");
    }
    int filelen = int(fileread.tellg());
    fileread.seekg(0, std::ios::beg);
    DataBlock block;
    block.resize(filelen);
    fileread.read(reinterpret_cast<char*>(block.data()), filelen);
    return block;
}



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

class Symbols
{
public:
    Symbols(fs::path p_filename)
    {
        ReadSymbols(p_filename);
    }
    void SetByte(DataBlock& p_block, const char* p_name, uint8_t val) const
    {
        p_block[GetSymbol(p_name)] = std::byte{ val };
    }
    void SetWord(DataBlock& p_block, const char* p_name, uint16_t val) const
    {
        auto sbm = GetSymbol(p_name);
        p_block[sbm] = std::byte(val & 0xff);           // z80 is little endian
        p_block[sbm + 1] = std::byte((val >> 8) & 0xff);
    }

    uint16_t GetSymbol(const std::string& p_name) const
    {
        if (m_symbols.find(p_name) != m_symbols.end())
        {
            return m_symbols.at(p_name);
        }
        throw std::runtime_error("Symbol " + p_name + " not found");
    }
    void ReadSymbols(fs::path p_filename)
    {
        std::ifstream fileread(p_filename);
        if (!fileread)
        {
            throw std::runtime_error("File " + p_filename.string() + " not found.");
        }
        std::string line;
        while (std::getline(fileread, line))
        {
            std::stringstream ss(line);
            std::string symb;
            ss >> symb;
            if (symb.back() == ':')
            {
                symb.pop_back();
            }
            ss.ignore(1);
            int value;
            std::string equ;
            ss >> equ >> std::hex >> value;
            m_symbols[symb] = uint16_t(value);
        }
    }
private:
    std::map<std::string, uint16_t> m_symbols;
};

/// A duration in sec that can also store fractional seconds
using Doublesec = std::chrono::duration<double>;



/// Define literal for std::byte
/// eg 123_byte
inline std::byte operator"" _byte(unsigned long long p_int)
{
    return std::byte(p_int);
}


/// Make std::byte 'usuable' for comression algorithms.
std::byte& operator ++(std::byte& p_byte)
{
    p_byte = std::byte(int(p_byte) + 1);
    return p_byte;
}
/// Make std::byte 'usuable' for comression algorithms.
std::byte& operator --(std::byte& p_byte)
{
    p_byte = std::byte(int(p_byte) - 1);
    return p_byte;
}

/// Make std::byte 'usuable' for comression algorithms.
/// Get maximum value for given TData including...
template <class TData>
TData GetMax()
{
    return std::numeric_limits<TData>::max();
}
/// Make std::byte 'usuable' for comression algorithms.
/// Get maximum value for std::byte
template <>
std::byte GetMax<std::byte>()
{
    return 255_byte;
}


/// Load data binary from given stream.
template <class TData>
TData LoadBinary(std::istream& p_stream)
{
    TData data;
    p_stream.read(reinterpret_cast<char*>(&data), sizeof(TData));
    return data;
}
/// Load a std::string binary from given stream.
template <class TData>
std::string LoadBinary(std::istream& p_stream, size_t p_len)
{
    std::vector<char>buf;
    buf.resize(p_len + 1);
    p_stream.read(buf.data(), p_len);
    buf[p_len] = 0;
    return buf.data();
}



static const int m_spectrum_clock = 3500000;
static const uint16_t SCREEN_23RD = 16384 + 4 * 1024;
static const uint16_t SCREEN_END = 16384 + 6 * 1024;
const Doublesec m_tstate_dur = 1s / double(m_spectrum_clock); // Tstate duration as std::duration in seconds

/// Make conversion from enum tags to string somewhat simpler
#define ENUM_TAG(prefix, tag) case prefix::tag: p_stream << #tag;break;


// Edge type
enum class Edge
{
    one,
    zero,
    no_change,
    toggle
};





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


// ZX BlockType (header or data)
enum class ZxBlockType : unsigned char
{
    header = 0x0,           // spectrum header
    data = 0xff,            // spectrum data
    raw = 0x1,              // eg header and checksum as read from tap file.
    unknown = 0x2,
    error = 0x3,            // eg eof
};

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


// ZX Spectrum header
#pragma pack(push, 1)
struct ZxHeader
{
    // ZxHeader::Type
    enum class Type : unsigned char
    {
        basic_program = 0,
        array_numbers = 1,
        array_text = 2,
        code = 3,
        screen = 4
    };

    uint16_t GetStartAddress() const
    {
        if (m_type == Type::code)
        {
            return m_start_address;
        }
        else if (m_type == Type::basic_program)
        {
            return 23755;       // Afaik typical start of basic (maybe should read PROG)
        }
        else
        {
            return 0;
        }
    }


    Type m_type;            // type see above
    char m_filename[10];    // seems obvious
    uint16_t m_length;      // total length of block to load (when basic this includes variables)
    union
    {
        uint16_t m_basic_program_start_line;    // auto start line when basic_program
        uint16_t m_start_address;               // start address when code. (Can also hold variable name)
    };
    uint16_t m_basic_program_length;        // basic program length after which start of variables follows
};
#pragma pack(pop)
static_assert(sizeof(ZxHeader) == 17, "Sizeof ZxHeader must be 17");


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



// Kind of windows style event.
class Event
{
public:
    Event()
    {
    }
    void Reset()
    {
        m_promise = std::promise<bool>();      // move assign so (re) init
        m_was_set = false;
        m_was_get = false;
    }

    void Signal()
    {
        if (!m_was_set)
        {
            m_was_set = true;
            m_promise.set_value(true);     // this is a promise
        }
    }
    void wait()
    {
        if (!m_was_get)
        {
            m_was_get = true;
            std::future<bool> fut = m_promise.get_future();
            fut.wait();                    // this waits for the promise m_done to be written to
        }
    }
    auto wait_for(std::chrono::milliseconds p_timeout)
    {
        if (!m_was_get)
        {
            m_was_get = true;
            std::future<bool> fut = m_promise.get_future();
            return fut.wait_for(p_timeout);                    // this waits for the promise m_done to be written to
        }
        return std::future_status::ready;
    }
private:
    bool m_was_set = false;
    bool m_was_get = false;
    std::promise<bool> m_promise;

};


#ifndef _WIN32

#include <termios.h>
int Key()
{
    struct termios prev, newatt;
    tcgetattr(STDIN_FILENO, &prev);
    newatt = prev;
    newatt.c_lflag &= ~ECHO;
    newatt.c_lflag &= ~ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &newatt);
    int ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &prev);
    return ch;
}
#else
int Key()
{
    return ::_getch();
}
#endif

//============================================================================
//============================================================================


/// Send (sound) samples using miniadio.
/// Uses call-backs set with:
/// SetOnGetDurationWait
/// SetOnNextSample
/// SetOnGetEdge
class SampleSender
{
public:
    using NextSampleFun = std::function<bool(void)>;
    using GetDurationFun = std::function<Doublesec(void)>;
    using GetEdgeFun = std::function<Edge(void)>;
public:
    SampleSender()
    {
    }
    SampleSender(bool)
    {
        Init();
    }
    ~SampleSender()
    {
        if (m_device)
        {
            Stop();
            ma_device_uninit(m_device.get());
        }
    }
    SampleSender(SampleSender&&) = default;
    SampleSender& operator = (SampleSender&&) = default;

    SampleSender& Init()
    {
        if (!m_device)
        {
            ma_device_config config = ma_device_config_init(ma_device_type_playback);
            config.playback.format = ma_format_f32;   // Set to ma_format_unknown to use the device's native format.
            config.playback.channels = 2;             // Set to 0 to use the device's native channel count.
            config.sampleRate = 0;                // @@ Set to 0 to use the device's native sample rate.
            config.dataCallback = data_callback;      // This function will be called when miniaudio needs more data.
            config.pUserData = this;   // Can be accessed from the device object (device.pUserData).

            auto device = std::make_unique<ma_device>();
            if (ma_device_init(nullptr, &config, device.get()) != MA_SUCCESS)
            {
                throw std::runtime_error("Failed to initialize the device.");
            }
            std::cout << "Sample rate = " << device->sampleRate << std::endl;
            m_device = std::move(device);
        }
        return *this;
    }

    /// Set callback to get wait duration.
    SampleSender& SetOnGetDurationWait(GetDurationFun p_fun)
    {
        m_OnGetDurationWait = std::move(p_fun);
        return *this;
    }

    /// Set callback to acquire sample
    SampleSender& SetOnNextSample(NextSampleFun p_fun)
    {
        m_OnNextSample = std::move(p_fun);
        return *this;
    }

    /// Set callback when edge seen.
    SampleSender& SetOnGetEdge(GetEdgeFun p_fun)
    {
        m_OnGetEdge = std::move(p_fun);
        return *this;
    }

    /// Start SampleSender / start miniaudio thread.
    SampleSender& Start()
    {
        Reset();
        m_is_running = true;
        ma_device_start(m_device.get());     // The device is sleeping by default so you'll need to start it manually.
        return *this;
    }

    /// Wait for SampleSender (that is: wait for miniaudio thread to stop)
    SampleSender& Wait()
    {
        if (m_is_running)
        {
            m_event.wait();
        }
        return *this;
    }

    /// Stop SampleSender (that is: stop miniaudio thread)
    SampleSender& Stop()
    {
        Wait();
        std::this_thread::sleep_for(500ms); // otherwise stops before all data was send
        ma_device_stop(m_device.get());
        m_is_running = false;
        return *this;
    }



    /// Run SampleSender (miniadio thread) so start, wait, stop.
    SampleSender& Run()
    {
        Reset();
        Start();
        Wait();
        Stop();
        return *this;
    }

    /// When negative basically inverts
    SampleSender& SetVolume(float p_volume)
    {
        if (p_volume > 1.0f || p_volume < 0.0f)
        {
            throw std::runtime_error("Volume must be between 0.0 and 1.0");
        }
        m_volume = p_volume;
        return *this;
    }

    /// Toggle left/right or both channels
    SampleSender& Toggle_channel(bool p_chan1, bool p_chan2)
    {
        m_toggle_channel[0] = p_chan1;
        m_toggle_channel[1] = p_chan2;
        return  *this;
    }

    /// Get (last) edge / mainly for debugging
    bool GetLastEdge() const
    {
        return m_edge;
    }

private:
    void Reset()
    {
        m_edge = false;
        //        m_edge = true;      // Does NOT work
        m_sample_time = 0ms;
    }

    /// A (static) data callback function for MiniAudio.
    /// Redirect to corresponding class member function immidiately.
    static void data_callback(ma_device* pDevice, void* pOutput, const void*, ma_uint32 frameCount)
    {
        SampleSender& samplesender = *static_cast<SampleSender*>(pDevice->pUserData);
        samplesender.DataCallback(pDevice, pOutput, frameCount);
    }





    // Get next sound samples, 
    // called in miniaudio device thread
    void DataCallback(ma_device* pDevice, void* pOutput, ma_uint32 frameCount)
    {
        auto sampleRate = pDevice->sampleRate;
        auto channels = pDevice->playback.channels;     // # channels eg 2 is stereo
        float* foutput = reinterpret_cast<float*>(pOutput);
        int index = 0;
        for (auto n = 0u; n < frameCount; n++)
        {
            float value = m_done ? 0.0f : GetNextSample(sampleRate);
            for (auto chan = 0u; chan < channels; chan++)
            {
                auto val = m_toggle_channel[chan % 1] ? -value : value;
                foutput[index] = val;
                index++;
            }
        }
        if (m_done)
        {
            m_event.Signal();
        }
    }

    // Get next single sound sample, 
    // called in miniaudio device thread (from DataCallback)
    // GetDurationWait -> GetEdge -> OnNextSample
    float GetNextSample(uint32_t p_samplerate)
    {
        Doublesec sample_period = 1s / double(p_samplerate);
        m_sample_time += sample_period;
        // Get duration to wait for next edge change based on what we need to do
        Doublesec time_to_wait = GetDurationWait();
        if (m_sample_time > time_to_wait)
        {
            // Get value what edge needs to become
            Edge edge = GetEdge();
            if (edge == Edge::toggle)
            {
                m_edge = !m_edge;
            }
            else if (edge == Edge::one)
            {
                m_edge = true;
            }
            else if (edge == Edge::zero)
            {
                m_edge = false;
            }
            // m_sample_time = time_to_wait - m_sample_time;
            m_sample_time = 0s;
            if (OnNextSample())
            {
                m_done = true;
            }
        }
        return m_edge ? m_volume : -m_volume;
    }
    Doublesec GetDurationWait() const
    {
        if (m_OnGetDurationWait)
        {
            return m_OnGetDurationWait();
        }
        return 0ms;
    }
    Edge GetEdge() const
    {
        if (m_OnGetEdge)
        {
            return m_OnGetEdge();
        }
        return Edge::no_change;
    }
    bool OnNextSample()
    {
        if (m_OnNextSample)
        {
            return m_OnNextSample();
        }
        return false;
    }


private:
    Event m_event;
    bool m_done = false;
    bool m_is_running = false;
    std::unique_ptr<ma_device> m_device;
    NextSampleFun m_OnNextSample;
    GetDurationFun m_OnGetDurationWait;
    GetEdgeFun m_OnGetEdge;
    bool m_edge = false;                                    // output value toggles between 1/0
    float m_volume = 1.0f;                                  // -1.0 .. 1.0
    bool m_toggle_channel[2] = {};
    Doublesec m_sample_time = 0ms;                          // time since last edge change

};

//============================================================================
//============================================================================

/// Base / interface for all pulsers
class Pulser
{
public:
    virtual ~Pulser() {}
    virtual bool Next() = 0;
    virtual int GetTstate() const = 0;
    virtual Edge GetEdge() const = 0;

protected:
    unsigned m_pulsnum = 0;                         // increased after each edge

};

//============================================================================

/// A pulser that when inserted causes a silence of given duration.
/// Does not need data.
/// Does not pulse, although can change pulse output at first call (after wait).
class PausePulser : public Pulser
{
    PausePulser(const PausePulser&) = delete;
    using Clock = std::chrono::system_clock;
public:
    PausePulser() = default;
    PausePulser(PausePulser&&) = default;

    /// Set length of pause in milliseconds.
    PausePulser& SetLength(std::chrono::milliseconds p_duration)
    {
        m_duration = p_duration;
        return *this;
    }

    /// Set length of pause in T-states.
    /// Note: waits first, then calls GetEdge.
    PausePulser& SetLength(int p_states)
    {
        m_duration_in_tstates = p_states;
        return *this;
    }

    /// Set edge value (so sound wave on/off) during pause. 
    /// Can be used to force it to a predefined value for blocks to follow.
    /// Egde::toggle will toggle once at first call.
    PausePulser& SetEdge(Edge p_edge)
    {
        m_edge = p_edge;
        return *this;
    }

    template<class TLoader>
    void MoveToLoader(TLoader& p_loader)
    {
        p_loader.AddBlock(std::move(*this));
    }

protected:
    bool AtEnd() const
    {
        return m_duration_in_tstates ? true : Clock::now() > (m_timepoint + m_duration);
    }
    virtual bool Next() override
    {
        if (m_pulsnum == 0)
        {
            m_timepoint = Clock::now();     // set start timepoint at first call
            m_pulsnum++;    // just to make sure timepoint only set once
        }
        return AtEnd();
    }
    int GetTstate() const override
    {
        return m_duration_in_tstates;
    }
    Edge GetEdge() const override
    {
        auto edge = m_edge;
        if (m_edge == Edge::toggle)
        {
            const_cast<PausePulser*>(this)->m_edge = Edge::no_change;       // toggle only first time.
        }
        return edge;
    }

private:
    int m_duration_in_tstates = 0;
    Edge m_edge = Edge::no_change;
    std::chrono::milliseconds m_duration = 0ms;
    std::chrono::time_point<Clock> m_timepoint;
};

//============================================================================

/// A pulser that when inserted can be used to show debug output.
/// To debug only.
/// Does not need data, neither pulses, so has no effect.
class DebugPulser : public Pulser
{
    DebugPulser(const DebugPulser&) = delete;
    using Clock = std::chrono::system_clock;
public:
    DebugPulser() = default;
    DebugPulser(DebugPulser&&) = default;


    template<class TLoader>
    void MoveToLoader(TLoader& p_loader)
    {
        p_loader.AddBlock(std::move(*this));
    }

protected:
    virtual bool Next() override
    {
        return true;
    }
    int GetTstate() const override
    {
        return 0;
    }
    Edge GetEdge() const override
    {
        return Edge::no_change;
    }

private:
};

//============================================================================

/// A pulser that when inserted can be used to create a tone or pulse sequence 
/// with a certain pattern. Eg a leader tone.
/// Does not need data.
class TonePulser : public Pulser
{
    TonePulser(const TonePulser&) = delete;
    using Clock = std::chrono::system_clock;
public:
    TonePulser() = default;
    TonePulser(TonePulser&&) = default;

    template<typename ... TParams>
    TonePulser& SetPattern(int p_first, TParams ... p_rest)
    {
        m_pattern.push_back(p_first);
        SetPattern(p_rest...);
        return *this;
    }

    // Set length in # of pulses that is # complete patterns.
    TonePulser& SetLength(unsigned p_max_pulses)
    {
        unsigned pattsize = unsigned(m_pattern.size());
        if (pattsize)
        {
            m_max_pulses = pattsize * p_max_pulses;
        }
        else
        {
            m_max_pulses = p_max_pulses;
        }
        return *this;
    }

    // Set length in milliseconds, rounds up to complete patterns.
    TonePulser& SetLength(std::chrono::milliseconds p_duration)
    {
        auto pat_dur = GetPatternDuration();
        if (pat_dur)
        {
            m_max_pulses = unsigned(p_duration / (m_tstate_dur * pat_dur));
            unsigned pattsize = unsigned(m_pattern.size());
            m_max_pulses += pattsize - (m_max_pulses % pattsize);    // round up to next multiple of pattsize
        }
        else
        {
            throw std::runtime_error("Cannot set length to time when pattern is unknown. Call SetPattern first.");
        }
        return *this;
    }
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader)
    {
        p_loader.AddBlock(std::move(*this));
    }
private:
    void SetPattern()
    {
        if (m_max_pulses == 0)
        {
            SetLength(1);       // default 1 pattern
        }
    }
    int GetPatternDuration() const
    {
        int retval = 0;
        for (auto pat : m_pattern)
        {
            retval += pat;
        }
        return retval;
    }
    int GetFirstPattern() const
    {
        return m_pattern.size() ? m_pattern[0] : 0;
    }
    bool AtEnd() const
    {
        return m_pulsnum >= m_max_pulses;
    }
    virtual bool Next() override
    {
        m_pulsnum++;
        return AtEnd();
    }
    int GetTstate() const override
    {
        auto retval = m_pattern.size() ? m_pattern[m_pulsnum % m_pattern.size()] : 0;
        return retval;
    }
    Edge GetEdge() const override
    {
        return Edge::toggle;
    }
private:

    std::vector<int> m_pattern;
    unsigned m_max_pulses = 0;

};

//============================================================================

/// A pulser that when inserted generated pulses based on the data attached.
/// So needs data attached.
/// Can optionally use 'pulse mode' having fixed length pulses like RS-232.
class DataPulser : public Pulser
{
    DataPulser(const DataPulser&) = delete;
public:
    DataPulser() = default;
    DataPulser(DataPulser&&) = default;


    /// Set Pattern (TStates) used to send a "1"
    template<typename ... TParams>
    DataPulser& SetOnePattern(int p_first, TParams ... p_rest)
    {
        m_one_pattern.push_back(p_first);
        SetOnePattern(p_rest...);
        return *this;
    }

    /// Set Pattern (TStates) used to send a "0"
    template<typename ... TParams>
    DataPulser& SetZeroPattern(int p_first, TParams ... p_rest)
    {
        m_zero_pattern.push_back(p_first);
        SetZeroPattern(p_rest...);
        return *this;
    }

    /// When != 0 switches to 'pulse mode' having fixed length pulses (given here) being 
    /// high or low for 1 or zero as RS-232.
    /// Needs start and stop bits for synchronisation.
    DataPulser& SetPulsDuration(int p_puls_duration)
    {
        m_puls_duration = p_puls_duration;
        m_start_duration = p_puls_duration;
        m_stop_duration = p_puls_duration;
        return *this;
    }

    /// Set start bit duration when in 'pulse mode'.
    DataPulser& SetStartBitDuration(int p_puls_duration)
    {
        m_start_duration = p_puls_duration;
        return *this;
    }

    /// Set stop bit duration when in 'pulse mode'.
    DataPulser& SetStopBitDuration(int p_puls_duration)
    {
        m_stop_duration = p_puls_duration;
        return *this;
    }
    /// Set the data to be pulsed.
    DataPulser& SetData(DataBlock p_data)
    {
        m_data = std::move(p_data);
        CalcChecksum();
        return *this;
    }

    /// Move this to given loader
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader)
    {
        p_loader.AddBlock(std::move(*this));
    }

    /// Set block type
    DataPulser& SetBlockType(ZxBlockType p_type)
    {
        m_type = p_type;
        return *this;
    }


protected:

    virtual int GetTstate() const override
    {
        if (m_puls_duration == 0)       // not in 'pulse mode'
        {
            if (NeedStartSync())
            {
                return m_start_duration;
            }
            if (GetCurrentBit())
            {
                return m_one_pattern[m_pulsnum];
            }
            else
            {
                return m_zero_pattern[m_pulsnum];
            }
        }
        // Here in 'pulse mode'
        auto bitnum = m_bitnum % 10;    // 0 -> 9
        if (bitnum == 0)
        {
            return m_start_duration;
        }
        else if (bitnum == 9)
        {
            return m_stop_duration;
        }
        else
        {
            return m_puls_duration;
        }
    }
    virtual Edge GetEdge() const override
    {
        if (m_puls_duration == 0)   // not in 'pulse mode'
        {
            return Edge::toggle;
        }
        // Here in 'pulse mode'
        if (GetCurrentBit())
        {
            return Edge::one;
        }
        else
        {
            return Edge::zero;
        }

    }

    virtual bool Next() override
    {
        if (NeedStartSync())
        {
            m_need_start_sync = false;
        }
        else
        {
            m_pulsnum++;
            if (m_pulsnum >= GetMaxPuls())
            {
                m_pulsnum = 0;
                m_bitnum++;
                m_need_start_sync = true;
            }
        }
        return AtEnd();
    }
private:

    bool NeedStartSync() const
    {
        return m_puls_duration == 0 &&      // else in pulse mode, has its own start bits
            m_need_start_sync &&            // else just done
            m_start_duration &&             // master switch using start sync at all
            m_bitnum % 8 == 0;              // only at first bit
    }

    void SetOnePattern() {}
    void SetZeroPattern() {}

    std::byte GetByte(unsigned p_index) const
    {
        if (m_type == ZxBlockType::header ||
            m_type == ZxBlockType::data)
        {
            if (p_index == 0)
            {
                return std::byte(m_type);
            }
            else if (p_index <= m_data.size())
            {
                return m_data[p_index - 1];
            }
            else
            {
                return m_checksum;
            }
        }
        else
        {
            return m_data[p_index];
        }
    }
    void SetSize(size_t p_size)
    {
        m_data.resize(p_size);
    }
    size_t GetTotalSize() const
    {
        return (m_type == ZxBlockType::raw) ? m_data.size() :
            m_data.size() + 1 + 1;   // + type byte + checksum
    }
    void CalcChecksum()
    {
        m_checksum = CalculateChecksum();
    }

    std::byte* GetDataPtr()
    {
        return m_data.data();
    }

    std::byte CalculateChecksum() const
    {
        std::byte retval{ std::byte(m_type) };       // type is included in checksum calc   
        for (const std::byte& b : m_data)
        {
            retval ^= b;
        }
        return retval;
    }



    bool AtEnd() const
    {
        return (m_bitnum / 8) >= GetTotalSize();
    }


    unsigned GetMaxPuls() const
    {
        if (GetCurrentBit())
        {
            return unsigned(m_one_pattern.size());
        }
        else
        {
            return unsigned(m_zero_pattern.size());
        }
    }
    bool GetCurrentBit() const
    {
        if (m_puls_duration == 0)
        {
            auto bytenum = m_bitnum / 8;
            auto bitnum = m_bitnum % 8;    // 0 -> 7
            bitnum = 7 - bitnum;            // 8 -> 0 spectrum saves most sign bit first
            const std::byte& byte = GetByte(bytenum);
            bool retval = bool((byte >> bitnum) & 0x1_byte);
            return retval;
        }
        else
        {
            // in puls mode needs start and stop bit for synchronisation
            auto bytenum = m_bitnum / 10;
            auto bitnum = m_bitnum % 10;    // 0 -> 9
            if (bitnum == 0)
            {
                return m_start_bit;
            }
            else if (bitnum == 9)
            {
                return !m_start_bit;
            }
            else
            {
                bitnum = 8 - bitnum;            // 8 -> 0 spectrum saves most sign bit first
                const std::byte& byte = GetByte(bytenum);
                bool retval = bool((byte >> bitnum) & 0x1_byte);
                return retval;
            }
        }
    }
private:
    unsigned m_bitnum = 0;          // bitnum from start in block to be send.
    bool m_need_start_sync = true;
    ZxBlockType m_type;             // returned as first byte unless raw
    DataBlock m_data;
    std::byte m_checksum{};         // returned as last byte unless raw

    std::vector<int> m_zero_pattern;
    std::vector<int> m_one_pattern;
    int m_puls_duration = 0;        // when !0 go to puls mode (! toggle)
    int m_start_duration = 0;       // default s/a m_puls_duration
    int m_stop_duration = 0;        // default s/a m_puls_duration
    bool m_start_bit = true;        // value for start bit. Stopbit is always !m_start_bit
    // and m_start_bit should also be true, see table at qloader.asm.
    // this means sync should end with 0!
};


//============================================================================
//============================================================================

class SpectrumLoader
{
private:
    // used clock
    using Clock = std::chrono::system_clock;
    using PulserPtr = std::unique_ptr<Pulser>;
    using Pulsers = std::vector<PulserPtr>;

public:

    SpectrumLoader()
    {
    }
    SpectrumLoader(bool)
    {
        Init();
    }

    SpectrumLoader& Init()
    {
        m_sample_sender.Init().
            SetOnGetDurationWait(std::bind(&SpectrumLoader::GetDurationWait, this)).
            SetOnGetEdge(std::bind(&SpectrumLoader::GetEdge, this)).
            SetOnNextSample(std::bind(&SpectrumLoader::Next, this));
        return *this;
    }


    /// Set (miniaudio) sample sender.
    SpectrumLoader& SetSampleSender(SampleSender&& p_sample_sender)
    {
        m_sample_sender = std::move(p_sample_sender);
        return Init();
    }

    /// Reset SpectrumLoader.
    SpectrumLoader& Reset()
    {
        m_current_block = 0;
        return *this;
    }

    /// And any pulser to process.
    template <class TPulser, typename std::enable_if<std::is_base_of<Pulser, TPulser>::value, int>::type = 0>
    SpectrumLoader& AddBlock(TPulser p_block)
    {
        PulserPtr ptr = std::make_unique< TPulser >(std::move(p_block));
        m_datablocks.push_back(std::move(ptr));
        return *this;
    }

    /// Convenience: add ZX Spectrum standard leader.
    SpectrumLoader& AddLeader(std::chrono::milliseconds p_duration)
    {
        TonePulser().SetPattern(2168).
            SetLength(p_duration).
            MoveToLoader(*this);
        return *this;
    }

    /// Convenience: add ZX Spectrum standard sync block. 
    SpectrumLoader& AddSync()
    {
        TonePulser().SetPattern(667, 735).MoveToLoader(*this);
        return *this;
    }

    /// Convenience: add ZX Spectrum standard data block.
    /// This is raw data so should already include startbyte + checksum
    SpectrumLoader& AddData(DataBlock p_data, int p_pulslen = 855)
    {
        DataPulser().SetBlockType(ZxBlockType::raw).
            SetZeroPattern(p_pulslen, p_pulslen).
            SetOnePattern(2 * p_pulslen, 2 * p_pulslen).
            SetData(std::move(p_data)).
            MoveToLoader(*this);
        return *this;
    }

    /// Convenience: add ZX Spectrum standard pause (eg before 2nd leader)
    SpectrumLoader& AddPause(std::chrono::milliseconds p_duration = 500ms)
    {
        PausePulser().SetLength(p_duration).MoveToLoader(*this);
        return *this;
    }

    /// Convenience: add ZX Spectrum standard leader+sync+data block.
    /// This is raw data which (should) already include startbyte + checksum
    SpectrumLoader& AddLeaderPlusData(DataBlock p_data, int p_pulslen = 855, std::chrono::milliseconds p_leader_duration = 3000ms)
    {
        return AddLeader(p_leader_duration).AddSync().AddData(std::move(p_data), p_pulslen);
    }


    /// Run SpectrumLoader. Play added pulser-blocks.
    SpectrumLoader& Run()
    {
        Reset();
        m_sample_sender.Run();
        return *this;
    }

    /// Mainly for debugging.
    bool GetLastEdge() const
    {
        return m_sample_sender.GetLastEdge();
    }



private:
    // CallBack; runs in miniaudio thread
    bool Next()
    {
        Pulser& current = GetCurrentBlock();
        if (current.Next())
        {
            m_current_block++;
            if (m_current_block >= m_datablocks.size())
            {
                return true;
            }
        }
        return false;
    }


    // Get duration to wait (in seconds)
    // CallBack; runs in miniaudio thread
    // depending on what is to be sent: (1/0/sync/leader).
    Doublesec GetDurationWait() const
    {
        Pulser& current = GetCurrentBlock();
        return current.GetTstate() * m_tstate_dur;
    }

    // Get duration to wait (in seconds)
    // CallBack; runs in miniaudio thread
    // depending on what is to be sent: (1/0/sync/leader).
    Edge GetEdge() const
    {
        Pulser& current = GetCurrentBlock();
        return current.GetEdge();
    }

    Pulser& GetCurrentBlock() const
    {
        return *(m_datablocks[m_current_block]);
    }

private:
    Pulsers m_datablocks;
    size_t m_current_block = 0;
    SampleSender m_sample_sender;

};

//============================================================================
//============================================================================


template <class TDataBlock>
class Compressor
{
public:
    //    using RLE_Meta  = std::tuple<TData, TData, TData, TData>;
    using DataBlock = TDataBlock;
    using TData = typename TDataBlock::value_type;
    using iterator = typename std::vector<TData>::iterator;
    using const_iterator = typename  std::vector<TData>::const_iterator;
    struct RLE_Meta
    {
        TData most;     // the value that occurs most
        TData min1;     // the value that occurs least, will be used to trigger RLE for max1
        TData min2;     // the value that occurs 2nd least, will be used to trigger RLE for max2
    };
private:
    using Hist = std::vector< std::pair<int, TData> >;
public:

    /// Compress, RLE. Return compressed data.
    /// RLE meta data is also written to compressed data.
    DataBlock Compress(const DataBlock& in_buf)
    {
        DataBlock compressed;
        auto max_min = DetermineCompressionRleValues(in_buf.begin(), in_buf.end());
        auto it = compressed.begin();
        WriteRleValues(compressed, it, max_min);
        Compress(in_buf.begin(), in_buf.end(), compressed, it, max_min);
        return compressed;
    }

    /// Compress, RLE. Return compressed data.
    /// Return RLE meta data as output parameter - not written to compressed data.
    DataBlock Compress(const DataBlock& in_buf, RLE_Meta& out_max_min)
    {
        DataBlock compressed;
        out_max_min = DetermineCompressionRleValues(in_buf.begin(), in_buf.end());
        auto it = compressed.begin();
        Compress(in_buf.begin(), in_buf.end(), compressed, it, out_max_min);
        return compressed;
    }

    /// Compress, RLE. Return compressed data as optional. 
    /// Tries meta data values for min1, min2 to make sure it can be decompressed inline.
    /// Return RLE meta data as output parameter - not written to compressed data.
    std::optional<DataBlock> Compress(const DataBlock& in_buf, RLE_Meta& out_max_min, int p_max_tries)
    {
        if (p_max_tries == 0)
        {
            return Compress(in_buf, out_max_min);
        }

        Hist hist = GetSortedHistogram(in_buf.begin(), in_buf.end());
        for (int tr = 0; tr < p_max_tries; tr++)
        {
            out_max_min = DetermineCompressionRleValues(hist, tr);
            DataBlock compressed;
            auto it = compressed.begin();
            Compress(in_buf.begin(), in_buf.end(), compressed, it, out_max_min);
            if (CanUseDecompressionInline(in_buf, compressed, out_max_min))
            {
                //                std::cout << "Compression attempt #" << (tr + 1) << " succeeded..." << std::endl;
                return compressed;
            }
            //std::cout << "Compression attempt #" << (tr + 1) << " Failed..." << std::endl;
        }
        std::cout << "Inline compression failed" << std::endl;
        return {};  // failed...
    }

    /// Compress, RLE. Return compressed data.
    /// Use RLE on given fixed p_max1 value with here determined out_min1.
    /// For Load-RLE where p_max must be zero.
    DataBlock Compress(const DataBlock& in_buf, TData p_max1, TData& out_min1)
    {
        DataBlock compressed;
        auto max_min = DetermineCompressionRleValues(in_buf.begin(), in_buf.end());     // only for max_min.min1
        max_min.max1 = p_max1;
        out_min1 = max_min.min1;
        auto it = compressed.begin();
        Compress(in_buf.begin(), in_buf.end(), compressed, it, max_min);
        return compressed;
    }






    /// Decompress given block, return decompressed data.
    /// RLE meta data is read from compressed buffer.
    DataBlock DeCompress(const DataBlock& p_compressed)
    {
        return DeCompress(p_compressed.begin(), p_compressed.end());
    }

    /// Decompress between given iterators, return decompressed data
    /// RLE meta data is read from compressed buffer.
    DataBlock DeCompress(const_iterator p_begin, const_iterator p_end)
    {
        DataBlock retval;
        auto min_max = ReadRleValues(p_begin);
        DeCompress(p_begin, p_end, retval, retval.begin(), min_max);
        return retval;
    }


    /// Decompress given block, return decompressed data.
    /// RLE meta data is given as parameters here.
    DataBlock DeCompress(const DataBlock& p_compressed, const RLE_Meta& p_max_min)
    {
        return DeCompress(p_compressed.begin(), p_compressed.end(), p_max_min);
    }

    /// Decompress between given iterators, return decompressed data
    /// RLE meta data is given as parameters here.
    DataBlock DeCompress(const_iterator p_begin, const_iterator p_end, const RLE_Meta& p_max_min)
    {
        DataBlock retval;
        DeCompress(p_begin, p_end, retval, retval.begin(), p_max_min);
        return retval;
    }
private:

    /// Compress block between given iterators to given output iterator.
    /// RLE meta data given here as parameter.
    void Compress(const_iterator p_begin, const_iterator p_end, DataBlock& out_buf, iterator& out_it, const RLE_Meta& p_max_min)
    {
        const auto& most = p_max_min.most;      // alias
        const auto& code_for_max = p_max_min.min1;
        const auto& code_for_triples = p_max_min.min2;


        const_iterator it;
        bool append_mode = (out_it == out_buf.end());
        auto Read = [&]()
        {
            return Compressor::Read(it);
        };
        auto Write = [&](TData p_byte)
        {
            Compressor::Write(out_buf, out_it, p_byte, append_mode);
        };

        TData max_count{};
        TData prev_count{};
        TData prev{};
        // write-out (flush) the max count, taking care of count equals to the min values.
        // because a duplicate min value codes for the single min value
        // and a single max value is just that value.
        auto WriteMax = [&]()
        {
            // keep writing single max values as long as maxcount equals min1 or min2 or we have just one
            while ((max_count == code_for_max || max_count == code_for_triples || max_count == TData(1)) && max_count > TData{ 0 })
            {
                --max_count;
                Write(most);
            }
            if (max_count > TData{ 0 })
            {
                Write(code_for_max);
                Write(max_count);
                max_count = TData{ 0 };
            }
        };
        auto WriteTriples = [&]()
        {
            // keep writing single prev values as long as maxcount equals min1 or min2 or we have just one
            while ((prev_count == code_for_max || prev_count == code_for_triples || prev_count <= TData(2)) && prev_count > TData{ 0 })
            {
                --prev_count;
                Write(prev);
            }
            if (prev_count > TData{ 0 })
            {
                Write(code_for_triples);
                Write(prev);
                Write(prev_count);
                prev_count = TData{ 0 };
            }
        };

        for (it = p_begin; it < p_end; )
        {
            auto b = Read();
            if (b == most)
            {
                WriteTriples();        // so flush 2nd max if there
                if (max_count == GetMax<TData>())       // flush when overflow
                {
                    WriteMax();        // max1count now 0
                }
                ++max_count;
            }
            else if (b == prev)
            {
                WriteMax();        // so flush 1st max if there
                if (prev_count == GetMax<TData>())
                {                                           // flush when overflow
                    WriteTriples();        // max2count now 0
                }
                ++prev_count;
            }
            else
            {
                WriteMax();
                WriteTriples();
                if (b == code_for_max || b == code_for_triples)
                {
                    Write(b);       // write the 2x times
                }
                Write(b);
            }
            prev = b;
        }
        WriteMax();        // at end flush remaining when present
        WriteTriples();
    }


    /// Decompress block between given iterators to given output iterator.
    /// RLE meta data is given as parameters here.
    void DeCompress(const_iterator p_begin, const_iterator p_end, DataBlock& out_buf, iterator out_it, const RLE_Meta& p_max_min)
    {
        const auto& most = p_max_min.most;      // alias
        const auto& code_for_most = p_max_min.min1;
        const auto& code_for_triples = p_max_min.min2;

        const_iterator it;
        bool at_end = false;

        bool append_mode = (out_it == out_buf.end());
        auto Read = [&]()
        {
            return Compressor::Read(it);
        };

        auto Write = [&](TData p_byte)
        {
            return Compressor::Write(out_buf, out_it, p_byte, append_mode);
        };




        auto WriteMax = [&]()
        {
            auto cnt = Read();
            if (cnt == code_for_most)       // 2nd seen?
            {
                at_end = Write(code_for_most);  
            }
            else
            {
                for (int n = 0; n < int(cnt) && !at_end; n++)
                {
                    at_end = Write(most);
                }
            }
        };
        auto WriteTriples = [&]()
        {
            auto val = Read();
            if (val == code_for_triples)    // 2nd seen?
            {
                at_end = Write(code_for_triples);
            }
            else
            {
                auto cnt = Read();
                for (int n = 0; n < int(cnt) && !at_end; n++)
                {
                    at_end = Write(val);
                }
            }
        };

        for (it = p_begin; it < p_end && !at_end; )
        {
            auto b = Read();
            if (b == code_for_most && it < p_end)
            {
                WriteMax();
            }
            else if (b == code_for_triples && it < p_end)
            {
                WriteTriples();
            }
            else
            {
                at_end = Write(b);
            }
        }
    }



    // Check if can use de-compress in same memory location as compressed data.
    // Where compressed data is stored at end of block, being overwritten during decompression.
    // By just trying.
    bool CanUseDecompressionInline(const DataBlock& p_orig_data, const DataBlock& p_compressed_data, const Compressor<DataBlock>::RLE_Meta& p_rle_meta)
    {
        DataBlock decompressed_data;
        auto sz = p_orig_data.size() - p_compressed_data.size();
        decompressed_data.resize(sz);
        // append compressed data
        decompressed_data.insert(decompressed_data.end(), p_compressed_data.begin(), p_compressed_data.end());
        DeCompress(decompressed_data.cbegin() + sz, decompressed_data.cend(), decompressed_data, decompressed_data.begin(), p_rle_meta);
        return decompressed_data == p_orig_data;
    }



    TData Read(const_iterator& p_it)
    {
        auto ret = *p_it;
        p_it++;
        return ret;
    };

    // Write one data to the output buffer at given iterator location.
    // when given iterator is end() append.
    bool Write(std::vector<TData>& out_buf, iterator& out_it, TData p_byte, bool p_append_mode = true)
    {
        if (p_append_mode)
        {
            out_buf.push_back(p_byte);      // append mode
            out_it = out_buf.end();
            return false;
        }
        else if (out_it != out_buf.end())
        {
            *out_it = p_byte;
            out_it++;
            return false;
        }
        return true;
    };



    Hist GetSortedHistogram(const_iterator p_begin, const_iterator p_end)
    {
        Hist hist{};
        // Make entries for each byte freq zero
        for (int n = 0; n < (1 << (8 * sizeof(TData))); n++)
        {
            hist.push_back({ 0, TData(n) });
        }
        // Determine byte frequencies
        for (auto it = p_begin; it != p_end; it++)
        {
            auto& pair = hist[int(*it)];
            pair.first++;
        }

        auto Compare = [] (typename Hist::value_type p1, typename Hist::value_type p2)
        {
            return p1.first == p2.first ? p1.second > p2.second : p1.first < p2.first;
        };
        std::sort(hist.begin(), hist.end(), Compare);    // on freq (at pair compare first element first)

        return hist;

    }

    // Determine RLE mata data from buffer defined by given iterators.
    RLE_Meta DetermineCompressionRleValues(const_iterator p_begin, const_iterator p_end)
    {
        return DetermineCompressionRleValues(GetSortedHistogram(p_begin, p_end), 0);
    }

    // Determine RLE mata data using suplied histogram.
    RLE_Meta DetermineCompressionRleValues(const Hist& p_hist, int p_try)
    {

        TData max = p_hist.back().second;                   // value that occurs most, will be coded with min1
        TData min1 = (p_hist.begin() + p_try)->second;       // value that occurs less, will be used to code max1
        TData min2 = (p_hist.begin() + p_try + 1)->second;   // value that occurs 2nd less, will be used to code max2

        // There is even no max1, dont code it - or skip
        if (max == min1 || max == min2)
        {
            max = TData{ 0 };
            min1 = TData{ 1 };
        }
        return { max, min1, min2 };

    }

    // Write RLE meta data to the given iterator position, thus becoming part of the compressed data.
    void WriteRleValues(DataBlock& out_buf, iterator& out_it, const RLE_Meta& p_max_min)
    {
        Write(out_buf, out_it, p_max_min.max1);
        Write(out_buf, out_it, p_max_min.max2);
        Write(out_buf, out_it, p_max_min.min1);
        Write(out_buf, out_it, p_max_min.min2);
    }

    // Write RLE meta data from given iterator position
    RLE_Meta ReadRleValues(const_iterator& p_it)
    {
        RLE_Meta retval;
        retval.max1 = Read(p_it);
        retval.max2 = Read(p_it);
        retval.min1 = Read(p_it);
        retval.min2 = Read(p_it);
        return retval;
    }
};


enum class CompressionType : uint8_t
{
    none,       // No compression; just copy to m_dest_address when given
    rle,
    automatic,  // will never be send to spectrum
};
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


///
/// Our turbo block as used with qloader.z80asm.
/// 
class TurboBlock
{
    friend class TurboBlocks;
public:
    // What to do after loading a turboblock
    enum AfterBlock
    {
        LoadNext = 0,               // go on to next block
        ReturnToBasic = 1,          // retrun to basic
        // all other values are like RANDOMIZE USR xxxxx so start MC there.
    };



#pragma pack(push, 1)
    struct Header
    {
        uint16_t m_length;                  // 0-1 length of data to receive including checksum byte, might be less than payload when rle_at_load
        uint16_t m_load_address;            // 2-3 where data will be stored initially during load (w/o header)
        uint16_t m_dest_address;            // 4-5 destination to copy or decompress to, when 0 do not copy/decompress 
        // (stays at load address)
        //     when compressed size of compressed data block. This is NOT the final/decompressed length.
        CompressionType m_compression_type; // 8 Type of compression-see enum
        uint8_t m_checksum;               // 9 RLE marker byte for load-RLE, when 0 do not use load-RLE
        uint16_t m_usr_start_address = LoadNext;   // 10-11 When 0: more blocks follow.
        // Else when>1 start mc code here as USR. Then this must be last block.
        // When 1 last block but do not do jp to usr.
        uint16_t m_clear_address = 0;       // 12-13 CLEAR and/or SP address
        uint8_t m_rle_most;                 // 14 RLE byte most
        uint8_t m_rle_min1;                 // 16 RLE byte min1
        uint8_t m_rle_min2;                 // 17 RLE byte min2
        uint8_t  m_copy_to_screen;          // when true it is a 'copy loader to screen' command

        friend std::ostream& operator <<(std::ostream& p_stream, const Header& p_header)
        {
            p_stream << "length = " << p_header.m_length << std::endl
                << "load_address = " << p_header.m_load_address << std::endl
                << "dest_address = " << p_header.m_dest_address << std::endl
                << "compression_type = " << p_header.m_compression_type << std::endl
                << "checksum = " << int(p_header.m_checksum) << std::endl
                << "usr_start_address = " << p_header.m_usr_start_address << std::endl
                << "m_clear_address = " << p_header.m_clear_address << std::endl
                << "m_rle_most = " << int(p_header.m_rle_most) << ' '
                << "m_rle_min1 = " << int(p_header.m_rle_min1) << ' '
                << "m_rle_min2 = " << int(p_header.m_rle_min2) << ' ' << std::endl
                << "m_copy_to_screen = " << int(p_header.m_copy_to_screen) << ' '
                //   << " is_last = " << int(p_header.m_is_last);
                ;

            return p_stream;
        }
    };
#pragma pack(pop)
    static_assert(sizeof(Header) == 16, "Check size of Header");
public:
    TurboBlock(const TurboBlock&) = delete;
    TurboBlock(TurboBlock&&) = default;
    TurboBlock()
    {
        m_data.resize(sizeof(Header));
        GetHeader().m_length = 0;
        GetHeader().m_load_address = 0;                 // where it initially loads payload, when 0 use load at basic buffer
        GetHeader().m_dest_address = 0;                 // no copy so keep at m_load_address
        GetHeader().m_compression_type = CompressionType::none;
        GetHeader().m_usr_start_address = ReturnToBasic;        // last block for now 
        GetHeader().m_clear_address = 0;
        GetHeader().m_checksum = 0;                    // MUST be zero at first so dont affect calc. itself
        GetHeader().m_copy_to_screen = false;
    }




    /// Set address where data is initially loaded.
    /// When 0 will take dest address (unless SetOverwritesLoader was called)
    TurboBlock& SetLoadAddress(uint16_t p_address)
    {
        GetHeader().m_load_address = p_address;
        return *this;
    }

    /// Set address where data will be copied / decompressed to after loading.
    /// (so final location).
    /// When 0 do not copy/decompress, stays at load address.
    TurboBlock& SetDestAddress(uint16_t p_address)
    {
        GetHeader().m_dest_address = p_address;
        return *this;
    }


    /// When 0 more blocks follow.
    /// When <> 0 this is the last block:
    ///  When 1 (and thus <> 0) this is the last block but no RANDOMIZE USR xxxxx is done. (default)
    ///  When <> 0 and <> 1 this is last block, and will jump to this address as if RANDOMIZE USR xxxxx.
    TurboBlock& SetUsrStartAddress(uint16_t p_address)
    {
        GetHeader().m_usr_start_address = p_address;
        return *this;
    }

    ///  Set address as found in CLEAR (BASIC), qloader might use this to set Stack Pointer.
    TurboBlock& SetClearAddress(uint16_t p_address)
    {
        GetHeader().m_clear_address = p_address;
        return *this;
    }

    ///  Set duration T state times for zero and one
    TurboBlock& SetDurations(int p_zero_duration, int p_one_duration)
    {
        m_zero_duration = p_zero_duration;
        m_one_duration = p_one_duration;
        std::cout << "Around " << (1000ms / m_tstate_dur) / ((m_zero_duration + m_one_duration) / 2) << " bps" << std::endl;
        return *this;
    }

    /// Set given data as payload at this TurboBlock.
    /// Also sets:
    /// m_length,           
    /// m_compression_type.
    TurboBlock& SetData(const DataBlock& p_data, CompressionType p_compression_type = CompressionType::automatic)
    {
        m_data_size = p_data.size();
        Compressor<DataBlock>::RLE_Meta rle_meta;
        // try inline decompression
        bool try_inline = !m_overwrites_loader && GetHeader().m_load_address == 0 && GetHeader().m_dest_address != 0;
        auto pair = TryCompress(p_data, p_compression_type, rle_meta, try_inline ? 5 : 0);
        CompressionType compression_type = pair.first;
        DataBlock& compressed_data = pair.second;
       // std::cout << "Compressed as: " << compression_type << " Uncompressed size: " << p_data.size() << "; Compressed size: " << compressed_data.size() << std::endl;
        if (try_inline && compression_type == CompressionType::rle)
        {
            SetLoadAddress(uint16_t(GetHeader().m_dest_address + p_data.size() - compressed_data.size()));
          //  std::cout << "Using inline decompression: Setting load address to " << GetHeader().m_load_address <<
          //      " (Dest Address = " << GetHeader().m_dest_address <<
          //      " Data size = " << p_data.size() <<
          //      " Compressed size = " << compressed_data.size() << ")" << std::endl;
        }
        const DataBlock* data;
        if (compression_type == CompressionType::rle)
        {
            GetHeader().m_rle_most = uint8_t(rle_meta.most);
            GetHeader().m_rle_min1 = uint8_t(rle_meta.min1);
            GetHeader().m_rle_min2 = uint8_t(rle_meta.min2);
            GetHeader().m_compression_type = compression_type;
            data = &compressed_data;
        }
        else // none
        {
            GetHeader().m_compression_type = compression_type;
            data = &p_data;
        }


        GetHeader().m_length = uint16_t(data->size());
        // GetHeader().m_length = uint16_t(compressed_data.size() + 2);       // @DEBUG should give ERROR
        // GetHeader().m_length = uint16_t(compressed_data.size() );       // @DEBUG should give CHECKSUM ERROR
        m_data.insert(m_data.end(), data->begin(), data->end());          // append given data at m_data (after header)




        if (!m_overwrites_loader && GetHeader().m_load_address == 0 && GetHeader().m_dest_address != 0)
        {
            // When only dest address given:
            // set load address to dest address, and dest address to 0 thus
            // loading immidiately at the correct location.
            SetLoadAddress(GetHeader().m_dest_address);
            SetDestAddress(0);
        }

        return *this;
    }

    bool IsEmpty() const
    {
        return  GetHeader().m_length == 0;
    }



    /// Move this TurboBlock to given loader (eg SpectrumLoader).
    /// Give it leader+sync as used by qloader.z80asm.
    /// Move all to given loader eg SpectrumLoader.
    /// p_pause_before this is important when ZX Spectrum needs some time to decompress.
    /// Give this an estimate how long it takes to handle previous block.
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader, std::chrono::milliseconds p_pause_before)
    {
        std::cout << "Pause before = " << p_pause_before.count() << "ms" << std::endl;

        PausePulser().SetLength(p_pause_before).MoveToLoader(p_loader);                 // pause before
        TonePulser().SetPattern(600, 600).SetLength(200ms).MoveToLoader(p_loader);      // leader
        TonePulser().SetPattern(300).SetLength(1).MoveToLoader(p_loader);               // sync

        auto data = GetData();
        DataBlock header(data.begin(), data.begin() + sizeof(Header));      // split
        DataBlock payload(data.begin() + sizeof(Header), data.end());

        MoveToLoader(p_loader, std::move(header));
        if (payload.size() != 0)
        {
            MoveToLoader(p_loader, std::move(payload));
        }
    }

    TurboBlock& DebugDump(int p_max = 0) const
    {
        std::cout << GetHeader() << std::endl;
        std::cout << "Orig. data length = " << m_data_size << std::endl;
        std::cout << "Compr. data length = " << GetHeader().m_length << std::endl;
        auto dest = GetHeader().m_dest_address != 0 ? GetHeader().m_dest_address : GetHeader().m_load_address;
        std::cout << "First byte written address = " << dest << std::endl;
        std::cout << "Last byte written address = " << (dest + m_data_size - 1) << "\n" << std::endl;
        for (int n = 0; (n < m_data.size()) && (n < p_max); n++)
        {
            if (n % 32 == 0 || (n == sizeof(Header)))
            {
                if (n != 0)
                {
                    std::cout << std::endl;
                }
                if (n == sizeof(Header))
                {
                    std::cout << std::endl;
                }
                std::cout << " DB ";
            }
            else
            {
                std::cout << ", ";
            }
            std::cout << std::hex << "0x" << int(m_data[n]);
        }
        std::cout << std::endl;
        return *const_cast<TurboBlock*>(this);
    }
private:

    //  Move given DataBlock to loader (eg SpectrumLoader)
    //  PausePulser(minisync) + DataPulser
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader, DataBlock p_block)
    {
        PausePulser().SetLength(500).SetEdge(Edge::toggle).MoveToLoader(p_loader);      // extra mini sync before

        DataPulser()        // data
            .SetBlockType(ZxBlockType::raw)
            //            .SetStartBitDuration(380)
            .SetZeroPattern(m_zero_duration)          // works with ONE_MAX 12 ONE_MIN 4
            .SetOnePattern(m_one_duration)
            .SetData(std::move(p_block))
            .MoveToLoader(p_loader);
    }

    Header& GetHeader()
    {
        return *(reinterpret_cast<Header*>(m_data.data()));
    }
    const Header& GetHeader() const
    {
        return *(reinterpret_cast<const Header*>(m_data.data()));
    }

    // After loading a compressed block ZX spectrum needs some time to 
    // decompress before it can accept next block. WIll wait this long after sending block.
    std::chrono::milliseconds EstimateHowLongSpectrumWillTakeToDecompress() const
    {
        if (GetHeader().m_dest_address == 0)
        {
            return 10ms;        // nothing done. Yeah checksum check, end check etc. 10ms should be enough.
        }
        if (GetHeader().m_compression_type == CompressionType::rle)
        {
            return (m_data_size * 100ms / 3000);      // eg 0.33 sec/10kb? Just tried.
        }
        return (m_data_size * 100ms / 20000);      // LDIR eg 0.05 sec/10kb? whatever.
    }

    // Set a flag indicating this block overwrites our qloader code at upper memory.
    // Need to load it elsewhere first, then copy to final location. Cannot load anything after that.
    // Adjusts loadaddress and dest address accordingly.
    TurboBlock& SetOverwritesLoader(uint16_t p_loader_upper_length)
    {
        m_overwrites_loader = true;
        SetLoadAddress(0);     // qloader.asm will put it at BASIC buffer
        SetDestAddress(0xffff - (p_loader_upper_length - 1));
        return *this;
    }

    TurboBlock& SetCopyToScreen(uint16_t p_loader_upper_length)
    {
        GetHeader().m_copy_to_screen = true;
        return *this;
    }
    // Move prepared data out of here. Append checksum.
    // Do some final checks and calculate checksum.
    // Makes data empty (moved away from)
    DataBlock GetData()
    {
        Check();
        GetHeader().m_checksum = CalculateChecksum();
        DebugDump(0);
        return std::move(m_data);
    }



    //  Do some checks, throws when not ok.
    void Check() const
    {
        if (GetHeader().m_dest_address == 0 && GetHeader().m_load_address == 0)
        {
            throw std::runtime_error("Both destination address to copy to and load address are zero");
        }
        if (GetHeader().m_dest_address == 0 && GetHeader().m_compression_type == CompressionType::rle)
        {
            throw std::runtime_error("Destination address to copy to after load is 0 while a RLE compression is set, will not decompress");
        }
        if (m_overwrites_loader && GetHeader().m_usr_start_address == LoadNext)
        {
            throw std::runtime_error("Block will overwrite our loader, but is not last");
        }
        if (GetHeader().m_load_address == 0 && !m_overwrites_loader)
        {
            throw std::runtime_error("Can not determine address where data will be loaded to");
        }
    }



    // Determine best compression, and return given data compressed  
    // using that algorithm.
    // TODO now only CompressionType::rle remains. Function not really usefull.
    // p_tries: when > 0 indicates must be able to use inline decompression.
    // This not always succeeds depending on choosen RLE paramters. The retry max this time.
    std::pair<CompressionType, DataBlock >
        TryCompress(const DataBlock& p_data, CompressionType p_compression_type, Compressor<DataBlock>::RLE_Meta& out_rle_meta, int p_tries)
    {
        if (p_compression_type == CompressionType::automatic)
        {
            // no compression when small
            // also no compression when m_dest_address is zero: will not run decompression.
            if (p_data.size() < 256 || GetHeader().m_dest_address == 0)
            {
                return { CompressionType::none, p_data.Clone() };       // done
            }
        }
        if (p_compression_type == CompressionType::rle ||
            p_compression_type == CompressionType::automatic)
        {
            DataBlock compressed_data;
            Compressor<DataBlock> compressor;


            // Compress. Also to check size
            auto try_compressed_data = compressor.Compress(p_data, out_rle_meta, p_tries);
            if (!try_compressed_data)
            {
                return { CompressionType::none, p_data.Clone() };
            }
            compressed_data = std::move(*try_compressed_data);
            // When automatic and it is bigger after compression... Then dont.
            if ((p_compression_type == CompressionType::automatic || p_tries != 0) && compressed_data.size() >= p_data.size())
            {
                return { CompressionType::none, p_data.Clone() };
            }


            // @DEBUG
            {
                Compressor<DataBlock> compressor;
                DataBlock decompressed_data = compressor.DeCompress(compressed_data, out_rle_meta);
                if (decompressed_data != p_data)
                {
                    throw std::runtime_error("Compression algorithm error!");
                }
            }
            // @DEBUG


            return { CompressionType::rle , std::move(compressed_data) };
        }
        return { p_compression_type, p_data.Clone() };
    }

    // Calculate a simple one-byte checksum over data.
    // including header and the length fields.
    uint8_t CalculateChecksum() const
    {
        int8_t retval = int8_t(sizeof(Header));
        //        int8_t retval = 1+ int8_t(sizeof(Header));  // @DEBUG must give CHECKSUM ERROR
        for (const std::byte& b : m_data)
        {
            retval += int8_t(b);
        }
        return uint8_t(-retval);
    }

private:
    size_t m_data_size;     // size of (uncompressed/final) data. Note: Spectrum does not need this.
    bool m_overwrites_loader = false;       // will this block overwrite our loader itself?
    DataBlock m_data;       // the data as send to Spectrum, starts with header
    //    int m_zero_duration = 80;
    //    int m_one_duration = 280;
    int m_zero_duration = 118;      // @@ see qloader.asm
    int m_one_duration = 293;     // @@ 175 more (3.5 cycle)
};
// TurboBlock


// Multiple turboblocks.
// Main reason for this class is to put a block that overlaps our loader code (qloader.z80asm) 
// at last position. Or handle the situation when the loader at BASIC REM statement is overwritten.
// Or when all is overwritten eg at snapshots.
// And handle the USR start address for chain of blocks so qloader.z80asm knows.
class TurboBlocks
{
public:
    TurboBlocks(fs::path p_symbol_file_name) :
        m_symbols(p_symbol_file_name)
    {
    }

    TurboBlocks& SetCompressionType(CompressionType p_compression_type)
    {
        m_compression_type = p_compression_type;
        return *this;
    }



    /// Add a Datablock as Turboblock at given symbol address. 
    TurboBlocks& AddDataBlock(DataBlock&& p_block, const std::string& p_symbol)
    {
        return AddDataBlock(std::move(p_block), m_symbols.GetSymbol(p_symbol));
    }

    // Add just a header with a 'copy to screen' command.
    // Mainly for debugging
    TurboBlocks& CopyLoaderToScreen()
    {
        m_loader_at_screen = true;
        DataBlock empty;
        AddTurboBlock(std::move(empty), 12345);
        return *this;
    }

    /// Add given Datablock as Turboblock at given address. 
    /// Check if qloader (upper) is overlapped.
    /// Check if qloader (lower, at basic) is overlapped.
    TurboBlocks& AddDataBlock(DataBlock&& p_block, uint16_t p_start_adr)
    {
        auto end_adr = p_start_adr + p_block.size();
        // Does it overwrite our loader (at upper regions)?
        auto loader_upper_len = m_symbols.GetSymbol("ASM_UPPER_LEN");
        DataBlock data_block;
        if (Overlaps(p_start_adr, end_adr, 1 + 0xffff - loader_upper_len, 1 + 0xffff))
        {
            if (!m_upper_block.IsEmpty())
            {
                throw std::runtime_error("Already found a block loading to loader-overlapped region, blocks overlap");
            }
            std::cout << "Block Overlaps our loader! Adding extra block..." << std::endl;
            int size2 = int(loader_upper_len + end_adr - (1 + 0xffff));
            int size1 = int(p_block.size() - size2);
            if (size1 > 0)
            {
                data_block = DataBlock(p_block.begin(), p_block.begin() + size1);   // the lower part
                end_adr = p_start_adr + data_block.size();
            }
            DataBlock block_upper(p_block.begin() + size1, p_block.end());  // part overwriting loader *must* be loaded last


            // *Must* be last block since our loader will be overwritten
            // So keep, then append as last.
            m_upper_block.SetOverwritesLoader(loader_upper_len);        // puts it at BASIC buffer

            m_upper_block.SetData(block_upper, m_compression_type);
        }
        else
        {
            data_block = std::move(p_block);
        }

        // block overwrites our loader code at the BASIC rem (CLEAR) statement
        if (Overlaps(p_start_adr, end_adr, SCREEN_END + 768, m_symbols.GetSymbol("CLEAR")))
        {
            // add some signal to copy loader to screen at 16384+4*1024
            m_loader_at_screen = true;
        }

        // block overwrites our loader code at screen
        // cut it in two pieces before and after
        // except when it is presumably a register block
        if (m_loader_at_screen &&
            Overlaps(p_start_adr, end_adr, SCREEN_23RD, SCREEN_END))
        {
            auto size_before = (SCREEN_23RD - int(p_start_adr));
            if (size_before > 0)
            {
                // max 4k before loader at screen
                DataBlock screen_4k(data_block.begin(), data_block.begin() + size_before);
                AddTurboBlock(std::move(screen_4k), p_start_adr);
            }
            auto size_after = int(end_adr) - SCREEN_END;
            if (size_after > 0)
            {
                DataBlock after_screen(data_block.begin() + data_block.size() - size_after, data_block.end());
                AddTurboBlock(std::move(after_screen), SCREEN_END);
            }
            if (size_before <= 0 && size_after <= 0)
            {
                // asume this is a snapshot/register block
                AddTurboBlock(std::move(data_block), p_start_adr);
            }
        }
        else
        {
            AddTurboBlock(std::move(data_block), p_start_adr);
        }
        return *this;
    }


    // Move all added turboblocks to loader.
    // no-op when there are no blocks.
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader, uint16_t p_usr_address, uint16_t p_clear_address = 0)
    {
        std::cout << std::endl;
        if (!m_upper_block.IsEmpty())
        {
            AddTurboBlock(std::move(m_upper_block));        // add upperblock as last to list
        }
        if (m_turbo_blocks.size() > 0)
        {
            if (m_loader_at_screen)
            {
                m_turbo_blocks.front().SetCopyToScreen(true);
            }
            m_turbo_blocks.back().SetUsrStartAddress(p_usr_address);    // now is last block
            m_turbo_blocks.back().SetClearAddress(p_clear_address);
        }
        auto pause_before = 100ms;        // time needed to start our loader after loading itself (basic!)
        for (auto& tblock : m_turbo_blocks)
        {
            auto next_pause = tblock.EstimateHowLongSpectrumWillTakeToDecompress(); // b4 because moved
            std::move(tblock).MoveToLoader(p_loader, pause_before);
            pause_before = next_pause;
        }
        m_turbo_blocks.clear();
    }
    const Symbols& GetSymbols() const
    {
        return m_symbols;
    }

private:
    template <class T1, class T2, class T3, class T4>
    static bool Overlaps(T1 p_start, T2 p_end, T3 p_start2, T4 p_end2)
    {
        return (int(p_end) > int(p_start2)) && (int(p_start) < int(p_end2));
    }



    // Add given datablock with given start address
    void AddTurboBlock(DataBlock&& p_block, uint16_t p_dest_address)
    {
        TurboBlock tblock;
        tblock.SetDestAddress(p_dest_address);
        tblock.SetData(std::move(p_block), m_compression_type);
        AddTurboBlock(std::move(tblock));
    }

    // Add given turboblock
    void AddTurboBlock(TurboBlock&& p_block)
    {
        if (m_turbo_blocks.size() > 0)
        {
            m_turbo_blocks.back().SetUsrStartAddress(TurboBlock::LoadNext);    // set 0 at previous block: so more blocks follow
        }
        m_turbo_blocks.push_back(std::move(p_block));
    }


private:
    std::vector<TurboBlock> m_turbo_blocks;
    TurboBlock m_upper_block;           // when a block is found that overlaps our loader (empty when not)
    bool m_loader_at_screen = false;    // when true the first block will have a 'copy loader to screen' command          
    CompressionType m_compression_type = CompressionType::none;
    Symbols m_symbols;
};

class TapLoaderInterface
{
public:
    virtual std::pair<ZxBlockType, DataBlock> LoadTapBlock(std::istream& p_stream) = 0;
    virtual bool HandleTapBlock(ZxBlockType p_type, const DataBlock p_block, std::string p_zxfilename) = 0;
};


class TapLoader
{
public:
    TapLoader(TapLoaderInterface& p_taploader) :
        m_taploader(p_taploader)
    {}
    TapLoader& Load(fs::path p_filename, std::string p_zxfilename)
    {
        std::ifstream fileread(p_filename, std::ios::binary);
        if (!fileread)
        {
            throw std::runtime_error("File " + p_filename.string() + " not found.");
        }
        else
        {
            std::cout << "Loading file " << p_filename << std::endl;
            Load(fileread, p_zxfilename);
        }
        return *this;
    }

    // https://sinclair.wiki.zxnet.co.uk/wiki/TAP_format
    TapLoader& Load(std::istream& p_stream, std::string p_zxfilename)
    {
        bool done = false;
        while (p_stream.peek() && p_stream.good() && !done)
        {
            auto pair = m_taploader.LoadTapBlock(p_stream);
            done = m_taploader.HandleTapBlock(pair.first, std::move(pair.second), p_zxfilename);        // virtual
        }
        return *this;
    }



private:
    TapLoaderInterface& m_taploader;

};

class TapToSpectrumLoader : public TapLoaderInterface
{
public:
    TapToSpectrumLoader(SpectrumLoader& p_spectrumloader) :
        m_spectrumloader(p_spectrumloader)
    {}

    /// Load a data block as in TAP format from given stream
    /// Includes type and checksum bytes in the block read.
    virtual std::pair<ZxBlockType, DataBlock> LoadTapBlock(std::istream& p_stream) override
    {
        auto len = LoadBinary< uint16_t>(p_stream);
        if (p_stream)
        {
            DataBlock block;
            block.resize(len);
            p_stream.read(reinterpret_cast<char*>(block.data()), len);
            ZxBlockType type = ZxBlockType(block[0]);       // type aka flag (header or data)
            return { type, std::move(block) };
        }
        throw std::runtime_error("Error reading tap block");
    }
    virtual bool HandleTapBlock(ZxBlockType p_type, DataBlock p_block, std::string p_zxfilename) override
    {
        m_spectrumloader.AddLeaderPlusData(std::move(p_block), 700, 1750ms);//.AddPause(100ms);
        return false;
    }
private:
    SpectrumLoader& m_spectrumloader;
};

class TapToTurboBlocks : public TapLoaderInterface
{
public:
    TapToTurboBlocks(TurboBlocks& p_tblocks) :
        m_tblocks(p_tblocks)
    {}



    /// Load a data block as in TAP format from given stream
    /// Does not store checksum and type bytes in returned block.
    /// type is returned but not stored.
    virtual std::pair<ZxBlockType, DataBlock> LoadTapBlock(std::istream& p_stream) override
    {
        auto len = LoadBinary< uint16_t>(p_stream) - 2;             // 2 bytes length, subtract checksum and type-flag
        if (p_stream)
        {
            ZxBlockType type = LoadBinary<ZxBlockType>(p_stream);       // type aka flag (header or data)
            DataBlock block;
            block.resize(len);
            p_stream.read(reinterpret_cast<char*>(block.data()), len);
            p_stream.ignore(1);                  // skip checksum byte
            return { type, std::move(block) };
        }
        throw std::runtime_error("Error reading tap block");
    }

    /// virtual
    /// Handle loaded data block:
    /// When header: check name; store as last header.
    /// When data: based on last header header:
    ///     When basic: try get addresses like RANDOMIZE USR XXXXX
    ///     When code: Add to list of blocks to (turbo) load
    virtual bool HandleTapBlock(ZxBlockType p_type, DataBlock p_block, std::string p_zxfilename) override
    {
        bool done = false;

        if (p_type == ZxBlockType::header)
        {
            if (p_block.size() != sizeof(ZxHeader))
            {
                throw std::runtime_error("Expecting header length to be " + std::to_string(sizeof(ZxHeader)) + ", but is : " + std::to_string(p_block.size()));
            }
            m_last_header = *reinterpret_cast<ZxHeader*>(p_block.data());
            std::string name(m_last_header.m_filename, 10);    // zx file name from tap/header
            if ((name == p_zxfilename || p_zxfilename == "") || m_headercnt >= 1)
            {
                m_last_block = p_type;
                m_headercnt++;
            }
            std::cout << m_last_header.m_type << ": ";
            std::cout << name << std::endl;
            std::cout << "Start address: " << m_last_header.GetStartAddress() << std::endl;
            std::cout << "Length: " << m_last_header.m_length << std::endl;
        }
        else if (p_type == ZxBlockType::data)
        {
            if (m_last_block != ZxBlockType::header && m_headercnt >= 1)
            {
                throw std::runtime_error("Found headerless, can not handle (can't know were it should go to)");
            }
            if (m_last_header.m_type == ZxHeader::Type::code ||
                m_last_header.m_type == ZxHeader::Type::screen)
            {
                auto start_adr = (m_loadcodes.size() > m_codecount) ?
                    m_loadcodes[m_codecount] :          // taking address from LOAD "" CODE Xxxxx as found in basic
                    m_last_header.GetStartAddress();

                m_tblocks.AddDataBlock(std::move(p_block), start_adr);
                m_codecount++;
            }
            if (m_last_header.m_type == ZxHeader::Type::basic_program)
            {
                if (m_codecount == 0)
                {
                    m_usr = TryFindUsr(p_block);
                    if (m_usr)
                    {
                        std::cout << "Found USR " << m_usr << " in BASIC." << std::endl;
                    }
                    m_clear = TryFindClear(p_block);
                    if (m_clear)
                    {
                        std::cout << "Found CLEAR " << m_clear << " in BASIC" << std::endl;
                    }
                    m_loadcodes = TryFindLoadCode(p_block);
                    for(auto code : m_loadcodes)
                    {
                        std::cout << "Found LOAD \"\" CODE " << code << " in BASIC" << std::endl;
                    }
                }
                else
                {
                    done = true;        // Found basic after code. We are done.
                }
            }
            m_last_block = p_type;
        }
        return done;
    }

    // Get MC start address found in BASIC as in RANDOMIZE USR xxxxx
    uint16_t GetUsrAddress() const
    {
        return m_usr == 0 ? 1 : m_usr;
    }

    // Get CLEAR address found in BASIC as in CLEAR xxxxx
    uint16_t GetClearAddress() const
    {
        return m_clear;
    }
private:

    // read a number from basic either as VAL "XXXXX" or a 2 byte int.
    // 0 when failed/not found. 
    // (note when it is truly 0 makes no sence like RANDOMIZE USR 0, CLEAR 0, LOAD "" CODE 0) 
    static uint16_t TryReadNumberFromBasic(const DataBlock& p_basic_block, int p_cnt)
    {
        std::string valstring;
        auto c = p_basic_block[p_cnt];
        if (p_cnt < p_basic_block.size() - 1 &&
            c == 0xB0_byte &&                                      //  VAL
            p_basic_block[p_cnt + 1] == std::byte('"'))            //  VAL "
        {
            // VAL "XXXX"
            p_cnt += 2;     // skip VAL "
            c = p_basic_block[p_cnt];
            while (c != std::byte('"') && p_cnt < p_basic_block.size())
            {
                valstring += char(c);
                p_cnt++;
                c = p_basic_block[p_cnt];
            }
            return uint16_t(std::atoi(valstring.c_str()));     // done
        }
        else if (p_cnt < p_basic_block.size() - 5 &&
            c == 0x0E_byte &&
            char(p_basic_block[p_cnt + 1]) == 0 &&
            char(p_basic_block[p_cnt + 2]) == 0 &&
            char(p_basic_block[p_cnt + 5]) == 0)
        {
            // https://retrocomputing.stackexchange.com/questions/5932/why-does-this-basic-program-declare-variables-for-the-numbers-0-to-4
            // 0x0E is kind of int flag at ZX basic. Followed by two zero's.
            // [0x0E] [0] [0] [LSB] [MSB] [0]
            // [RANDOMIZE USR] XXXXX       stored as int.
            p_cnt += 3;
            return *reinterpret_cast<const uint16_t*>(&p_basic_block[p_cnt]);
        }
        return 0;
    }


    // Try to find one ore more numbers after giving code in given BASIC block.
    // p_check_fun must match a cpde pattern to search for, then looks for following numbers.
    template <class TCheckFun>
    static std::vector<uint16_t> TryFindInBasic(const DataBlock& p_basic_block, TCheckFun p_check_fun)
    {
        int seen = 0;       // seen code 
        std::vector<uint16_t> retval;
        for (int cnt = 0; cnt < p_basic_block.size(); cnt++)
        {
            //   std::cout << std::hex << int(c) << ' ' << c << ' ';
            if (seen)
            {
                auto val = TryReadNumberFromBasic(p_basic_block, cnt);
                if (val >= 16384 && val < 65536)
                {
                    retval.push_back(val);
                    seen = 0;
                }
                seen--;
            }

            if (p_check_fun(p_basic_block, cnt))
            {
                seen = 8;       // skip some chars searching for number after code(eg the value stored as ASCII)
            }
        }
        return retval;
    }


    // Try to find (first) USR start address in given BASIC block
    // eg RANDOMIZE USR XXXXX
    // or RANDOMIZE USR VAL "XXXXX"
    static uint16_t TryFindUsr(const DataBlock& p_basic_block)
    {
        auto values = TryFindInBasic(p_basic_block, [](const DataBlock& p_basic_block, int cnt)
            {
                return cnt > 1 &&
                p_basic_block[cnt] == 0xC0_byte && (               // USR
                    p_basic_block[cnt - 1] == 0xf9_byte ||         // RANDOMIZE USR
                    p_basic_block[cnt - 1] == 0xf5_byte ||         // PRINT USR
                    p_basic_block[cnt - 1] == std::byte('='));     // (LET x ) = USR
            });
        return values.size() ? values.front() : 0;
    }
    // Try to find (first) CLEAR XXXXX address in given BASIC block
    // eg CLEAR XXXXX
    // or CLEAR VAL "XXXXX"
    static uint16_t TryFindClear(const DataBlock& p_basic_block)
    {
        auto values = TryFindInBasic(p_basic_block, [](const DataBlock& p_basic_block, int cnt)
            {
                return p_basic_block[cnt] == 0xFD_byte;    // CLEAR
            });
        return values.size() ? values.front() : 0;
    }
    // Try to find all LOAD "" CODE XXXXX addresses in given BASIC block
    static std::vector<uint16_t>  TryFindLoadCode(const DataBlock& p_basic_block)
    {
        return TryFindInBasic(p_basic_block, [](const DataBlock& p_basic_block, int cnt)
            {
                return cnt > 0 &&
                p_basic_block[cnt]     == 0xAF_byte &&       // CODE
                p_basic_block[cnt - 1] == std::byte('"');    // [LOAD "]" CODE
            });
    }
private:
    ZxHeader m_last_header;
    ZxBlockType m_last_block = ZxBlockType::unknown;        // not header or data
    int m_headercnt = 0;
    int m_codecount = 0;
    TurboBlocks& m_tblocks;
    uint16_t m_usr = 0;
    uint16_t m_clear = 0;
    std::vector< uint16_t> m_loadcodes;         // LOAD "" CODE xxxx found in basic
};


enum class TzxBlockType : uint8_t
{
    StandardSpeedDataBlock = 0x10,
    TurboSpeedDataBlock = 0x11,
    Puretone = 0x12,
    PulseSequence = 0x13,
    PureDataBlock = 0x14,
    DirectRecordingBlock = 0x15,
    CSWRecordingBlock = 0x18,
    GeneralizedDataBlock = 0x19,
    PauseOrStopthetapecommand = 0x20,
    GroupStart = 0x21,
    GroupEnd = 0x22,
    Jumptoblock = 0x23,
    Loopstart = 0x24,
    Loopend = 0x25,
    Callsequence = 0x26,
    Returnfromsequence = 0x27,
    Selectblock = 0x28,
    Stopthetapeifin48Kmode = 0x2A,
    Setsignallevel = 0x2B,
    Textdescription = 0x30,
    Messageblock = 0x31,
    Archiveinfo = 0x32,
    Hardwaretype = 0x33,
    Custominfoblock = 0x35,
    Glueblock = 0x5A
};
std::ostream& operator << (std::ostream& p_stream, TzxBlockType p_enum)
{
    switch (p_enum)
    {
        ENUM_TAG(TzxBlockType, StandardSpeedDataBlock);
        ENUM_TAG(TzxBlockType, TurboSpeedDataBlock);
        ENUM_TAG(TzxBlockType, Puretone);
        ENUM_TAG(TzxBlockType, PulseSequence);
        ENUM_TAG(TzxBlockType, PureDataBlock);
        ENUM_TAG(TzxBlockType, DirectRecordingBlock);
        ENUM_TAG(TzxBlockType, CSWRecordingBlock);
        ENUM_TAG(TzxBlockType, GeneralizedDataBlock);
        ENUM_TAG(TzxBlockType, PauseOrStopthetapecommand);
        ENUM_TAG(TzxBlockType, GroupStart);
        ENUM_TAG(TzxBlockType, GroupEnd);
        ENUM_TAG(TzxBlockType, Jumptoblock);
        ENUM_TAG(TzxBlockType, Loopstart);
        ENUM_TAG(TzxBlockType, Loopend);
        ENUM_TAG(TzxBlockType, Callsequence);
        ENUM_TAG(TzxBlockType, Returnfromsequence);
        ENUM_TAG(TzxBlockType, Selectblock);
        ENUM_TAG(TzxBlockType, Stopthetapeifin48Kmode);
        ENUM_TAG(TzxBlockType, Setsignallevel);
        ENUM_TAG(TzxBlockType, Textdescription);
        ENUM_TAG(TzxBlockType, Messageblock);
        ENUM_TAG(TzxBlockType, Archiveinfo);
        ENUM_TAG(TzxBlockType, Hardwaretype);
        ENUM_TAG(TzxBlockType, Custominfoblock);
        ENUM_TAG(TzxBlockType, Glueblock);
    default:
        p_stream << "Unknown: " << int(p_enum);

    }
    return p_stream;
}


class TzxLoader
{

public:
    TzxLoader(TapLoaderInterface& p_taploader) :
        m_taploader(p_taploader)
    {}
    TzxLoader& Load(fs::path p_filename, std::string p_zxfilename)
    {
        std::ifstream fileread(p_filename, std::ios::binary);
        if (!fileread)
        {
            throw std::runtime_error("File " + p_filename.string() + " not found.");
        }
        std::cout << "Loading file " << p_filename << std::endl;
        Load(fileread, p_zxfilename);
        //   MoveToLoader(p_loader);
        return *this;
    }
    // http://k1.spdns.de/Develop/Projects/zasm/Info/TZX%20format.html
    // https://worldofspectrum.net/TZXformat.html
    TzxLoader& Load(std::istream& p_stream, std::string p_zxfilename)
    {
        std::string s = LoadBinary<std::string>(p_stream, 7);

        if (s != "ZXTape!")
        {
            throw std::runtime_error("Not a tzx file");
        }
        m_eof_marker = LoadBinary<std::byte>(p_stream);
        auto version1 = LoadBinary<char>(p_stream);
        auto version2 = LoadBinary<char>(p_stream);
        std::cout << "TZX file version: " << int(version1) << '.' << int(version2) << std::endl;
        bool done = false;
        while (p_stream.peek() && p_stream.good() && !done)
        {
            TzxBlockType id = LoadBinary<TzxBlockType>(p_stream);
            if (std::byte(id) == m_eof_marker)
            {
                break;
            }
            std::cout << id << std::endl;
            switch (id)
            {
            case TzxBlockType::StandardSpeedDataBlock:
                done = LoadDataAsInTapFile(p_stream, p_zxfilename, 0x4 - 2);
                break;
            case TzxBlockType::TurboSpeedDataBlock:
                done = LoadDataAsInTapFile(p_stream, p_zxfilename, 0x12 - 2);
                break;
            case TzxBlockType::Puretone:
                p_stream.ignore(4);
                break;
            case TzxBlockType::PulseSequence:
                p_stream.ignore(2 * LoadBinary<std::uint8_t>(p_stream));
                break;
            case TzxBlockType::PureDataBlock:
                done = LoadDataAsInTapFile(p_stream, p_zxfilename, 0x0A - 2);
                break;
            case TzxBlockType::DirectRecordingBlock:
            {
                p_stream.ignore(5);
                auto len1 = LoadBinary<std::uint8_t>(p_stream);     // bit of weird 24 bit length
                auto len2 = LoadBinary<std::uint8_t>(p_stream);
                auto len3 = LoadBinary<std::uint8_t>(p_stream);
                p_stream.ignore(0xffff * len1 + 0xff * len2 + len3);
                break;
            }
            case TzxBlockType::CSWRecordingBlock:
                p_stream.ignore(0x0a);
                p_stream.ignore(LoadBinary<std::uint32_t>(p_stream));
                break;
            case TzxBlockType::GeneralizedDataBlock:
                p_stream.ignore(LoadBinary<std::uint32_t>(p_stream));
                break;
            case TzxBlockType::PauseOrStopthetapecommand:
                p_stream.ignore(2);
                break;
            default:
                std::cout << "TODO: TZX:" << id << std::endl;

            }
        }
        return *this;
    }


    bool LoadDataAsInTapFile(std::istream& p_stream, std::string p_zxfilename, size_t p_ignore)
    {
        // [2 byte len] - [spectrum data incl. checksum]
        p_stream.ignore(p_ignore);     // Pause after this block (ms.) {1000}
        auto pair = m_taploader.LoadTapBlock(p_stream);
        return m_taploader.HandleTapBlock(pair.first, std::move(pair.second), p_zxfilename);
    }
private:
    TapLoaderInterface& m_taploader;
    std::byte m_eof_marker;
};



class Z80SnapShotLoader
{
#pragma pack(push, 1)
    // z80 snapshot header https://worldofspectrum.org/faq/reference/z80format.htm
    struct Z80SnapShotHeader
    {
        uint8_t  A_reg;
        uint8_t  F_reg;
        uint16_t BC_reg;        // BC register pair(LSB, i.e.C, first)
        uint16_t HL_reg;
        uint16_t PC_reg;
        uint16_t SP_reg;
        uint8_t  I_reg;
        uint8_t  R_reg;
        uint8_t  flags_and_border;
        uint16_t DE_reg;
        uint16_t BCa_reg;
        uint16_t DEa_reg;
        uint16_t HLa_reg;
        uint8_t  Aa_reg;
        uint8_t  Fa_reg;
        uint16_t IY_reg;    // IY first!
        uint16_t IX_reg;
        uint8_t  ei_di;     // Interrupt flipflop, 0=DI, otherwise EI
        uint8_t  iff2;      // (not particularly important...)
        uint8_t  flags_and_imode;       // Interrupt mode (0, 1 or 2)
    };

    // z80 v2 snapshot header
    struct Z80SnapShotHeader2
    {
        uint16_t  length_and_version;
        uint16_t PC_reg;
        uint8_t  hardware_mode;
        uint8_t  out_mode;
        uint8_t  out_mode_intf1;
        uint8_t  simulator_flags;
        uint8_t  soundchip_reg_number;
        uint8_t  soundchip_registers[16];
    };

    // z80 v3 snapshot header
    struct Z80SnapShotHeader3
    {
        uint16_t  low_tstate_counter;
        uint8_t  high_tstate_counter;
        uint8_t  reserved_for_spectator;
        uint8_t  mgt_rom_paged;
        uint8_t  multiface_rom_paged;
        uint8_t  rom_is_ram1;
        uint8_t  rom_is_ram2;
        uint8_t  keyboard_joystick_mappings1[10];
        uint8_t  keyboard_joystick_mappings2[10];
        uint8_t  mgt_type;
        uint8_t  inhibit_button_status;
        uint8_t  inhibit_flag_rom;
        //uint8_t  last_out_0x1ffd;
    };
    struct Z80SnapShotDataHeader
    {
        uint16_t length;
        uint8_t page_num;
    };
 #pragma pack(pop)

    static_assert(sizeof(Z80SnapShotHeader) == 30, "Sizeof Z80SnapShotHeader must be 30");
    static_assert(sizeof(Z80SnapShotHeader2) == 25, "Sizeof Z80SnapShotHeader2 must be 23");
    static_assert(sizeof(Z80SnapShotHeader) + sizeof(Z80SnapShotHeader2) + sizeof(Z80SnapShotHeader3) == 86, "Sizeof Z80SnapShotHeaders must be 86");


public:
    Z80SnapShotLoader()
    {}
    Z80SnapShotLoader& Load(fs::path p_filename)
    {
        std::ifstream fileread(p_filename, std::ios::binary);
        if (!fileread)
        {
            throw std::runtime_error("File " + p_filename.string() + " not found.");
        }
        std::cout << "Loading file " << p_filename << std::endl;
        Load(fileread);
        //   MoveToLoader(p_loader);
        return *this;
    }

    // https://worldofspectrum.org/faq/reference/z80format.htm
    Z80SnapShotLoader& Load(std::istream& p_stream)
    {
        auto header = LoadBinary<Z80SnapShotHeader>(p_stream);
        // "Because of compatibility, if byte 12 is 255, it has to be regarded as being 1."
        if(header.flags_and_border == 255)
        {
            header.flags_and_border = 1;
        }
        // adjust the weird bit 8 for R-register
        header.R_reg = (header.R_reg & 0x7f) | ((header.flags_and_border & 0x1) << 8);
        m_mem48k.resize(48 * 1024);
        if (header.PC_reg == 0)      // v2 or v3
        {
            auto header2 = LoadBinary<Z80SnapShotHeader2>(p_stream);
            if (header2.length_and_version == 23)
            {
                std::cout << "Z80 version2 file" << std::endl;
            }
            if (header2.length_and_version == 54 ||
                header2.length_and_version == 55)
            {
                std::cout << "Z80 version3 file" << std::endl;
            }
            p_stream.ignore(header2.length_and_version - (sizeof(Z80SnapShotHeader2) - 2));
            header.PC_reg = header2.PC_reg;
            while (p_stream.peek() && p_stream.good())
            {
                auto data_header = LoadBinary<Z80SnapShotDataHeader>(p_stream);
                DataBlock block;
                if (data_header.length != 0xffff)
                {
                    DataBlock cblock;
                    cblock.resize(data_header.length);
                    p_stream.read(reinterpret_cast<char*>(cblock.data()), data_header.length);
                    block = DeCompress(cblock);        // z80 decompression algo
                }
                else
                {
                    block.resize(16384);
                    p_stream.read(reinterpret_cast<char*>(block.data()), 16384);
                }
                if (block.size() != 16384)
                {
                    throw std::runtime_error("Error reading z80 file block size not correct must be 16384 but is: " + std::to_string(block.size()));
                }
                uint16_t offset;
                switch (data_header.page_num)
                {
                case 4:
                    offset = 0x8000 - 16384;
                    break;
                case 5:
                    offset = 0xc000 - 16384;
                    break;
                case 8:
                    offset = 0x4000 - 16384;
                    break;
                default:
                    throw std::runtime_error("Error z80 pagenum is: " + std::to_string(data_header.page_num) + "; not a 48 snapshot?");
                }
                std::copy(block.begin(),
                    block.end(),
                    m_mem48k.begin() + offset);
            }
        }
        else
        {
            std::cout << "Z80 version1 file" << std::endl;
            p_stream.read(reinterpret_cast<char*>(m_mem48k.data()), 48 * 1024);       // will normally read less tahn 48k
            m_mem48k = DeCompress(m_mem48k);
            std::cout << "Size of uncompressed 48k block: " << m_mem48k.size();      // TODO throw otherwise
        }
        m_z80_snapshot_header = std::move(header);
        // m_snapshot_regs = Z80SnapShotHeaderToSnapShotRegs(header);
        return *this;
    }

    void MoveToTurboBlocks(TurboBlocks& p_turbo_blocks)
    {
        p_turbo_blocks.AddDataBlock(GetData(), 16384);
//        p_turbo_blocks.CopyLoaderToScreen();      // @DEBUG
        Z80SnapShotHeaderToSnapShotRegs(p_turbo_blocks.GetSymbols());
        p_turbo_blocks.AddDataBlock(std::move(m_reg_block), "LOAD_SNAPSHOT");
        m_usr = p_turbo_blocks.GetSymbols().GetSymbol("LOAD_SNAPSHOT");
    }

    uint16_t GetUsrAddress() const
    {
        return m_usr;
    }

    DataBlock GetData()
    {
        return std::move(m_mem48k);
    }

    Z80SnapShotLoader &SetRegBlock(DataBlock p_snapshot_regs_block)
    {
        m_reg_block = std::move(p_snapshot_regs_block);
        return *this;
    }
    
private:
    void Z80SnapShotHeaderToSnapShotRegs(const Symbols& p_symbols)
    {
        p_symbols.SetByte(m_reg_block, "flags_and_border", (m_z80_snapshot_header.flags_and_border >> 1) & 0xb00000111);
        p_symbols.SetWord(m_reg_block, "BC_reg", m_z80_snapshot_header.BC_reg);
        p_symbols.SetWord(m_reg_block, "DE_reg", m_z80_snapshot_header.DE_reg);
        p_symbols.SetWord(m_reg_block, "HL_reg", m_z80_snapshot_header.HL_reg);
        p_symbols.SetWord(m_reg_block, "BCa_reg", m_z80_snapshot_header.BCa_reg);
        p_symbols.SetWord(m_reg_block, "DEa_reg", m_z80_snapshot_header.DEa_reg);
        p_symbols.SetWord(m_reg_block, "HLa_reg", m_z80_snapshot_header.HLa_reg);
        p_symbols.SetWord(m_reg_block, "IX_reg", m_z80_snapshot_header.IX_reg);
        p_symbols.SetWord(m_reg_block, "IY_reg", m_z80_snapshot_header.IY_reg);
        p_symbols.SetByte(m_reg_block, "R_reg", m_z80_snapshot_header.R_reg);
        p_symbols.SetByte(m_reg_block, "I_reg", m_z80_snapshot_header.I_reg);
        p_symbols.SetWord(m_reg_block, "imode", 
            (m_z80_snapshot_header.flags_and_imode & 0x3) == 2 ? 0x5EED :       // IM 2 // !! endianess swapped!! ;=(   
            (m_z80_snapshot_header.flags_and_imode & 0x3) == 1 ? 0x56ED :       // IM 1
            (m_z80_snapshot_header.flags_and_imode & 0x3) == 0 ? 0x46ED : 0);   // IM 0
        p_symbols.SetByte(m_reg_block, "ei_di", (m_z80_snapshot_header.ei_di == 0) ?
                    0xF3 :      // DI
                    0xFB);      // EI
        p_symbols.SetWord(m_reg_block, "SP_reg", m_z80_snapshot_header.SP_reg);
        p_symbols.SetWord(m_reg_block, "PC_reg", m_z80_snapshot_header.PC_reg);
        p_symbols.SetByte(m_reg_block, "Aa_reg", m_z80_snapshot_header.Aa_reg);
        p_symbols.SetByte(m_reg_block, "Fa_reg", m_z80_snapshot_header.Fa_reg);
        p_symbols.SetByte(m_reg_block, "A_reg", m_z80_snapshot_header.A_reg);
        p_symbols.SetByte(m_reg_block, "F_reg", m_z80_snapshot_header.F_reg);
    }
    // Z80 decompress 
    // see https://worldofspectrum.org/faq/reference/z80format.htm
    DataBlock DeCompress(const DataBlock& p_block)
    {
        DataBlock retval;
        auto len = p_block.size();
        std::byte prev{};

        for (int n = 0; n < len; n++)
        {
            if (n <= len - 4 &&
                p_block[n + 0] == 0x0_byte &&
                p_block[n + 1] == 0xed_byte &&
                p_block[n + 2] == 0xed_byte &&
                p_block[n + 3] == 0x0_byte)
            {
                break;
            }

            auto b = p_block[n];

            if (b == 0xed_byte && prev == 0xed_byte)
            {
                int  xx = int(p_block[++n]);        // naming as in https://worldofspectrum.org/faq/reference/z80format.htm
                auto yy = p_block[++n];
                for (int i = 0; i < xx; i++)
                {
                    retval.push_back(yy);
                }
                prev = 0x0_byte;
            }
            else if (b != 0xed_byte && prev == 0xed_byte)
            {
                retval.push_back(prev);
                retval.push_back(b);
                prev = b;
            }
            else if (b != 0xed_byte)
            {
                retval.push_back(b);
                prev = b;
            }
            else
            {
                prev = b;
            }
        }
        return retval;
    }
private:
    Z80SnapShotHeader m_z80_snapshot_header;
    DataBlock m_mem48k;
    DataBlock m_reg_block;
    uint16_t m_usr;
};


template <class TIterator>
uint16_t Crc16(TIterator p_begin, TIterator p_end)
{
    uint16_t shiftreg = 0xffff;
    uint16_t polynomial = 0x1021;

    for (auto it = p_begin; it != p_end; it++)
    {
        auto b = *it;
        shiftreg = shiftreg ^ (uint16_t(b) << 8);

        for (int i = 0; i < 8; i++)
        {
            bool do_xor = shiftreg & 0x8000;
            shiftreg <<= 1;
            if (do_xor)
            {
                shiftreg ^= polynomial;
            }
        }
    }
    return shiftreg;
}
uint16_t Crc16(const std::vector<std::byte>& p_data)
{
    return Crc16(p_data.begin(), p_data.end());
}



uint16_t Test(TurboBlocks& p_blocks, fs::path p_filename)
{
#if 1
    // "C:\Projects\Visual Studio\Projects\qloader\vscode\qloader_test.bin"
    std::cout << "Reading binary file " << p_filename << std::endl;
    DataBlock block = LoadFromFile(p_filename);
    auto filelen = block.size();

    block.push_back(0_byte);        // make space for length filled in later
    block.push_back(0_byte);        // ,,

    block.push_back(0x10_byte);     // poly
    block.push_back(0x21_byte);     // poly

    for (int n = 0; n < 20; n++)
    {
        block.push_back(10_byte);
    }


    for (int n = 0; n < 254; n++)
    {
        block.push_back(std::byte(n));
        //block.push_back(10_byte);
    }
    for (int n = 0; n < 254; n++)
    {
        block.push_back(std::byte(n));
        //block.push_back(10_byte);
    }
    for (int n = 0; n < 254; n++)
    {
        block.push_back(std::byte(n));
        //block.push_back(10_byte);
    }

    for (int n = 0; n < 20; n++)
    {
        block.push_back(10_byte);
    }

    for (int n = 0; n < 2000; n++)
    {
        block.push_back(0_byte);
        block.push_back(0_byte);
        block.push_back(0_byte);
        block.push_back(0_byte);
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(std::byte(Random(0, 255)));
        block.push_back(1_byte);
        block.push_back(1_byte);
        block.push_back(1_byte);
        block.push_back(1_byte);
    }


    auto start = 0xffff - block.size() + 1;
    //    auto start = 32768;
    auto len = uint16_t(block.size()) - uint16_t(filelen) - 4;
    std::cout << "Length of test data = " << std::dec << len << " (crc_code_size = " << filelen << ")" << std::endl;
    std::cout << "Start of test data = " << std::dec << (start + 4 + filelen) << std::endl;
    block[filelen] = std::byte((len & 0xff00) >> 8);       // fill in length
    block[filelen + 1] = std::byte(len & 0xff);

    auto crc = Crc16(block.begin() + filelen + 4, block.end());     // excluding length and poly
//    p_blocks.CopyLoaderToScreen();
    p_blocks.AddDataBlock(std::move(block), uint16_t(start));
    std::cout << "=> CRC of test data = " << std::dec << crc << "; 0x" << std::hex << crc << std::dec << std::endl;
    return uint16_t(start);
#endif
#if 0
    DataBlock block;
    for (int n = 0; block.size() < 4 * 1024; n++)
    {
        //  block.push_back(0_byte);
        //  block.push_back(0_byte);
        for (int i = 0; i < 32; i++)
            block.push_back(255_byte);
        for (int i = 0; i < 32; i++)
            block.push_back(0_byte);
        for (int i = 0; i < 32; i++)
            block.push_back(0b11001010_byte);
        for (int i = 0; i < 32; i++)
            block.push_back(0b01010101_byte);
        //  block.push_back(0b11001100_byte);
    }
    std::cout << "Size = " << block.size() << std::endl;
    //  TurboBlocks tblocks(p_loader_length, CompressionType::none);
    p_blocks.AddDataBlock(std::move(block), uint16_t(16384));
    //  p_blocks.MoveToLoader(spectrumloader);
#endif
#if 0
    TonePulser tone;
    tone.SetPattern(80, 80).SetLength(20s).MoveToLoader(spectrumloader);
#endif
#if 0
    {
        PausePulser pause;
        pause.SetLength(100ms).
            SetEdge(Edge::zero).
            MoveToLoader(spectrumloader);
        spectrumloader.Run();

        std::cout << "Edge = " << spectrumloader.GetLastEdge() << std::endl;
    }
    {
        SpectrumLoader spectrumloader;
        TonePulser tone;
        tone.SetPattern(700, 700).
            //            SetLength(500ms).
            SetLength(1).
            MoveToLoader(spectrumloader);
        spectrumloader.Run();
        std::cout << "Edge = " << spectrumloader.GetLastEdge() << std::endl;
    }
    {
        SpectrumLoader spectrumloader;
        TonePulser tone;
        tone.SetLength(1).   // 000ms).
            SetPattern(700, 700, 700).SetLength(1).
            MoveToLoader(spectrumloader);
        spectrumloader.Run();

        std::cout << "Edge = " << spectrumloader.GetLastEdge() << std::endl;
    }
#endif
}






void Help()
{
    std::cout << 1 + &*R"(
QLoader

This is a turbo loader to load machine code (typical games) into a *real* ZX Spectrum at high speed.  
This loader is capable of loading a 48K game in about 30 seconds. This time includes the time of 
loading the loader itself (which uses traditional ROM loader/speed) plus a splash screen.  
For it to work it is needed to connect the audio output of your computer to the ZX Spectrum EAR input.
Set the volume of your computer to maximum. Then type LOAD "" at the ZX spectrum.
This loader generates the sound pulses, that the ZX Spectrum can load as data.


syntax:
qloader.exe
    path/to/filename        First file: can be a tap or tzx file and will be loaded at normal speed
                            into a real ZX spectrum. 
                            Only when the file here is 'qloader.tap' it can load the second file:
    path/to/filename        Second file, also a .tap or .tzx or a z80 snapshot file. When given will 
                            be send to the ZX spectrum at turbo speed.
    volume                  A float between 0.0 and 1.0: sets volume. Default 1.0 (max).
    invert_left             Invert the left (stereo) sound channel.
    invert_right            Invert the right (stereo) sound channel.


(C) 2023 Daan Scherft.
This project uses the miniaudio libarary by David Read.

)";
}

int main(int argc, char** argv)
{
    try
    {
        if (argc <= 1)
        {
            Help();
            std::cout << "Please give .tap or tzx filename." << std::endl;
            return -1;
        }

        SpectrumLoader spectrumloader;
        spectrumloader.SetSampleSender(SampleSender());
        {
            fs::path filename = argv[1];
            std::ifstream fileread(filename, std::ios::binary);
            if (!fileread)
            {
                std::cout << "File " << filename << " not found." << std::endl;
                return -2;
            }
            std::cout << "Processing file " << filename << std::endl;

            if (filename.extension() == ".tap" ||
                filename.extension() == ".TAP")
            {
                TapToSpectrumLoader loader(spectrumloader);
                TapLoader taploader(loader);
                taploader.Load(filename, "");
            }
            if (filename.extension() == ".tzx" ||
                filename.extension() == ".TZX")
            {
                TapToSpectrumLoader loader(spectrumloader);
                TzxLoader taploader(loader);
                taploader.Load(filename, "");
            }
        }

        if (argc > 2)
        {
            fs::path filename_exp = argv[1];
            filename_exp.replace_extension("exp");      // qloader.exp (symbols)
            fs::path filename = argv[2];
            TurboBlocks tblocks(filename_exp);
            tblocks.SetCompressionType(CompressionType::automatic);
            if (false)
            {
                TurboBlocks tblocks(filename_exp);
                for (int n = 0; n < 20; n++)
                {
                    Test(tblocks, filename);
                }
                tblocks.MoveToLoader(spectrumloader, TurboBlock::ReturnToBasic);
            }
            else if (filename.extension() == ".tap" ||
                filename.extension() == ".TAP")
            {
                TapToTurboBlocks loader(tblocks);
                TapLoader taploader(loader);
                taploader.Load(filename, "");
                tblocks.MoveToLoader(spectrumloader, loader.GetUsrAddress(), loader.GetClearAddress());
            }
            else if (filename.extension() == ".tzx" ||
                filename.extension() == ".TZX")
            {
                TapToTurboBlocks loader(tblocks);
                TzxLoader tzxloader(loader);
                tzxloader.Load(filename, "");
                tblocks.MoveToLoader(spectrumloader, loader.GetUsrAddress(), loader.GetClearAddress());
            }
            else if (filename.extension() == ".z80" ||
                filename.extension() == ".Z80")
            {
                Z80SnapShotLoader z80loader;
                // Read file snapshotregs.bin -> regblock
                fs::path snapshot_regs_filename = argv[1];
                snapshot_regs_filename.replace_filename("snapshotregs");
                snapshot_regs_filename.replace_extension("bin");      
                DataBlock regblock = LoadFromFile(snapshot_regs_filename);
                z80loader.Load(filename).SetRegBlock(std::move(regblock)).MoveToTurboBlocks(tblocks);
                tblocks.MoveToLoader(spectrumloader, z80loader.GetUsrAddress());
                //                tblocks.MoveToLoader(spectrumloader, 1);        // @DEBUG
            }
            else if (filename.extension() == ".bin" ||
                filename.extension() == ".BIN")
            {
                auto adr = Test(tblocks, filename);
                tblocks.MoveToLoader(spectrumloader, adr);
            }
        }
        auto start = std::chrono::steady_clock::now();
        spectrumloader.Run();
        auto end = std::chrono::steady_clock::now();
        auto dura = end - start;
        std::cout << "Took: " << std::dec << std::chrono::duration_cast<std::chrono::seconds>(dura).count() << " s" << std::endl;
        std::cout << "Edge = " << spectrumloader.GetLastEdge() << std::endl;
        Key();
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: " << e.what();
        Key();
        return -1;
    }
    return 0;
}

