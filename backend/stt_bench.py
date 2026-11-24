#!/usr/bin/env python3
"""STT benchmark for Backend_Protocol step 4a. Python standard library only.

Runs each model on a folder of WAV files, scores it against reference sentences
typed by the owner, and reports WER, CER, processing time per second of audio,
peak RAM and peak VRAM.

Reference file (UTF-8, one line per WAV, TAB or '|' between the name and the sentence):
    mic_001.wav<TAB>qua tao noi tieng anh la gi
An empty sentence marks a file that has no speech (a model that returns text for it
is counted as a hallucination and its words count as errors). Lines that start with
'#' are ignored.

Model specs (--models, one or more, run one at a time, each in its own process so
RAM is clean):
    fake                      no model, for testing this script (see --selftest)
    viet_asr                  pip package viet-asr (Vietnamese Zipformer, CPU)
    faster_whisper:<name>     pip package faster-whisper, <name> is a model name or a
                              local folder, for example small.en

Examples:
    python stt_bench.py --wav-dir recordings --refs refs.tsv --models viet_asr
    python stt_bench.py --wav-dir recordings --refs refs.tsv --models faster_whisper:small.en --device cuda --lang en
    python stt_bench.py --wav-dir recordings --draft draft.tsv --models viet_asr
    python stt_bench.py --selftest

Notes on the numbers:
- Text is normalized before scoring: NFC, lower case, punctuation removed. Digits
  written as digits and digits written as words differ, so type the references the
  way the model writes them or compare by eye.
- WER counts words split by spaces (a Vietnamese syllable is one word). CER counts
  characters including single spaces. wer_nodiac ignores tone and vowel marks (shows
  how much of the error is only diacritics).
- Corpus WER and CER are total errors over total reference words or characters, so a
  hallucinated sentence on a silent file adds errors but no reference words.
- VRAM is the growth of total GPU memory seen by nvidia-smi while the model runs, so
  other programs using the GPU at the same time add noise.
"""

import argparse
import csv
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import unicodedata
import wave

WORKER_MARK = "@@STT_BENCH_RESULT@@"


# ---------------------------------------------------------------- text and metrics

def normalize(text):
    """NFC, lower case, punctuation and symbols to spaces, apostrophes dropped, spaces collapsed."""
    t = unicodedata.normalize("NFC", text).lower()
    out = []
    for ch in t:
        if ch in "'\u2019":
            continue
        if unicodedata.category(ch)[0] in ("P", "S"):
            out.append(" ")
        else:
            out.append(ch)
    return " ".join("".join(out).split())


def strip_diacritics(text):
    d = unicodedata.normalize("NFD", text)
    d = "".join(c for c in d if unicodedata.category(c) != "Mn")
    return unicodedata.normalize("NFC", d.replace("\u0111", "d"))


def edit_distance(a, b):
    """Levenshtein distance between two sequences (lists of words or strings)."""
    prev = list(range(len(b) + 1))
    for i, x in enumerate(a, 1):
        cur = [i] + [0] * len(b)
        for j, y in enumerate(b, 1):
            cur[j] = min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (0 if x == y else 1))
        prev = cur
    return prev[-1]


def score_pair(ref, hyp):
    rn, hn = normalize(ref), normalize(hyp)
    rw, hw = rn.split(), hn.split()
    rd, hd = strip_diacritics(rn).split(), strip_diacritics(hn).split()
    return {
        "ref_norm": rn,
        "hyp_norm": hn,
        "ref_words": len(rw),
        "word_errs": edit_distance(rw, hw),
        "nodiac_errs": edit_distance(rd, hd),
        "ref_chars": len(rn),
        "char_errs": edit_distance(rn, hn),
    }


def pct(num, den):
    return None if den == 0 else 100.0 * num / den


# ---------------------------------------------------------------- files

def read_refs(path):
    refs = {}
    with open(path, encoding="utf-8-sig") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if "\t" in line:
                name, _, text = line.partition("\t")
            elif "|" in line:
                name, _, text = line.partition("|")
            else:
                name, text = line, ""
            refs[name.strip()] = text.strip()
    return refs


def wav_info(path):
    with wave.open(path, "rb") as w:
        rate, ch, width, frames = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()
    return rate, ch, width, (frames / rate if rate else 0.0)


# ---------------------------------------------------------------- measurement helpers

def peak_ram_mb():
    """Peak resident memory of this process in MB, or None. The Windows branch is untested outside Windows."""
    try:
        if os.name == "nt":
            import ctypes
            from ctypes import wintypes

            class PMC(ctypes.Structure):
                _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD),
                            ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                            ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                            ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                            ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                            ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t)]

            kernel32, psapi = ctypes.windll.kernel32, ctypes.windll.psapi
            kernel32.GetCurrentProcess.restype = wintypes.HANDLE
            psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(PMC), wintypes.DWORD]
            c = PMC()
            c.cb = ctypes.sizeof(c)
            if psapi.GetProcessMemoryInfo(kernel32.GetCurrentProcess(), ctypes.byref(c), c.cb):
                return c.PeakWorkingSetSize / 1048576.0
            return None
        import resource
        return resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024.0   # Linux: kilobytes
    except Exception:
        return None


class GpuSampler:
    """Polls nvidia-smi (first GPU) while a worker runs and keeps the highest memory use."""

    def __init__(self):
        self.exe = shutil.which("nvidia-smi")
        self.base = None
        self.peak = None
        self._stop = threading.Event()
        self._thread = None

    def _read(self):
        try:
            out = subprocess.run([self.exe, "--query-gpu=memory.used", "--format=csv,noheader,nounits"],
                                 capture_output=True, text=True, timeout=5).stdout.split()
            return int(out[0])
        except Exception:
            return None

    def _loop(self):
        while not self._stop.is_set():
            v = self._read()
            if v is not None and (self.peak is None or v > self.peak):
                self.peak = v
            self._stop.wait(0.25)

    def start(self):
        if not self.exe:
            return
        self.base = self._read()
        self.peak = self.base
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self):
        """Returns the growth in MB, or None when nvidia-smi is not available."""
        if self._thread is None:
            return None
        self._stop.set()
        self._thread.join(timeout=3)
        if self.base is None or self.peak is None:
            return None
        return float(max(0, self.peak - self.base))


# ---------------------------------------------------------------- backends (used inside the worker process)

class FakeBackend:
    """Returns the text of <wav>.fake.txt when present, else STT_BENCH_FAKE_TEXT. Only for testing the script."""
    device_used = "none"

    def load(self):
        pass

    def transcribe(self, path):
        side = path + ".fake.txt"
        if os.path.exists(side):
            with open(side, encoding="utf-8") as f:
                return f.read().strip()
        return os.environ.get("STT_BENCH_FAKE_TEXT", "")


class VietAsrBackend:
    """pip install viet-asr. API as read from the model page in the previous session (UNVERIFIED here,
    the sandbox cannot reach huggingface.co): Pipeline.preset('transcribe').transcribe(path).text"""

    def __init__(self, device, lang, compute):
        if lang != "vi":
            raise RuntimeError("viet_asr is Vietnamese only")
        self.device_used = "cpu (device option not used)"

    def load(self):
        import vietasr
        self.pipe = vietasr.Pipeline.preset("transcribe")

    def transcribe(self, path):
        return self.pipe.transcribe(path).text


class FasterWhisperBackend:
    """pip install faster-whisper. name is a model name (small.en) or a CTranslate2 model folder or repo id."""

    def __init__(self, name, device, lang, compute):
        self.name, self.device, self.lang = name, device, lang
        self.compute = compute or ("float16" if device == "cuda" else "int8")
        self.device_used = "%s/%s" % (device, self.compute)
        if name.endswith(".en") and lang != "en":
            raise RuntimeError("English-only model %s needs --lang en" % name)

    def load(self):
        from faster_whisper import WhisperModel
        self.model = WhisperModel(self.name, device=self.device, compute_type=self.compute)

    def transcribe(self, path):
        language = None if self.name.endswith(".en") else self.lang
        # condition_on_previous_text off: a short turn has no context, and it limits repeated text.
        segments, _info = self.model.transcribe(path, language=language, beam_size=5, vad_filter=False,
                                                condition_on_previous_text=False)
        return "".join(s.text for s in segments).strip()


def make_backend(spec, device, lang, compute):
    if spec == "fake":
        return FakeBackend()
    if spec == "viet_asr":
        return VietAsrBackend(device, lang, compute)
    if spec.startswith("faster_whisper:"):
        return FasterWhisperBackend(spec.split(":", 1)[1], device, lang, compute)
    raise RuntimeError("unknown model spec: %s" % spec)


def worker_main(job_path):
    with open(job_path, encoding="utf-8") as f:
        job = json.load(f)
    result = {"model": job["model"], "files": [], "error": None, "load_s": None,
              "ram_peak_mb": None, "device_used": None}
    try:
        be = make_backend(job["model"], job["device"], job["lang"], job["compute"])
        result["device_used"] = be.device_used
        t0 = time.perf_counter()
        be.load()
        result["load_s"] = time.perf_counter() - t0
        for _ in range(job["warmup"]):
            # The first run is slow (graph and CUDA setup), keep it out of the timing.
            if job["wavs"]:
                try:
                    be.transcribe(job["wavs"][0])
                except Exception:
                    pass
        for path in job["wavs"]:
            t = time.perf_counter()
            try:
                hyp, err = be.transcribe(path), None
            except Exception as exc:
                hyp, err = "", "%s: %s" % (type(exc).__name__, exc)
            result["files"].append({"path": path, "hyp": hyp, "secs": time.perf_counter() - t, "error": err})
    except Exception as exc:
        result["error"] = "%s: %s" % (type(exc).__name__, exc)
    result["ram_peak_mb"] = peak_ram_mb()
    sys.stdout.write("\n" + WORKER_MARK + json.dumps(result, ensure_ascii=False) + "\n")
    sys.stdout.flush()
    return 0


# ---------------------------------------------------------------- parent

def run_model(spec, wavs, args, tmpdir):
    job = {"model": spec, "device": args.device, "lang": args.lang, "compute": args.compute,
           "warmup": args.warmup, "wavs": wavs}
    job_path = os.path.join(tmpdir, "job.json")
    with open(job_path, "w", encoding="utf-8") as f:
        json.dump(job, f, ensure_ascii=False)
    gpu = GpuSampler()
    gpu.start()
    proc = subprocess.run([sys.executable, os.path.abspath(__file__), "--worker", job_path],
                          stdout=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
    vram = gpu.stop()
    result = None
    for line in proc.stdout.splitlines():
        if line.startswith(WORKER_MARK):
            result = json.loads(line[len(WORKER_MARK):])
    if result is None:
        result = {"model": spec, "files": [], "load_s": None, "ram_peak_mb": None, "device_used": None,
                  "error": "worker ended with code %s and no result" % proc.returncode}
    result["vram_growth_mb"] = vram
    return result


def evaluate(result, refs, durations):
    """Scores one worker result. Returns (summary, per_file_rows)."""
    rows = []
    tot = {"rw": 0, "we": 0, "nd": 0, "rc": 0, "ce": 0, "proc": 0.0, "audio": 0.0, "ok": 0, "err": 0,
           "halluc": 0, "empty_refs": 0, "max_s": 0.0}
    for f in result.get("files", []):
        name = os.path.basename(f["path"])
        ref = refs.get(name)
        if f["error"] is not None:
            tot["err"] += 1
            rows.append({"file": name, "error": f["error"]})
            continue
        row = {"file": name, "audio_s": durations[f["path"]], "proc_s": f["secs"], "hyp": f["hyp"], "ref": ref}
        if ref is not None:
            sc = score_pair(ref, f["hyp"])
            row.update(sc)
            tot["rw"] += sc["ref_words"]
            tot["we"] += sc["word_errs"]
            tot["nd"] += sc["nodiac_errs"]
            tot["rc"] += sc["ref_chars"]
            tot["ce"] += sc["char_errs"]
            if sc["ref_norm"] == "":
                tot["empty_refs"] += 1
                if sc["hyp_norm"] != "":
                    tot["halluc"] += 1
        tot["ok"] += 1
        tot["proc"] += f["secs"]
        tot["audio"] += row["audio_s"]
        tot["max_s"] = max(tot["max_s"], f["secs"])
        rows.append(row)
    summary = {
        "model": result["model"],
        "device": result.get("device_used"),
        "error": result.get("error"),
        "files_ok": tot["ok"],
        "files_failed": tot["err"],
        "wer": pct(tot["we"], tot["rw"]),
        "wer_nodiac": pct(tot["nd"], tot["rw"]),
        "cer": pct(tot["ce"], tot["rc"]),
        "proc_per_audio_s": (tot["proc"] / tot["audio"]) if tot["audio"] > 0 else None,
        "max_file_s": tot["max_s"],
        "load_s": result.get("load_s"),
        "ram_peak_mb": result.get("ram_peak_mb"),
        "vram_growth_mb": result.get("vram_growth_mb"),
        "empty_refs": tot["empty_refs"],
        "hallucinations": tot["halluc"],
    }
    return summary, rows


def fmt(v, spec="%.1f"):
    return "n/a" if v is None else spec % v


def run_benchmark(args):
    wavs = sorted(os.path.join(args.wav_dir, n) for n in os.listdir(args.wav_dir) if n.lower().endswith(".wav"))
    if not wavs:
        print("no WAV files in %s" % args.wav_dir)
        return None
    durations = {}
    for p in wavs:
        rate, ch, width, secs = wav_info(p)
        durations[p] = secs
        if (rate, ch, width) != (16000, 1, 2):
            print("warning: %s is %d Hz, %d ch, %d bytes per sample (the board sends 16000 Hz, 1 ch, 2 bytes)"
                  % (os.path.basename(p), rate, ch, width))
    refs = {}
    if args.refs:
        refs = read_refs(args.refs)
        names = {os.path.basename(p) for p in wavs}
        for n in sorted(set(refs) - names):
            print("warning: reference for a missing file: %s" % n)
        for n in sorted(names - set(refs)):
            print("warning: no reference for %s (it is run but not scored)" % n)
    elif not args.draft:
        print("give --refs (or --draft to only write draft sentences)")
        return None

    out_dir = args.out_dir
    os.makedirs(out_dir, exist_ok=True)
    summaries, all_rows, draft_done = [], [], False
    with tempfile.TemporaryDirectory() as tmpdir:
        for spec in args.models:
            print("\n=== %s: %d files, device %s, lang %s ===" % (spec, len(wavs), args.device, args.lang),
                  flush=True)
            result = run_model(spec, wavs, args, tmpdir)
            if result.get("error"):
                print("model failed: %s" % result["error"])
            summary, rows = evaluate(result, refs, durations)
            summaries.append(summary)
            for r in rows:
                r["model"] = spec
                all_rows.append(r)
                if "error" in r:
                    print("  %s: ERROR %s" % (r["file"], r["error"]))
                else:
                    tag = "" if r["ref"] is None else "  wer_errs %d/%d" % (r["word_errs"], r["ref_words"])
                    print("  %s  %.2fs audio, %.2fs proc%s" % (r["file"], r["audio_s"], r["proc_s"], tag))
                    if r["ref"] is not None:
                        print("    ref: %s" % r["ref"])
                    print("    hyp: %s" % r["hyp"])
            if args.draft and not draft_done and summary["files_ok"] > 0:
                with open(args.draft, "w", encoding="utf-8-sig", newline="") as f:
                    for r in rows:
                        if "error" not in r:
                            f.write("%s\t%s\n" % (r["file"], r["hyp"].replace("\t", " ").replace("\n", " ")))
                print("draft sentences written to %s (correct them by hand, then use as --refs)" % args.draft)
                draft_done = True

    print("\n=== summary ===")
    print("%-34s %6s %8s %6s %9s %6s %7s %8s %8s %6s" % (
        "model", "ok", "WER%", "CER%", "proc/aud", "max_s", "load_s", "RAM_MB", "VRAM_MB", "halluc"))
    for s in summaries:
        print("%-34s %6d %8s %6s %9s %6s %7s %8s %8s %6s" % (
            s["model"][:34], s["files_ok"], fmt(s["wer"]), fmt(s["cer"]), fmt(s["proc_per_audio_s"], "%.2f"),
            fmt(s["max_file_s"], "%.2f"), fmt(s["load_s"]), fmt(s["ram_peak_mb"], "%.0f"),
            fmt(s["vram_growth_mb"], "%.0f"), "%d/%d" % (s["hallucinations"], s["empty_refs"])))
    print("proc/aud = processing seconds per second of audio (below 1 is faster than real time).")

    stamp = time.strftime("%Y%m%d_%H%M%S")
    with open(os.path.join(out_dir, "bench_%s.json" % stamp), "w", encoding="utf-8") as f:
        json.dump({"summaries": summaries, "files": all_rows}, f, ensure_ascii=False, indent=1)
    with open(os.path.join(out_dir, "bench_%s.csv" % stamp), "w", encoding="utf-8-sig", newline="") as f:
        cols = ["model", "file", "audio_s", "proc_s", "ref", "hyp", "ref_words", "word_errs", "nodiac_errs",
                "ref_chars", "char_errs", "error"]
        w = csv.DictWriter(f, fieldnames=cols, extrasaction="ignore")
        w.writeheader()
        w.writerows(all_rows)
    print("results saved in %s (bench_%s.json and .csv)" % (out_dir, stamp))
    return summaries


# ---------------------------------------------------------------- self test

def _write_wav(path, seconds, tone):
    import math
    import struct
    n = int(16000 * seconds)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(16000)
        w.writeframes(b"".join(
            struct.pack("<h", int(8000 * math.sin(2 * math.pi * 440 * i / 16000)) if tone else 0)
            for i in range(n)))


def selftest():
    fails = []

    def check(cond, what):
        if not cond:
            fails.append(what)
            print("FAIL: " + what)

    check(normalize("Xin ch\u00e0o, Vi\u1ec7t Nam!") == "xin ch\u00e0o vi\u1ec7t nam",
          "normalize keeps diacritics, drops punctuation")
    check(normalize("e\u0302\u0309") == normalize("\u1ec3"), "normalize joins decomposed marks (NFC)")
    check(normalize("Don't stop - now.") == "dont stop now", "normalize apostrophe and hyphen")
    check(strip_diacritics("\u0111\u1eb7c bi\u1ec7t") == "dac biet", "strip_diacritics")
    check(edit_distance(["a", "b", "c"], ["a", "c"]) == 1, "edit_distance deletion")
    check(edit_distance("kitten", "sitting") == 3, "edit_distance classic case")
    sc = score_pair("qu\u1ea3 t\u00e1o", "qua t\u00e1o")
    check(sc["word_errs"] == 1 and sc["nodiac_errs"] == 0, "tone error counts for WER, not for nodiac")

    work = tempfile.mkdtemp()
    try:
        wav_dir = os.path.join(work, "wavs")
        os.makedirs(wav_dir)
        # name, seconds, tone, reference, fake hypothesis
        cases = [("a.wav", 1.0, True, "xin ch\u00e0o c\u00e1c b\u1ea1n", "xin ch\u00e0o b\u1ea1n"),
                 ("b.wav", 2.0, True, "qu\u1ea3 t\u00e1o", "qu\u1ea3 t\u00e1o"),
                 ("c.wav", 1.0, False, "", "c\u1ea3m \u01a1n c\u00e1c b\u1ea1n \u0111\u00e3 xem")]
        refs = os.path.join(work, "refs.tsv")
        with open(refs, "w", encoding="utf-8-sig") as rf:
            rf.write("# test\n")
            for name, secs, tone, ref, hyp in cases:
                _write_wav(os.path.join(wav_dir, name), secs, tone)
                with open(os.path.join(wav_dir, name + ".fake.txt"), "w", encoding="utf-8") as hf:
                    hf.write(hyp)
                rf.write("%s\t%s\n" % (name, ref))
        a = argparse.Namespace(wav_dir=wav_dir, refs=refs, models=["fake", "viet_asr_missing"], device="cpu",
                               lang="vi", compute=None, warmup=1, out_dir=os.path.join(work, "out"), draft=None)
        summaries = run_benchmark(a)
        check(summaries is not None and len(summaries) == 2, "two summaries")
        if summaries:
            s = summaries[0]
            check(s["files_ok"] == 3 and s["files_failed"] == 0, "fake model ran all files")
            # 1 error (a.wav) + 0 (b.wav) + 6 hallucinated words (c.wav) = 7 errors over 4 + 2 + 0 = 6 words.
            check(abs(s["wer"] - 100.0 * 7 / 6) < 1e-6,
                  "corpus WER is 7 errors in 6 reference words (got %s)" % s["wer"])
            check(s["hallucinations"] == 1 and s["empty_refs"] == 1, "silence file counted as a hallucination")
            check(s["proc_per_audio_s"] is not None and s["ram_peak_mb"] is not None, "timing and RAM measured")
            check(summaries[1]["error"] is not None, "an unknown model is reported, not fatal")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print("\nSELFTEST: " + ("ALL PASSED" if not fails else "%d FAILED" % len(fails)))
    return 0 if not fails else 1


def main():
    ap = argparse.ArgumentParser(description="STT benchmark on WAV files (see the top of this file).")
    ap.add_argument("--wav-dir")
    ap.add_argument("--refs")
    ap.add_argument("--models", nargs="+", default=["viet_asr"])
    ap.add_argument("--device", choices=["cpu", "cuda"], default="cpu")
    ap.add_argument("--lang", choices=["vi", "en"], default="vi")
    ap.add_argument("--compute", default=None, help="faster-whisper compute type, for example int8 or float16")
    ap.add_argument("--warmup", type=int, default=1, help="untimed runs of the first file before timing")
    ap.add_argument("--out-dir", default="stt_bench_out")
    ap.add_argument("--draft", help="write the first working model's sentences here as a draft reference file")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--worker", help=argparse.SUPPRESS)
    a = ap.parse_args()
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass
    if a.worker:
        return worker_main(a.worker)
    if a.selftest:
        return selftest()
    if not a.wav_dir:
        ap.error("--wav-dir is required")
    return 0 if run_benchmark(a) is not None else 1


if __name__ == "__main__":
    sys.exit(main())