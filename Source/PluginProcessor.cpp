#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct ChoiceDef { int s; const char* name; juce::StringArray items; int def; };
}

EchoGridProcessor::EchoGridProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EchoGrid", createLayout())
{
    for (int s = 1; s <= eg::NS; ++s) params[(size_t) s] = apvts.getParameter (pid (s));
    for (int s = 0; s <= eg::NS; ++s) { mOn[(size_t) s] = false; mLo[(size_t) s] = 0.0f; mHi[(size_t) s] = 1.0f; }
}

juce::AudioProcessorValueTreeState::ParameterLayout EchoGridProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    const ChoiceDef choices[] = {
        { 1,  "Playback Mode", { "Normal", "Dual Echo", "Ping-Pong" }, 0 },
        { 4,  "Mode",          { "Normal", "Sequencer" }, 0 },
        { 5,  "Time Unit",     { "Beat Div", "ms" }, 0 },
        { 6,  "Type",          { "Digital", "Analog BBD", "Studio Tape", "Cheap Tape", "Space Echo", "Dark BBD", "Lo-Fi", "Ambient" }, 2 },
        { 9,  "Time (Beat)",   { "1/64", "1/32T", "1/32", "1/16T", "1/32D", "1/16", "1/8T", "1/16D", "1/8", "1/4T",
                                 "1/8D", "1/4", "1/2T", "1/4D", "1/2", "1/1T", "1/2D", "1 Bar", "2 Bars" }, 11 },
        { 11, "Grid Length",   { "1 Beat", "1/2 Bar", "1 Bar", "2 Bars" }, 2 },
        { 12, "Grid Snap",     { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" }, 2 },
        { 14, "FX Affect",     { "Everything", "Only Echoes", "In Loop" }, 0 },
        { 21, "Filter Type",   { "Low Pass", "High Pass", "Band Pass", "Notch", "Peak", "Low Shelf", "High Shelf" }, 0 },
        { 40, "Shifter Mode",  { "Pitch", "Frequency" }, 0 },
    };
    const std::pair<int, const char*> bools[] = { { 18, "Freeze" }, { 20, "Filter On" }, { 25, "Phaser On" },
                                                  { 32, "Flanger On" }, { 39, "Shifter On" } };
    const std::pair<int, const char*> floats[] = {
        { 2, "Stereo Width" }, { 3, "Dual R Offset" }, { 7, "Character" }, { 8, "Feedback" }, { 10, "Time (ms)" },
        { 13, "Grid Length (ms)" }, { 15, "Dry" }, { 16, "Wet" }, { 17, "Macro" }, { 19, "Duck" },
        { 22, "Filter Cutoff" }, { 23, "Filter Q" }, { 24, "Filter Gain" },
        { 26, "Phaser Cutoff" }, { 27, "Phaser Rate" }, { 28, "Phaser Depth" }, { 30, "Phaser Spread" }, { 31, "Phaser Mix" },
        { 33, "Flanger Delay" }, { 34, "Flanger Depth" }, { 35, "Flanger Rate" }, { 36, "Flanger Spread" },
        { 37, "Flanger Feedback" }, { 38, "Flanger Mix" },
        { 41, "Shifter Pitch" }, { 42, "Shifter Freq" }, { 43, "Shifter LFO Depth" }, { 44, "Shifter LFO Rate" },
        { 45, "Shifter Stereo" }, { 46, "Shifter Mix" } };

    std::vector<std::unique_ptr<RangedAudioParameter>> all ((size_t) eg::NS + 1);
    for (auto& c : choices)
        all[(size_t) c.s] = std::make_unique<AudioParameterChoice> (ParameterID { pid (c.s), 1 }, c.name, c.items, c.def);
    for (auto& b : bools)
        all[(size_t) b.first] = std::make_unique<AudioParameterBool> (ParameterID { pid (b.first), 1 }, b.second, false);
    all[29] = std::make_unique<AudioParameterInt> (ParameterID { pid (29), 1 }, "Phaser Order", 2, 12, 4);
    for (auto& f : floats)
    {
        const int s = f.first;
        const auto& m = eg::meta (s);
        NormalisableRange<float> r (m.mn, m.mx,
            [s] (float, float, float n) { return (float) eg::dnm (s, n); },
            [s] (float, float, float v) { return (float) eg::nrm (s, v); },
            [] (float, float, float v) { return v; });
        all[(size_t) s] = std::make_unique<AudioParameterFloat> (ParameterID { pid (s), 1 }, f.second, r, m.def);
    }
    for (int s = 1; s <= eg::NS; ++s) l.add (std::move (all[(size_t) s]));
    return l;
}

//==============================================================================
void EchoGridProcessor::setSlider (int s, double v)
{
    auto* p = params[(size_t) s];
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
    p->endChangeGesture();
}

double EchoGridProcessor::effective (int s) const
{
    if (eg::hasMeta (s) && mOn[(size_t) s].load())
    {
        const double mv = slider (17) * 0.01;
        const double lo = mLo[(size_t) s].load(), hi = mHi[(size_t) s].load();
        return eg::dnm (s, lo + (hi - lo) * mv);
    }
    return slider (s);
}

void EchoGridProcessor::macroAssign (int s)
{
    const double n = eg::nrm (s, slider (s));
    mLo[(size_t) s] = (float) n;
    mHi[(size_t) s] = n < 0.9 ? 1.0f : 0.0f;
    mOn[(size_t) s] = true;
}

void EchoGridProcessor::macroRemove (int s)
{
    const double v = effective (s);
    mOn[(size_t) s] = false;
    setSlider (s, v);
}

void EchoGridProcessor::resetPattern()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    engine.pat.reset();
    engine.syncSmoothing();
    selected = 0;
}

void EchoGridProcessor::randomizePattern()
{
    double sl[eg::NS + 1] = {}, eff[eg::NS + 1] = {};
    for (int s = 1; s <= eg::NS; ++s) { sl[s] = slider (s); eff[s] = effective (s); }
    const auto t = eg::calcTimes (sl, eff, hostTempo.load(), hostNum.load(), hostDen.load(),
                                  engine.getSrate(), engine.getMaxmod(), engine.getMaxd());
    const double st = (int) sl[5] == 0 ? t.resQ / t.lenQ : 1.0 / 16;
    const int steps = std::max (1, (int) std::floor (1 / st + 0.5));
    const int n = 3 + rand.nextInt (5);

    const juce::SpinLock::ScopedLockType l (lock);
    auto& p = engine.pat;
    for (int i = 0; i < n; ++i)
    {
        const size_t k = (size_t) i;
        p.time[k] = (float) std::min (1.0, (1 + rand.nextInt (steps)) * st);
        p.lev[k]  = 0.35f + rand.nextFloat() * 0.65f;
        p.pan[k]  = std::floor ((rand.nextFloat() * 2 - 1) * 7) / 10.0f;
        p.tone[k] = std::floor ((rand.nextFloat() * 2 - 1) * 5) / 10.0f;
    }
    p.nh = n;
    engine.syncSmoothing();
    selected = 0;
}

//==============================================================================
bool EchoGridProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void EchoGridProcessor::prepareToPlay (double sampleRate, int)
{
    const juce::SpinLock::ScopedLockType l (lock);
    engine.prepare (sampleRate);
}

void EchoGridProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = getTotalNumOutputChannels();
    for (int c = getTotalNumInputChannels(); c < nch; ++c) buffer.clear (c, 0, n);
    if (n == 0 || nch == 0) return;

    double sl[eg::NS + 1] = {}, eff[eg::NS + 1] = {};
    for (int s = 1; s <= eg::NS; ++s)
    {
        sl[s] = apvts.getRawParameterValue (pid (s))->load();
        eff[s] = sl[s];
    }
    for (int s = 1; s <= eg::NS; ++s)
        if (eg::hasMeta (s) && mOn[(size_t) s].load())
        {
            const double lo = mLo[(size_t) s].load(), hi = mHi[(size_t) s].load();
            eff[s] = eg::dnm (s, lo + (hi - lo) * sl[17] * 0.01);
        }

    double tempo = hostTempo.load();
    int num = hostNum.load(), den = hostDen.load();
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) tempo = *b;
            if (auto ts = pos->getTimeSignature()) { num = ts->numerator; den = ts->denominator; }
            playing = pos->getIsPlaying();
        }
    hostTempo = tempo; hostNum = num; hostDen = den;

    const juce::SpinLock::ScopedLockType l (lock);
    engine.block (sl, eff, tempo, num, den, playing, n);
    auto* L = buffer.getWritePointer (0);
    auto* R = nch > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        float a = L[i], b = R != nullptr ? R[i] : L[i];
        engine.process (a, b);
        L[i] = a;
        if (R != nullptr) R[i] = b;
    }
}

juce::AudioProcessorEditor* EchoGridProcessor::createEditor() { return new EchoGridEditor (*this); }

//==============================================================================
void EchoGridProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    state.removeChild (state.getChildWithName ("Pattern"), nullptr);
    state.removeChild (state.getChildWithName ("Macros"), nullptr);

    juce::ValueTree pat ("Pattern");
    {
        const juce::SpinLock::ScopedLockType l (lock);
        const auto& p = engine.pat;
        pat.setProperty ("nh", p.nh, nullptr);
        for (int i = 0; i < p.nh; ++i)
        {
            juce::ValueTree h ("H");
            h.setProperty ("t", p.time[(size_t) i], nullptr);
            h.setProperty ("l", p.lev[(size_t) i], nullptr);
            h.setProperty ("p", p.pan[(size_t) i], nullptr);
            h.setProperty ("o", p.tone[(size_t) i], nullptr);
            pat.appendChild (h, nullptr);
        }
    }
    state.appendChild (pat, nullptr);

    juce::ValueTree mac ("Macros");
    for (int s = 1; s <= eg::NS; ++s)
        if (mOn[(size_t) s].load())
        {
            juce::ValueTree m ("M");
            m.setProperty ("s", s, nullptr);
            m.setProperty ("lo", mLo[(size_t) s].load(), nullptr);
            m.setProperty ("hi", mHi[(size_t) s].load(), nullptr);
            mac.appendChild (m, nullptr);
        }
    state.appendChild (mac, nullptr);

    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void EchoGridProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto t = juce::ValueTree::fromXml (*xml);
    uiScale = (float) t.getProperty ("uiScale", 1.0f);

    auto pat = t.getChildWithName ("Pattern");
    auto mac = t.getChildWithName ("Macros");
    if (pat.isValid())
    {
        const juce::SpinLock::ScopedLockType l (lock);
        auto& p = engine.pat;
        p.reset();
        int i = 0;
        for (auto h : pat)
        {
            if (i >= eg::MaxHandles) break;
            p.time[(size_t) i] = (float) h.getProperty ("t", 0.5);
            p.lev[(size_t) i]  = (float) h.getProperty ("l", 0.8);
            p.pan[(size_t) i]  = (float) h.getProperty ("p", 0.0);
            p.tone[(size_t) i] = (float) h.getProperty ("o", 0.0);
            ++i;
        }
        if (i > 0) p.nh = i;
        engine.syncSmoothing();
    }
    for (int s = 0; s <= eg::NS; ++s) mOn[(size_t) s] = false;
    if (mac.isValid())
        for (auto m : mac)
        {
            const int s = (int) m.getProperty ("s", 0);
            if (s < 1 || s > eg::NS) continue;
            mLo[(size_t) s] = (float) m.getProperty ("lo", 0.0);
            mHi[(size_t) s] = (float) m.getProperty ("hi", 1.0);
            mOn[(size_t) s] = true;
        }
    t.removeChild (pat, nullptr);
    t.removeChild (mac, nullptr);
    apvts.replaceState (t);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EchoGridProcessor(); }
