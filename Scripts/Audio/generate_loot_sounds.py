"""Synthesize the loot sounds (the grappling hook grabbing a resource) and the resource-counter tick as WAV files.

Standard library only, so it runs on the Python bundled with Unreal:
  "<UE>/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" -I Scripts/Audio/generate_loot_sounds.py
Writes Content/Audio/Sounds/*.wav (22.05 kHz, 16-bit mono: a little lo-fi, to sit with the PS1 look).
Every sound uses its own fixed random seed, so re-running gives the same files. Import them into
/Game/Audio/Sounds with Scripts/Editor/import_loot_sounds.py.

| File | Resource | Idea |
| --- | --- | --- |
| SFX_Loot_Ammo | Ammo box | Wooden crate knock, then brass shells clattering inside |
| SFX_Loot_Money | Money pouch | Soft leather thump, then two shakes of ringing coins |
| SFX_Loot_Wood | Log | Two hollow wooden knocks, the second a smaller bounce |
| SFX_Loot_Coal | Coal | Heavy lump thud with a gritty crunch, then ember crackle and hiss |
| SFX_Loot_Crystal | Crystal | Rising three-note glass chime that shimmers out |
| SFX_Counter_Tick | (counter) | Odometer drum click with a small ratchet after it |
"""
import math
import os
import random
import struct
import wave

RATE = 22050
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Content', 'Audio', 'Sounds')


# --- Building blocks ------------------------------------------------------------------------

def silence(seconds):
    return [0.0] * int(seconds * RATE)


def mix(dest, src, at, gain=1.0):
    """Adds src into dest starting at `at` seconds, growing dest if needed."""
    start = int(at * RATE)
    if start + len(src) > len(dest):
        dest.extend([0.0] * (start + len(src) - len(dest)))
    for i, s in enumerate(src):
        dest[start + i] += s * gain


def tone(freq, seconds, decay, phase=0.0, attack=0.002, vibrato=0.0, vibrato_hz=0.0):
    """A sine that starts after a short attack and dies away exponentially (decay = seconds to fall by 1/e)."""
    n = int(seconds * RATE)
    out = []
    ph = phase
    for i in range(n):
        t = i / RATE
        env = math.exp(-t / decay) * min(1.0, t / attack if attack > 0 else 1.0)
        f = freq * (1.0 + vibrato * math.sin(2 * math.pi * vibrato_hz * t))
        ph += 2 * math.pi * f / RATE
        out.append(math.sin(ph) * env)
    return out


def partials(freq, ratios, amps, decays, seconds, rng, attack=0.001):
    """An inharmonic struck object: several decaying sines at freq * ratio, random phases."""
    out = silence(seconds)
    for r, a, d in zip(ratios, amps, decays):
        if freq * r < RATE / 2 - 200:
            mix(out, tone(freq * r, seconds, d, rng.uniform(0, 2 * math.pi), attack), 0, a)
    return out


def noise(seconds, rng):
    return [rng.uniform(-1.0, 1.0) for _ in range(int(seconds * RATE))]


def envelope(samples, decay, attack=0.0005):
    return [s * math.exp(-(i / RATE) / decay) * min(1.0, (i / RATE) / attack) for i, s in enumerate(samples)]


def biquad(samples, kind, freq, q):
    """RBJ cookbook filter: 'lowpass', 'highpass' or 'bandpass' (constant peak gain)."""
    w0 = 2 * math.pi * min(freq, RATE / 2 - 100) / RATE
    alpha = math.sin(w0) / (2 * q)
    cw = math.cos(w0)
    if kind == 'lowpass':
        b0, b1, b2 = (1 - cw) / 2, 1 - cw, (1 - cw) / 2
    elif kind == 'highpass':
        b0, b1, b2 = (1 + cw) / 2, -(1 + cw), (1 + cw) / 2
    else:
        b0, b1, b2 = alpha, 0.0, -alpha
    a0, a1, a2 = 1 + alpha, -2 * cw, 1 - alpha
    b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
    x1 = x2 = y1 = y2 = 0.0
    out = []
    for x in samples:
        y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, x, y1, y
        out.append(y)
    return out


def burst(seconds, decay, rng, kind, freq, q):
    """A filtered noise hit: the 'scrape', 'click' or 'thump' part of an impact."""
    return envelope(biquad(noise(seconds, rng), kind, freq, q), decay)


def finish(samples, peak_db=-3.0, fade_out=0.03):
    """Removes DC, fades the tail and normalizes the peak."""
    mean = sum(samples) / max(1, len(samples))
    samples = [s - mean for s in samples]
    fade = int(fade_out * RATE)
    for i in range(min(fade, len(samples))):
        samples[-1 - i] *= i / fade
    for i in range(min(int(0.002 * RATE), len(samples))):
        samples[i] *= i / (0.002 * RATE)
    peak = max(abs(s) for s in samples) or 1.0
    target = 10 ** (peak_db / 20.0)
    return [s / peak * target for s in samples]


def write(name, samples):
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.normpath(os.path.join(OUT_DIR, name + '.wav'))
    with wave.open(path, 'wb') as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, s)) * 32767)) for s in samples))
    print('%-22s %.2f s  %s' % (name, len(samples) / RATE, path))


# --- The sounds -----------------------------------------------------------------------------

def ammo():
    """Wooden crate knock, then brass shells clattering inside."""
    rng = random.Random(101)
    out = silence(0.7)
    # The crate: a short boxy knock.
    mix(out, partials(170, [1.0, 1.9, 3.1], [1.0, 0.5, 0.25], [0.06, 0.035, 0.02], 0.2, rng), 0.0, 0.8)
    mix(out, burst(0.05, 0.012, rng, 'bandpass', 900, 1.2), 0.0, 0.6)
    # The shells: bright brass tinks and a few heavier clacks, bunched after the knock.
    for _ in range(11):
        at = 0.03 + rng.random() ** 1.6 * 0.42
        f = rng.uniform(2600, 3900)
        mix(out, partials(f, [1.0, 1.47, 2.31], [1.0, 0.55, 0.3], [0.03, 0.02, 0.012], 0.12, rng), at, rng.uniform(0.25, 0.55))
        mix(out, burst(0.01, 0.002, rng, 'highpass', 3000, 0.7), at, 0.25)
    for _ in range(4):
        at = 0.02 + rng.random() * 0.3
        mix(out, partials(rng.uniform(1150, 1700), [1.0, 1.62, 2.7], [1.0, 0.5, 0.2], [0.035, 0.02, 0.01], 0.12, rng), at, 0.35)
    return finish(out)


def money():
    """Soft leather thump, then two shakes of ringing coins."""
    rng = random.Random(202)
    out = silence(0.95)
    mix(out, burst(0.12, 0.04, rng, 'lowpass', 380, 0.8), 0.0, 1.0)
    mix(out, tone(95, 0.15, 0.04), 0.0, 0.5)
    for shake_at, count in ((0.03, 8), (0.3, 6)):
        for _ in range(count):
            at = shake_at + rng.random() ** 1.3 * 0.22
            f = rng.uniform(3200, 5200)
            # A disc's partials (ratios of a free plate) ring longer than brass shells: gold.
            coin = partials(f, [1.0, 2.76, 5.40], [1.0, 0.45, 0.2], [rng.uniform(0.08, 0.22), 0.06, 0.03], 0.4, rng)
            mix(out, coin, at, rng.uniform(0.2, 0.45))
            mix(out, burst(0.008, 0.0015, rng, 'highpass', 4000, 0.7), at, 0.15)
    return finish(out)


def wood():
    """Two hollow wooden knocks, the second a smaller bounce."""
    rng = random.Random(303)
    out = silence(0.55)
    for at, gain, pitch in ((0.0, 1.0, 1.0), (0.12, 0.55, 1.04)):
        body = partials(205 * pitch, [1.0, 2.28, 4.6], [1.0, 0.6, 0.3], [0.09, 0.05, 0.025], 0.3, rng)
        mix(out, body, at, gain)
        mix(out, burst(0.03, 0.008, rng, 'bandpass', 1300, 1.0), at, gain * 0.7)   # the woody click
        mix(out, tone(85 * pitch, 0.2, 0.05), at, gain * 0.45)                       # the log's weight
    return finish(out)


def coal():
    """Heavy lump thud with a gritty crunch, then ember crackle and hiss."""
    rng = random.Random(404)
    out = silence(0.8)
    mix(out, tone(68, 0.3, 0.07), 0.0, 0.9)
    mix(out, burst(0.1, 0.03, rng, 'lowpass', 300, 0.7), 0.0, 0.7)
    # Crunch: many short grains of filtered noise, dense at first, thinning out.
    for _ in range(55):
        at = rng.random() ** 2.2 * 0.28
        grain = burst(rng.uniform(0.004, 0.012), rng.uniform(0.0015, 0.004), rng, 'bandpass', rng.uniform(1200, 4200), 1.5)
        mix(out, grain, at, rng.uniform(0.25, 0.7))
    # Embers: a soft hiss and a few sharp pops.
    hiss = biquad(noise(0.6, rng), 'highpass', 5000, 0.7)
    hiss = [s * 0.12 * math.sin(math.pi * i / len(hiss)) for i, s in enumerate(hiss)]
    mix(out, hiss, 0.15, 1.0)
    for _ in range(10):
        mix(out, burst(0.006, 0.0012, rng, 'highpass', 2500, 0.7), rng.uniform(0.18, 0.75), rng.uniform(0.2, 0.5))
    return finish(out)


def crystal():
    """Rising three-note glass chime that shimmers out."""
    rng = random.Random(505)
    out = silence(1.6)
    mix(out, burst(0.01, 0.002, rng, 'highpass', 5000, 0.7), 0.0, 0.4)   # the tink of the strike
    for i, f in enumerate((1318.5, 1975.5, 2637.0)):                     # E6, B6, E7
        note = silence(1.4)
        for r, a, d in ((1.0, 1.0, 0.75), (2.32, 0.35, 0.35), (4.25, 0.15, 0.18)):
            if f * r < RATE / 2 - 200:
                mix(note, tone(f * r, 1.4, d, rng.uniform(0, 6.28), 0.003, 0.003, 6.0), 0, a)
        mix(out, note, i * 0.075, (0.85, 0.75, 0.65)[i])
    return finish(out, fade_out=0.15)


def counter_tick():
    """Odometer drum click with a small ratchet after it."""
    rng = random.Random(606)
    out = silence(0.13)
    mix(out, burst(0.006, 0.001, rng, 'highpass', 2500, 0.7), 0.0, 1.0)
    mix(out, partials(2200, [1.0, 2.6], [1.0, 0.4], [0.02, 0.01], 0.06, rng), 0.0, 0.6)
    mix(out, burst(0.004, 0.0008, rng, 'highpass', 3500, 0.7), 0.035, 0.45)
    mix(out, partials(2900, [1.0], [1.0], [0.012], 0.04, rng), 0.035, 0.3)
    return finish(out, peak_db=-6.0, fade_out=0.02)


if __name__ == '__main__':
    write('SFX_Loot_Ammo', ammo())
    write('SFX_Loot_Money', money())
    write('SFX_Loot_Wood', wood())
    write('SFX_Loot_Coal', coal())
    write('SFX_Loot_Crystal', crystal())
    write('SFX_Counter_Tick', counter_tick())
