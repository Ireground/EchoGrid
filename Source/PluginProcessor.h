#pragma once
#include <JuceHeader.h>
#include "EchoEngine.h"

class EchoGridProcessor : public juce::AudioProcessor
{
public:
    EchoGridProcessor();
    ~EchoGridProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EchoGrid"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- parameters (s = original JSFX slider number 1..46)
    static juce::String pid (int s) { return "p" + juce::String (s); }
    juce::RangedAudioParameter* param (int s) const { return params[(size_t) s]; }
    float slider (int s) const { return params[(size_t) s]->convertFrom0to1 (params[(size_t) s]->getValue()); }
    void setSlider (int s, double v);            // with host gesture
    double effective (int s) const;              // value after the macro

    // ---- macro assignments
    std::array<std::atomic<bool>, eg::NS + 1> mOn {};
    std::array<std::atomic<float>, eg::NS + 1> mLo {}, mHi {};
    void macroAssign (int s);
    void macroRemove (int s);

    // ---- pattern (edit only while holding `lock`)
    juce::SpinLock lock;
    eg::Engine engine;
    void randomizePattern();
    void resetPattern();

    // ---- host info for the GUI
    std::atomic<double> hostTempo { 120.0 };
    std::atomic<int> hostNum { 4 }, hostDen { 4 };
    std::atomic<float> uiScale { 1.0f };
    std::atomic<int> selected { 0 };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::array<juce::RangedAudioParameter*, eg::NS + 1> params {};
    juce::Random rand;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoGridProcessor)
};
