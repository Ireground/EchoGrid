#pragma once
// EchoGrid parameter metadata (indices match the original JSFX slider numbers 1..46)
#include <cmath>
#include <algorithm>

namespace eg
{
constexpr int NS = 46;

// unit: 0 %, 1 ms, 2 dB, 3 Hz, 4 plain, 5 deg, 6 int, 7 signed %, 8 semitones, 9 signed Hz
// lg:   0 linear, 1 log, 2 bipolar squared
struct PMeta { float mn, mx; int lg, unit; bool mac; float def; const char* label; const char* macName; };

inline const PMeta& meta (int s)
{
    struct Table { PMeta t[NS + 1] = {}; };
    static const Table tbl = []
    {
        Table T;
        auto& t = T.t;
        auto d = [&t] (int s, float mn, float mx, int lg, int unit, bool mac, float def, const char* l, const char* m)
        { t[s] = { mn, mx, lg, unit, mac, def, l, m }; };
        d (2, 0, 200, 0, 0, true, 100, "WIDTH", "Width");
        d (3, -50, 100, 0, 7, true, 50, "R OFFSET", "R Offset");
        d (7, 0, 200, 0, 0, true, 100, "CHARACTER", "Character");
        d (8, 0, 110, 0, 0, true, 40, "FEEDBACK", "Feedback");
        d (10, 1, 4000, 1, 1, true, 375, "TIME", "Time");
        d (13, 50, 4000, 1, 1, true, 1000, "LENGTH", "Grid Len");
        d (15, -60, 6, 0, 2, true, 0, "DRY", "Dry");
        d (16, -60, 6, 0, 2, true, -6, "WET", "Wet");
        d (17, 0, 100, 0, 0, false, 0, "MACRO", "Macro");
        d (19, 0, 100, 0, 0, true, 0, "DUCK", "Duck");
        d (22, 20, 20000, 1, 3, true, 2500, "CUTOFF", "Flt Cutoff");
        d (23, 0.3f, 12, 1, 4, true, 0.71f, "Q", "Flt Q");
        d (24, -18, 18, 0, 2, true, 0, "GAIN", "Flt Gain");
        d (26, 100, 8000, 1, 3, true, 800, "CUTOFF", "Ph Cutoff");
        d (27, 0.02f, 10, 1, 3, true, 0.3f, "RATE", "Ph Rate");
        d (28, 0, 100, 0, 0, true, 60, "DEPTH", "Ph Depth");
        d (29, 2, 12, 0, 6, false, 4, "ORDER", "Ph Order");
        d (30, 0, 180, 0, 5, true, 90, "SPREAD", "Ph Spread");
        d (31, 0, 100, 0, 0, true, 100, "MIX", "Ph Mix");
        d (33, 0.3f, 15, 1, 1, true, 2.5f, "DELAY", "Fl Delay");
        d (34, 0, 100, 0, 0, true, 70, "DEPTH", "Fl Depth");
        d (35, 0.02f, 10, 1, 3, true, 0.2f, "RATE", "Fl Rate");
        d (36, 0, 180, 0, 5, true, 90, "SPREAD", "Fl Spread");
        d (37, -95, 95, 0, 7, true, 40, "FEEDBACK", "Fl Fdbk");
        d (38, 0, 100, 0, 0, true, 100, "MIX", "Fl Mix");
        d (41, -24, 24, 0, 8, true, 12, "PITCH", "Sh Pitch");
        d (42, -1000, 1000, 2, 9, true, 50, "FREQUENCY", "Sh Freq");
        d (43, 0, 100, 0, 0, true, 0, "LFO DEPTH", "Sh LFO Dep");
        d (44, 0.02f, 10, 1, 3, true, 0.5f, "LFO RATE", "Sh LFO Rate");
        d (45, 0, 180, 0, 5, true, 0, "STEREO", "Sh Stereo");
        d (46, 0, 100, 0, 0, true, 100, "MIX", "Sh Mix");
        return T;
    }();
    return tbl.t[std::clamp (s, 0, NS)];
}

inline bool hasMeta (int s) { return meta (s).label != nullptr; }

// normalised 0..1 -> value
inline double dnm (int s, double n)
{
    const auto& m = meta (s);
    n = std::clamp (n, 0.0, 1.0);
    const double u = 2.0 * n - 1.0;
    if (m.lg == 2) return m.mx * u * std::abs (u);
    if (m.lg == 1) return m.mn * std::pow ((double) m.mx / m.mn, n);
    return m.mn + (m.mx - m.mn) * n;
}

// value -> normalised 0..1
inline double nrm (int s, double v)
{
    const auto& m = meta (s);
    double r;
    if (m.lg == 2)      r = 0.5 + 0.5 * (v < 0 ? -1.0 : 1.0) * std::sqrt (std::abs (v) / m.mx);
    else if (m.lg == 1) r = std::log (std::max (1.0e-9, v / m.mn)) / std::log ((double) m.mx / m.mn);
    else                r = (v - m.mn) / (m.mx - m.mn);
    return std::clamp (r, 0.0, 1.0);
}

// division tables
inline double divQuarters (int i)
{
    static const double t[] = { 0.0625, 1.0 / 12, 0.125, 1.0 / 6, 0.1875, 0.25, 1.0 / 3, 0.375, 0.5, 2.0 / 3,
                                0.75, 1, 4.0 / 3, 1.5, 2, 8.0 / 3, 3, 4, 8 };
    return t[std::clamp (i, 0, 18)];
}
inline const char* divName (int i)
{
    static const char* const t[] = { "1/64", "1/32T", "1/32", "1/16T", "1/32D", "1/16", "1/8T", "1/16D", "1/8", "1/4T",
                                     "1/8D", "1/4", "1/2T", "1/4D", "1/2", "1/1T", "1/2D", "1 BAR", "2 BARS" };
    return t[std::clamp (i, 0, 18)];
}
inline double resQuarters (int i)
{
    static const double t[] = { 1, 0.5, 0.25, 0.125, 1.0 / 3, 1.0 / 6 };
    return t[std::clamp (i, 0, 5)];
}
inline const char* resName (int i)
{
    static const char* const t[] = { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
    return t[std::clamp (i, 0, 5)];
}
inline const char* lenName (int i)
{
    static const char* const t[] = { "1 BEAT", "1/2 BAR", "1 BAR", "2 BARS" };
    return t[std::clamp (i, 0, 3)];
}
inline const char* typeName (int i)
{
    static const char* const t[] = { "DIGITAL", "ANALOG BBD", "STUDIO TAPE", "CHEAP TAPE", "SPACE ECHO", "DARK BBD", "LO-FI", "AMBIENT" };
    return t[std::clamp (i, 0, 7)];
}
inline const char* filterName (int i)
{
    static const char* const t[] = { "LOW PASS", "HIGH PASS", "BAND PASS", "NOTCH", "PEAK", "LOW SHELF", "HIGH SHELF" };
    return t[std::clamp (i, 0, 6)];
}

// RBJ filter coefficients (b0 b1 b2 a1 a2), shared by DSP and the GUI curve
inline void filterCoefs (int ft, double fc, double fq, double gainDb, double srate, double* c)
{
    const double TWO_PI = 6.283185307179586;
    fc = std::max (20.0, std::min (fc, 0.45 * srate)); fq = std::max (0.1, fq);
    const double w0 = TWO_PI * fc / srate, cw = std::cos (w0), sw = std::sin (w0), al = sw / (2 * fq);
    const double A = std::pow (10.0, gainDb / 40.0);
    double b0, b1, b2, a0, a1, a2, sa;
    switch (ft)
    {
        case 0:  b0 = (1 - cw) * 0.5; b1 = 1 - cw; b2 = b0; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case 1:  b0 = (1 + cw) * 0.5; b1 = -(1 + cw); b2 = b0; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case 2:  b0 = al; b1 = 0; b2 = -al; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case 3:  b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
        case 4:  b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A; break;
        case 5:
            sa = 2 * std::sqrt (A) * al;
            b0 = A * ((A + 1) - (A - 1) * cw + sa); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - sa);
            a0 = (A + 1) + (A - 1) * cw + sa; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - sa;
            break;
        default:
            sa = 2 * std::sqrt (A) * al;
            b0 = A * ((A + 1) + (A - 1) * cw + sa); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - sa);
            a0 = (A + 1) - (A - 1) * cw + sa; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - sa;
            break;
    }
    c[0] = b0 / a0; c[1] = b1 / a0; c[2] = b2 / a0; c[3] = a1 / a0; c[4] = a2 / a0;
}

// timing derived from the parameters (shared by DSP and GUI)
struct Times { double T, TR, Lp, lenQ, resQ, barq, qs; };
inline Times calcTimes (const double* sl, const double* eff, double tempo, int tsNum, int tsDen, double srate, double maxmod, int maxd)
{
    Times t;
    const double bpm = tempo > 0 ? tempo : 120;
    t.qs = 60.0 / bpm * srate;
    t.barq = (tsNum > 0 && tsDen > 0) ? tsNum * 4.0 / tsDen : 4.0;
    const int u = (int) sl[5];
    const int ii = std::clamp ((int) sl[9], 0, 18);
    const double dq = ii == 17 ? t.barq : ii == 18 ? 2 * t.barq : divQuarters (ii);
    const int li = std::clamp ((int) sl[11], 0, 3);
    t.lenQ = li == 0 ? 1 : li == 1 ? t.barq * 0.5 : li == 2 ? t.barq : 2 * t.barq;
    t.resQ = resQuarters ((int) sl[12]);
    const double lim = maxd - 64 - maxmod;
    double T = u == 0 ? dq * t.qs : eff[10] * 0.001 * srate;
    t.T = std::max (16.0, std::min (lim, T));
    t.TR = (int) sl[1] == 1 ? std::max (16.0, std::min (lim, t.T * (1 + eff[3] * 0.01))) : t.T;
    double Lp = u == 0 ? t.lenQ * t.qs : eff[13] * 0.001 * srate;
    t.Lp = std::max (64.0, std::min (lim, Lp));
    return t;
}
} // namespace eg
