"""
Dead Country - placeholder sound synthesizer.

Generates rough procedural WAVs (gunshot, mechanical clicks, footsteps, zombie voices, impacts)
so every audio hook in the game works before real recordings are licensed. Replace any file in
SourceArt/Audio with a real recording of the same name and re-run the import script.

Run with Blender's bundled Python (has numpy):
  "C:\\Program Files\\Blender Foundation\\Blender 3.2\\3.2\\python\\bin\\python.exe" synth_sounds.py <out_dir>
"""

import math
import os
import sys
import wave

import numpy as np

SR = 44100
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "SourceArt", "Audio")
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(1770)


# ---------------------------------------------------------------------------
# DSP helpers
# ---------------------------------------------------------------------------
def t_axis(seconds):
    return np.arange(int(SR * seconds)) / SR


def noise(seconds):
    return rng.standard_normal(int(SR * seconds))


def biquad(x, b0, b1, b2, a1, a2):
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i in range(len(x)):
        xi = x[i]
        yi = b0 * xi + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        y[i] = yi
        x2, x1 = x1, xi
        y2, y1 = y1, yi
    return y


def _coeffs(kind, f, q):
    w = 2 * math.pi * min(f, SR * 0.45) / SR
    cw, sw = math.cos(w), math.sin(w)
    alpha = sw / (2 * q)
    if kind == "lp":
        b = [(1 - cw) / 2, 1 - cw, (1 - cw) / 2]
    elif kind == "hp":
        b = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2]
    else:  # band-pass, constant 0 dB peak
        b = [alpha, 0.0, -alpha]
    a0, a1, a2 = 1 + alpha, -2 * cw, 1 - alpha
    return [v / a0 for v in b] + [a1 / a0, a2 / a0]


def lp(x, f, q=0.707):
    return biquad(x, *_coeffs("lp", f, q))


def hp(x, f, q=0.707):
    return biquad(x, *_coeffs("hp", f, q))


def bp(x, f, q=2.0):
    return biquad(x, *_coeffs("bp", f, q))


def env_exp(n, tau, attack=0.0):
    t = np.arange(n) / SR
    e = np.exp(-t / max(tau, 1e-4))
    if attack > 0:
        e *= np.clip(t / attack, 0, 1)
    return e


def pad(x, seconds):
    n = int(SR * seconds)
    return np.pad(x, (0, max(0, n - len(x))))[:n]


def place(dst, src, at_seconds, gain=1.0):
    i = int(at_seconds * SR)
    if i >= len(dst):
        return dst
    m = min(len(src), len(dst) - i)
    dst[i:i + m] += src[:m] * gain
    return dst


def ring(freqs, seconds, taus, amps=None):
    t = t_axis(seconds)
    out = np.zeros_like(t)
    amps = amps or [1.0] * len(freqs)
    for f, tau, a in zip(freqs, taus, amps):
        out += a * np.sin(2 * math.pi * f * t + rng.uniform(0, 6.28)) * np.exp(-t / tau)
    return out


def finish(x, peak_db=-1.0, drive=1.0, fade=0.01):
    if drive != 1.0:
        x = np.tanh(x * drive)
    x = x - np.mean(x)
    n = int(fade * SR)
    if n > 0 and len(x) > n:
        x[-n:] *= np.linspace(1, 0, n)
    p = np.max(np.abs(x)) or 1.0
    return x / p * (10 ** (peak_db / 20))


def save(name, x):
    data = (np.clip(x, -1, 1) * 32767).astype(np.int16)
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(data.tobytes())
    print(f"  {name}.wav  {len(x) / SR:.2f}s")


# ---------------------------------------------------------------------------
# Weapons
# ---------------------------------------------------------------------------
def gunshot(seed_shift):
    L = 2.2
    out = np.zeros(int(SR * L))
    crack = hp(noise(0.012), 1800) * env_exp(int(SR * 0.012), 0.003)
    body = lp(noise(0.25), 1600 + seed_shift * 150) * env_exp(int(SR * 0.25), 0.035)
    t = t_axis(0.3)
    sweep = np.sin(2 * math.pi * np.cumsum(np.linspace(140, 45, len(t))) / SR) * np.exp(-t / 0.07)
    tail = lp(noise(L), 650) * env_exp(int(SR * L), 0.55, attack=0.03)
    place(out, crack, 0.0, 1.6)
    place(out, body, 0.0, 1.0)
    place(out, sweep, 0.0, 1.1)
    place(out, tail, 0.0, 0.22)
    # Outback has few walls: two soft, late echoes off distant ground/sheds
    echo = lp(body, 900)
    place(out, echo, 0.19 + seed_shift * 0.02, 0.18)
    place(out, echo, 0.47 + seed_shift * 0.03, 0.09)
    return finish(out, -0.5, drive=2.2)


def click(freq=3200, tau=0.004, ring_f=None):
    x = bp(noise(0.03), freq, 3.0) * env_exp(int(SR * 0.03), tau)
    if ring_f:
        x += ring(ring_f, 0.03, [0.012] * len(ring_f), [0.3] * len(ring_f))
    return x


def dry_fire():
    out = np.zeros(int(SR * 0.25))
    place(out, click(3500, 0.003, [4600, 7100]), 0.0, 1.0)
    place(out, click(2600, 0.004), 0.035, 0.5)
    return finish(out, -6)


def mag_out():
    out = np.zeros(int(SR * 0.5))
    place(out, click(2800, 0.004, [3900]), 0.0, 1.0)
    fr = bp(noise(0.12), 1500, 1.2) * env_exp(int(SR * 0.12), 0.05, attack=0.01)
    place(out, fr, 0.02, 0.35)
    thunk = lp(noise(0.06), 320) * env_exp(int(SR * 0.06), 0.015)
    place(out, thunk, 0.13, 0.8)
    return finish(out, -4)


def mag_in():
    out = np.zeros(int(SR * 0.45))
    fr = bp(noise(0.1), 1300, 1.2) * env_exp(int(SR * 0.1), 0.06, attack=0.04)
    place(out, fr, 0.0, 0.4)
    place(out, click(2400, 0.006, [3100, 5300]), 0.09, 1.0)
    place(out, click(3600, 0.003), 0.115, 0.6)
    return finish(out, -3)


def slide_release():
    out = np.zeros(int(SR * 0.5))
    hit = hp(noise(0.02), 1200) * env_exp(int(SR * 0.02), 0.004)
    place(out, hit, 0.0, 1.0)
    place(out, ring([1850, 3120, 5240, 7300], 0.4, [0.05, 0.04, 0.03, 0.02], [0.6, 0.5, 0.4, 0.25]), 0.0, 0.5)
    place(out, lp(noise(0.05), 400) * env_exp(int(SR * 0.05), 0.012), 0.0, 0.7)
    return finish(out, -2)


def casing_tink():
    base = rng.uniform(3800, 4600)
    partials = [base, base * 1.62, base * 2.27, base * 2.91]
    out = np.zeros(int(SR * 0.6))
    place(out, ring(partials, 0.5, [0.09, 0.06, 0.04, 0.03], [1, 0.6, 0.4, 0.25]), 0.0, 1.0)
    bounce = ring([p * 1.01 for p in partials], 0.3, [0.04, 0.03, 0.02, 0.015], [1, 0.6, 0.4, 0.25])
    place(out, bounce, rng.uniform(0.11, 0.16), 0.35)
    place(out, bounce, rng.uniform(0.21, 0.26), 0.12)
    return finish(out, -10)


# ---------------------------------------------------------------------------
# World
# ---------------------------------------------------------------------------
def footstep_dirt():
    L = 0.25
    out = np.zeros(int(SR * L))
    thud = lp(noise(0.06), 140) * env_exp(int(SR * 0.06), 0.02)
    place(out, thud, 0.0, 1.0)
    # crunch: a sprinkle of tiny grit impulses through a mid band
    grit = np.zeros(int(SR * 0.12))
    for _ in range(rng.integers(18, 34)):
        i = rng.integers(0, len(grit) - 50)
        grit[i:i + 30] += rng.standard_normal(30) * np.exp(-np.arange(30) / 6)
    grit = bp(grit, rng.uniform(1600, 2600), 0.9) * env_exp(len(grit), 0.05)
    place(out, grit, 0.005, 0.9)
    return finish(out, -8)


def impact_dirt():
    out = np.zeros(int(SR * 0.5))
    place(out, lp(noise(0.08), 250) * env_exp(int(SR * 0.08), 0.02), 0.0, 1.0)
    place(out, bp(noise(0.35), 1200, 0.8) * env_exp(int(SR * 0.35), 0.09), 0.01, 0.35)
    return finish(out, -6)


def impact_flesh():
    out = np.zeros(int(SR * 0.4))
    place(out, lp(noise(0.06), 380) * env_exp(int(SR * 0.06), 0.02), 0.0, 1.0)
    sq = noise(0.2)
    t = t_axis(0.2)
    # crude sweeping band-pass for a wet "slap"
    seg = len(sq) // 4
    wet = np.concatenate([bp(sq[i * seg:(i + 1) * seg], f, 2.5) for i, f in enumerate([1600, 1100, 700, 450])])
    place(out, wet * np.exp(-t[:len(wet)] / 0.06), 0.005, 0.8)
    return finish(out, -5, drive=1.5)


# ---------------------------------------------------------------------------
# Zombies: glottal pulse train through drifting vowel formants
# ---------------------------------------------------------------------------
def voice(seconds, f0_start, f0_end, formants, rasp=0.35, gurgle=14.0):
    t = t_axis(seconds)
    f0 = np.linspace(f0_start, f0_end, len(t))
    f0 *= 1 + 0.03 * np.sin(2 * math.pi * 5.5 * t) + 0.02 * np.cumsum(rng.standard_normal(len(t))) / np.sqrt(len(t)) * 40
    phase = np.cumsum(f0) / SR
    saw = 2 * (phase - np.floor(phase)) - 1
    src = saw + rasp * rng.standard_normal(len(t))
    out = np.zeros_like(t)
    for (fa, fb), q, g in formants:
        # split into chunks so the vowel drifts over time
        chunks = 6
        n = len(src) // chunks
        parts = [bp(src[i * n:(i + 1) * n if i < chunks - 1 else len(src)], fa + (fb - fa) * i / (chunks - 1), q) for i in range(chunks)]
        out += g * np.concatenate(parts)
    # wet, irregular amplitude (gurgling throat)
    am = 0.75 + 0.25 * np.sin(2 * math.pi * gurgle * t + rng.uniform(0, 6)) * np.sin(2 * math.pi * 2.3 * t)
    swell = np.sin(np.pi * np.clip(t / seconds, 0, 1)) ** 0.7
    return out * am * swell


def zombie_groan():
    L = rng.uniform(1.8, 2.6)
    f = rng.uniform(78, 98)
    x = voice(L, f, f * rng.uniform(0.82, 0.95),
              [((420, 620), 4.0, 1.0), ((880, 760), 5.0, 0.6), ((2400, 2550), 6.0, 0.25)])
    return finish(lp(x, 3200), -6, drive=1.8)


def zombie_alert():
    L = rng.uniform(0.9, 1.2)
    f = rng.uniform(125, 150)
    x = voice(L, f * 1.15, f * 0.85,
              [((700, 520), 3.0, 1.0), ((1250, 950), 4.0, 0.7), ((2700, 2500), 5.0, 0.35)], rasp=0.6, gurgle=22.0)
    return finish(lp(x, 4500), -2, drive=3.0)


def zombie_attack():
    L = 0.55
    f = rng.uniform(105, 125)
    x = voice(L, f * 1.2, f * 0.7,
              [((650, 450), 3.0, 1.0), ((1150, 850), 4.0, 0.6)], rasp=0.8, gurgle=30.0)
    x = pad(x, 0.6)
    place(x, lp(noise(0.05), 600) * env_exp(int(SR * 0.05), 0.01), 0.12, 0.8)   # snap of teeth/grab
    return finish(x, -1, drive=3.5)


# ---------------------------------------------------------------------------
print(f"[DCAUDIO] writing to {os.path.abspath(OUT)}")
for i in range(3):
    save(f"SW_Gunshot_P9_{i + 1:02d}", gunshot(i))
save("SW_DryFire", dry_fire())
save("SW_MagOut", mag_out())
save("SW_MagIn", mag_in())
save("SW_SlideRelease", slide_release())
for i in range(3):
    save(f"SW_CasingTink_{i + 1:02d}", casing_tink())
for i in range(4):
    save(f"SW_Footstep_Dirt_{i + 1:02d}", footstep_dirt())
for i in range(3):
    save(f"SW_Impact_Dirt_{i + 1:02d}", impact_dirt())
for i in range(2):
    save(f"SW_Impact_Flesh_{i + 1:02d}", impact_flesh())
for i in range(4):
    save(f"SW_Zombie_Groan_{i + 1:02d}", zombie_groan())
for i in range(2):
    save(f"SW_Zombie_Alert_{i + 1:02d}", zombie_alert())
for i in range(2):
    save(f"SW_Zombie_Attack_{i + 1:02d}", zombie_attack())
print("[DCAUDIO] done")
