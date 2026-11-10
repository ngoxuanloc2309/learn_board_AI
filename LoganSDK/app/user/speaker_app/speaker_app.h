#ifndef SPEAKER_APP_H
#define SPEAKER_APP_H

#include <stdbool.h>

#ifndef APP_SPEAKER_TASK_NAME
#define APP_SPEAKER_TASK_NAME "app_speaker_task"
#endif

/*
 * Stack in bytes. The task uses a static buffer for the audio, so the stack holds only the call
 * chain of printf and the I2S driver. Chosen by design, not measured. The task prints the lowest
 * free stack when it ends.
 */
#ifndef APP_SPEAKER_STACK_SIZE
#define APP_SPEAKER_STACK_SIZE 6144
#endif

/* Below the Wi-Fi task (3), same as the other bring-up tasks. Chosen by design, not measured. */
#ifndef APP_SPEAKER_TASK_PRIORITY
#define APP_SPEAKER_TASK_PRIORITY 2
#endif

/*
 * Speaker volume of the tone test, 0 to 100 percent of the sample values. Chosen by design, LOW on
 * purpose: the gain of the amplifier on the board is not verified, so the first sound must be
 * quiet. Raise it only after the first test was heard at a comfortable level.
 */
#ifndef APP_SPEAKER_TONE_VOLUME
#define APP_SPEAKER_TONE_VOLUME 20U
#endif

/* Peak of the tone before the volume is applied, in 16 bit sample units (full scale is 32767). Design. */
#ifndef APP_SPEAKER_TONE_AMPLITUDE
#define APP_SPEAKER_TONE_AMPLITUDE 16000
#endif

/* Length of each of the three tones and of the pause after each one. Chosen by design. */
#ifndef APP_SPEAKER_TONE_MS
#define APP_SPEAKER_TONE_MS 700U
#endif
#ifndef APP_SPEAKER_GAP_MS
#define APP_SPEAKER_GAP_MS 300U
#endif

/*
 * After the three tones the task plays the recorded word "hello" (see hello_pcm.h) this many
 * times, with APP_SPEAKER_HELLO_GAP_MS of silence after each one. Set the count to 0 to skip it.
 * The volume is a percent of the sample values. LOW on purpose, like the tone volume: the amplifier
 * gain is not verified. All three values are chosen by design, not measured.
 */
#ifndef APP_SPEAKER_HELLO_REPEAT
#define APP_SPEAKER_HELLO_REPEAT 3U
#endif

#ifndef APP_SPEAKER_HELLO_VOLUME
#define APP_SPEAKER_HELLO_VOLUME 30U
#endif

#ifndef APP_SPEAKER_HELLO_GAP_MS
#define APP_SPEAKER_HELLO_GAP_MS 700U
#endif

/*
 * Creates a task for the speaker bring-up test. The task calls board_init and board_init_speaker,
 * then plays three sine tones of 440, 880 and 1320 Hz, each APP_SPEAKER_TONE_MS long with a short
 * fade in and out, at the volume APP_SPEAKER_TONE_VOLUME, with a pause after each one, then plays the
 * word "hello" APP_SPEAKER_HELLO_REPEAT times at the volume APP_SPEAKER_HELLO_VOLUME, then stops
 * the speaker and ends. The word is a synthetic recording, so it sounds robotic; whether it is
 * understood is judged by ear. Each step prints one log line prefixed with the tag speaker in square
 * brackets.
 *
 * The program can only check that the I2S driver accepts the data and that it is paced at the
 * sample rate (the log line says how many ms of audio were queued in how many ms). WHETHER THE
 * TONES ARE HEARD, and how loud, is judged by ear: three rising beeps. If the log says ok but
 * nothing is heard, the cause is outside the code: the amplifier may need an enable pin (not
 * handled, not verified), the I2S pins in board.h are REF, or the speaker is not on the two speaker output terminals of the amplifier.
 *
 * mic_app uses the speaker after this test, so it waits for speaker_app_tone_test_done before it
 * touches any hardware. Returns 0 when the task was created, -1 when it could not be created.
 * Call it once, from app_start in app.c.
 */
int speaker_app_start(void);

/*
 * True once the speaker task has finished with the speaker: the tones were played, or the test
 * failed, or it was skipped because the speaker could not be initialised. It never goes back to
 * false. It is false until then, and stays false if speaker_app_start was never called or the
 * task could not be created. Safe to call from any task.
 */
bool speaker_app_tone_test_done(void);

#endif /* SPEAKER_APP_H */