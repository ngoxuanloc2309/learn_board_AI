"""
Fake device for step 1: plays the role of the board when no board is connected.

Sends one voice turn to the server (hello, listen_start, PCM frames, listen_stop) and prints the
reply. The audio is a WAV file (must be PCM 16 bit, mono, 16000 Hz; there is no resampling) or a
generated 440 Hz tone. Frames follow the 4 byte header of docs/Backend_Protocol.md section 5.

Examples:
  python fake_device.py --tone 3
  python fake_device.py --wav my_voice.wav
  python fake_device.py --tone 2 --skip-frame 10      (server must report 1 missing frame)
  python fake_device.py --tone 2 --late-frames 5      (frames after listen_stop must be ignored)
  python fake_device.py --tone 5 --abort-after 1      (server must delete the turn)
"""

import argparse
import asyncio
import json
import math
import struct
import sys
import time
import wave

from websockets.asyncio.client import connect

RATE = 16000
FRAME_MS = 20
FRAME_SAMPLES = RATE * FRAME_MS // 1000  # 320 samples = 640 bytes (protocol section 5)


def load_pcm(args):
    if args.wav:
        with wave.open(args.wav, "rb") as w:
            if (w.getnchannels(), w.getsampwidth(), w.getframerate()) != (1, 2, RATE):
                sys.exit("WAV must be mono, 16 bit, 16000 Hz (got %d ch, %d bit, %d Hz)" %
                         (w.getnchannels(), w.getsampwidth() * 8, w.getframerate()))
            return w.readframes(w.getnframes())
    n = int(args.tone * RATE)
    samples = [int(8000 * math.sin(2 * math.pi * 440 * i / RATE)) for i in range(n)]
    return struct.pack("<%dh" % n, *samples)


def frame(seq, pcm):
    # byte 0 kind 0x01 (mic), byte 1 flags (0 in v0), bytes 2-3 sequence, little endian
    return struct.pack("<BBH", 0x01, 0, seq & 0xFFFF) + pcm


async def run(args):
    pcm = load_pcm(args)
    chunk = FRAME_SAMPLES * 2
    chunks = [pcm[i:i + chunk] for i in range(0, len(pcm) - chunk + 1, chunk)]
    url = "ws://%s:%d/v1/voice" % (args.host, args.port)
    print("connecting to", url, "-", len(chunks), "frames of", FRAME_MS, "ms")

    async with connect(url, compression=None, ping_interval=None) as ws:
        async def send_json(obj):
            await ws.send(json.dumps(obj, ensure_ascii=False, separators=(",", ":")))

        await send_json({"type": "hello", "protocol_version": args.version, "device_id": "FAKE00000001",
                         "fw": "fake", "audio": {"codec": "pcm_s16le", "rate": RATE, "channels": 1,
                                                 "frame_ms": FRAME_MS}})
        print("server:", await ws.recv())
        if args.version != 0:
            return

        await send_json({"type": "listen_start", "turn": 1, "reason": "touch", "lang": args.lang})
        t0 = time.monotonic()
        for i, c in enumerate(chunks):
            if i != args.skip_frame:
                await ws.send(frame(i, c))
            if args.abort_after is not None and i + 1 >= args.abort_after * 1000 // FRAME_MS:
                await send_json({"type": "abort", "turn": 1})
                print("sent abort")
                await asyncio.sleep(0.3)
                return
            if not args.fast:
                # real pace: frame i is due at t0 + (i + 1) * 20 ms
                await asyncio.sleep(max(0.0, t0 + (i + 1) * FRAME_MS / 1000 - time.monotonic()))
        await send_json({"type": "listen_stop", "turn": 1, "reason": "touch"})
        for k in range(args.late_frames):
            await ws.send(frame(len(chunks) + k, chunks[0]))
        print("streamed in %.2f s, waiting for stt_result" % (time.monotonic() - t0))
        while True:
            reply = json.loads(await asyncio.wait_for(ws.recv(), timeout=10))
            print("server:", reply)
            if reply.get("type") in ("stt_result", "error"):
                break


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description="Fake device for the step 1 server")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8765)
    src = ap.add_mutually_exclusive_group()
    src.add_argument("--wav", help="WAV file: PCM 16 bit mono 16000 Hz")
    src.add_argument("--tone", type=float, default=3.0, help="seconds of 440 Hz tone (default 3)")
    ap.add_argument("--lang", default="vi", choices=["vi", "en"])
    ap.add_argument("--fast", action="store_true", help="send without the 20 ms pacing")
    ap.add_argument("--skip-frame", type=int, default=-1, help="do not send this frame index")
    ap.add_argument("--late-frames", type=int, default=0, help="frames sent after listen_stop")
    ap.add_argument("--abort-after", type=float, default=None, help="send abort after N seconds")
    ap.add_argument("--version", type=int, default=0, help="protocol_version in hello (test bad_version)")
    asyncio.run(run(ap.parse_args()))


if __name__ == "__main__":
    main()
