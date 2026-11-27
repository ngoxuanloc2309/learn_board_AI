#!/usr/bin/env python3
"""Build a supervised training set in the layout VietASR expects.

Standard library only. Layout written under --out-dir:

    train/data1/uttNNNNN.wav ...  and  data1.trans.txt   (lines: "uttNNNNN TEXT")
    dev/data1/...                                          (same layout, held out)
    manifest.tsv     (utt id, split, original wav name, text)

This matches ASR/local/prepare_manifest.py of VietASR: it looks for the
folders train, dev, test, reads every *.trans.txt below them, and opens
<id>.wav next to the transcript file. A "test" folder is optional.

Pairing mode 1 (default): recordings and prompts are matched by ORDER.
The board names files by time, so the sorted file names follow the order in
which the sentences were read. Use --first-file to say which recording is the
first prompt, and --drop-wav to remove misread or repeated takes BEFORE
pairing. Use --list first: it prints the pairing and writes nothing.

Pairing mode 2: --pairs FILE with lines "name.wav|sentence" (TAB also works).

Text rules: NFC, upper case, punctuation removed, one space between words.
Upper case is a guess based on the checkpoint printing upper case: check it
against tokens.txt of the checkpoint, and use --keep-case if tokens are not
upper case.

Held-out rule: --exclude-dir lists folders whose WAV names are never used for
training (give the benchmark folder, so it cannot leak into training).

Windows note: VietASR opens *.trans.txt without an encoding argument, which
breaks Vietnamese text under a legacy code page. Run the VietASR prepare step
on Linux (Colab), or set PYTHONUTF8=1 on Windows.

Examples:
  python make_train_set.py --list --wav-dir recordings --prompts train_prompts_vi.txt --first-file 20261125_100000_turn30.wav
  python make_train_set.py --wav-dir recordings --prompts train_prompts_vi.txt --first-file 20261125_100000_turn30.wav --drop-wav a.wav,b.wav --exclude-dir bench2 --out-dir trainset
  python make_train_set.py --selftest
"""
import argparse
import os
import random
import shutil
import sys
import tempfile
import unicodedata
import wave


def clean_text(text, keep_case=False):
    t = unicodedata.normalize("NFC", text)
    out = []
    for ch in t:
        if unicodedata.category(ch)[0] in ("P", "S"):
            out.append(" ")
        else:
            out.append(ch)
    t = " ".join("".join(out).split())
    return t if keep_case else t.upper()


def wav_check(path):
    """Returns (ok, seconds, message)."""
    try:
        with wave.open(path, "rb") as w:
            rate, ch, width, frames = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()
    except Exception as exc:
        return False, 0.0, "cannot read: %s" % exc
    secs = frames / rate if rate else 0.0
    if (rate, ch, width) != (16000, 1, 2):
        return False, secs, "%d Hz, %d ch, %d bytes per sample (need 16000 Hz, 1 ch, 2 bytes)" % (rate, ch, width)
    return True, secs, ""


def read_lines(path):
    with open(path, encoding="utf-8-sig") as f:
        return [ln.rstrip("\r\n") for ln in f if ln.strip() and not ln.lstrip().startswith("#")]


def pair_by_order(args):
    names = sorted(n for n in os.listdir(args.wav_dir) if n.lower().endswith(".wav"))
    if args.first_file:
        if args.first_file not in names:
            raise SystemExit("--first-file %s is not in %s" % (args.first_file, args.wav_dir))
        names = names[names.index(args.first_file):]
    drop = {n.strip() for n in (args.drop_wav or "").split(",") if n.strip()}
    for n in sorted(drop - set(names)):
        print("warning: --drop-wav name not found: %s" % n)
    names = [n for n in names if n not in drop]
    prompts = read_lines(args.prompts)
    if len(names) != len(prompts):
        print("warning: %d recordings but %d prompts; pairing the first %d only"
              % (len(names), len(prompts), min(len(names), len(prompts))))
    return [(os.path.join(args.wav_dir, n), n, p) for n, p in zip(names, prompts)]


def pair_from_file(args):
    pairs = []
    for ln in read_lines(args.pairs):
        if "\t" in ln:
            name, _, text = ln.partition("\t")
        else:
            name, _, text = ln.partition("|")
        name = name.strip()
        pairs.append((os.path.join(args.wav_dir, name), name, text.strip()))
    return pairs


def collect(args):
    """Pair, check and clean. Returns (items, skipped, total_seconds)."""
    pairs = pair_from_file(args) if args.pairs else pair_by_order(args)
    excluded = set()
    for d in (args.exclude_dir or []):
        excluded |= {n for n in os.listdir(d) if n.lower().endswith(".wav")}
    items, skipped, total_s = [], 0, 0.0
    for path, name, text in pairs:
        if name in excluded:
            print("excluded (held out): %s" % name)
            skipped += 1
            continue
        ok, secs, msg = wav_check(path)
        if not ok:
            print("skipped %s: %s" % (name, msg))
            skipped += 1
            continue
        txt = clean_text(text, args.keep_case)
        if not txt:
            print("skipped %s: empty text" % name)
            skipped += 1
            continue
        if secs < 0.5 or secs > 30.0:
            print("warning: %s lasts %.1f s" % (name, secs))
        items.append((path, name, txt, secs))
        total_s += secs
    return items, skipped, total_s


def list_pairs(args):
    items, skipped, total_s = collect(args)
    for _path, name, txt, secs in items:
        print("%-44s %5.1f s  %s" % (name, secs, txt))
    print("\n%d usable, %d skipped, %.1f minutes (nothing written)" % (len(items), skipped, total_s / 60.0))
    return 0


def build(args):
    items, skipped, total_s = collect(args)
    if not items:
        raise SystemExit("nothing to write")
    rng = random.Random(args.seed)
    order = list(range(len(items)))
    rng.shuffle(order)
    n_dev = max(1, int(round(len(items) * args.dev_fraction))) if len(items) > 1 else 0
    dev_set = set(order[:n_dev])
    for split in ("train", "dev"):
        os.makedirs(os.path.join(args.out_dir, split, "data1"), exist_ok=True)
    trans = {"train": [], "dev": []}
    manifest = []
    for i, (path, name, txt, _secs) in enumerate(items, 1):
        split = "dev" if (i - 1) in dev_set else "train"
        utt = "utt%05d" % i
        shutil.copyfile(path, os.path.join(args.out_dir, split, "data1", utt + ".wav"))
        trans[split].append("%s %s" % (utt, txt))
        manifest.append("%s\t%s\t%s\t%s" % (utt, split, name, txt))
    for split in ("train", "dev"):
        with open(os.path.join(args.out_dir, split, "data1", "data1.trans.txt"), "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(trans[split]) + ("\n" if trans[split] else ""))
    with open(os.path.join(args.out_dir, "manifest.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(manifest) + "\n")
    words = {w for _p, _n, t, _s in items for w in t.split()}
    print("\nwritten to %s" % args.out_dir)
    print("train: %d, dev: %d, skipped: %d" % (len(trans["train"]), len(trans["dev"]), skipped))
    print("total audio: %.1f minutes, distinct words: %d" % (total_s / 60.0, len(words)))
    return 0


def _write_wav(path, seconds, rate=16000, channels=1):
    n = int(rate * seconds) * channels
    with wave.open(path, "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"\x01\x00" * n)


def selftest():
    fails = []

    def check(cond, what):
        if not cond:
            fails.append(what)
            print("FAIL: " + what)

    check(clean_text("Qu\u1ea3 t\u00e1o, ti\u1ebfng Anh!") == "QU\u1ea2 T\u00c1O TI\u1ebeNG ANH",
          "clean_text upper case, no punctuation")
    check(clean_text("\u0111\u1ecdc  l\u1ea1i") == "\u0110\u1eccC L\u1ea0I", "clean_text collapses spaces")
    check(clean_text("e\u0302\u0309") == clean_text("\u1ec3"), "clean_text joins decomposed marks")
    check(clean_text("Ab", keep_case=True) == "Ab", "keep_case keeps case")
    work = tempfile.mkdtemp()
    try:
        wd = os.path.join(work, "rec")
        ex = os.path.join(work, "bench")
        os.makedirs(wd)
        os.makedirs(ex)
        for n in ("a1.wav", "a2.wav", "a3.wav", "a4.wav", "a5.wav"):
            _write_wav(os.path.join(wd, n), 1.0)
        _write_wav(os.path.join(wd, "a6.wav"), 1.0, rate=8000)    # wrong rate, must be skipped
        _write_wav(os.path.join(ex, "a2.wav"), 1.0)               # held out
        pf = os.path.join(work, "p.txt")
        with open(pf, "w", encoding="utf-8") as f:
            f.write("# comment\nM\u1ed9t\nHai,\nBa\nB\u1ed1n\nN\u0103m\nS\u00e1u\n")
        a = argparse.Namespace(wav_dir=wd, prompts=pf, pairs=None, first_file="a1.wav", drop_wav="a3.wav",
                               exclude_dir=[ex], out_dir=os.path.join(work, "out"), dev_fraction=0.25,
                               seed=1, keep_case=False, list=False)
        # a3 dropped: a1,a2,a4,a5,a6 get Mot,Hai,Ba,Bon,Nam. a2 held out, a6 skipped.
        build(a)
        out = os.path.join(work, "out")
        lines = []
        for split in ("train", "dev"):
            tp = os.path.join(out, split, "data1", "data1.trans.txt")
            with open(tp, encoding="utf-8") as f:
                for ln in f.read().split("\n"):
                    if ln:
                        uid, text = ln.split(" ", 1)
                        lines.append((split, uid, text))
                        check(os.path.isfile(os.path.join(out, split, "data1", uid + ".wav")),
                              "wav exists next to transcript for " + uid)
        texts = sorted(t for _s, _u, t in lines)
        check(texts == sorted(["M\u1ed8T", "BA", "B\u1ed0N"]), "exact kept texts (got %r)" % (texts,))
        check(sum(1 for s, _u, _t in lines if s == "dev") == 1, "one dev utterance")
        check(sum(1 for s, _u, _t in lines if s == "train") == 2, "two train utterances")
        check(os.path.exists(os.path.join(out, "manifest.tsv")), "manifest written")
        # pairs mode
        pp = os.path.join(work, "pairs.txt")
        with open(pp, "w", encoding="utf-8") as f:
            f.write("a1.wav|Xin ch\u00e0o\na4.wav\tT\u1ea1m bi\u1ec7t!\n")
        b = argparse.Namespace(wav_dir=wd, prompts=None, pairs=pp, first_file=None, drop_wav=None,
                               exclude_dir=None, out_dir=os.path.join(work, "out2"), dev_fraction=0.5,
                               seed=1, keep_case=False, list=False)
        build(b)
        check(os.path.exists(os.path.join(work, "out2", "train", "data1", "data1.trans.txt")), "pairs mode output")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print("\nSELFTEST: " + ("ALL PASSED" if not fails else "%d FAILED" % len(fails)))
    return 0 if not fails else 1


def main():
    ap = argparse.ArgumentParser(description="Build a VietASR training set (see the top of this file).")
    ap.add_argument("--wav-dir")
    ap.add_argument("--prompts", help="text file, one sentence per line, in reading order")
    ap.add_argument("--pairs", help="alternative to --prompts: lines name.wav|sentence")
    ap.add_argument("--first-file", help="first WAV (by sorted name) that belongs to the prompts")
    ap.add_argument("--drop-wav", help="comma separated WAV names to remove before pairing")
    ap.add_argument("--exclude-dir", action="append", help="folder whose WAV names must not enter training (repeatable)")
    ap.add_argument("--out-dir", default="trainset")
    ap.add_argument("--dev-fraction", type=float, default=0.1)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--keep-case", action="store_true")
    ap.add_argument("--list", action="store_true", help="print the pairing and exit, write nothing")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass
    if a.selftest:
        return selftest()
    if not a.wav_dir:
        ap.error("--wav-dir is required")
    if not (a.prompts or a.pairs):
        ap.error("give --prompts (order pairing) or --pairs")
    if a.list:
        return list_pairs(a)
    return build(a)


if __name__ == "__main__":
    sys.exit(main())