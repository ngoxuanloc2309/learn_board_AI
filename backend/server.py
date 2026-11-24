"""
Step 1 server for the AI Learning Device (see docs/Backend_Protocol.md).

Receives voice turns over one WebSocket connection, saves each turn as a WAV file and replies with
a FAKE stt_result (there is no speech recognition in this step).

Protocol summary (Backend_Protocol.md, v0.1):
  TEXT frame   one JSON object with a "type" field (UTF-8).
  BINARY frame 4 byte header then PCM: byte 0 = kind (0x01 mic to server), byte 1 = flags,
               bytes 2-3 = sequence number, uint16 little endian. Then PCM s16le mono 16 kHz.

Design choices made in this file that are NOT yet in the protocol document (DESIGN, to be confirmed):
  - The sequence number starts at 0 for each turn and wraps at 65536.
  - Flags (byte 1) are ignored in v0.
  - Binary frames carry no turn number: they belong to the turn opened by the last listen_start.
    Frames received while no turn is open are ignored and counted.
  - WebSocket ping is OFF by default (see --ping-interval) until transport_ws.c of ESP-IDF v5.4.3
    has been read to see how the board answers pings.
"""

import argparse
import asyncio
import json
import logging
import struct
import sys
import time
import unicodedata
import uuid
import wave
from pathlib import Path

from websockets.asyncio.server import serve
from websockets.exceptions import ConnectionClosed

PROTOCOL_VERSION = 0
URL_PATH = "/v1/voice"
HEADER_LEN = 4
KIND_MIC = 0x01
SAMPLE_RATE = 16000
SAMPLE_BYTES = 2
MAX_TURN_S = 60  # safety cap on one turn (disk protection), not a protocol value
GAP_SLOW_MS = 40.0  # an arrival gap above this counts as slow (the nominal frame spacing is 20 ms)

log = logging.getLogger("server")


def to_json(obj):
    """Build a TEXT frame: NFC text, UTF-8, no \\uXXXX escapes (protocol section 4)."""
    return json.dumps(obj, ensure_ascii=False, separators=(",", ":"))


def nfc(text):
    return unicodedata.normalize("NFC", text)


class Turn:
    """State of one open voice turn."""

    def __init__(self, number, lang, path, save):
        self.number = number
        self.lang = lang
        self.path = path
        self.expected_seq = 0
        self.frames = 0
        self.lost = 0
        self.disorder = 0
        self.pcm_bytes = 0
        self.t_start = time.monotonic()
        # Arrival timing of frames, measured when the frame reaches the application. A big gap
        # is a stall somewhere between the board and this code (Wi-Fi, TCP Nagle or delayed ACK,
        # or this event loop), so it does not say which. A missing frame adds 20 ms to a gap.
        self.last_arrival = None
        self.gap_count = 0
        self.gap_sum = 0.0
        self.gap_min = None
        self.gap_max = 0.0
        self.gap_max_seq = 0
        self.gap_slow = 0
        self.wav = None
        if save:
            path.parent.mkdir(parents=True, exist_ok=True)
            self.wav = wave.open(str(path), "wb")
            self.wav.setnchannels(1)
            self.wav.setsampwidth(SAMPLE_BYTES)
            self.wav.setframerate(SAMPLE_RATE)

    def note_arrival(self, seq, now):
        """Record the time since the previous frame of this turn (the first frame has none)."""
        if self.last_arrival is not None:
            gap = now - self.last_arrival
            self.gap_count += 1
            self.gap_sum += gap
            if self.gap_min is None or gap < self.gap_min:
                self.gap_min = gap
            if gap > self.gap_max:
                self.gap_max = gap
                self.gap_max_seq = seq
            if gap * 1000.0 > GAP_SLOW_MS:
                self.gap_slow += 1
        self.last_arrival = now

    def gap_summary(self):
        if self.gap_count == 0:
            return "arrival gaps: n/a (fewer than 2 frames)"
        return ("arrival gaps: min %.1f ms, avg %.1f ms, max %.1f ms (at seq %d), "
                "over %d ms: %d of %d" %
                (self.gap_min * 1000.0, self.gap_sum / self.gap_count * 1000.0,
                 self.gap_max * 1000.0, self.gap_max_seq, int(GAP_SLOW_MS),
                 self.gap_slow, self.gap_count))

    def add_frame(self, seq, pcm, now):
        self.note_arrival(seq, now)
        diff = (seq - self.expected_seq) & 0xFFFF
        if diff == 0:
            self.expected_seq = (seq + 1) & 0xFFFF
        elif diff < 0x8000:
            # Frames were skipped. The WAV is NOT padded, so a gap is audible as a glitch.
            self.lost += diff
            self.expected_seq = (seq + 1) & 0xFFFF
            log.warning("turn %d: %d frame(s) missing before seq %d", self.number, diff, seq)
        else:
            # Older than expected: duplicate or reordered. Drop it.
            self.disorder += 1
            log.warning("turn %d: out of order frame seq %d (expected %d)",
                        self.number, seq, self.expected_seq)
            return
        self.frames += 1
        self.pcm_bytes += len(pcm)
        if self.wav is not None:
            self.wav.writeframes(pcm)

    def seconds(self):
        return self.pcm_bytes / (SAMPLE_RATE * SAMPLE_BYTES)

    def close(self, keep=True):
        if self.wav is not None:
            self.wav.close()
            self.wav = None
            if not keep:
                self.path.unlink(missing_ok=True)


class Session:
    """One WebSocket connection."""

    def __init__(self, ws, out_dir, save):
        self.ws = ws
        self.out_dir = out_dir
        self.save = save
        self.hello_done = False
        self.session_id = "s" + uuid.uuid4().hex[:8]  # unique even for connections in the same second
        self.turn = None
        self.late_frames = 0

    async def send(self, obj):
        await self.ws.send(to_json(obj))

    async def send_error(self, code, message, turn=None):
        obj = {"type": "error", "code": code, "message": message}
        if turn is not None:
            obj["turn"] = turn
        await self.send(obj)

    async def run(self):
        peer = self.ws.remote_address
        log.info("connected: %s", peer)
        try:
            async for message in self.ws:
                if isinstance(message, str):
                    ok = await self.on_text(message)
                else:
                    ok = await self.on_binary(message)
                if not ok:
                    break
        except ConnectionClosed:
            log.info("connection closed by peer: %s", peer)
        finally:
            if self.turn is not None:
                log.warning("turn %d dropped: connection ended (%d frames)",
                            self.turn.number, self.turn.frames)
                self.turn.close(keep=False)
                self.turn = None
            if self.late_frames:
                log.info("frames ignored (no open turn): %d", self.late_frames)
        log.info("session ended: %s", peer)

    async def on_text(self, raw):
        try:
            msg = json.loads(raw)
            if not isinstance(msg, dict):
                raise ValueError("not an object")
            mtype = msg.get("type")
        except ValueError:
            await self.send_error("bad_frame", "invalid JSON")
            return True
        log.info("rx %s", raw if len(raw) < 300 else raw[:300] + "...")

        if mtype == "hello":
            return await self.on_hello(msg)
        if not self.hello_done:
            await self.send_error("bad_frame", "hello expected first")
            return True
        if mtype == "listen_start":
            await self.on_listen_start(msg)
        elif mtype == "listen_stop":
            await self.on_listen_stop(msg)
        elif mtype == "abort":
            await self.on_abort(msg)
        else:
            log.info("unknown type ignored: %r", mtype)  # protocol section 4: ignore unknown
        return True

    async def on_hello(self, msg):
        version = msg.get("protocol_version")
        if version != PROTOCOL_VERSION:
            await self.send_error("bad_version", "server speaks protocol %d" % PROTOCOL_VERSION)
            return False
        audio = msg.get("audio") or {}
        if (audio.get("codec") != "pcm_s16le" or audio.get("rate") != SAMPLE_RATE
                or audio.get("channels") != 1):
            await self.send_error("bad_frame", "audio must be pcm_s16le 16000 Hz mono")
            return False
        self.hello_done = True
        log.info("hello: device_id=%s fw=%s frame_ms=%s",
                 msg.get("device_id"), msg.get("fw"), audio.get("frame_ms"))
        await self.send({"type": "hello_ack", "protocol_version": PROTOCOL_VERSION,
                         "session_id": self.session_id})
        return True

    async def on_listen_start(self, msg):
        number = msg.get("turn")
        if not isinstance(number, int) or isinstance(number, bool):
            await self.send_error("bad_frame", "listen_start needs integer turn")
            return
        if self.turn is not None:
            await self.send_error("busy", "turn %d is still open" % self.turn.number, number)
            return
        lang = msg.get("lang", "vi")
        stamp = time.strftime("%Y%m%d_%H%M%S")
        path = self.out_dir / ("%s_turn%d_%s.wav" % (stamp, number, self.session_id))
        self.turn = Turn(number, lang, path, self.save)
        log.info("turn %d started (lang=%s) -> %s", number, lang, path if self.save else "(not saved)")

    async def on_listen_stop(self, msg):
        turn = self.turn
        if turn is None or msg.get("turn") != turn.number:
            log.info("listen_stop for turn %r ignored (open turn: %s)",
                     msg.get("turn"), turn.number if turn else None)
            return
        self.turn = None
        turn.close(keep=True)
        wall = time.monotonic() - turn.t_start
        log.info("turn %d stopped (reason=%s): %d frames, %.2f s of audio in %.2f s wall, "
                 "missing %d, out of order %d", turn.number, msg.get("reason"), turn.frames,
                 turn.seconds(), wall, turn.lost, turn.disorder)
        log.info("turn %d %s", turn.number, turn.gap_summary())
        if turn.frames == 0:
            await self.send_error("no_speech", "no audio received", turn.number)
            return
        # FAKE result: there is no speech recognition in step 1. The Vietnamese text also
        # checks that UTF-8 and NFC reach the device unchanged.
        text = nfc("Xin chào, đây là kết quả giả (chưa có nhận dạng giọng nói). "
                   "Lượt %d: %.2f giây, %d khối, mất %d, sai thứ tự %d." %
                   (turn.number, turn.seconds(), turn.frames, turn.lost, turn.disorder))
        await self.send({"type": "stt_result", "turn": turn.number, "text": text, "final": True})

    async def on_abort(self, msg):
        turn = self.turn
        if turn is None or msg.get("turn") != turn.number:
            log.info("abort for turn %r ignored", msg.get("turn"))
            return
        self.turn = None
        turn.close(keep=False)  # an aborted turn is deleted
        log.info("turn %d aborted, file deleted", turn.number)

    async def on_binary(self, data):
        now = time.monotonic()  # first thing: closest we get to the arrival time of this frame
        if len(data) < HEADER_LEN:
            await self.send_error("bad_frame", "binary frame shorter than header")
            return True
        kind, _flags, seq = struct.unpack("<BBH", data[:HEADER_LEN])
        pcm = data[HEADER_LEN:]
        if kind != KIND_MIC:
            await self.send_error("bad_frame", "unexpected kind 0x%02x" % kind)
            return True
        if len(pcm) % SAMPLE_BYTES:
            await self.send_error("bad_frame", "odd PCM length", self.turn.number if self.turn else None)
            return True
        if self.turn is None:
            self.late_frames += 1  # protocol section 8: frames after listen_stop are ignored
            return True
        self.turn.add_frame(seq, pcm, now)
        if self.turn.seconds() > MAX_TURN_S:
            number = self.turn.number
            self.turn.close(keep=True)
            self.turn = None
            await self.send_error("internal", "turn longer than %d s" % MAX_TURN_S, number)
        return True


async def main():
    # Windows consoles may not use UTF-8; never let a Vietnamese log line raise.
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description="Step 1 echo server (fake stt_result)")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--out", default="recordings", help="folder for WAV files")
    ap.add_argument("--no-save", action="store_true", help="do not write WAV files")
    ap.add_argument("--ping-interval", type=float, default=0,
                    help="WebSocket ping period in s, 0 = off (default: off)")
    args = ap.parse_args()
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")

    out_dir = Path(args.out)
    ping = args.ping_interval if args.ping_interval > 0 else None

    async def handler(ws):
        if ws.request.path != URL_PATH:
            log.warning("rejected path %r", ws.request.path)
            await ws.close(code=1008, reason="bad path")
            return
        await Session(ws, out_dir, not args.no_save).run()

    async with serve(handler, args.host, args.port, compression=None,
                     ping_interval=ping, ping_timeout=None if ping is None else 20):
        log.info("listening on ws://%s:%d%s  (WAV folder: %s, ping: %s)", args.host, args.port,
                 URL_PATH, "none" if args.no_save else out_dir, ping or "off")
        await asyncio.get_running_loop().create_future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass