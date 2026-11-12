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
#ifndef APP_MIC_LIVE_PLAY
#define APP_MIC_LIVE_PLAY 1
#endif
/* Speaker volume in percent while the live playback runs. Start low: the speaker is near the microphone. */
#ifndef APP_MIC_LIVE_VOLUME
#define APP_MIC_LIVE_VOLUME 10U
#endif
/* Whole number gain applied to the microphone samples before playback (saturates at full scale). */
#ifndef APP_MIC_LIVE_GAIN
#define APP_MIC_LIVE_GAIN 1
#endif
/* Howl guard: if the microphone peak stays at or above this for 300 ms, the speaker is muted for the time below. */
#ifndef APP_MIC_LIVE_HOWL_PEAK
#define APP_MIC_LIVE_HOWL_PEAK 20000
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
 *   With APP_MIC_LIVE_PLAY the samples (DC offset removed, times APP_MIC_LIVE_GAIN) are written to
 *   the speaker at APP_MIC_LIVE_VOLUME. Speaker and microphone are close, so this can howl. The howl
 *   guard mutes the speaker for APP_MIC_LIVE_MUTE_MS when the microphone is loud for 300 ms in a
 *   row. The task does not end while the monitor runs (by default never).
 *
 * Microphone wiring (GIVEN by the owner, see board.h): SCK to GPIO 10, WS to GPIO 11, SD to
 * GPIO 12, L/R to GND, a 3.3 V supply. The task ends when the tests (and the live monitor) are done. Wi-Fi and HTTP are
 * not affected. Returns 0 when the task was created, -1 when it could not be created. Call it
 * once, from app_start in app.c.
 */
int mic_app_start(void);

#endif /* MIC_APP_H */