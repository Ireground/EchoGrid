#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Immediate-mode editor: frame() lays out the whole UI once per paint (drawing)
// and once per mouse event (interaction), just like the original JSFX @gfx code.
class EchoGridEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit EchoGridEditor (EchoGridProcessor&);
    ~EchoGridEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    using R = juce::Rectangle<float>;
    static constexpr int W = 1180, H = 720;

    void timerCallback() override { repaint(); }
    void frame();
    void updateMouse (const juce::MouseEvent&);

    // ---- widgets
    bool inside (float x, float y, float w, float h) const { return mx >= x && mx < x + w && my >= y && my < y + h; }
    bool takeClick (bool hov);
    void panel (float x, float y, float w, float h, const juce::String& title, juce::Colour dot);
    void dim (float x, float y, float w, float h);
    bool pill (float x, float y, float w, float h, const juce::String& label, bool on, juce::Colour acc, float fs = 10.5f);
    void onpill (int s, float x, float y, juce::Colour acc);
    void sl (int s, float x, float y, float w, juce::Colour acc);
    void asl (int id, const juce::String& label, int field, int ix, float x, float y, float w, int unit, juce::Colour acc);
    void txt (float x, float y, float w, float h, const juce::String& s, juce::Justification j);
    void ficon (int t, float x, float y, float w, float h);
    void stem (float xx, float lv, juce::Colour c, float gy, float gh);
    void gridSection();
    void fxSection();
    void macroSection();

    // ---- values
    double eff (int s) const { return proc.effective (s); }
    float  raw (int s) const { return proc.slider (s); }
    int    ri (int s) const  { return (int) std::lround (proc.slider (s)); }
    void setv (int s, double v);
    void setn (int s, double n);
    static juce::String fmtv (int s, double v);
    float snapf (float f) const;
    float& patField (int field, int ix);

    void openSizeMenu (R area);
    float uiScale() const { return (float) getWidth() / (float) W; }

    EchoGridProcessor& proc;
    juce::Graphics* g = nullptr;     // non-null while drawing

    // input state (logical coordinates)
    float mx = -1000, my = -1000, dmx = 0, pmx = 0;
    bool lb = false, shift = false, ctrl = false;
    bool click = false, rclick = false, dbl = false;
    float wheel = 0;
    int act = 0, gestureParam = 0;
    double dragn = 0;
    float hgox = 0, hgoy = 0;
    bool dirty = false;

    eg::Times tm {};
    double srate = 48000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoGridEditor)
};
