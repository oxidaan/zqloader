// Focused waveform-export fixtures; verified through libspectrum by the caller.
#include "tzxwriter.h"
#include "spectrum_consts.h"
#include "spectrum_loader.h"
#include "sampletowav.h"
#include <limits>
#include <random>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <string>

// The fixture links internal writer sources rather than the application's DLL.
const char *GetVersion() { return "waveform-test"; }

class SequencePulser : public Pulser
{
public:
    using Item = std::pair<int, Edge>;
    explicit SequencePulser(std::vector<Item> items, Doublesec period = spectrum::tstate_dur)
        : Pulser(period), items_(std::move(items)) {}
    Edge GetEdge() const override { return items_.at(index_).second; }
    bool Next() override { return ++index_ == items_.size(); }
    int GetDurationInTStates() const override
    {
        int total = 0;
        for(const auto &item : items_) total += item.first;
        return total;
    }
protected:
    int GetTstate() const override { return items_.at(index_).first; }
private:
    std::vector<Item> items_;
    size_t index_ = 0;
};

int main(int argc, char **argv)
{
    if(argc != 2) throw std::runtime_error("Expected output directory");
    auto write = [&](const char *name, TzxWriter::Pulsers &pulsers)
    {
        std::ofstream out(std::string(argv[1]) + "/" + name, std::ios::binary);
        if(!out) throw std::runtime_error("Cannot open fixture output");
        TzxWriter::WriteTzxFile(pulsers, out, spectrum::tstate_dur);
        if(!out) throw std::runtime_error("Cannot write fixture");
    };
    TzxWriter::Pulsers pulsers;
    pulsers.push_back(std::make_unique<SequencePulser>(std::vector<SequencePulser::Item>{
        {10, Edge::toggle}, {20, Edge::toggle}, {30, Edge::no_change},
        {40, Edge::one}, {50, Edge::zero}}));
    write("levels.tzx", pulsers);
    pulsers.clear();
    auto data = std::make_unique<DataPulser>(spectrum::tstate_dur);
    DataBlock bytes;
    bytes.push_back(std::byte{0});
    bytes.push_back(std::byte{0x80});
    data->SetData(std::move(bytes)).SetZeroPattern(91).SetOnePattern(231).SetEndOfByteDelay(64);
    pulsers.push_back(std::move(data));
    write("byte-delay.tzx", pulsers);
    pulsers.clear();
    pulsers.push_back(std::make_unique<SequencePulser>(std::vector<SequencePulser::Item>{
        {140000, Edge::toggle}}));
    write("long-hold.tzx", pulsers);
    pulsers.clear();
    std::vector<SequencePulser::Item> many;
    for(int i = 0; i < 300; ++i) many.emplace_back(1000 + i, Edge::toggle);
    pulsers.push_back(std::make_unique<SequencePulser>(std::move(many)));
    write("alphabet-boundary.tzx", pulsers);
    pulsers.clear();
    std::vector<SequencePulser::Item> long_stream;
    for(int i = 0; i < 65540; ++i) long_stream.emplace_back(91, Edge::toggle);
    pulsers.push_back(std::make_unique<SequencePulser>(std::move(long_stream)));
    write("stream-boundary.tzx", pulsers);
    pulsers.clear();
    pulsers.push_back(std::make_unique<SequencePulser>(std::vector<SequencePulser::Item>{
        {10, Edge::toggle}}, spectrum::tstate_dur / 2));
    write("clock.tzx", pulsers);
    pulsers.clear();
    write("empty.tzx", pulsers);
    pulsers.push_back(std::make_unique<DataPulser>(spectrum::tstate_dur));
    write("empty-data.tzx", pulsers);
    pulsers.clear();
    auto rejects = [&](Doublesec period)
    {
        std::ostringstream out;
        try { TzxWriter::WriteTzxFile(pulsers, out, period); }
        catch(const std::runtime_error &) { return; }
        throw std::runtime_error("Expected invalid waveform to be rejected");
    };
    rejects(Doublesec{});
    auto infinite = std::make_unique<TonePulser>(spectrum::tstate_dur);
    infinite->SetPattern(500).SetInfiniteLength();
    pulsers.push_back(std::move(infinite));
    rejects(spectrum::tstate_dur);
    pulsers.clear();
    pulsers.push_back(std::make_unique<SequencePulser>(std::vector<SequencePulser::Item>{
        {-1, Edge::toggle}}));
    rejects(spectrum::tstate_dur);
    pulsers.clear();
    pulsers.push_back(std::make_unique<SequencePulser>(std::vector<SequencePulser::Item>{
        {1, Edge::toggle}}, Doublesec(std::numeric_limits<double>::infinity())));
    rejects(spectrum::tstate_dur);
    pulsers.clear();

    pulsers.push_back(std::make_unique<SequencePulser>(std::vector<SequencePulser::Item>{
        {1, Edge::toggle}}, Doublesec(1e100)));
    rejects(spectrum::tstate_dur);
    pulsers.clear();

    // Compare against the real WAV writer, not a duplicate rounding formula.
    std::vector<SequencePulser::Item> sampled = {
        {0, Edge::toggle}, {91, Edge::toggle}, {231, Edge::toggle},
        {155, Edge::toggle}, {295, Edge::toggle}, {35000, Edge::no_change},
        {70000, Edge::one}, {91, Edge::zero}, {146, Edge::toggle}};
    std::mt19937 random(538);
    for(int i = 0; i < 256; ++i)
        sampled.emplace_back(int(random() % 5000), Edge(random() % 4));
    for(uint32_t rate : {44100u, 48000u, 96000u, 192000u})
    {
        auto stem = std::string(argv[1]) + "/sampled-" + std::to_string(rate);
        pulsers.push_back(std::make_unique<SequencePulser>(sampled));
        std::ofstream tzx(stem + ".tzx", std::ios::binary);
        TzxWriter::WriteTzxFile(pulsers, tzx, spectrum::tstate_dur, rate);
        pulsers.clear();
        SpectrumLoader loader;
        loader.AddPulser(SequencePulser(sampled));
        SampleToWav wav;
        wav.SetSampleRate(rate);
        wav.SetVolume(100, 100);
        loader.Attach(wav);
        std::ofstream audio(stem + ".wav", std::ios::binary);
        wav.WriteToFile(audio);
        if(!tzx || !audio) throw std::runtime_error("Fixture output failed");
    }
    return 0;
}
