#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "screenHMI.h"
#include "touch.h"
#include "inmp441.h"
#include "textdraw.h"
#include "sx_vad.h"
#include "sx_hpf.h"
#include "sx_time.h"
#include "AI_net_app.h"
#include "AI_app.h"

/* Samples of one audio frame. The microphone read of this size paces the whole loop. */
#define FRAME_SAMPLES       ((BOARD_AUDIO_SAMPLE_RATE_HZ / 1000U) * APP_AI_FRAME_MS)
#define READ_TIMEOUT_MS     1000U

#if (((BOARD_AUDIO_SAMPLE_RATE_HZ / 1000U) * APP_AI_FRAME_MS) != AI_NET_FRAME_SAMPLES)
#error "APP_AI_FRAME_MS and the sample rate must give AI_NET_FRAME_SAMPLES samples (20 ms at 16 kHz)"
#endif

/* RGB565 words as the screen module expects them (same values as screen_app, shown right on this board). */
#define COLOR_BLACK         0x0000U
#define COLOR_WHITE         0xFFFFU
#define COLOR_RED           0xF800U
#define COLOR_GREEN         0x07E0U
#define COLOR_YELLOW        0xFFE0U
#define COLOR_GRAY          0x8410U

/*
 * Layout in pixels. Text is drawn at scale 2: cells of 16 x 32 pixels, 30 columns and 10 rows on
 * the 480 x 320 panel. Rows 0 to 9 are used as: 0 title, 1 and 2 status, 3 to 6 info, 8 and 9 button.
 */
#define UI_SCALE            2U
#define UI_CELL_W           (8U * UI_SCALE)
#define UI_CELL_H           (16U * UI_SCALE)
#define UI_TITLE_Y          0U
#define UI_STATUS_Y         (1U * UI_CELL_H)
#define UI_STATUS_ROWS      2U
#define UI_INFO_Y           (3U * UI_CELL_H)
#define UI_INFO_ROWS        4U
#define UI_BTN_X            80U
#define UI_BTN_Y            248U
#define UI_BTN_W            320U
#define UI_BTN_H            64U
#define UI_TOUCH_MARGIN     8U

#define TOUCH_ERROR_REPORT_EVERY 50U
/*
 * One touch can look like several presses when the controller reports no point for a frame or two.
 * A touch counts as over only after this long without a point (DESIGN, not measured).
 */
#define TOUCH_RELEASE_MS         300U
#define MIC_ERROR_REPORT_EVERY   50U
#define DRAW_WAIT_MS        100U

/* Texts shown to the user. UTF-8, NFC form. Every character must exist in font_vi_8x16. */
static const char STR_TITLE[]      = "Thử nghiệm giọng nói";
static const char STR_STARTING[]   = "Đang khởi động micro...";
static const char STR_CALIB[]      = "Đang đo tiếng ồn nền...\nXin giữ yên lặng.";
static const char STR_READY[]      = "Chạm NÓI rồi nói\nmột câu ngắn.";
static const char STR_LISTENING[]  = "Đang nghe...\nChạm DỪNG khi nói xong.";
static const char STR_HEARD[]      = "Nghe thấy giọng nói.\nChạm DỪNG khi nói xong.";
static const char STR_FONT_TEST[]  = "Tiếng Việt: ệ Ữ Ấ ở ặ ẫ ỹ đ Đ";
static const char STR_BTN_TALK[]   = "NÓI";
static const char STR_BTN_STOP[]   = "DỪNG";
static const char STR_BTN_WAIT[]   = "CHỜ";
static const char STR_PROCESSING[] = "Đang xử lý...";
static const char STR_NO_LINK[]    = "Chưa kết nối máy chủ.\nThử lại sau vài giây.";
static const char STR_LINK_LOST[]  = "Mất kết nối máy chủ.\nChạm NÓI để thử lại.";
static const char STR_NO_REPLY[]   = "Máy chủ không trả lời.\nChạm NÓI để thử lại.";
static const char STR_SEND_FAIL[]  = "Không gửi được lượt này.\nChạm NÓI để thử lại.";
static const char STR_SERVER_ERR[] = "Máy chủ báo lỗi: %s\nChạm NÓI để thử lại.";
static const char STR_NO_TEXT[]    = "(không nhận ra chữ nào)";
static const char STR_YES[]        = "có";
static const char STR_NO[]         = "không";
static const char STR_RESULT_FMT[] = "Lượt %u xong\nLý do: %s\nDài: %u.%02u giây\nCó tiếng nói: %s";

/* Why a turn ended: name is the reason string of docs/Backend_Protocol.md, text is shown to the user. */
typedef struct end_reason {
    const char *name;
    const char *text;
} end_reason_t;

static const end_reason_t REASON_TOUCH      = { "touch",      "bạn chạm DỪNG" };
static const end_reason_t REASON_VAD        = { "vad",        "im lặng, tự dừng" };
static const end_reason_t REASON_TIMEOUT    = { "timeout",    "không nghe thấy gì" };
static const end_reason_t REASON_MAX_LENGTH = { "max_length", "nói quá dài" };

typedef enum ai_state {
    AI_STATE_INIT = 0,
    AI_STATE_IDLE,
    AI_STATE_LISTENING,
    AI_STATE_WAITING
} ai_state_t;

typedef struct ai_ctx {
    ai_state_t   state;
    screen_hmi_t *hmi;
    touch_t      *touch;
    inmp441_t    *mic;
    uint32_t     turn;
    uint32_t     turn_start_ms;
    uint32_t     last_log_ms;
    uint32_t     wait_start_ms;
    bool         touch_down;
    bool         no_link_logged;   /* "turn not started" already printed for the current touch */
    uint32_t     last_touch_ms;    /* last time a touch point was seen, used only with no_link_logged */
    uint32_t     touch_fail_streak;
    uint32_t     mic_fail_streak;
} ai_ctx_t;

/* Static by rule: the audio frame, the text drawer (holds one glyph cell) and the detector. */
static int16_t    s_frame[FRAME_SAMPLES];
static textdraw_t s_text;
static sx_vad_t   s_vad;
static ai_ctx_t   s_ctx;
/* Step 3: the frame that goes to the server is a filtered COPY (the detector keeps the raw frame). */
static int16_t    s_send[FRAME_SAMPLES];
static sx_hpf_t   s_hpf;          /* high pass filter of the stream to the server (100 Hz) */
static ai_net_event_t s_ev;       /* event of the net task, used by this task only */

/* Prints the lowest free stack, then ends the calling task. */
static void end_task(void)
{
    printf("[ai] task ends, lowest free stack %u of %u bytes\n",
           (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)APP_AI_STACK_SIZE);
    vTaskDelete(NULL);
}

static void print_stack(const char *tag)
{
    printf("[ai] %s, lowest free stack %u of %u bytes\n", tag,
           (unsigned)uxTaskGetStackHighWaterMark(NULL), (unsigned)APP_AI_STACK_SIZE);
}

/* Blit function of the text drawer: draws one cell and returns when the transfer is done. */
static int ui_blit(void *ctx, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                   const uint16_t *pixels, size_t len)
{
    screen_hmi_t *hmi = (screen_hmi_t *)ctx;
    uint32_t waited = 0U;
    int ret;

    board_screen_flush_clear();
    ret = screen_hmi_draw_bitmap(hmi, x0, y0, x1, y1, pixels, len);
    if (ret != SCREEN_HMI_OK) {
        printf("[ai] draw_bitmap failed: %d\n", ret);
        return ret;
    }
    while (!board_screen_flush_is_done()) {
        if (waited >= DRAW_WAIT_MS) {
            printf("[ai] draw_bitmap did not finish in %u ms\n", (unsigned)DRAW_WAIT_MS);
            return SCREEN_HMI_ERR_TIMEOUT;
        }
        sx_delay_ms(1U);
        waited++;
    }
    return 0;
}

static void ui_fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    int ret = screen_hmi_fill_rect(s_ctx.hmi, x0, y0, x1, y1, color);

    if (ret != SCREEN_HMI_OK) {
        printf("[ai] fill_rect failed: %d\n", ret);
    }
}

/* Clears a band of whole rows over the full width, then draws the text in it. */
static void ui_text_rows(uint16_t first_y, unsigned rows, const char *text, uint16_t fg)
{
    uint16_t h = (uint16_t)(rows * UI_CELL_H);
    int ret;

    ui_fill(0U, first_y, (uint16_t)(BOARD_LCD_WIDTH - 1U), (uint16_t)(first_y + h - 1U), COLOR_BLACK);
    ret = textdraw_draw(&s_text, 0U, first_y, (uint16_t)BOARD_LCD_WIDTH, h, text, fg, COLOR_BLACK, NULL);
    if (ret < 0) {
        printf("[ai] textdraw_draw failed: %d\n", ret);
    }
}

static void ui_draw_button(const char *label, uint16_t bg, uint16_t fg)
{
    unsigned glyphs = (unsigned)textdraw_count_glyphs(label);
    uint16_t text_w = (uint16_t)(glyphs * UI_CELL_W);
    uint16_t x = (uint16_t)(UI_BTN_X + ((UI_BTN_W - text_w) / 2U));
    uint16_t y = (uint16_t)(UI_BTN_Y + ((UI_BTN_H - UI_CELL_H) / 2U));
    int ret;

    ui_fill(UI_BTN_X, UI_BTN_Y, (uint16_t)(UI_BTN_X + UI_BTN_W - 1U), (uint16_t)(UI_BTN_Y + UI_BTN_H - 1U), bg);
    ret = textdraw_draw(&s_text, x, y, text_w, (uint16_t)UI_CELL_H, label, fg, bg, NULL);
    if (ret < 0) {
        printf("[ai] button text failed: %d\n", ret);
    }
}

static uint32_t now_ms(void)
{
    uint32_t t = 0U;

    sx_get_time_ms(&t);
    return t;
}

/* Brings up screen, touch and the text drawer. Returns false on any failure (already printed). */
static bool init_screen_and_touch(ai_ctx_t *ctx)
{
    textdraw_config_t tcfg;
    touch_id_t id;
    int ret;

    ret = board_init();
    if (ret != BOARD_OK) {
        printf("[ai] board_init failed: %d\n", ret);
        return false;
    }
    ret = board_init_screen();
    if (ret != BOARD_OK) {
        printf("[ai] board_init_screen failed: %d\n", ret);
        return false;
    }
    ctx->hmi = board_get_screen();
    if (ctx->hmi == NULL) {
        printf("[ai] board_get_screen returned NULL\n");
        return false;
    }
    ret = screen_hmi_set_backlight(ctx->hmi, (uint8_t)APP_AI_BACKLIGHT_PERCENT);
    if (ret != SCREEN_HMI_OK) {
        printf("[ai] set_backlight failed: %d\n", ret);
        return false;
    }

    tcfg.blit = ui_blit;
    tcfg.ctx = ctx->hmi;
    tcfg.width = (uint16_t)BOARD_LCD_WIDTH;
    tcfg.height = (uint16_t)BOARD_LCD_HEIGHT;
    tcfg.scale = (uint8_t)UI_SCALE;
    ret = textdraw_init(&s_text, &tcfg);
    if (ret != TEXTDRAW_OK) {
        printf("[ai] textdraw_init failed: %d\n", ret);
        return false;
    }

    ui_fill(0U, 0U, (uint16_t)(BOARD_LCD_WIDTH - 1U), (uint16_t)(BOARD_LCD_HEIGHT - 1U), COLOR_BLACK);
    ui_text_rows(UI_TITLE_Y, 1U, STR_TITLE, COLOR_WHITE);

    ret = board_init_touch();
    if (ret != BOARD_OK) {
        printf("[ai] board_init_touch failed: %d\n", ret);
        return false;
    }
    ctx->touch = board_get_touch();
    if ((ctx->touch == NULL) || (touch_read_id(ctx->touch, &id) != TOUCH_OK)) {
        printf("[ai] touch controller does not answer\n");
        return false;
    }
    printf("[ai] touch ok: chip_id=0x%02X\n", (unsigned)id.chip_id);
    return true;
}

/* Brings up the microphone, throws away the start-up samples, lets the detector learn the noise. */
static bool init_mic(ai_ctx_t *ctx)
{
    const uint32_t settle_frames = (APP_AI_MIC_SETTLE_MS + APP_AI_FRAME_MS - 1U) / APP_AI_FRAME_MS;
    const uint32_t calib_frames = (APP_AI_MIC_CALIB_MS + APP_AI_FRAME_MS - 1U) / APP_AI_FRAME_MS;
    sx_vad_stats_t st;
    uint32_t i;
    size_t got = 0U;
    int ret;

    ret = board_init_mic();
    if (ret != BOARD_OK) {
        printf("[ai] board_init_mic failed: %d\n", ret);
        return false;
    }
    ctx->mic = board_get_mic();
    if (ctx->mic == NULL) {
        printf("[ai] board_get_mic returned NULL\n");
        return false;
    }
    ret = inmp441_start(ctx->mic);
    if (ret != INMP441_OK) {
        printf("[ai] inmp441_start failed: %d\n", ret);
        return false;
    }

    ui_text_rows(UI_STATUS_Y, UI_STATUS_ROWS, STR_STARTING, COLOR_YELLOW);
    for (i = 0U; i < settle_frames; i++) {
        ret = inmp441_read(ctx->mic, s_frame, FRAME_SAMPLES, &got, READ_TIMEOUT_MS);
        if (ret != INMP441_OK) {
            printf("[ai] mic read failed during settle: %d\n", ret);
            return false;
        }
    }

    ui_text_rows(UI_STATUS_Y, UI_STATUS_ROWS, STR_CALIB, COLOR_YELLOW);
    for (i = 0U; i < calib_frames; i++) {
        ret = inmp441_read(ctx->mic, s_frame, FRAME_SAMPLES, &got, READ_TIMEOUT_MS);
        if (ret != INMP441_OK) {
            printf("[ai] mic read failed during calibration: %d\n", ret);
            return false;
        }
        (void)sx_vad_feed_idle(&s_vad, s_frame, FRAME_SAMPLES);
    }
    (void)sx_vad_get_stats(&s_vad, &st);
    printf("[ai] mic ready: noise_rms=%u threshold_rms=%u (settle %u ms, calibration %u ms)\n",
           (unsigned)st.noise_rms, (unsigned)st.threshold_rms,
           (unsigned)APP_AI_MIC_SETTLE_MS, (unsigned)APP_AI_MIC_CALIB_MS);
    return true;
}

static void enter_idle(ai_ctx_t *ctx)
{
    ctx->state = AI_STATE_IDLE;
    ui_text_rows(UI_STATUS_Y, UI_STATUS_ROWS, STR_READY, COLOR_YELLOW);
    ui_draw_button(STR_BTN_TALK, COLOR_GREEN, COLOR_BLACK);
}

static void begin_turn(ai_ctx_t *ctx)
{
    sx_vad_stats_t st;
    uint32_t turn = ctx->turn + 1U;
    int net = ai_net_turn_begin(turn);

    if (net != AI_NET_OK) {
        /* No link to the server (or no room in the buffer to the net task): nothing was sent, no turn starts. */
        if (!ctx->no_link_logged) {   /* once per touch, not once per frame while the finger stays down */
            printf("[ai] turn %u not started: ai_net_turn_begin=%d\n", (unsigned)turn, net);
            ctx->no_link_logged = true;
            ctx->last_touch_ms = now_ms();
        }
        ui_text_rows(UI_STATUS_Y, UI_STATUS_ROWS, STR_NO_LINK, COLOR_RED);
        return;
    }
    ctx->no_link_logged = false;
    ctx->turn = turn;
    /* The filter starts at the level of the last sample read, so the DC offset does not make a step. */
    sx_hpf_reset(&s_hpf, s_frame[FRAME_SAMPLES - 1U]);
    (void)sx_vad_start_turn(&s_vad);
    (void)sx_vad_get_stats(&s_vad, &st);
    ctx->turn_start_ms = now_ms();
    ctx->last_log_ms = ctx->turn_start_ms;
    ctx->state = AI_STATE_LISTENING;
    printf("[ai] turn %u start (touch), noise_rms=%u threshold_rms=%u\n",
           (unsigned)ctx->turn, (unsigned)st.noise_rms, (unsigned)st.threshold_rms);
    ui_text_rows(UI_STATUS_Y, UI_STATUS_ROWS, STR_LISTENING, COLOR_YELLOW);
    ui_draw_button(STR_BTN_STOP, COLOR_RED, COLOR_WHITE);
}

/* Ends the running turn and shows the result. */
static void end_turn(ai_ctx_t *ctx, const end_reason_t *reason)
{
    sx_vad_stats_t st;
    char text[160];
    uint32_t wall_ms = now_ms() - ctx->turn_start_ms;
    int net;

    (void)sx_vad_get_stats(&s_vad, &st);
    sx_vad_cancel_turn(&s_vad);
    printf("[ai] turn %u end reason=%s wall_ms=%u frames=%u (%u ms) speech=%s peak_rms=%u noise_rms=%u threshold_rms=%u\n",
           (unsigned)ctx->turn, reason->name, (unsigned)wall_ms, (unsigned)st.turn_frames,
           (unsigned)(st.turn_frames * APP_AI_FRAME_MS), st.speech_started ? "yes" : "no",
           (unsigned)st.peak_rms, (unsigned)st.noise_rms, (unsigned)st.threshold_rms);

    (void)snprintf(text, sizeof(text), STR_RESULT_FMT, (unsigned)ctx->turn, reason->text,
                   (unsigned)(wall_ms / 1000U), (unsigned)((wall_ms % 1000U) / 10U),
                   st.speech_started ? STR_YES : STR_NO);
    net = ai_net_turn_end(ctx->turn, reason->name);
    if (net != AI_NET_OK) {
        printf("[ai] turn %u: ai_net_turn_end failed: %d\n", (unsigned)ctx->turn, net);
        enter_idle(ctx);
        ui_text_rows(UI_INFO_Y, UI_INFO_ROWS, text, COLOR_WHITE);
        ui_text_rows(UI_STATUS_Y, UI_STATUS_ROWS, STR_SEND_FAIL, COLOR_RED);
        print_stack("turn done (not sent)");
        return;
    }
    ctx->state = AI_STATE_WAITING;
    ctx->wait_start_ms = now_ms();
    ui_text_rows(UI_STATUS_Y, UI_STATUS_ROWS, STR_PROCESSING, COLOR_YELLOW);
    ui_text_rows(UI_INFO_Y, UI_INFO_ROWS, text, COLOR_WHITE);
    ui_draw_button(STR_BTN_WAIT, COLOR_GRAY, COLOR_BLACK);
    print_stack("turn done");
}

/* Filters a copy of the frame (the detector keeps the raw one) and hands it to the net task. A full buffer drops it. */
static void send_frame_to_net(void)
{
    (void)memcpy(s_send, s_frame, sizeof(s_send));
    sx_hpf_process(&s_hpf, s_send, s_send, FRAME_SAMPLES);
    /* A drop is counted on the producer side and printed at the end of the turn by the net task. */
    (void)ai_net_turn_audio(s_send, FRAME_SAMPLES);
}

/* Leaves a turn that cannot go on (link lost, server error, no answer): tells the net task, shows why, goes IDLE. */
static void abandon_turn(ai_ctx_t *ctx, const char *message, uint16_t color)
{
    if ((ctx->state == AI_STATE_LISTENING) || (ctx->state == AI_STATE_WAITING)) {
        (void)ai_net_turn_abort(ctx->turn);
    }
    if (ctx->state == AI_STATE_LISTENING) {
        sx_vad_cancel_turn(&s_vad);
    }
    enter_idle(ctx);
    ui_text_rows(UI_STATUS_Y, UI_STATUS_ROWS, message, color);
}

/* Handles every event that the net task has queued. Called once per frame, before the button. */
static void handle_net_events(ai_ctx_t *ctx)
{
    char text[200];

    while (ai_net_poll_event(&s_ev) != 0) {
        bool mine = (!s_ev.has_turn) || (s_ev.turn == ctx->turn);
        bool busy = (ctx->state == AI_STATE_LISTENING) || (ctx->state == AI_STATE_WAITING);

        switch (s_ev.type) {
        case AI_BACKEND_EVENT_CONNECTED:
            printf("[ai] server link up\n");
            if (ctx->state == AI_STATE_IDLE) {
                enter_idle(ctx);   /* clears an old "link lost" message */
            }
            break;
        case AI_BACKEND_EVENT_DISCONNECTED:
            printf("[ai] server link lost\n");
            if (busy) {
                abandon_turn(ctx, STR_LINK_LOST, COLOR_RED);
            }
            break;
        case AI_BACKEND_EVENT_STT_RESULT:
            if ((ctx->state == AI_STATE_WAITING) && s_ev.has_turn && (s_ev.turn == ctx->turn)) {
                printf("[ai] stt turn %u (%u ms after stop): %s\n", (unsigned)s_ev.turn,
                       (unsigned)s_ev.latency_ms, s_ev.text);
                enter_idle(ctx);
                ui_text_rows(UI_INFO_Y, UI_INFO_ROWS, (s_ev.text[0] != '\0') ? s_ev.text : STR_NO_TEXT, COLOR_WHITE);
            }
            break;
        case AI_BACKEND_EVENT_ERROR:
            if (busy && mine) {
                printf("[ai] server error for turn %u: %s\n", (unsigned)ctx->turn, s_ev.code);
                (void)snprintf(text, sizeof(text), STR_SERVER_ERR, s_ev.code);
                abandon_turn(ctx, text, COLOR_RED);
            }
            break;
        case AI_BACKEND_EVENT_LISTEN_END:
            /* The server has enough audio. The protocol has no stop reason for it, so the turn ends as "vad". */
            if ((ctx->state == AI_STATE_LISTENING) && mine) {
                end_turn(ctx, &REASON_VAD);
            }
            break;
        case AI_BACKEND_EVENT_NONE:
        default:
            break;
        }
    }
}

/* Reads the touch controller. Returns true once per new touch that starts on the button. */
static bool poll_button(ai_ctx_t *ctx)
{
    touch_point_t pts[TOUCH_MAX_POINTS];
    uint8_t count = 0U;
    int ret = touch_read(ctx->touch, pts, TOUCH_MAX_POINTS, &count);
    bool inside;
    bool edge;

    if (ret != TOUCH_OK) {
        if (ctx->touch_fail_streak < UINT32_MAX) {
            ctx->touch_fail_streak++;
        }
        if ((ctx->touch_fail_streak == 1U) || ((ctx->touch_fail_streak % TOUCH_ERROR_REPORT_EVERY) == 0U)) {
            printf("[ai] touch_read failed: %d (%u in a row)\n", ret, (unsigned)ctx->touch_fail_streak);
        }
        if ((ctx->touch_fail_streak % TOUCH_ERROR_REPORT_EVERY) == 0U) {
            printf("[ai] touch_recover: %d\n", touch_recover(ctx->touch));
        }
        ctx->touch_down = false;
        return false;
    }
    ctx->touch_fail_streak = 0U;
    if (count == 0U) {
        ctx->touch_down = false;
        if (ctx->no_link_logged && ((now_ms() - ctx->last_touch_ms) >= TOUCH_RELEASE_MS)) {
            ctx->no_link_logged = false;   /* the touch is over, the next one may print again */
        }
        return false;
    }
    ctx->last_touch_ms = now_ms();
    inside = (pts[0].x + UI_TOUCH_MARGIN >= UI_BTN_X) && (pts[0].x < UI_BTN_X + UI_BTN_W + UI_TOUCH_MARGIN) &&
             (pts[0].y + UI_TOUCH_MARGIN >= UI_BTN_Y) && (pts[0].y < UI_BTN_Y + UI_BTN_H + UI_TOUCH_MARGIN);
    edge = (!ctx->touch_down) && inside;
    ctx->touch_down = true;
    return edge;
}

static void log_progress(ai_ctx_t *ctx, uint32_t now)
{
    sx_vad_stats_t st;

    if ((APP_AI_LOG_EVERY_MS == 0U) || ((now - ctx->last_log_ms) < APP_AI_LOG_EVERY_MS)) {
        return;
    }
    ctx->last_log_ms = now;
    (void)sx_vad_get_stats(&s_vad, &st);
    printf("[ai] t=%u ms rms=%u noise=%u thr=%u speech=%s\n", (unsigned)(now - ctx->turn_start_ms),
           (unsigned)st.rms, (unsigned)st.noise_rms, (unsigned)st.threshold_rms,
           st.speech_started ? "yes" : "no");
}

/* Handles the audio frame that was just read and the button, in the current state. */
static void handle_frame(ai_ctx_t *ctx)
{
    bool pressed;
    uint32_t now = now_ms();

    handle_net_events(ctx);
    pressed = poll_button(ctx);

    switch (ctx->state) {
    case AI_STATE_IDLE:
        (void)sx_vad_feed_idle(&s_vad, s_frame, FRAME_SAMPLES);
        if (pressed) {
            begin_turn(ctx);
        }
        break;

    case AI_STATE_LISTENING: {
        sx_vad_event_t ev = SX_VAD_EVENT_NONE;
        int ret;

        send_frame_to_net();   /* every frame of the turn goes out, also the one on which the turn ends */
        ret = sx_vad_process(&s_vad, s_frame, FRAME_SAMPLES, &ev);

        if (pressed) {
            end_turn(ctx, &REASON_TOUCH);
            break;
        }
        if (ret != SX_VAD_OK) {
            printf("[ai] sx_vad_process failed: %d\n", ret);
            end_turn(ctx, &REASON_TIMEOUT);
            break;
        }
        if (ev == SX_VAD_EVENT_SPEECH_START) {
            printf("[ai] speech start after %u ms\n", (unsigned)(now - ctx->turn_start_ms));
            ui_text_rows(UI_STATUS_Y, UI_STATUS_ROWS, STR_HEARD, COLOR_YELLOW);
        } else if (ev == SX_VAD_EVENT_END_SILENCE) {
            end_turn(ctx, &REASON_VAD);
        } else if (ev == SX_VAD_EVENT_NO_SPEECH_TIMEOUT) {
            end_turn(ctx, &REASON_TIMEOUT);
        } else if (ev == SX_VAD_EVENT_MAX_LENGTH) {
            end_turn(ctx, &REASON_MAX_LENGTH);
        } else if ((now - ctx->turn_start_ms) > (APP_AI_VAD_MAX_TURN_MS + 2000U)) {
            /* The detector counts frames. This clock guard catches lost frames or a stuck microphone. */
            printf("[ai] wall clock guard: the detector did not end the turn\n");
            end_turn(ctx, &REASON_MAX_LENGTH);
        } else {
            log_progress(ctx, now);
        }
        break;
    }

    case AI_STATE_WAITING:
        (void)sx_vad_feed_idle(&s_vad, s_frame, FRAME_SAMPLES);
        if ((now - ctx->wait_start_ms) >= APP_AI_STT_WAIT_MS) {
            printf("[ai] turn %u: no answer in %u ms\n", (unsigned)ctx->turn, (unsigned)APP_AI_STT_WAIT_MS);
            abandon_turn(ctx, STR_NO_REPLY, COLOR_RED);
        }
        break;

    case AI_STATE_INIT:
    default:
        break;
    }
}

static void run_loop(ai_ctx_t *ctx)
{
    for (;;) {
        size_t got = 0U;
        int ret = inmp441_read(ctx->mic, s_frame, FRAME_SAMPLES, &got, READ_TIMEOUT_MS);

        if (ret != INMP441_OK) {
            if (ctx->mic_fail_streak < UINT32_MAX) {
                ctx->mic_fail_streak++;
            }
            if ((ctx->mic_fail_streak == 1U) || ((ctx->mic_fail_streak % MIC_ERROR_REPORT_EVERY) == 0U)) {
                printf("[ai] inmp441_read failed: %d (%u in a row)\n", ret, (unsigned)ctx->mic_fail_streak);
            }
            /* The touch button still works, so a turn can be stopped. 20 ms or more yields the CPU. */
            if ((ctx->state == AI_STATE_LISTENING) && poll_button(ctx)) {
                end_turn(ctx, &REASON_TOUCH);
            }
            sx_delay_ms(APP_AI_FRAME_MS);
            continue;
        }
        ctx->mic_fail_streak = 0U;
        handle_frame(ctx);
    }
}

static void ai_task(void *arg)
{
    sx_vad_config_t vcfg;
    int ret;

    (void)arg;

    vcfg.sample_rate_hz = BOARD_AUDIO_SAMPLE_RATE_HZ;
    vcfg.frame_ms = APP_AI_FRAME_MS;
    vcfg.onset_ms = APP_AI_VAD_ONSET_MS;
    vcfg.end_silence_ms = APP_AI_VAD_END_SILENCE_MS;
    vcfg.wait_speech_ms = APP_AI_VAD_WAIT_SPEECH_MS;
    vcfg.max_turn_ms = APP_AI_VAD_MAX_TURN_MS;
    vcfg.initial_noise_rms = APP_AI_VAD_INITIAL_NOISE_RMS;
    vcfg.min_threshold_rms = APP_AI_VAD_MIN_THRESHOLD_RMS;
    vcfg.threshold_ratio_x10 = APP_AI_VAD_RATIO_X10;
    sx_hpf_init(&s_hpf);
    ret = sx_vad_init(&s_vad, &vcfg);
    if (ret != SX_VAD_OK) {
        printf("[ai] sx_vad_init failed: %d\n", ret);
        end_task();
        return;
    }

    if (!init_screen_and_touch(&s_ctx)) {
        end_task();
        return;
    }
    if (!init_mic(&s_ctx)) {
        end_task();
        return;
    }

#if APP_AI_FONT_TEST
    ui_text_rows(UI_INFO_Y, UI_INFO_ROWS, STR_FONT_TEST, COLOR_WHITE);
#endif
    enter_idle(&s_ctx);
    print_stack("ready, touch NOI to talk");
    run_loop(&s_ctx);   /* never returns */
    end_task();
}

int ai_app_start(void)
{
    /* The net task waits for the Wi-Fi IP by itself (wifi_app_start runs in app.c). Without it no turn can start. */
    if (ai_net_start() != AI_NET_OK) {
        printf("[ai] ai_net_start failed, turns will not start\n");
    }
    if (xTaskCreate(ai_task, APP_AI_TASK_NAME, APP_AI_STACK_SIZE, NULL,
                    APP_AI_TASK_PRIORITY, NULL) != pdPASS) {
        printf("[ai] cannot create the task\n");
        return -1;
    }
    return 0;
}