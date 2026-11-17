#include "PluginEditor.h"

namespace
{
    // HatsOff light scheme
    const juce::Colour cBg      { 0xFF141414 };
    const juce::Colour cPanel   { 0xFFE4E4E1 };
    const juce::Colour cInset   { 0xFFC9C9C5 };
    const juce::Colour cInset2  { 0xFFBBBBB7 };
    const juce::Colour cLabel   { 0xFF5C5C59 };
    const juce::Colour cText    { 0xFF1C1C1A };
    const juce::Colour cTrack   { 0xFFB4B4B0 };
    const juce::Colour cOrange  { 0xFFEF5C25 };
    const juce::Colour cBlue    { 0xFF3960B6 };
    const juce::Colour cTeal    { 0xFF2D908B };
    const juce::Colour cPurple  { 0xFF8A4AB8 };
    const juce::Colour cGreen   { 0xFF23955F };
    const juce::Colour cRed     { 0xFFD23A2A };

    juce::Font mono (float size, bool bold = false)
    {
       #if JUCE_WINDOWS
        const juce::String name ("Consolas");
       #else
        const juce::String name (juce::Font::getDefaultMonospacedFontName());
       #endif
        return juce::Font (juce::FontOptions (name, size, bold ? juce::Font::bold : juce::Font::plain));
    }

    const juce::String dot (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    const juce::String deg (juce::CharPointer_UTF8 ("\xc2\xb0"));

    // pattern fields
    enum { FTime, FLev, FPan, FTone };
}

//==============================================================================
EchoGridEditor::EchoGridEditor (EchoGridProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setResizable (true, true);
    setResizeLimits (W / 2, H / 2, W * 2, H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    const float s = juce::jlimit (0.5f, 2.0f, proc.uiScale.load());
    setSize (juce::roundToInt (W * s), juce::roundToInt (H * s));
    startTimerHz (30);
}

void EchoGridEditor::resized() { proc.uiScale = uiScale(); }

//==============================================================================
// value helpers
void EchoGridEditor::setv (int s, double v)
{
    auto* p = proc.param (s);
    const float n = p->convertTo0to1 ((float) v);
    if (gestureParam == s) p->setValueNotifyingHost (n);
    else { p->beginChangeGesture(); p->setValueNotifyingHost (n); p->endChangeGesture(); }
}

void EchoGridEditor::setn (int s, double n)
{
    double v = eg::dnm (s, n);
    if (s == 29) v = 2 * std::floor (v * 0.5 + 0.5);
    setv (s, v);
}

juce::String EchoGridEditor::fmtv (int s, double v)
{
    switch (eg::meta (s).unit)
    {
        case 0: return juce::String (juce::roundToInt (v)) + "%";
        case 1: return v < 10 ? juce::String (v, 2) + " ms" : v < 100 ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms";
        case 2: return v <= -59.9 ? juce::String ("-inf") : (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB";
        case 3: return v >= 1000 ? juce::String (v / 1000, 2) + " kHz" : v >= 100 ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v, 2) + " Hz";
        case 4: return juce::String (v, 2);
        case 5: return juce::String (juce::roundToInt (v)) + deg;
        case 6: return juce::String ((int) std::lround (v));
        case 8: return (v >= 0 ? "+" : "") + juce::String (v, 2) + " st";
        case 9: return (v >= 0 ? "+" : "") + (std::abs (v) < 100 ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " Hz";
        default: return (v >= 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + "%";
    }
}

float EchoGridEditor::snapf (float f) const
{
    f = juce::jlimit (0.005f, 1.0f, f);
    if (ri (5) == 0 && ! ctrl)
    {
        const double st = tm.resQ / tm.lenQ;
        double v = std::max (1.0, std::floor (f / st + 0.5)) * st;
        if (v > 1.0001) v = std::floor (1 / st) * st;
        f = (float) v;
    }
    return f;
}

float& EchoGridEditor::patField (int field, int ix)
{
    auto& p = proc.engine.pat;
    const size_t k = (size_t) juce::jlimit (0, eg::MaxHandles - 1, ix);
    return field == FTime ? p.time[k] : field == FLev ? p.lev[k] : field == FPan ? p.pan[k] : p.tone[k];
}

//==============================================================================
// widgets
bool EchoGridEditor::takeClick (bool hov)
{
    if (hov && click && act == 0) { click = false; return true; }
    return false;
}

void EchoGridEditor::txt (float x, float y, float w, float h, const juce::String& s, juce::Justification j)
{
    if (g != nullptr) g->drawText (s, R (x, y, w, h), j, false);
}

void EchoGridEditor::panel (float x, float y, float w, float h, const juce::String& title, juce::Colour dotCol)
{
    if (g == nullptr) return;
    g->setColour (cPanel);
    g->fillRoundedRectangle (x, y, w, h, 7.0f);
    g->setColour (cLabel);
    g->setFont (mono (12.0f));
    txt (x + 14, y + 8, w - 40, 14, title, juce::Justification::centredLeft);
    g->setColour (dotCol);
    g->fillEllipse (x + w - 19, y + 11, 8, 8);
}

void EchoGridEditor::dim (float x, float y, float w, float h)
{
    if (g == nullptr) return;
    g->setColour (cPanel.withAlpha (0.62f));
    g->fillRect (x, y, w, h);
}

bool EchoGridEditor::pill (float x, float y, float w, float h, const juce::String& label, bool on, juce::Colour acc, float fs)
{
    const bool hov = inside (x, y, w, h);
    if (g != nullptr)
    {
        g->setColour (on ? (hov ? acc.darker (0.15f) : acc) : (hov && act == 0 ? cInset2 : cInset));
        g->fillRoundedRectangle (x, y, w, h, h * 0.5f);
        g->setColour (on ? juce::Colours::white : cText);
        g->setFont (mono (fs, true));
        txt (x, y, w, h, label, juce::Justification::centred);
    }
    return takeClick (hov);
}

void EchoGridEditor::onpill (int s, float x, float y, juce::Colour acc)
{
    const bool on = raw (s) >= 0.5f;
    if (pill (x, y, 46, 20, on ? "ON" : "OFF", on, acc)) setv (s, on ? 0 : 1);
}

// slim slider: label left + bold value right on top, thin track below
void EchoGridEditor::sl (int s, float x, float y, float w, juce::Colour acc)
{
    const auto& m = eg::meta (s);
    double v = eff (s);
    double n = eg::nrm (s, v);
    const bool mo = proc.mOn[(size_t) s].load();
    const bool hov = inside (x - 4, y, w + 8, 28);

    if (takeClick (hov))
    {
        if (dbl) { if (! mo) setv (s, m.def); }
        else if (! mo)
        {
            act = s; dragn = n; gestureParam = s;
            proc.param (s)->beginChangeGesture();
        }
    }
    if (act == s && lb && dmx != 0)
    {
        dragn = juce::jlimit (0.0, 1.0, dragn + dmx / w * (shift ? 0.12 : 1.0));
        setn (s, dragn);
        n = dragn; v = eg::dnm (s, n);
    }
    if (hov && wheel != 0 && act == 0 && ! mo)
    {
        n = juce::jlimit (0.0, 1.0, n + wheel * (shift ? 0.002 : 0.02));
        setn (s, n);
        v = eg::dnm (s, n);
        wheel = 0;
    }
    if (hov && rclick && act == 0 && m.mac)
    {
        rclick = false;
        juce::PopupMenu menu;
        menu.addItem (1, mo ? "Remove from Macro" : "Assign to Macro");
        menu.addItem (2, "Reset to default");
        juce::Component::SafePointer<EchoGridEditor> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options(), [safe, s, mo] (int r)
        {
            if (safe == nullptr) return;
            auto& pr = safe->proc;
            if (r == 1) { if (mo) pr.macroRemove (s); else pr.macroAssign (s); }
            if (r == 2) { if (mo) pr.macroRemove (s); pr.setSlider (s, eg::meta (s).def); }
            safe->repaint();
        });
    }

    if (g == nullptr) return;
    g->setFont (mono (10.5f));
    g->setColour (cLabel);
    txt (x, y, w, 14, m.label, juce::Justification::centredLeft);
    g->setFont (mono (12.0f, true));
    g->setColour (mo ? cPurple : cText);
    txt (x, y, w, 14, fmtv (s, v), juce::Justification::centredRight);

    const float ty = y + 20;
    const auto fillCol = mo ? cPurple : acc;
    if (mo)
    {
        const float lo = proc.mLo[(size_t) s].load(), hi = proc.mHi[(size_t) s].load();
        g->setColour (cPurple.withAlpha (0.25f));
        g->fillRoundedRectangle (x + std::min (lo, hi) * w, ty - 3, std::max (4.0f, std::abs (hi - lo) * w), 8, 3);
    }
    g->setColour (cTrack);
    g->fillRect (x, ty, w, 2.0f);
    g->setColour (fillCol);
    g->fillRect (x, ty, (float) n * w, 2.0f);
    g->setColour ((act == s || (hov && act == 0)) ? cText : fillCol);
    g->fillRect (x + (float) n * w - 1.5f, ty - 5, 3.0f, 12.0f);
}

// slim slider for per-repeat values (unit: 0 level, 1 pan, 2 tone)
void EchoGridEditor::asl (int id, const juce::String& label, int field, int ix, float x, float y, float w, int unit, juce::Colour acc)
{
    const float mn = unit == 0 ? 0.0f : -1.0f, mxv = 1.0f, dflt = unit == 0 ? 1.0f : 0.0f;
    float& ref = patField (field, ix);
    float v = ref;
    float n = (v - mn) / (mxv - mn);
    const bool hov = inside (x - 4, y, w + 8, 28);

    if (takeClick (hov))
    {
        if (dbl) { const juce::SpinLock::ScopedLockType l (proc.lock); ref = dflt; dirty = true; }
        else { act = 500 + id; dragn = n; }
    }
    if (act == 500 + id && lb && dmx != 0)
    {
        dragn = juce::jlimit (0.0, 1.0, dragn + dmx / w * (shift ? 0.12 : 1.0));
        const juce::SpinLock::ScopedLockType l (proc.lock);
        ref = mn + (mxv - mn) * (float) dragn;
        dirty = true;
    }
    if (hov && wheel != 0 && act == 0)
    {
        const juce::SpinLock::ScopedLockType l (proc.lock);
        ref = juce::jlimit (mn, mxv, v + wheel * (mxv - mn) * (shift ? 0.002f : 0.02f));
        dirty = true; wheel = 0;
    }
    v = ref; n = (v - mn) / (mxv - mn);

    if (g == nullptr) return;
    juce::String vs;
    if (unit == 0)      vs = juce::String (juce::roundToInt (v * 100)) + "%";
    else if (unit == 1) vs = std::abs (v) < 0.005f ? juce::String ("CENTER") : (v < 0 ? "L " : "R ") + juce::String (juce::roundToInt (std::abs (v) * 100));
    else                vs = std::abs (v) < 0.005f ? juce::String ("NEUTRAL") : (v < 0 ? "DARKER " : "BRIGHTER ") + juce::String (juce::roundToInt (std::abs (v) * 100)) + "%";
    g->setFont (mono (10.5f));
    g->setColour (cLabel);
    txt (x, y, w, 14, label, juce::Justification::centredLeft);
    g->setFont (mono (12.0f, true));
    g->setColour (cText);
    txt (x, y, w, 14, vs, juce::Justification::centredRight);
    const float ty = y + 20;
    g->setColour (cTrack);
    g->fillRect (x, ty, w, 2.0f);
    g->setColour (acc);
    if (mn < 0) g->fillRect (x + std::min (n, 0.5f) * w, ty, std::abs (n - 0.5f) * w, 2.0f);
    else        g->fillRect (x, ty, n * w, 2.0f);
    g->setColour ((act == 500 + id || (hov && act == 0)) ? cText : acc);
    g->fillRect (x + n * w - 1.5f, ty - 5, 3.0f, 12.0f);
}

void EchoGridEditor::ficon (int t, float x, float y, float w, float h)
{
    if (g == nullptr) return;
    auto ln = [&] (float ax, float ay, float bx, float by) { g->drawLine (x + ax * w, y + ay * h, x + bx * w, y + by * h, 1.3f); };
    switch (t)
    {
        case 0: ln (0.12f, 0.35f, 0.55f, 0.35f); ln (0.55f, 0.35f, 0.66f, 0.22f); ln (0.66f, 0.22f, 0.88f, 0.85f); break;
        case 1: ln (0.12f, 0.85f, 0.34f, 0.22f); ln (0.34f, 0.22f, 0.45f, 0.35f); ln (0.45f, 0.35f, 0.88f, 0.35f); break;
        case 2: ln (0.12f, 0.85f, 0.5f, 0.2f); ln (0.5f, 0.2f, 0.88f, 0.85f); break;
        case 3: ln (0.12f, 0.3f, 0.42f, 0.3f); ln (0.42f, 0.3f, 0.5f, 0.85f); ln (0.5f, 0.85f, 0.58f, 0.3f); ln (0.58f, 0.3f, 0.88f, 0.3f); break;
        case 4: ln (0.12f, 0.62f, 0.38f, 0.62f); ln (0.38f, 0.62f, 0.5f, 0.15f); ln (0.5f, 0.15f, 0.62f, 0.62f); ln (0.62f, 0.62f, 0.88f, 0.62f); break;
        case 5: ln (0.12f, 0.25f, 0.36f, 0.25f); ln (0.36f, 0.25f, 0.6f, 0.68f); ln (0.6f, 0.68f, 0.88f, 0.68f); break;
        default: ln (0.12f, 0.68f, 0.4f, 0.68f); ln (0.4f, 0.68f, 0.64f, 0.25f); ln (0.64f, 0.25f, 0.88f, 0.25f); break;
    }
}

void EchoGridEditor::stem (float xx, float lv, juce::Colour c, float gy, float gh)
{
    if (g == nullptr) return;
    const float yy = gy + 8 + (1 - lv) * (gh - 8);
    g->setColour (c.withAlpha (0.85f));
    g->fillRect (xx - 1.0f, yy, 2.0f, gy + gh - yy);
    g->setColour (c);
    g->fillEllipse (xx - 5, yy - 5, 10, 10);
}

void EchoGridEditor::openSizeMenu (R area)
{
    juce::PopupMenu m;
    const int pcts[] = { 60, 75, 90, 100, 125, 150 };
    const int cur = juce::roundToInt (uiScale() * 100.0f);
    for (int i = 0; i < 6; ++i) m.addItem (i + 1, juce::String (pcts[i]) + "%", true, std::abs (cur - pcts[i]) <= 1);
    juce::Component::SafePointer<EchoGridEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                               .withTargetScreenArea (localAreaToGlobal ((area * uiScale()).toNearestInt())),
                     [safe, pcts] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         const float f = pcts[r - 1] / 100.0f;
                         safe->setSize (juce::roundToInt (W * f), juce::roundToInt (H * f));
                     });
}

//==============================================================================
// mouse
void EchoGridEditor::updateMouse (const juce::MouseEvent& e)
{
    const auto p = e.position / uiScale();
    mx = p.x; my = p.y;
    shift = e.mods.isShiftDown();
    ctrl = e.mods.isCtrlDown() || e.mods.isCommandDown();
}

void EchoGridEditor::mouseMove (const juce::MouseEvent& e) { updateMouse (e); pmx = mx; repaint(); }
void EchoGridEditor::mouseExit (const juce::MouseEvent&)   { mx = my = -1000; repaint(); }

void EchoGridEditor::mouseDown (const juce::MouseEvent& e)
{
    updateMouse (e);
    pmx = mx; dmx = 0;
    if (e.mods.isPopupMenu()) rclick = true;
    else { click = true; lb = true; dbl = e.getNumberOfClicks() >= 2; }
    frame();
    click = rclick = dbl = false;
    repaint();
}

void EchoGridEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! lb) return;
    updateMouse (e);
    dmx = mx - pmx; pmx = mx;
    frame();
    dmx = 0;
    repaint();
}

void EchoGridEditor::mouseUp (const juce::MouseEvent& e)
{
    updateMouse (e);
    lb = false;
    if (gestureParam > 0) { proc.param (gestureParam)->endChangeGesture(); gestureParam = 0; }
    act = 0;
    if (dirty) { proc.updateHostDisplay(); dirty = false; }
    repaint();
}

void EchoGridEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) < 1.0e-4f) return;
    updateMouse (e);
    wheel = w.deltaY > 0 ? 1.0f : -1.0f;
    frame();
    wheel = 0;
    repaint();
}

//==============================================================================
void EchoGridEditor::paint (juce::Graphics& gr)
{
    gr.fillAll (cBg);
    gr.addTransform (juce::AffineTransform::scale (uiScale()));
    g = &gr;
    const bool c = click, rc = rclick;
    click = rclick = false;   // drawing never consumes input
    const float wh = wheel; wheel = 0;
    frame();
    click = c; rclick = rc; wheel = wh;
    g = nullptr;
}

void EchoGridEditor::frame()
{
    // ---- derived values for this frame
    srate = proc.engine.getSrate();
    {
        double slv[eg::NS + 1] = {}, ev[eg::NS + 1] = {};
        for (int s = 1; s <= eg::NS; ++s) { slv[s] = raw (s); ev[s] = eff (s); }
        tm = eg::calcTimes (slv, ev, proc.hostTempo.load(), proc.hostNum.load(), proc.hostDen.load(),
                            srate, proc.engine.getMaxmod(), proc.engine.getMaxd());
    }
    const int gpb = ri (1), gmd = ri (4), gu = ri (5), gtyp = ri (6), gfxa = ri (14);
    const bool frozen = raw (18) >= 0.5f;
    const double tempo = proc.hostTempo.load();

    // ---------------- header ----------------
    if (g != nullptr)
    {
        g->setColour (cPanel);
        g->fillRoundedRectangle (16, 10, 1148, 40, 7.0f);
        g->setFont (mono (24.0f, true));
        g->setColour (cText);
        txt (30, 10, 80, 40, "ECHO", juce::Justification::centredLeft);
        g->setColour (cOrange);
        txt (30 + 58, 10, 80, 40, "GRID", juce::Justification::centredLeft);
        g->setFont (mono (11.0f));
        g->setColour (cLabel);
        txt (160, 12, 200, 40, "SEQUENCER DELAY", juce::Justification::centredLeft);
    }
    {
        const R sz (330, 19, 96, 22);
        if (pill (sz.getX(), sz.getY(), sz.getWidth(), sz.getHeight(), "SIZE " + juce::String (juce::roundToInt (uiScale() * 100)) + "%", false, cBlue))
            openSizeMenu (sz);
    }
    {
        juce::String st; juce::Colour sc = cTeal;
        if (frozen)                       { st = "FROZEN"; sc = cOrange; }
        else if (proc.engine.clipping())  { st = "CLIP"; sc = cRed; }
        else if (gmd == 1)
            st = "SEQUENCER" + dot + juce::String (proc.engine.pat.nh) + " TAPS" + dot
               + (gu == 0 ? juce::String (eg::lenName (ri (11))) : juce::String (juce::roundToInt (eff (13))) + " ms");
        else
            st = "DELAY" + dot + (gu == 0 ? juce::String (eg::divName (ri (9))) + dot + juce::String (juce::roundToInt (tempo)) + " BPM"
                                          : juce::String (juce::roundToInt (eff (10))) + " ms");
        if (g != nullptr)
        {
            const float pw = st.length() * 6.6f + 26.0f;
            g->setColour (sc);
            g->fillRoundedRectangle (1150 - pw, 19, pw, 22, 11);
            g->setColour (juce::Colours::white);
            g->setFont (mono (11.0f, true));
            txt (1150 - pw, 19, pw, 22, st, juce::Justification::centred);
        }
    }

    // ---------------- row A ----------------
    panel (16, 58, 330, 118, "PLAYBACK MODE", cOrange);
    if (pill (30, 88, 98, 26, "NORMAL", gpb == 0, cOrange))     setv (1, 0);
    if (pill (132, 88, 98, 26, "DUAL ECHO", gpb == 1, cOrange)) setv (1, 1);
    if (pill (234, 88, 98, 26, "PING-PONG", gpb == 2, cOrange)) setv (1, 2);
    sl (2, 30, 128, 142, cOrange); if (gpb == 0) dim (24, 126, 154, 32);
    sl (3, 190, 128, 142, cOrange); if (gpb != 1 || gmd == 1) dim (184, 126, 154, 32);

    panel (356, 58, 270, 118, "MODE", cBlue);
    if (pill (370, 88, 119, 26, "NORMAL", gmd == 0, cBlue))    setv (4, 0);
    if (pill (493, 88, 119, 26, "SEQUENCER", gmd == 1, cBlue)) setv (4, 1);
    if (g != nullptr) { g->setFont (mono (10.5f)); g->setColour (cLabel); }
    txt (370, 120, 200, 12, "TIME UNIT", juce::Justification::centredLeft);
    if (pill (370, 136, 119, 26, "BEAT DIV.", gu == 0, cBlue)) setv (5, 0);
    if (pill (493, 136, 119, 26, "MS (FREE)", gu == 1, cBlue)) setv (5, 1);

    panel (636, 58, 300, 118, "FX AFFECT", cPurple);
    if (pill (650, 88, 88, 26, "EVERYTHING", gfxa == 0, cPurple, 10.0f))  setv (14, 0);
    if (pill (742, 88, 92, 26, "ONLY ECHOES", gfxa == 1, cPurple, 10.0f)) setv (14, 1);
    if (pill (838, 88, 84, 26, "IN LOOP", gfxa == 2, cPurple, 10.0f))     setv (14, 2);
    if (g != nullptr)
    {
        g->setFont (mono (10.5f)); g->setColour (cLabel);
        const char* a = gfxa == 0 ? "Filter, phaser & flanger process" : gfxa == 1 ? "FX process only the echoes -" : "FX sit inside the feedback loop:";
        const char* b = gfxa == 0 ? "the dry signal and the echoes together." : gfxa == 1 ? "the dry signal stays clean." : "every repeat gets processed again.";
        txt (650, 126, 272, 14, a, juce::Justification::centredLeft);
        txt (650, 142, 272, 14, b, juce::Justification::centredLeft);
    }

    panel (946, 58, 218, 118, "OUTPUT", cGreen);
    sl (15, 960, 82, 190, cGreen);
    sl (16, 960, 112, 190, cGreen);
    sl (19, 960, 142, 190, cGreen);

    // ---------------- row B: type ----------------
    panel (16, 186, 330, 252, "TYPE", cOrange);
    for (int i = 0; i < 8; ++i)
        if (pill (30.0f + (i % 2) * 153, 214.0f + (i / 2) * 29, 148, 24, eg::typeName (i), gtyp == i, cOrange)) setv (6, i);
    sl (8, 30, 336, 302, cOrange);
    sl (7, 30, 368, 302, cOrange);
    if (pill (30, 404, 302, 24, frozen ? "FROZEN  (CLICK TO RELEASE)" : "FREEZE", frozen, cOrange)) setv (18, frozen ? 0 : 1);

    gridSection();
    fxSection();
    macroSection();
}

//==============================================================================
void EchoGridEditor::gridSection()
{
    const int gpb = ri (1), gmd = ri (4), gu = ri (5);
    auto& pat = proc.engine.pat;
    const int nh = juce::jlimit (1, eg::MaxHandles, pat.nh);
    int hsel = juce::jlimit (0, nh - 1, proc.selected.load());

    panel (356, 186, 808, 252, gmd == 1 ? "GRID" + dot + "SEQUENCER" : "GRID" + dot + "CONVENTIONAL DELAY", cBlue);
    auto label = [this] { if (g != nullptr) { g->setFont (mono (10.5f, true)); g->setColour (cLabel); } };
    auto hint  = [this] { if (g != nullptr) { g->setFont (mono (10.5f)); g->setColour (cLabel); } };

    if (gmd == 0)
    {
        if (gu == 0)
        {
            label(); txt (370, 214, 40, 24, "TIME", juce::Justification::centredLeft);
            const int d = ri (9);
            if (pill (410, 214, 26, 24, "<", false, cBlue)) setv (9, std::max (0, d - 1));
            if (pill (440, 214, 92, 24, eg::divName (d), true, cBlue))
            {
                juce::PopupMenu m;
                for (int i = 0; i < 19; ++i) m.addItem (i + 1, eg::divName (i), true, i == d);
                juce::Component::SafePointer<EchoGridEditor> safe (this);
                m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r) { if (safe != nullptr && r > 0) { safe->setv (9, r - 1); safe->repaint(); } });
            }
            if (pill (536, 214, 26, 24, ">", false, cBlue)) setv (9, std::min (18, d + 1));
            if (inside (410, 214, 152, 24) && wheel != 0) { setv (9, juce::jlimit (0, 18, d + (wheel > 0 ? 1 : -1))); wheel = 0; }
            hint(); txt (572, 214, 100, 24, "= " + juce::String (juce::roundToInt (tm.T / srate * 1000)) + " ms", juce::Justification::centredLeft);
        }
        else sl (10, 370, 212, 300, cBlue);
        hint(); txt (700, 214, 450, 24, "Switch MODE to SEQUENCER to place every repeat yourself", juce::Justification::centredRight);
    }
    else
    {
        if (gu == 0)
        {
            label(); txt (370, 214, 48, 24, "LENGTH", juce::Justification::centredLeft);
            for (int i = 0; i < 4; ++i) if (pill (420.0f + i * 62, 214, 58, 24, eg::lenName (i), ri (11) == i, cBlue, 10.0f)) setv (11, i);
            label(); txt (676, 214, 36, 24, "SNAP", juce::Justification::centredLeft);
            for (int i = 0; i < 6; ++i) if (pill (712.0f + i * 46, 214, 42, 24, eg::resName (i), ri (12) == i, cBlue, 10.0f)) setv (12, i);
        }
        else
        {
            sl (13, 370, 212, 300, cBlue);
            hint(); txt (690, 214, 290, 24, "Free placement (no snap) in ms mode", juce::Justification::centredLeft);
        }
        if (pill (1000, 214, 74, 24, "RANDOM", false, cBlue)) proc.randomizePattern();
        if (pill (1078, 214, 72, 24, "RESET", false, cBlue))  proc.resetPattern();
    }

    const float gx = 370, gy = 250, gw = 780, gh = gmd == 1 ? 112.0f : 150.0f;
    if (g != nullptr) { g->setColour (cInset); g->fillRoundedRectangle (gx - 6, gy - 6, gw + 12, gh + 12, 6); }
    const juce::Colour colL = cBlue, colR = cTeal;

    if (gmd == 1)
    {
        // ----- grid lines -----
        if (g != nullptr)
        {
            if (gu == 0)
            {
                const int gn = std::max (1, (int) std::floor (tm.lenQ / tm.resQ + 0.5));
                for (int i = 0; i <= gn; ++i)
                {
                    const double q = i * tm.resQ;
                    const float xx = gx + (float) (q / tm.lenQ) * gw;
                    const bool bar = std::abs (q / tm.barq - std::floor (q / tm.barq + 0.5)) < 0.001;
                    const bool bt = std::abs (q - std::floor (q + 0.5)) < 0.001;
                    g->setColour (bar ? cLabel.withAlpha (0.8f) : bt ? juce::Colour (0xFFA3A39F) : cInset2);
                    g->fillRect (xx, gy, 1.0f, gh);
                    if (bt && i < gn)
                    {
                        const int b = (int) std::floor (q / tm.barq + 0.0001);
                        g->setFont (mono (10.0f)); g->setColour (cLabel);
                        txt (xx + 3, gy + gh + 6, 40, 12, juce::String (b + 1) + "." + juce::String ((int) std::floor (q - b * tm.barq + 0.5) + 1),
                             juce::Justification::centredLeft);
                    }
                }
            }
            else
            {
                for (int i = 0; i <= 16; ++i)
                {
                    const float xx = gx + i / 16.0f * gw;
                    g->setColour ((i % 4) == 0 ? juce::Colour (0xFFA3A39F) : cInset2);
                    g->fillRect (xx, gy, 1.0f, gh);
                    if ((i % 4) == 0 && i < 16)
                    {
                        g->setFont (mono (10.0f)); g->setColour (cLabel);
                        txt (xx + 3, gy + gh + 6, 60, 12, juce::String (juce::roundToInt (eff (13) * i / 16)) + " ms", juce::Justification::centredLeft);
                    }
                }
            }
            g->setColour (cBlue.withAlpha (0.6f));
            g->fillRect (gx + gw - 1, gy, 2.0f, gh);
            g->setFont (mono (10.0f)); g->setColour (cLabel);
            txt (gx + gw - 160, gy + gh + 6, 160, 12, "LOOP END (FEEDBACK)", juce::Justification::centredRight);
        }

        // ----- handles: hit test -----
        int ghov = -1;
        for (int i = 0; i < nh; ++i)
        {
            const float xx = gx + pat.time[(size_t) i] * gw, yy = gy + 8 + (1 - pat.lev[(size_t) i]) * (gh - 8);
            if ((std::abs (mx - xx) < 8 && std::abs (my - yy) < 8) || (std::abs (mx - xx) < 4 && my > yy && my < gy + gh)) ghov = i;
        }
        if (act >= 100 && act < 116) ghov = act - 100;
        if (click && act == 0 && inside (gx - 6, gy - 6, gw + 12, gh + 12))
        {
            click = false;
            if (ghov >= 0)
            {
                act = 100 + ghov; hsel = ghov; proc.selected = ghov;
                hgox = pat.time[(size_t) ghov] - (mx - gx) / gw;
                hgoy = pat.lev[(size_t) ghov] - (1 - (my - gy - 8) / (gh - 8));
            }
            else if (dbl)
            {
                const float t = snapf ((mx - gx) / gw), lv = 1 - (my - gy - 8) / (gh - 8);
                const juce::SpinLock::ScopedLockType l (proc.lock);
                if (pat.nh < eg::MaxHandles) { proc.engine.addHandle (t, lv); hsel = pat.nh - 1; proc.selected = hsel; }
                dirty = true;
            }
        }
        if (rclick && act == 0 && ghov >= 0)
        {
            rclick = false;
            { const juce::SpinLock::ScopedLockType l (proc.lock); proc.engine.delHandle (ghov); }
            if (hsel >= ghov) hsel = std::max (0, hsel - 1);
            proc.selected = hsel;
            ghov = -1; dirty = true;
        }
        if (act >= 100 && act < 116 && lb && g == nullptr)
        {
            const int i = act - 100;
            const juce::SpinLock::ScopedLockType l (proc.lock);
            pat.time[(size_t) i] = snapf ((mx - gx) / gw + hgox);
            pat.lev[(size_t) i] = juce::jlimit (0.0f, 1.0f, 1 - (my - gy - 8) / (gh - 8) + hgoy);
            dirty = true;
        }
        if (ghov >= 0 && wheel != 0 && act == 0)
        {
            const juce::SpinLock::ScopedLockType l (proc.lock);
            pat.lev[(size_t) ghov] = juce::jlimit (0.0f, 1.0f, pat.lev[(size_t) ghov] + wheel * 0.05f);
            wheel = 0; dirty = true;
        }

        // ----- handles: draw -----
        const int nh2 = juce::jlimit (1, eg::MaxHandles, pat.nh);
        hsel = juce::jlimit (0, nh2 - 1, hsel);
        if (g != nullptr)
        {
            for (int i = 0; i < nh2; ++i)
            {
                const size_t k = (size_t) i;
                const float xx = gx + pat.time[k] * gw, yy = gy + 8 + (1 - pat.lev[k]) * (gh - 8);
                const auto c = (gpb > 0 && proc.engine.tapSide (i) > 0) ? colR : colL;
                g->setColour (c.withAlpha (0.9f));
                g->fillRect (xx - 1.0f, yy, 2.0f, gy + gh - yy);
                const float glow = (float) std::min (1.0, proc.engine.tapEnv (i) * 3);
                if (glow > 0.02f) { g->setColour (c.withAlpha (0.28f * glow)); const float r = 7 + 9 * glow; g->fillEllipse (xx - r, yy - r, 2 * r, 2 * r); }
                if (i == hsel) { g->setColour (cText); g->drawEllipse (xx - 9, yy - 9, 18, 18, 1.5f); }
                g->setColour (i == ghov ? cText : c);
                g->fillEllipse (xx - 6, yy - 6, 12, 12);
                if (std::abs (pat.pan[k]) > 0.01f)
                {
                    g->setColour (cText.withAlpha (0.7f));
                    g->fillRect (xx + std::min (0.0f, pat.pan[k] * 10), yy + 9, std::max (1.0f, std::abs (pat.pan[k]) * 10), 2.0f);
                }
                if (i == ghov)
                {
                    g->setFont (mono (10.0f)); g->setColour (cText);
                    txt (std::min (gx + gw - 130, xx + 10), std::max (gy, yy - 18), 130, 12,
                         juce::String (juce::roundToInt (pat.lev[k] * 100)) + "%" + dot + juce::String (juce::roundToInt (pat.time[k] * tm.Lp / srate * 1000)) + " ms",
                         juce::Justification::centredLeft);
                }
            }
            if (gpb > 0)
            {
                g->setFont (mono (10.0f, true));
                g->setColour (colL); txt (gx + 6, gy + 2, 70, 12, "L ECHOES", juce::Justification::centredLeft);
                g->setColour (colR); txt (gx + 80, gy + 2, 70, 12, "R ECHOES", juce::Justification::centredLeft);
            }
        }

        // ----- selected repeat: level / pan / tone -----
        if (g != nullptr) { g->setFont (mono (11.0f, true)); g->setColour (cBlue); }
        txt (370, 384, 84, 26, "REPEAT " + juce::String (hsel + 1), juce::Justification::centredLeft);
        asl (1, "LEVEL", FLev, hsel, 460, 382, 206, 0, cBlue);
        asl (2, "PAN", FPan, hsel, 688, 382, 210, 1, cBlue);
        asl (3, "TONE", FTone, hsel, 920, 382, 230, 2, cBlue);
        hint();
        txt (370, 418, 780, 14, "Click: select" + dot + "Drag: time / level" + dot + "Double-click empty: add" + dot
                                + "Right-click: delete" + dot + "Wheel: level" + dot + "Ctrl-drag: no snap", juce::Justification::centredLeft);
    }
    else if (g != nullptr)
    {
        // ----- conventional mode: show the echo train (read only) -----
        const double gwin = std::max (tm.T, tm.TR) * 8.5;
        if (gu == 0)
        {
            const double step = gwin / tm.qs > 48 ? tm.barq * tm.qs : tm.qs;
            const int cnt = std::min (200, (int) std::floor (gwin / step) + 1);
            g->setColour (cInset2);
            for (int i = 0; i < cnt; ++i) g->fillRect (gx + (float) (i * step / gwin) * gw, gy, 1.0f, gh);
        }
        else
        {
            g->setColour (cInset2);
            for (int i = 0; i <= 16; ++i) g->fillRect (gx + i / 16.0f * gw, gy, 1.0f, gh);
        }
        const double fv = std::min (1.0, eff (8) * 0.01);
        for (int n = 1; n <= 8; ++n)
        {
            const float lv = (float) std::pow (fv, n - 1);
            const float xx = gx + (float) (n * tm.T / gwin) * gw;
            if (gpb == 2) stem (xx, lv, (n % 2) == 0 ? colR : colL, gy, gh);
            else
            {
                stem (xx, lv, colL, gy, gh);
                if (gpb == 1) stem (gx + (float) (n * tm.TR / gwin) * gw, lv, colR, gy, gh);
            }
        }
        if (gpb > 0)
        {
            g->setFont (mono (10.0f, true));
            g->setColour (colL); txt (gx + 6, gy + 2, 70, 12, "L ECHOES", juce::Justification::centredLeft);
            g->setColour (colR); txt (gx + 80, gy + 2, 70, 12, "R ECHOES", juce::Justification::centredLeft);
        }
        hint();
        txt (370, 418, 780, 14, "Showing 8 repeats" + dot + "window " + juce::String (gwin / srate, 2) + " s", juce::Justification::centredLeft);
    }
}

//==============================================================================
void EchoGridEditor::fxSection()
{
    // FILTER
    panel (16, 448, 280, 258, "FILTER", cTeal);
    onpill (20, 236, 454, cTeal);
    const bool fOn = raw (20) >= 0.5f;
    const int ft = ri (21);
    if (g != nullptr)
    {
        g->setColour (cInset);
        g->fillRoundedRectangle (30, 480, 252, 64, 6);
        g->setColour (cInset2);
        g->fillRect (34.0f, 512.0f, 244.0f, 1.0f);
        double c[5];
        eg::filterCoefs (ft, eff (22), eff (23), eff (24), srate, c);
        juce::Path p;
        for (int i = 0; i < 80; ++i)
        {
            const double fq = 20 * std::pow (1000.0, i / 79.0);
            const double w0 = 6.283185307179586 * std::min (fq, 0.49 * srate) / srate, c1 = std::cos (w0), c2 = std::cos (2 * w0);
            const double num = c[0] * c[0] + c[1] * c[1] + c[2] * c[2] + 2 * (c[0] * c[1] + c[1] * c[2]) * c1 + 2 * c[0] * c[2] * c2;
            const double den = 1 + c[3] * c[3] + c[4] * c[4] + 2 * (c[3] + c[3] * c[4]) * c1 + 2 * c[4] * c2;
            const double db = juce::jlimit (-24.0, 24.0, 10 * std::log10 (std::max (1e-12, num) / std::max (1e-12, den)));
            const float xx = 34 + i / 79.0f * 244, yy = 512 - (float) db / 24 * 27;
            if (i == 0) p.startNewSubPath (xx, yy); else p.lineTo (xx, yy);
        }
        g->setColour (fOn ? cTeal : cLabel);
        g->strokePath (p, juce::PathStrokeType (1.6f));
        g->setFont (mono (10.0f)); g->setColour (cLabel);
        txt (38, 483, 120, 12, eg::filterName (ft), juce::Justification::centredLeft);
    }
    for (int i = 0; i < 7; ++i)
    {
        const float xx = 30.0f + i * 36;
        const bool hv = inside (xx, 552, 34, 22), on = ft == i;
        if (g != nullptr)
        {
            g->setColour (on ? cTeal : hv && act == 0 ? cInset2 : cInset);
            g->fillRoundedRectangle (xx, 552, 34, 22, 6);
            g->setColour (on ? juce::Colours::white : cText);
            ficon (i, xx + 3, 554, 28, 18);
        }
        if (takeClick (hv)) setv (21, i);
    }
    sl (22, 30, 586, 252, cTeal);
    sl (23, 30, 618, 252, cTeal);
    sl (24, 30, 650, 252, cTeal); if (ft < 4) dim (24, 648, 264, 32);
    if (! fOn) dim (24, 478, 264, 206);

    // PHASER
    panel (306, 448, 210, 258, "PHASER", cPurple);
    onpill (25, 456, 454, cPurple);
    for (int i = 0; i < 6; ++i) sl (26 + i, 320, 482.0f + i * 36, 182, cPurple);
    if (raw (25) < 0.5f) dim (314, 478, 196, 220);

    // FLANGER
    panel (526, 448, 210, 258, "FLANGER", cBlue);
    onpill (32, 676, 454, cBlue);
    for (int i = 0; i < 6; ++i) sl (33 + i, 540, 482.0f + i * 36, 182, cBlue);
    if (raw (32) < 0.5f) dim (534, 478, 196, 220);

    // SHIFTER (always in the feedback loop)
    panel (746, 448, 180, 258, "SHIFTER", cOrange);
    onpill (39, 866, 454, cOrange);
    if (pill (760, 480, 74, 22, "PITCH", ri (40) == 0, cOrange)) setv (40, 0);
    if (pill (838, 480, 74, 22, "FREQ", ri (40) == 1, cOrange))  setv (40, 1);
    if (ri (40) == 0) sl (41, 760, 512, 152, cOrange); else sl (42, 760, 512, 152, cOrange);
    sl (43, 760, 548, 152, cOrange);
    sl (44, 760, 584, 152, cOrange);
    sl (45, 760, 620, 152, cOrange);
    sl (46, 760, 656, 152, cOrange);
    if (g != nullptr) { g->setFont (mono (10.0f)); g->setColour (cLabel); }
    txt (760, 686, 152, 14, "Every repeat shifts again", juce::Justification::centredLeft);
    if (raw (39) < 0.5f) dim (754, 506, 166, 196);
}

//==============================================================================
void EchoGridEditor::macroSection()
{
    panel (936, 448, 228, 258, "MACRO", cPurple);
    double gmv = raw (17) * 0.01;
    if (g != nullptr) { g->setFont (mono (12.0f, true)); g->setColour (cPurple); }
    txt (950, 456, 180, 14, juce::String (juce::roundToInt (raw (17))) + "%", juce::Justification::centredRight);

    const float bx = 950, by = 482, bw = 200;
    const bool hv = inside (bx - 6, by - 8, bw + 12, 28);
    if (takeClick (hv))
    {
        if (dbl) setv (17, 0);
        else { act = 17; dragn = gmv; gestureParam = 17; proc.param (17)->beginChangeGesture(); }
    }
    if (act == 17 && lb && dmx != 0)
    {
        dragn = juce::jlimit (0.0, 1.0, dragn + dmx / bw * (shift ? 0.12 : 1.0));
        setv (17, dragn * 100); gmv = dragn;
    }
    if (hv && wheel != 0 && act == 0)
    {
        setv (17, juce::jlimit (0.0, 100.0, raw (17) + wheel * (shift ? 0.2 : 2.0)));
        gmv = raw (17) * 0.01; wheel = 0;
    }
    if (g != nullptr)
    {
        g->setColour (cInset);
        g->fillRoundedRectangle (bx, by, bw, 12, 6);
        g->setColour (cPurple);
        g->fillRoundedRectangle (bx, by, std::max (12.0f, (float) gmv * bw), 12, 6);
        g->setColour (juce::Colours::white);
        g->fillEllipse (bx + (float) gmv * bw - 7, by - 1, 14, 14);
        g->setColour (cPurple);
        g->drawEllipse (bx + (float) gmv * bw - 7, by - 1, 14, 14, 2.0f);
        g->setFont (mono (10.5f, true)); g->setColour (cLabel);
    }
    txt (950, 508, 200, 12, "ASSIGNED", juce::Justification::centredLeft);

    int gc = 0;
    for (int s = 1; s <= eg::NS; ++s)
    {
        if (! proc.mOn[(size_t) s].load()) continue;
        if (gc < 8)
        {
            const float ry = 526.0f + gc * 21, rx = 1032, rw = 106;
            float lo = proc.mLo[(size_t) s].load(), hi = proc.mHi[(size_t) s].load();
            if (g != nullptr) { g->setFont (mono (10.5f)); g->setColour (cText); }
            txt (950, ry, 80, 14, eg::meta (s).macName, juce::Justification::centredLeft);

            // remove button first so its click isn't taken by the range
            const bool xh = inside (1144, ry, 16, 16);
            if (g != nullptr)
            {
                g->setColour (xh && act == 0 ? cInset2 : cInset);
                g->fillEllipse (1144, ry, 16, 16);
                g->setColour (cText);
                g->drawLine (1148.5f, ry + 4.5f, 1155.5f, ry + 11.5f, 1.4f);
                g->drawLine (1155.5f, ry + 4.5f, 1148.5f, ry + 11.5f, 1.4f);
            }
            if (takeClick (xh)) { proc.macroRemove (s); continue; }

            if (takeClick (inside (rx - 6, ry - 2, rw + 12, 18)))
                act = std::abs (mx - (rx + lo * rw)) <= std::abs (mx - (rx + hi * rw)) ? 300 + s : 400 + s;
            if (act == 300 + s && lb) proc.mLo[(size_t) s] = juce::jlimit (0.0f, 1.0f, (mx - rx) / rw);
            if (act == 400 + s && lb) proc.mHi[(size_t) s] = juce::jlimit (0.0f, 1.0f, (mx - rx) / rw);
            if (inside (950, ry - 2, 190, 18) && rclick && act == 0)
            {
                rclick = false;
                juce::PopupMenu m;
                m.addItem (1, "Invert range"); m.addItem (2, "Full range"); m.addItem (3, "Remove from Macro");
                juce::Component::SafePointer<EchoGridEditor> safe (this);
                m.showMenuAsync (juce::PopupMenu::Options(), [safe, s] (int r)
                {
                    if (safe == nullptr) return;
                    auto& pr = safe->proc;
                    if (r == 1) { const float t = pr.mLo[(size_t) s]; pr.mLo[(size_t) s] = pr.mHi[(size_t) s].load(); pr.mHi[(size_t) s] = t; }
                    if (r == 2) { pr.mLo[(size_t) s] = 0.0f; pr.mHi[(size_t) s] = 1.0f; }
                    if (r == 3) pr.macroRemove (s);
                    safe->repaint();
                });
            }
            lo = proc.mLo[(size_t) s].load(); hi = proc.mHi[(size_t) s].load();
            if (g != nullptr)
            {
                g->setColour (cTrack);
                g->fillRoundedRectangle (rx, ry + 5, rw, 4, 2);
                g->setColour (cPurple.withAlpha (0.8f));
                g->fillRoundedRectangle (rx + std::min (lo, hi) * rw, ry + 5, std::max (4.0f, std::abs (hi - lo) * rw), 4, 2);
                g->setColour (cLabel);
                g->fillEllipse (rx + lo * rw - 4.5f, ry + 2.5f, 9, 9);
                g->setColour (cPurple);
                g->fillEllipse (rx + hi * rw - 4.5f, ry + 2.5f, 9, 9);
                const float pos = lo + (hi - lo) * (float) gmv;
                g->setColour (cText);
                g->fillRect (rx + pos * rw - 1, ry + 1, 2.0f, 12.0f);
            }
        }
        ++gc;
    }
    if (g != nullptr) { g->setFont (mono (10.5f)); g->setColour (cLabel); }
    if (gc == 0)
    {
        txt (950, 530, 200, 14, "Right-click any slider and", juce::Justification::centredLeft);
        txt (950, 545, 200, 14, "choose 'Assign to Macro'.", juce::Justification::centredLeft);
        txt (950, 568, 200, 14, "Then drag the grey (start) and", juce::Justification::centredLeft);
        txt (950, 583, 200, 14, "purple (end) dots to set range.", juce::Justification::centredLeft);
    }
    if (gc > 8) txt (950, 694, 200, 12, "+ " + juce::String (gc - 8) + " more", juce::Justification::centredLeft);
}
