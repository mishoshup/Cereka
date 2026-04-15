#!/usr/bin/env python3
"""
One-time generator for Cereka placeholder template assets.
Run this once to produce the binary files; they are committed to the repo.
CMake embeds them at build time — Python is NOT required to build.

Usage: python3 generate.py
Output: placeholder_bg.png  placeholder_char.png  placeholder_sfx.wav  placeholder_bgm.wav
"""
import io, math, os, struct, wave, zlib

OUT = os.path.dirname(os.path.abspath(__file__))


# ── PNG helpers ──────────────────────────────────────────────────────────────

def _chunk(name: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(name + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + name + data + struct.pack(">I", crc)

def _png(width, height, color_type, rows):
    ihdr = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + _chunk(b"IHDR", ihdr)
        + _chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
        + _chunk(b"IEND", b"")
    )


# ── Background: 1280×720 RGB, dark indigo gradient + vignette ────────────────

def make_bg():
    W, H = 1280, 720
    rows = []
    for y in range(H):
        t = y / (H - 1)
        br = int(22 * (1 - t) + 8  * t)
        bg = int(18 * (1 - t) + 10 * t)
        bb = int(48 * (1 - t) + 30 * t)
        row = bytearray([0])
        for x in range(W):
            cx = x / (W - 1) - 0.5
            cy = y / (H - 1) - 0.5
            v  = max(0.0, 1.0 - (cx*cx + cy*cy) * 3.6)
            row += bytes([max(0, int(br*v)), max(0, int(bg*v)), max(0, int(bb*v))])
        rows.append(bytes(row))
    return _png(W, H, 2, rows)


# ── Character: 300×600 RGBA, teal silhouette on transparent ──────────────────

def make_char():
    W, H, CX = 300, 600, 150
    R, G, B, A = 72, 202, 183, 230
    HCX, HCY, HR = CX, 78, 54
    RECTS = [
        (CX-18, CX+18, 128, 155),   # neck
        (CX-58, CX+58, 155, 390),   # torso
        (CX-92, CX-58, 162, 345),   # left arm
        (CX+58, CX+92, 162, 345),   # right arm
        (CX-52, CX- 6, 390, 592),   # left leg
        (CX+ 6, CX+52, 390, 592),   # right leg
    ]
    rows = []
    for y in range(H):
        row = bytearray([0])
        for x in range(W):
            dx, dy = x - HCX, y - HCY
            if dx*dx + dy*dy <= HR*HR:
                row += bytes([R, G, B, A])
            elif any(x1 <= x < x2 and y1 <= y < y2 for x1,x2,y1,y2 in RECTS):
                row += bytes([R, G, B, A])
            else:
                row += bytes([0, 0, 0, 0])
        rows.append(bytes(row))
    return _png(W, H, 6, rows)


# ── SFX: 44100 Hz mono WAV, 100ms 880 Hz decaying sine ───────────────────────

def make_sfx():
    sr, n = 44100, int(44100 * 0.10)
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
        frames = []
        for i in range(n):
            t = i / sr
            s = int(32767 * math.exp(-t*40) * math.sin(2*math.pi*880*t))
            frames.append(struct.pack("<h", max(-32768, min(32767, s))))
        w.writeframes(b"".join(frames))
    return buf.getvalue()


# ── BGM: 22050 Hz mono WAV, 2s C-major ambient loop ──────────────────────────

def make_bgm():
    sr, dur = 22050, 2.0
    freqs = [261.63, 329.63, 392.00]  # C4 E4 G4
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr)
        frames = []
        for i in range(int(sr * dur)):
            t    = i / sr
            fade = min(t/0.15, 1.0) * min((dur-t)/0.15, 1.0)
            sf   = sum(math.sin(2*math.pi*f*t) for f in freqs) / len(freqs)
            s    = int(32767 * 0.35 * fade * sf)
            frames.append(struct.pack("<h", max(-32768, min(32767, s))))
        w.writeframes(b"".join(frames))
    return buf.getvalue()


if __name__ == "__main__":
    assets = [
        ("placeholder_bg.png",   make_bg),
        ("placeholder_char.png", make_char),
        ("placeholder_sfx.wav",  make_sfx),
        ("placeholder_bgm.wav",  make_bgm),
    ]
    for fname, fn in assets:
        data = fn()
        path = os.path.join(OUT, fname)
        with open(path, "wb") as f:
            f.write(data)
        print(f"  {fname:<26} {len(data):>7} bytes")
    print("Done.")
