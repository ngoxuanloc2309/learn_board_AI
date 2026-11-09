#ifndef AUDIO_APP_H
#define AUDIO_APP_H

#ifndef APP_AUDIO_TASK_NAME
#define APP_AUDIO_TASK_NAME "app_audio_task"
#endif

/*
 * Stack in bytes. The task uses static buffers for the audio, so the stack holds only the call
 * chain of printf and the I2S driver. Chosen by design, not measured. The task prints the lowest
 * free stack when it ends.
 */
#ifndef APP_AUDIO_STACK_SIZE
#define APP_AUDIO_STACK_SIZE 6144
#endif

/* Below the Wi-Fi task (3), same as the other bring-up tasks. Chosen by design, not measured. */
#ifndef APP_AUDIO_TASK_PRIORITY
#define APP_AUDIO_TASK_PRIORITY 2
#endif

/*
 * Speaker volume of the tone test, 0 to 100 percent of the sample values. Chosen by design, LOW on
 * purpose: the gain of the amplifier on the board is not verified, so the first sound must be
 * quiet. Raise it only after the first test was heard at a comfortable level.
 */
#ifndef APP_AUDIO_TONE_VOLUME
#define APP_AUDIO_TONE_VOLUME 20U
#endif

/* Peak of the tone before the volume is applied, in 16 bit sample units (full scale is 32767). Design. */
#ifndef APP_AUDIO_TONE_AMPLITUDE
#define APP_AUDIO_TONE_AMPLITUDE 16000
#endif

/* Length of each of the three tones and of the pause after each one. Chosen by design. */
#ifndef APP_AUDIO_TONE_MS
#define APP_AUDIO_TONE_MS 700U
#endif
#ifndef APP_AUDIO_GAP_MS
#define APP_AUDIO_GAP_MS 300U
#endif

/* How long the level meter of the microphone runs. Chosen by design. */
#ifndef APP_AUDIO_MIC_METER_MS
#define APP_AUDIO_MIC_METER_MS 5000U
#endif

/*
 * Time thrown away after the microphone clock starts, because the first samples are not valid.
 * Chosen by design (the usual INMP441 datasheet gives a start-up time, not checked on this board).
 */
#ifndef APP_AUDIO_MIC_SETTLE_MS
#define APP_AUDIO_MIC_SETTLE_MS 200U
#endif

/* Length of the recording that is played back. 0 skips that test. Taken from the heap while it runs. Design. */
#ifndef APP_AUDIO_RECORD_MS
#define APP_AUDIO_RECORD_MS 2000U
#endif

/* Speaker volume while the recording is played, percent. Chosen by design. */
#ifndef APP_AUDIO_PLAY_VOLUME
#define APP_AUDIO_PLAY_VOLUME 40U
#endif

/*
 * The recording is amplified in software so that its peak reaches APP_AUDIO_PLAY_PEAK, but never
 * by more than APP_AUDIO_MAX_GAIN times, so that room noise is not turned into loud noise.
 * Both are chosen by design.
 */
#ifndef APP_AUDIO_PLAY_PEAK
#define APP_AUDIO_PLAY_PEAK 20000
#endif
#ifndef APP_AUDIO_MAX_GAIN
#define APP_AUDIO_MAX_GAIN 16
#endif

/*
 * Creates a task for the speaker and microphone bring-up test. The task calls board_init,
 * board_init_speaker and board_init_mic, then runs three tests. A test whose device failed to start
 * is skipped, the others still run. Each step prints one line starting with [audio].
 *
 * 1 Speaker: three sine tones of 440, 880 and 1320 Hz, each APP_AUDIO_TONE_MS long with a short
 *   fade in and out, at the volume APP_AUDIO_TONE_VOLUME, then silence. The program can only check
 *   that the I2S driver accepts the data and that it is paced at the sample rate (the line says how
 *   many ms of audio were queued in how many ms). WHETHER THE TONES ARE HEARD, and how loud, is
 *   judged by ear: three rising beeps. If the log says ok but nothing is heard, the cause is
 *   outside the code: the amplifier may need an enable pin (not handled, not verified), the I2S
 *   pins in board.h are REF, or the speaker is not on SPK+ and SPK-.
 *
 * 2 Microphone level meter: for APP_AUDIO_MIC_METER_MS the task prints one line per 100 ms with the
 *   peak, the RMS and the DC offset of that block (16 bit samples) and a bar of # characters, one
 *   per bit of the peak. TALK OR CLAP during this test. The verdict line then says: all samples
 *   zero (the microphone is silent or sends on the right slot: L/R must be tied to GND), samples
 *   that never change (stuck data line), or samples that vary. For the last case compare the
 *   quietest and the loudest block yourself: speech should be far above the room noise. The code
 *   does not judge a noise level, because the real noise level of this board is not known.
 *
 * 3 Record and play: after a one second countdown the task records APP_AUDIO_RECORD_MS of the
 *   microphone into a buffer from the heap, amplifies it (see APP_AUDIO_MAX_GAIN), plays it on the
 *   speaker at APP_AUDIO_PLAY_VOLUME and frees the buffer. Skipped when the microphone gave only
 *   zeros, or when the heap cannot hold the buffer.
 *
 * Microphone wiring (GIVEN by the owner, see board.h): SCK to GPIO 10, WS to GPIO 11, SD to
 * GPIO 12, L/R to GND, a 3.3 V supply. The speaker goes on SPK+ and SPK- of the board amplifier,
 * never on a GPIO.
 *
 * The task ends when all tests are done. Wi-Fi and HTTP are not affected. Returns 0 when the task
 * was created, -1 when it could not be created. Call it once, from app_start in app.c.
 */
int audio_app_start(void);

#endif /* AUDIO_APP_H */