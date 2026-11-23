#ifndef AI_APP_H
#define AI_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Step 2 of docs/Backend_Protocol.md section 13: the talk button, the turn state machine, the voice
 * activity detector and Vietnamese text on the screen. There is NO network yet. A turn only measures
 * how long the user spoke and why it ended, and shows that on the screen and in the log.
 *
 * Every number below is a DESIGN value chosen by the app, none is measured on the board.
 */

#ifndef APP_AI_TASK_NAME
#define APP_AI_TASK_NAME "app_ai_task"
#endif

/*
 * Stack in bytes. The task brings up the screen, the touch controller and the microphone, then
 * reads the microphone and the touch controller in one loop and draws text from the same task.
 * Chosen by design, not measured: the task prints the lowest free stack when it is ready and
 * after every turn.
 */
#ifndef APP_AI_STACK_SIZE
#define APP_AI_STACK_SIZE 6144
#endif

/* Same as the other app tasks. Chosen by design. */
#ifndef APP_AI_TASK_PRIORITY
#define APP_AI_TASK_PRIORITY 2
#endif

#ifndef APP_AI_BACKLIGHT_PERCENT
#define APP_AI_BACKLIGHT_PERCENT 100U
#endif

/*
 * Microphone samples thrown away after inmp441_start. docs/HandOver.md records a large start-up
 * pulse that fades over about 2 to 4 seconds, so the first samples must not reach the detector.
 */
#ifndef APP_AI_MIC_SETTLE_MS
#define APP_AI_MIC_SETTLE_MS 3000U
#endif

/* After the settle time the detector learns the room noise for this long before the button is enabled. */
#ifndef APP_AI_MIC_CALIB_MS
#define APP_AI_MIC_CALIB_MS 1500U
#endif

/* Length of one audio frame. Must match what the microphone read paces: 20 ms is 320 samples at 16 kHz. */
#ifndef APP_AI_FRAME_MS
#define APP_AI_FRAME_MS 20U
#endif

/* Speech must stay above the threshold this long to count as started. */
#ifndef APP_AI_VAD_ONSET_MS
#define APP_AI_VAD_ONSET_MS 100U
#endif

/* Quiet time after speech that ends the turn. 3500 ms chosen by the owner (session 24). */
#ifndef APP_AI_VAD_END_SILENCE_MS
#define APP_AI_VAD_END_SILENCE_MS 3500U
#endif

/* The turn is cancelled when nobody speaks within this time after the touch (Backend_Protocol section 7). */
#ifndef APP_AI_VAD_WAIT_SPEECH_MS
#define APP_AI_VAD_WAIT_SPEECH_MS 6000U
#endif

/* Hard limit of one turn (Backend_Protocol section 7). */
#ifndef APP_AI_VAD_MAX_TURN_MS
#define APP_AI_VAD_MAX_TURN_MS 15000U
#endif

/*
 * Noise floor before the first measurement, and the lowest speech threshold, both as RMS of 16 bit
 * samples after the high pass filter. Starting points taken from the numbers in docs/HandOver.md
 * section 5 (quiet room about 8 to 19, fridge far away about 70 to 85, speech at 30 cm with peaks
 * of 1000 to 4500). They need tuning with the log lines of this app.
 */
#ifndef APP_AI_VAD_INITIAL_NOISE_RMS
#define APP_AI_VAD_INITIAL_NOISE_RMS 20U
#endif
#ifndef APP_AI_VAD_MIN_THRESHOLD_RMS
#define APP_AI_VAD_MIN_THRESHOLD_RMS 50U
#endif

/* Speech threshold = noise floor times this value divided by 10. */
#ifndef APP_AI_VAD_RATIO_X10
#define APP_AI_VAD_RATIO_X10 30U
#endif

/* How long the result of a turn stays on the screen before the button works again. Placeholder of step 2. */
#ifndef APP_AI_RESULT_HOLD_MS
#define APP_AI_RESULT_HOLD_MS 2500U
#endif

/*
 * Period of the log line during a turn (rms, noise floor, threshold). 0 switches it off. One short
 * line at 115200 baud takes a few milliseconds, so keep this at 250 ms or more.
 */
#ifndef APP_AI_LOG_EVERY_MS
#define APP_AI_LOG_EVERY_MS 500U
#endif

/* 1: the first screen shows a line with the hardest Vietnamese letters, to check the font by eye. */
#ifndef APP_AI_FONT_TEST
#define APP_AI_FONT_TEST 1
#endif

/*
 * Creates the task. It calls board_init, brings up the screen (backlight on), the touch controller and
 * the microphone, discards the start-up samples, learns the room noise, and then runs for ever:
 *   IDLE: the button says NOI (green). Touching it starts a turn.
 *   LISTENING: the button says DUNG (red). The turn ends on a touch of the button (reason "touch"),
 *     when the detector sees speech and then APP_AI_VAD_END_SILENCE_MS of quiet (reason "vad"), when
 *     nobody speaks within APP_AI_VAD_WAIT_SPEECH_MS (reason "timeout"), or at APP_AI_VAD_MAX_TURN_MS
 *     (reason "max_length"). The reason names are the ones of docs/Backend_Protocol.md.
 *   WAITING: placeholder of step 2. The result of the turn (reason, length, speech seen) is shown for
 *     APP_AI_RESULT_HOLD_MS, then the state goes back to IDLE. Step 3 replaces this with the wait for
 *     the server answer.
 * The microphone runs all the time and is read in this one task, so a screen update can delay a read.
 * Each turn prints one summary line: "[ai] turn N end reason=... wall_ms=... frames=... speech=...".
 * It does not use Wi-Fi, the speaker or the SD card. Returns 0 when the task was created, -1 when
 * it could not be created. Call it once. Do not run it together with screen_app (both own the screen).
 */
int ai_app_start(void);

#ifdef __cplusplus
}
#endif

#endif /* AI_APP_H */