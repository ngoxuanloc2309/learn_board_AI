#ifndef MIC_APP_H
#define MIC_APP_H

#ifndef APP_MIC_TASK_NAME
#define APP_MIC_TASK_NAME "app_mic_task"
#endif

/*
 * Stack in bytes. The task uses a static buffer for one block of microphone data, so the stack
 * holds only the call chain of printf and the I2S driver. Chosen by design, not measured. The
 * task prints the lowest free stack when it ends.
 */
#ifndef APP_MIC_STACK_SIZE
#define APP_MIC_STACK_SIZE 6144
#endif

/* Below the Wi-Fi task (3), same as the other bring-up tasks. Chosen by design, not measured. */
#ifndef APP_MIC_TASK_PRIORITY
#define APP_MIC_TASK_PRIORITY 2
#endif

/*
 * How long the task waits for speaker_app to finish its tone test before it touches any hardware.
 * The tone test lasts about 3 s, so this leaves a wide margin. Chosen by design. If it runs out
 * (for example speaker_app_start was never called), the level meter still runs but record and play
 * is skipped.
 */
#ifndef APP_MIC_WAIT_SPEAKER_MS
#define APP_MIC_WAIT_SPEAKER_MS 15000U
#endif

/* How long the level meter of the microphone runs. Chosen by design. */
#ifndef APP_MIC_METER_MS
#define APP_MIC_METER_MS 5000U
#endif

/*
 * Time thrown away after the microphone clock starts, because the first samples are not valid.
 * Chosen by design (the usual INMP441 datasheet gives a start-up time, not checked on this board).
 */
#ifndef APP_MIC_SETTLE_MS
#define APP_MIC_SETTLE_MS 200U
#endif

/*
 * Length of the recording that is played back. 0 skips that test. Taken from the heap while it runs.
 * Design. The default is 0 now that the live monitor below does the same job continuously.
 */
#ifndef APP_MIC_RECORD_MS
#define APP_MIC_RECORD_MS 3000U
#endif

/*
 * Live monitor: after the level meter the task reads the microphone continuously, prints one line
 * per second and (when APP_MIC_LIVE_PLAY is 1) plays what it hears on the speaker. APP_MIC_LIVE_MS
 * is how long it runs: APP_MIC_LIVE_FOREVER (default) never ends, 0 skips the monitor, any other
 * value is a time in ms. All values here are chosen by design, not measured.
 */
#define APP_MIC_LIVE_FOREVER 0xFFFFFFFFU
#ifndef APP_MIC_LIVE_MS
#define APP_MIC_LIVE_MS APP_MIC_LIVE_FOREVER//0U//APP_MIC_LIVE_FOREVER
#endif
/*
 * Diagnostic (1 = run it once, after the record and play step and before the live monitor). It looks
 * for the reason why the continuous live path (microphone and speaker together) sounds harsh while
 * the recorded playback is clean. It needs a SILENT room (do not talk, do not touch the board).
 * The microphone is started once and kept running. First a trace of APP_MIC_DIAG_SETTLE_MS shows how
 * the DC and the noise decay after the start (one line per 250 ms). Then five phases of
 * APP_MIC_DIAG_MS each: A1 mic only, B1 mic with the speaker running on digital silence (clocks and
 * amplifier run, no sound), A2 mic only, B2 speaker again, A3 mic only. Only the speaker is started
 * and stopped between phases. Each phase prints peak and rms after the 100 Hz high-pass, rms of the
 * raw signal with the mean removed, the mean, and the smallest and largest rms of a 100 ms window.
 * All lines are printed at the end, none while the microphone runs. There is no automatic
 * conclusion. Set 0 when it is no longer needed. Chosen by design.
 */
/*
 * Coupling probe (1 = run it once, after the record and play step, before the live monitor). It
 * measures how much of what the speaker plays reaches the microphone, as a sweep of sine tones from
 * 400 to 3000 Hz (14 frequencies, one amplitude, volume APP_MIC_LIVE_VOLUME), with microphone and
 * speaker running together and the room silent. A first phase with no tone is the baseline.
 * Per phase it prints the amplitude of the tone found in the microphone signal (Goertzel filter), the
 * level at an unrelated frequency (the floor of the measurement), the rms, and the ratio of mic tone
 * to output amplitude in per mille. A sharp peak at one frequency shows where the loop is most likely to ring.
 * Run it twice with the same build: speaker connected, then speaker wires unplugged (the amplifier
 * stays powered). The tone still in the microphone without the speaker means the path is not the
 * speaker or the air. Takes about 24 s. Chosen by design.
 */
#ifndef APP_MIC_COUPLING
#define APP_MIC_COUPLING 1
#endif
#ifndef APP_MIC_DIAG
#define APP_MIC_DIAG 0
#endif
#ifndef APP_MIC_DIAG_MS
#define APP_MIC_DIAG_MS 3000U
#endif
#ifndef APP_MIC_DIAG_SETTLE_MS
#define APP_MIC_DIAG_SETTLE_MS 5000U
#endif
#ifndef APP_MIC_LIVE_PLAY
#define APP_MIC_LIVE_PLAY 1
#endif
/*
 * Segment mode (default, 1): the live monitor does not run the microphone and the speaker together.
 * It repeats exactly what the recorded playback does, on short pieces: record APP_MIC_SEG_MS with the
 * speaker off, stop the microphone, play the piece (100 Hz high-pass and the same automatic scaling
 * as the recorded playback, at APP_MIC_PLAY_VOLUME) with the microphone off, and start again. There
 * is no acoustic loop, so no howl, and the sound path is the one that was heard to be clean. The cost:
 * the sound is delayed by about APP_MIC_SEG_MS plus the start-up time of the microphone, and what is
 * said while a piece is playing is not recorded. A piece whose peak (after the high-pass) is below
 * APP_MIC_SEG_GATE is room noise and is not played, so there is no hiss between sentences. Set 0 to
 * get the old continuous live path (microphone and speaker together). Chosen by design, not measured.
 */
#ifndef APP_MIC_LIVE_SEGMENT
#define APP_MIC_LIVE_SEGMENT 1
#endif
#ifndef APP_MIC_SEG_MS
#define APP_MIC_SEG_MS 1500U
#endif
#ifndef APP_MIC_SEG_GATE
#define APP_MIC_SEG_GATE 150
#endif
/*
 * Quiet live monitor: 1 means the live loop prints nothing, measures nothing and runs no band
 * filters, so it only reads the microphone and writes the speaker. A print on the 115200 baud
 * console takes tens of ms, longer than the lead the speaker has, and a stall of the reader drops
 * microphone data without an error (ESP-IDF v5.4.3, i2s_common.c, rx callback), so each print could
 * be heard as a crackle. Set 0 to get the one-line-per-second report and the howl guard messages
 * back (for diagnosis). Chosen by design, not measured.
 */
#ifndef APP_MIC_LIVE_QUIET
#define APP_MIC_LIVE_QUIET 1
#endif
/* Speaker volume in percent while the live playback runs. Start low: the speaker is near the microphone. */
#ifndef APP_MIC_LIVE_VOLUME
#define APP_MIC_LIVE_VOLUME 30U
#endif
/* Whole number gain, used only when APP_MIC_LIVE_AGC is 0 (saturates at full scale). */
#ifndef APP_MIC_LIVE_GAIN
#define APP_MIC_LIVE_GAIN 3
#endif
/*
 * Automatic gain for the live playback, the live version of what the recorded playback does (it
 * scales the recording so that its peak reaches APP_MIC_PLAY_PEAK, at most APP_MIC_MAX_GAIN times).
 * TARGET is the peak to reach, MAX the largest gain, START the gain at the beginning, GATE the chunk
 * peak (after the 100 Hz high-pass) below which the sound counts as room noise and the gain is held,
 * RELEASE the number of 8 ms chunks the gain takes to rise (a fall is immediate, so no clipping).
 * All chosen by design: the room noise after the high-pass was a peak of about 25 to 50 in one
 * recording, speech at 20 cm a peak of about 1000 to 4500. Set APP_MIC_LIVE_AGC to 0 for the fixed
 * APP_MIC_LIVE_GAIN. DEFAULT IS 0: with the speaker next to the microphone the AGC raises the gain
 * whenever the speaker sound reaches the microphone above GATE, which makes the feedback loop worse
 * (a first test with it on gave a loud howl before anybody spoke). The recorded playback has no such
 * loop because the speaker is off while it records.
 */
#ifndef APP_MIC_LIVE_AGC
#define APP_MIC_LIVE_AGC 0
#endif
#ifndef APP_MIC_LIVE_AGC_TARGET
#define APP_MIC_LIVE_AGC_TARGET 16000
#endif
#ifndef APP_MIC_LIVE_AGC_MAX
#define APP_MIC_LIVE_AGC_MAX 16
#endif
#ifndef APP_MIC_LIVE_AGC_START
#define APP_MIC_LIVE_AGC_START 4
#endif
#ifndef APP_MIC_LIVE_AGC_GATE
#define APP_MIC_LIVE_AGC_GATE 120
#endif
#ifndef APP_MIC_LIVE_AGC_RELEASE
#define APP_MIC_LIVE_AGC_RELEASE 64
#endif
/*
 * Howl guard trigger: the rms of the output (the microphone after the 100 Hz filter, times
 * APP_MIC_LIVE_GAIN, limited to 16 bit) over a window of 100 ms. When it stays at or above this for
 * 300 ms the guard mutes the speaker briefly to test whether it is feedback or a voice (see the end of
 * this file). Speech has a peak about four times its rms, so a loud voice reaches the full scale with
 * its peaks at an rms of about a third of it, while a howl is a tone that grows until it clips, so its
 * rms is 20000 or more. A log with the microphone at the mouth gave about 8400 at the speaker (gain 3).
 * The trigger level is chosen by design, not measured on a howl.
 */
#ifndef APP_MIC_LIVE_HOWL_RMS
#define APP_MIC_LIVE_HOWL_RMS 16000
#endif
#ifndef APP_MIC_LIVE_MUTE_MS
#define APP_MIC_LIVE_MUTE_MS 3000U
#endif

/* Speaker volume while the recording is played, in percent. Chosen by design. */
#ifndef APP_MIC_PLAY_VOLUME
#define APP_MIC_PLAY_VOLUME 40U
#endif

/*
 * The recording is amplified in software so that its peak reaches APP_MIC_PLAY_PEAK, but never
 * by more than APP_MIC_MAX_GAIN times, so that room noise is not turned into loud noise.
 * Both are chosen by design.
 */
#ifndef APP_MIC_PLAY_PEAK
#define APP_MIC_PLAY_PEAK 20000
#endif
#ifndef APP_MIC_MAX_GAIN
#define APP_MIC_MAX_GAIN 16
#endif

/*
 * Creates a task for the microphone bring-up test. Each step prints one log line prefixed with
 * the tag mic in square brackets. The task runs in this order:
 *
 * 0 Wait (at most APP_MIC_WAIT_SPEAKER_MS) until speaker_app_tone_test_done is true, so the
 *   speaker and the microphone never run together and no driver is initialised by two tasks at
 *   once. Then it calls board_init and board_init_mic. If the microphone cannot be initialised
 *   the test is skipped.
 *
 * 1 Level meter: for APP_MIC_METER_MS the task prints one line per 100 ms with the peak, the RMS
 *   and the DC offset of that block (16 bit samples) and a bar of hash characters, one per bit of
 *   the peak. TALK OR CLAP during this test. The verdict line then says: all samples zero (the
 *   microphone is silent or sends on the right slot: L/R must be tied to GND), samples that never
 *   change (stuck data line), or samples that vary. For the last case compare the quietest and the
 *   loudest block yourself: speech should be far above the room noise. The code does not judge a
 *   noise level, because the real noise level of this board is not known.
 *
 * 2 Record and play: after a one second countdown the task records APP_MIC_RECORD_MS of the
 *   microphone into a buffer from the heap, amplifies it (see APP_MIC_MAX_GAIN), plays it on the
 *   speaker (taken from board_get_speaker) at APP_MIC_PLAY_VOLUME and frees the buffer. Skipped
 *   when the level meter did not end ok, when the speaker is not available or the speaker test
 *   did not end in time, when the microphone gave only zeros, or when the heap cannot hold the
 *   buffer.
 *
 * 3 Live monitor (see APP_MIC_LIVE_MS): only when the level meter ended ok. The task reads the
 *   microphone in 8 ms chunks and prints one line per second:
 *     peak  the largest absolute sample in that second,
 *     ac    the RMS of that second after its mean was removed,
 *     dc    the mean (the DC offset),
 *     LOW, MID, HIGH  the RMS of the sound inside three frequency bands, each followed by a bar of
 *           hash characters (one per bit of the level, at most 12).
 *   The bands are LOW 100 to 400 Hz, MID 600 to 1700 Hz, HIGH 2500 to 7000 Hz (the -3 dB edges).
 *   Each band is a 2nd order high-pass followed by a 2nd order low-pass, so the sides are gentle
 *   (12 dB per octave): a tone just outside a band still shows partly, and a tone between two
 *   bands (for example 500 Hz) shows in both. The levels are relative and not calibrated. The
 *   high-pass removes the DC offset, so a large dc with small band levels means the "sound" is only
 *   an offset.
 *   Sounds that should show up (general speech knowledge, not measured on this board): humming
 *   "mmm" or a low "ooo" (voice pitch about 85 to 255 Hz) lifts LOW; a long "aaa" (vowel
 *   resonances near 700 and 1100 Hz) lifts MID; a hissing "sss" (energy mostly above 4 kHz) lifts
 *   HIGH; a clap lifts all three. Silence should keep all three low.
 *   With APP_MIC_LIVE_PLAY the samples (100 Hz high-pass, times APP_MIC_LIVE_GAIN) are written to
 *   the speaker at APP_MIC_LIVE_VOLUME. Speaker and microphone are close, so this can howl. When the
 *   output rms stays very high for 300 ms the howl guard mutes the speaker for 400 ms and listens: if the
 *   microphone is still above APP_MIC_LIVE_GUARD_VOICE_PEAK (set in mic_app.c) it was a voice and the
 *   speaker comes back, otherwise it was feedback and the speaker stays muted for
 *   APP_MIC_LIVE_MUTE_MS in total. The task does not end while the monitor runs (by default never).
 *
 * Microphone wiring (GIVEN by the owner, see board.h): SCK to GPIO 10, WS to GPIO 11, SD to
 * GPIO 12, L/R to GND, a 3.3 V supply. The task ends when the tests (and the live monitor) are done. Wi-Fi and HTTP are
 * not affected. Returns 0 when the task was created, -1 when it could not be created. Call it
 * once, from app_start in app.c.
 */
int mic_app_start(void);

#endif /* MIC_APP_H */