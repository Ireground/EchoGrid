#pragma once
// EchoGrid DSP - a direct C++ port of the EchoGrid JSFX (no JUCE dependency).
#include "Params.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

namespace eg
{
constexpr int MaxHandles = 16;

struct Pattern
{
    int nh = 4;
    std::array<float, MaxHandles> time {}, lev {}, pan {}, tone {};
    void reset()
    {
        time.fill (0); lev.fill (0); pan.fill (0); tone.fill (0);
        nh = 4;
        time[0] = 0.1875f; lev[0] = 0.9f; time[1] = 0.375f; lev[1] = 0.65f;
        time[2] = 0.5f;    lev[2] = 0.8f; time[3] = 0.75f;  lev[3] = 0.5f;
    }
    Pattern() { reset(); }
};

class Engine
{
public:
    static constexpr double TWO_PI = 6.283185307179586;

    // ---------------------------------------------------------------- setup
    void prepare (double sampleRate)
    {
        srate = sampleRate;
        MAXD = (int) std::floor (srate * 12) + 16;
        PSN = (int) std::floor (srate * 0.25);
        PW = (int) std::floor (srate * 0.06);
        FLN = (int) std::floor (srate * 0.04) + 8;
        bufL.assign ((size_t) MAXD, 0.0); bufR.assign ((size_t) MAXD, 0.0);
        psL.assign ((size_t) PSN, 0.0);   psR.assign ((size_t) PSN, 0.0);
        flL.assign ((size_t) FLN, 0.0);   flR.assign ((size_t) FLN, 0.0);
        difL.assign (8192, 0.0);          difR.assign (8192, 0.0);

        apn1 = std::max (8, (int) std::floor (142 * srate / 44100)); apn2 = std::max (8, (int) std::floor (107 * srate / 44100));
        apn3 = std::max (8, (int) std::floor (379 * srate / 44100)); apn4 = std::max (8, (int) std::floor (277 * srate / 44100));
        colL = {}; colR = {}; colL.base = difL.data(); colR.base = difR.data();
        wp = fwp = pwp = 0; pphL = pphR = oscL = oscR = 0;
        tlpL.fill (0); tlpR.fill (0);
        hilL = {}; hilR = {};
        phzL.fill (0); phzR.fill (0);
        bqL = {}; bqR = {};
        gsc = 1 - std::exp (-1 / (0.01 * srate));
        eatt = 1 - std::exp (-1 / (0.003 * srate));
        erel = std::exp (-1 / (0.25 * srate));
        envdec = std::exp (-1 / (0.12 * srate));
        drc = 1 - std::exp (-1 / (0.12 * srate));
        started = false; Ts = TRs = Ls = 0; maxmod = 16;
        tsm.fill (0); tenv.fill (0);
        env = 0; dgS = 1; wph = fph = 0; dcnt = 0; dtar = drift = 0; php = flp = shph = 0;
        fonS = ponS = flonS = shonS = 0;
        clipc = 0;
    }

    void clearBuffers()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0); std::fill (bufR.begin(), bufR.end(), 0.0);
        std::fill (psL.begin(), psL.end(), 0.0);   std::fill (psR.begin(), psR.end(), 0.0);
        std::fill (flL.begin(), flL.end(), 0.0);   std::fill (flR.begin(), flR.end(), 0.0);
        std::fill (difL.begin(), difL.end(), 0.0); std::fill (difR.begin(), difR.end(), 0.0);
    }

    // ---------------------------------------------------------------- pattern edits (call with the processor lock held)
    Pattern pat;

    void addHandle (float t, float lv)
    {
        if (pat.nh >= MaxHandles) return;
        const int i = pat.nh;
        pat.time[(size_t) i] = t; pat.lev[(size_t) i] = std::clamp (lv, 0.05f, 1.0f);
        pat.pan[(size_t) i] = 0; pat.tone[(size_t) i] = 0;
        ttg[(size_t) i] = t * Lp; tsm[(size_t) i] = ttg[(size_t) i]; tenv[(size_t) i] = 0; tlpL[(size_t) i] = tlpR[(size_t) i] = 0;
        pat.nh++;
    }
    void delHandle (int ix)
    {
        if (pat.nh <= 1 || ix < 0 || ix >= pat.nh) return;
        for (int j = ix; j < pat.nh - 1; ++j)
        {
            const size_t a = (size_t) j, b = (size_t) j + 1;
            pat.time[a] = pat.time[b]; pat.lev[a] = pat.lev[b]; pat.pan[a] = pat.pan[b]; pat.tone[a] = pat.tone[b];
            tsm[a] = tsm[b]; ttg[a] = ttg[b]; tenv[a] = tenv[b]; tlpL[a] = tlpL[b]; tlpR[a] = tlpR[b]; tcfa[a] = tcfa[b];
        }
        pat.nh--;
    }
    void syncSmoothing()   // after reset / randomize: jump taps to their targets
    {
        tsm.fill (0); tenv.fill (0);   // next block starts every tap at its target
    }

    // ---------------------------------------------------------------- per block
    // sl: raw slider values [1..46], eff: values after macro [1..46]
    void block (const double* sl, const double* eff, double tempo, int tsNum, int tsDen, bool playing, int nsamples)
    {
        typ = std::clamp ((int) sl[6], 0, 7);
        const double* tb = typeTable (typ);
        const double chr = eff[7] * 0.01;
        double lpHz = typ == 0 ? 18000 : tb[0] * std::pow (2.0, 1 - chr);
        lpHz = std::max (200.0, std::min (lpHz, std::min (20000.0, 0.45 * srate)));
        double gg = std::tan (kPi * lpHz / srate);
        lK = 1 / tb[2]; lA1 = 1 / (1 + gg * (gg + lK)); lA2 = gg * lA1; lA3 = gg * lA2;
        gg = std::tan (kPi * tb[1] / srate);
        hK = 1.4142136; hA1 = 1 / (1 + gg * (gg + hK)); hA2 = gg * hA1; hA3 = gg * hA2;
        drvE = tb[3] > 0 ? 0.35 * (1 + (tb[3] - 1) * chr) : 0;
        wowD = tb[4] * 0.001 * srate * chr; wowInc = TWO_PI * tb[5] / srate;
        flD = tb[6] * 0.001 * srate * chr;  flInc = TWO_PI * tb[7] / srate;
        nAmt = tb[8] * chr; sph = tb[9];
        crB = tb[10] > 0 ? std::max (4.0, std::min (16.0, std::floor (tb[10] - 3 * (chr - 1)))) : 0;
        crQ = std::pow (2.0, crB - 1);
        crR = tb[11] > 0 ? std::min (1.0, tb[11] / (0.5 + 0.5 * chr) / srate) : 1;
        dfG = tb[12] > 0 ? std::min (0.75, 0.45 + 0.15 * chr) : 0;
        maxmod = 1.6 * wowD + flD + 16;
        tcoef = typ == 0 ? 1 - std::exp (-1 / (0.04 * srate)) : 1 - std::exp (-1 / (0.18 * srate));

        const auto t = calcTimes (sl, eff, tempo, tsNum, tsDen, srate, maxmod, MAXD);
        T = t.T; TR = t.TR; Lp = t.Lp;
        pb = (int) sl[1]; md = (int) sl[4]; fxa = (int) sl[14];
        width = eff[2] * 0.01;
        fbT = eff[8] * 0.01;
        dryT = eff[15] <= -59.9 ? 0 : std::pow (10.0, eff[15] / 20);
        wetT = eff[16] <= -59.9 ? 0 : std::pow (10.0, eff[16] / 20);
        frzT = sl[18] >= 0.5 ? 1 : 0;
        duckA = eff[19] * 0.01;

        pat.nh = std::clamp (pat.nh, 1, MaxHandles);
        const int nh = pat.nh;
        for (int i = 0; i < nh; ++i)
        {
            const size_t bi = (size_t) i;
            ttg[bi] = std::max (16.0, std::min (Lp, (double) pat.time[bi] * Lp));
            if (tsm[bi] <= 0) tsm[bi] = ttg[bi];
            int br = 0;
            for (int j = 0; j < nh; ++j)
                if (pat.time[(size_t) j] < pat.time[bi] || (pat.time[(size_t) j] == pat.time[bi] && j < i)) ++br;
            tpan[bi] = (br % 2) == 0 ? -1 : 1;
            const double btn = std::clamp ((double) pat.tone[bi], -1.0, 1.0);
            const double bfc = btn <= 0 ? std::min (0.45 * srate, 20000 * std::pow (2.0, 7 * btn)) : 30 * std::pow (2.0, 6.5 * btn);
            tcfa[bi] = 1 - std::exp (-TWO_PI * bfc / srate);
        }

        double c[5];
        filterCoefs ((int) sl[21], eff[22], eff[23], eff[24], srate, c);
        fb0 = c[0]; fb1 = c[1]; fb2 = c[2]; fa1 = c[3]; fa2 = c[4];
        fonT = sl[20] >= 0.5 ? 1 : 0;
        ponT = sl[25] >= 0.5 ? 1 : 0; phC = eff[26]; phInc = TWO_PI * eff[27] / srate; phD = eff[28] * 0.01;
        phN = std::max (2, std::min (12, 2 * (int) std::floor (eff[29] * 0.5 + 0.5))); phS = eff[30] * kPi / 180; phM = eff[31] * 0.01;
        flonT = sl[32] >= 0.5 ? 1 : 0; fdl = eff[33] * 0.001 * srate; fdep = eff[34] * 0.01; fInc = TWO_PI * eff[35] / srate;
        fSp = eff[36] * kPi / 180; ffb = eff[37] * 0.01; fmx = eff[38] * 0.01;
        shonT = sl[39] >= 0.5 ? 1 : 0; shMode = (int) sl[40]; shSemi = eff[41]; shFreq = eff[42]; shDep = eff[43] * 0.01;
        shInc = TWO_PI * eff[44] / srate; shSt = eff[45] * kPi / 180; shMix = eff[46] * 0.01;

        if (Ts <= 0) Ts = T;
        if (TRs <= 0) TRs = TR;
        if (Ls <= 0) Ls = Lp;
        if (! started) { dryS = dryT; wetS = wetT; fbS = fbT; frS = frzT; dgS = 1; started = true; }
        if (playing && ! lastPlaying) clearBuffers();
        lastPlaying = playing;
        clipc = std::max (0, clipc - nsamples);
    }

    // ---------------------------------------------------------------- per sample
    inline void process (float& spl0, float& spl1)
    {
        const double inL = spl0, inR = spl1, mono = 0.5 * (inL + inR);
        dryS += (dryT - dryS) * gsc; wetS += (wetT - wetS) * gsc; fbS += (fbT - fbS) * gsc; frS += (frzT - frS) * gsc;
        fonS += (fonT - fonS) * gsc; ponS += (ponT - ponS) * gsc; flonS += (flonT - flonS) * gsc; shonS += (shonT - shonS) * gsc;
        const double fbE = fbS * (1 - frS) + frS, inG = 1 - frS;

        // ducking envelope
        const double ea = std::abs (mono);
        env = ea > env ? env + (ea - env) * eatt : env * erel;
        dgS += ((1 - duckA * std::min (1.0, env * 4)) - dgS) * gsc;

        // wow / flutter / drift
        wph += wowInc; if (wph >= TWO_PI) wph -= TWO_PI;
        fph += flInc;  if (fph >= TWO_PI) fph -= TWO_PI;
        if (--dcnt <= 0) { dtar = (rnd() * 2 - 1) * wowD * 0.6; dcnt = srate * 0.3; }
        drift += (dtar - drift) * drc;
        const double modsc = 1 - frS;
        const double mL = (wowD * std::sin (wph) + flD * std::sin (fph) + drift) * modsc;
        const double mR = (wowD * std::sin (wph + sph) + flD * std::sin (fph + sph * 1.7) + drift) * modsc;

        double rL, rR, wL, wR;
        if (md == 0)
        {
            Ts += (T - Ts) * tcoef; TRs += (TR - TRs) * tcoef;
            rL = hread (bufL, Ts + mL); rR = hread (bufR, TRs + mR);
            wL = rL; wR = rR;
        }
        else
        {
            Ls += (Lp - Ls) * tcoef;
            rL = hread (bufL, Ls + mL); rR = hread (bufR, Ls + mR);
            wL = 0; wR = 0;
            for (int i = 0; i < pat.nh; ++i)
            {
                const size_t k = (size_t) i;
                tsm[k] += (ttg[k] - tsm[k]) * tcoef;
                double a = hread (bufL, tsm[k] + mL), b = hread (bufR, tsm[k] + mR);
                const double c = tcfa[k];
                tlpL[k] += c * (a - tlpL[k]); tlpR[k] += c * (b - tlpR[k]);
                if (pat.tone[k] <= 0) { a = tlpL[k]; b = tlpR[k]; } else { a -= tlpL[k]; b -= tlpR[k]; }
                const double g = pat.lev[k], p = tpan[k];
                double q = pat.pan[k], oL, oR;
                if (pb == 1)
                {
                    a = (a + b) * 0.5 * g; q = std::clamp (p + q, -1.0, 1.0);
                    oL = a * std::min (1.0, 1 - q); oR = a * std::min (1.0, 1 + q);
                }
                else
                {
                    if (pb == 2 && p > 0) { oL = b * g; oR = a * g; } else { oL = a * g; oR = b * g; }
                    oL *= std::min (1.0, 1 - q); oR *= std::min (1.0, 1 + q);
                }
                const double te = std::abs (oL) + std::abs (oR);
                tenv[k] = std::max (te, tenv[k] * envdec);
                wL += oL; wR += oR;
            }
        }

        // SHIFTER: always in the feedback path
        if (shonS > 0.0001) { shL = rL; shR = rR; doshift(); rL = shL; rR = shR; }

        double xL, xR;
        if (pb == 2) { xL = mono * inG + rR * fbE; xR = rL * fbE; }
        else         { xL = inL * inG + rL * fbE; xR = inR * inG + rR * fbE; }

        if (pb > 0) { const double mm = (wL + wR) * 0.5, ss = (wL - wR) * 0.5 * width; wL = mm + ss; wR = mm - ss; }

        if (fxa == 2) { fxL = xL; fxR = xR; dofx(); xL = fxL; xR = fxR; }
        const double cL = col (colL, xL), cR = col (colR, xR);
        xL = safe (cL + (xL - cL) * frS); xR = safe (cR + (xR - cR) * frS);
        bufL[(size_t) wp] = xL; bufR[(size_t) wp] = xR;
        if (++wp >= MAXD) wp = 0;

        const double wg = wetS * dgS; wL *= wg; wR *= wg;
        double o0, o1;
        if (fxa == 0)      { fxL = inL * dryS + wL; fxR = inR * dryS + wR; dofx(); o0 = fxL; o1 = fxR; }
        else if (fxa == 1) { fxL = wL; fxR = wR; dofx(); o0 = inL * dryS + fxL; o1 = inR * dryS + fxR; }
        else               { o0 = inL * dryS + wL; o1 = inR * dryS + wR; }
        if (std::max (std::abs (o0), std::abs (o1)) > 1) clipc = (int) srate;
        spl0 = (float) o0; spl1 = (float) o1;
    }

    // ---------------------------------------------------------------- read-outs for the GUI
    double getT() const  { return T; }
    double getTR() const { return TR; }
    double getLp() const { return Lp; }
    double getSrate() const { return srate; }
    double getMaxmod() const { return maxmod; }
    int    getMaxd() const { return MAXD; }
    bool   clipping() const { return clipc > 0; }
    double tapEnv (int i) const { return tenv[(size_t) std::clamp (i, 0, MaxHandles - 1)]; }
    double tapSide (int i) const { return tpan[(size_t) std::clamp (i, 0, MaxHandles - 1)]; }

private:
    static constexpr double kPi = 3.14159265358979323846;

    // delay types (Echoboy-inspired): lp, hp, q, drive, wowms, wowhz, flms, flhz, noise, stereo phase, bits, srr, diffusion
    static const double* typeTable (int t)
    {
        static const double tbl[8][13] = {
            { 18000, 10, 0.707, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
            { 3800, 60, 0.85, 1.6, 0.25, 0.5, 0.03, 4, 0.0005, 1.57, 0, 0, 0 },
            { 9000, 35, 0.6, 1.4, 0.3, 0.45, 0.05, 6.5, 0.00015, 0, 0, 0, 0 },
            { 4200, 140, 0.7, 2.4, 1.1, 0.7, 0.12, 8, 0.0012, 0.3, 0, 0, 0 },
            { 4800, 180, 1.3, 2.0, 0.6, 0.55, 0.08, 5, 0.0007, 0.2, 0, 0, 0 },
            { 2300, 80, 0.9, 1.8, 0.5, 0.9, 0, 0, 0.0008, 1.57, 0, 0, 0 },
            { 7500, 100, 0.7, 1.2, 0, 0, 0, 0, 0, 0, 10, 12000, 0 },
            { 7000, 120, 0.6, 1.0, 0.25, 0.3, 0, 0, 0.0001, 0.8, 0, 0, 1 } };
        return tbl[std::clamp (t, 0, 7)];
    }

    inline double rnd() { rng = rng * 1664525u + 1013904223u; return (double) (rng >> 8) * (1.0 / 16777216.0); }

    static inline double sat (double x) { return x > 3 ? 1 : x < -3 ? -1 : x * (27 + x * x) / (27 + 9 * x * x); }
    static inline double safe (double x)
    {
        const double a = std::abs (x);
        return a <= 1 ? x : (x < 0 ? -1.0 : 1.0) * (1 + 0.5 * sat ((a - 1) * 2));
    }

    inline double hread (const std::vector<double>& buf, double d) const
    {
        d = std::max (4.0, std::min ((double) MAXD - 8, d));
        double p = wp - d; if (p < 0) p += MAXD;
        int i0 = (int) std::floor (p); const double f = p - i0;
        if (i0 >= MAXD) i0 -= MAXD;
        int im = i0 - 1; if (im < 0) im += MAXD;
        int i1 = i0 + 1; if (i1 >= MAXD) i1 -= MAXD;
        int i2 = i0 + 2; if (i2 >= MAXD) i2 -= MAXD;
        const double xm = buf[(size_t) im], x0 = buf[(size_t) i0], x1 = buf[(size_t) i1], x2 = buf[(size_t) i2];
        const double c1 = 0.5 * (x1 - xm);
        const double c2 = xm - 2.5 * x0 + 2 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    static inline double lread (const std::vector<double>& buf, int n, int wpos, double d)
    {
        double p = wpos - d; if (p < 0) p += n;
        int i0 = (int) std::floor (p); const double f = p - i0;
        if (i0 >= n) i0 -= n;
        int i1 = i0 + 1; if (i1 >= n) i1 -= n;
        return buf[(size_t) i0] * (1 - f) + buf[(size_t) i1] * f;
    }

    struct BQ { double z1 = 0, z2 = 0; };
    inline double bq (BQ& s, double x) const
    {
        const double y = fb0 * x + s.z1;
        s.z1 = fb1 * x - fa1 * y + s.z2;
        s.z2 = fb2 * x - fa2 * y;
        return y;
    }

    struct Col { double l1 = 0, l2 = 0, h1 = 0, h2 = 0, hold = 0, cnt = 0; int p1 = 0, p2 = 0, p3 = 0, p4 = 0; double* base = nullptr; };
    inline double col (Col& c, double x)
    {
        double v3 = x - c.h2, v1 = hA1 * c.h1 + hA2 * v3, v2 = c.h2 + hA2 * c.h1 + hA3 * v3;
        c.h1 = 2 * v1 - c.h1; c.h2 = 2 * v2 - c.h2;
        double y = x - hK * v1 - v2;
        v3 = y - c.l2; v1 = lA1 * c.l1 + lA2 * v3; v2 = c.l2 + lA2 * c.l1 + lA3 * v3;
        c.l1 = 2 * v1 - c.l1; c.l2 = 2 * v2 - c.l2;
        y = v2;
        if (drvE > 0) y = sat (y * drvE) / drvE;
        if (crB > 0)
        {
            c.cnt += crR;
            if (c.cnt >= 1) { c.cnt -= 1; c.hold = std::floor (y * crQ + 0.5) / crQ; }
            y = c.hold;
        }
        if (dfG > 0)
        {
            double o, v;
            o = c.base[c.p1]; v = y - dfG * o; c.base[c.p1] = v; y = o + dfG * v; if (++c.p1 >= apn1) c.p1 = 0;
            o = c.base[2048 + c.p2]; v = y - dfG * o; c.base[2048 + c.p2] = v; y = o + dfG * v; if (++c.p2 >= apn2) c.p2 = 0;
            o = c.base[4096 + c.p3]; v = y - dfG * o; c.base[4096 + c.p3] = v; y = o + dfG * v; if (++c.p3 >= apn3) c.p3 = 0;
            o = c.base[6144 + c.p4]; v = y - dfG * o; c.base[6144 + c.p4] = v; y = o + dfG * v; if (++c.p4 >= apn4) c.p4 = 0;
        }
        return y + nAmt * (rnd() * 2 - 1);
    }

    // FX chain on fxL / fxR: Filter -> Phaser -> Flanger
    inline void dofx()
    {
        if (fonS > 0.0001)
        {
            double y = bq (bqL, fxL); fxL += (y - fxL) * fonS;
            y = bq (bqR, fxR); fxR += (y - fxR) * fonS;
        }
        if (ponS > 0.0001)
        {
            php += phInc; if (php >= TWO_PI) php -= TWO_PI;
            double f = std::max (20.0, std::min (0.45 * srate, phC * std::pow (2.0, phD * 2.5 * std::sin (php))));
            double t = std::tan (kPi * f / srate), a = (t - 1) / (t + 1);
            double y = fxL;
            for (int j = 0; j < phN; ++j) { const double z = phzL[(size_t) j], o = a * y + z; phzL[(size_t) j] = y - a * o; y = o; }
            fxL += (y - fxL) * 0.5 * phM * ponS;
            f = std::max (20.0, std::min (0.45 * srate, phC * std::pow (2.0, phD * 2.5 * std::sin (php + phS))));
            t = std::tan (kPi * f / srate); a = (t - 1) / (t + 1);
            y = fxR;
            for (int j = 0; j < phN; ++j) { const double z = phzR[(size_t) j], o = a * y + z; phzR[(size_t) j] = y - a * o; y = o; }
            fxR += (y - fxR) * 0.5 * phM * ponS;
        }
        if (flonS > 0.0001)
        {
            flp += fInc; if (flp >= TWO_PI) flp -= TWO_PI;
            double d = std::max (2.0, fdl * (1 - fdep * 0.95 * (0.5 + 0.5 * std::sin (flp))));
            double y = lread (flL, FLN, fwp, d); flL[(size_t) fwp] = safe (fxL + ffb * y);
            fxL += (y - fxL) * 0.5 * fmx * flonS;
            d = std::max (2.0, fdl * (1 - fdep * 0.95 * (0.5 + 0.5 * std::sin (flp + fSp))));
            y = lread (flR, FLN, fwp, d); flR[(size_t) fwp] = safe (fxR + ffb * y);
            fxR += (y - fxR) * 0.5 * fmx * flonS;
            if (++fwp >= FLN) fwp = 0;
        }
    }

    // Hilbert transformer (Olli Niemitalo's 2 x 4 allpass network)
    struct Hil { double s[32] = {}; double dly = 0; };
    inline void hil (Hil& h, double x)
    {
        static const double hca[4] = { 0.6923878 * 0.6923878, 0.9360654322959 * 0.9360654322959,
                                       0.9882295226860 * 0.9882295226860, 0.9987488452737 * 0.9987488452737 };
        static const double hcb[4] = { 0.4021921162426 * 0.4021921162426, 0.8561710882420 * 0.8561710882420,
                                       0.9722909545651 * 0.9722909545651, 0.9952884791278 * 0.9952884791278 };
        double xa = x, xb = x;
        for (int j = 0; j < 4; ++j)
        {
            double* s = h.s + j * 4;
            double y = hca[j] * (xa + s[3]) - s[1]; s[1] = s[0]; s[0] = xa; s[3] = s[2]; s[2] = y; xa = y;
            s = h.s + 16 + j * 4;
            y = hcb[j] * (xb + s[3]) - s[1]; s[1] = s[0]; s[0] = xb; s[3] = s[2]; s[2] = y; xb = y;
        }
        hre = h.dly; him = xb; h.dly = xa;
    }

    inline double psread (const std::vector<double>& buf, double ph) const
    {
        double ph2 = ph + 0.5; if (ph2 >= 1) ph2 -= 1;
        return lread (buf, PSN, pwp, 4 + ph * PW) * std::sin (kPi * ph) + lread (buf, PSN, pwp, 4 + ph2 * PW) * std::sin (kPi * ph2);
    }

    inline void doshift()
    {
        shph += shInc; if (shph >= TWO_PI) shph -= TWO_PI;
        const double lfL = std::sin (shph), lfR = std::sin (shph + shSt);
        if (shMode == 0)
        {
            psL[(size_t) pwp] = shL; psR[(size_t) pwp] = shR;
            double r = std::pow (2.0, (shSemi + shDep * 12 * lfL) / 12); pphL += (1 - r) / PW;
            if (pphL >= 1) pphL -= 1;
            if (pphL < 0) pphL += 1;
            r = std::pow (2.0, (shSemi + shDep * 12 * lfR) / 12); pphR += (1 - r) / PW;
            if (pphR >= 1) pphR -= 1;
            if (pphR < 0) pphR += 1;
            double y = psread (psL, pphL); shL += (y - shL) * shMix * shonS;
            y = psread (psR, pphR); shR += (y - shR) * shMix * shonS;
            if (++pwp >= PSN) pwp = 0;
        }
        else
        {
            double f = shFreq + shDep * 300 * lfL; oscL += TWO_PI * f / srate;
            if (oscL >= TWO_PI) oscL -= TWO_PI;
            if (oscL < 0) oscL += TWO_PI;
            hil (hilL, shL); double y = hre * std::cos (oscL) + him * std::sin (oscL); shL += (y - shL) * shMix * shonS;
            f = shFreq + shDep * 300 * lfR; oscR += TWO_PI * f / srate;
            if (oscR >= TWO_PI) oscR -= TWO_PI;
            if (oscR < 0) oscR += TWO_PI;
            hil (hilR, shR); y = hre * std::cos (oscR) + him * std::sin (oscR); shR += (y - shR) * shMix * shonS;
        }
    }

    // ---------------------------------------------------------------- state
    double srate = 48000;
    int MAXD = 1, PSN = 1, PW = 1, FLN = 1;
    std::vector<double> bufL, bufR, psL, psR, flL, flR, difL, difR;
    int apn1 = 8, apn2 = 8, apn3 = 8, apn4 = 8;
    Col colL, colR;
    int wp = 0, fwp = 0, pwp = 0;
    double pphL = 0, pphR = 0, oscL = 0, oscR = 0;
    Hil hilL, hilR;
    double hre = 0, him = 0;
    std::array<double, 12> phzL {}, phzR {};
    BQ bqL, bqR;
    double gsc = 0, eatt = 0, erel = 0, envdec = 0, drc = 0;
    bool started = false, lastPlaying = false;
    double Ts = 0, TRs = 0, Ls = 0, maxmod = 16;
    std::array<double, MaxHandles> tsm {}, tenv {}, ttg {}, tpan {}, tlpL {}, tlpR {}, tcfa {};

    int typ = 2, pb = 0, md = 0, fxa = 0, phN = 4, shMode = 0;
    double lK = 0, lA1 = 0, lA2 = 0, lA3 = 0, hK = 0, hA1 = 0, hA2 = 0, hA3 = 0;
    double drvE = 0, wowD = 0, wowInc = 0, flD = 0, flInc = 0, nAmt = 0, sph = 0, crB = 0, crQ = 1, crR = 1, dfG = 0, tcoef = 0.001;
    double T = 1000, TR = 1000, Lp = 1000;
    double width = 1, fbT = 0, dryT = 1, wetT = 0.5, frzT = 0, duckA = 0;
    double fb0 = 1, fb1 = 0, fb2 = 0, fa1 = 0, fa2 = 0;
    double fonT = 0, ponT = 0, phC = 800, phInc = 0, phD = 0, phS = 0, phM = 0;
    double flonT = 0, fdl = 100, fdep = 0, fInc = 0, fSp = 0, ffb = 0, fmx = 0;
    double shonT = 0, shSemi = 0, shFreq = 0, shDep = 0, shInc = 0, shSt = 0, shMix = 0;

    double dryS = 1, wetS = 0, fbS = 0, frS = 0, fonS = 0, ponS = 0, flonS = 0, shonS = 0;
    double env = 0, dgS = 1, wph = 0, fph = 0, dcnt = 0, dtar = 0, drift = 0, php = 0, flp = 0, shph = 0;
    double fxL = 0, fxR = 0, shL = 0, shR = 0;
    int clipc = 0;
    uint32_t rng = 0x2468ace1u;
};
} // namespace eg
