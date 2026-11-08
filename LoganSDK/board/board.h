#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "screenHMI.h"
#include "touch.h"
#include "speaker.h"
#include "inmp441.h"

/*This is the board with HMI: WT32S3-WROVER - Model: ZX3D50CE08S-V15-USRC, 230208*/
typedef enum{
    BOARD_WT32S3_WROVER = 0,
    /*Other boards in the future*/
    BOARD_UNKNOWN
} BoardType_t;

/*
 * Layer 4 board description: pins, port numbers, wiring values and the instances of the devices.
 * This is the only place that knows which GPIO goes where. Layers below take the values through
 * their configuration structures. Pins are plain integers, this header does not include
 * board_device/ZX3D50CE08S-V15-USRC.h.
 *
 * Every group of values carries one of these labels:
 *   REF:    taken from public documents of the WT32-SC01 Plus family (through the board_device
 *           header of the project owner). NOT verified on revision V15.
 *   GIVEN:  stated by the project owner. Not checked on hardware by the firmware side.
 *   DESIGN: chosen by the firmware design with no source. A conservative guess, to be confirmed or
 *           tuned during bring-up.
 *
 * BOARD_PINMAP_VERIFIED stays 0 until the pins are checked against the schematic of revision V15
 * or measured on the board. Do not set it to 1 on the strength of a document of another revision.
 */
#define BOARD_PINMAP_VERIFIED       0

/*
 * LCD, 8080 parallel bus, 8 bit. REF:
 * GPIO 0, 3, 45 and 46 are strapping pins of the ESP32-S3. They are used here by the display
 * (RS, DB2, backlight, DB1), so check boot behavior during bring-up.
 * The panel chip select is assumed tied low (no pin in the reference). DESIGN: not verified.
 * The tearing effect pin of the reference (GPIO 48) is not used by any module and is not defined.
 */
#define BOARD_PIN_LCD_DB0           9
#define BOARD_PIN_LCD_DB1           46
#define BOARD_PIN_LCD_DB2           3
#define BOARD_PIN_LCD_DB3           8
#define BOARD_PIN_LCD_DB4           18
#define BOARD_PIN_LCD_DB5           17
#define BOARD_PIN_LCD_DB6           16
#define BOARD_PIN_LCD_DB7           15
#define BOARD_PIN_LCD_WR            47
#define BOARD_PIN_LCD_RS            0
#define BOARD_PIN_LCD_RESET         4      /* shared with the touch reset, driven by the display only */
#define BOARD_PIN_BACKLIGHT         45     /* PWM, active high */

/*
 * LCD values. DESIGN: none of these has a source for this board.
 * Panel size is 480 x 320 as seen after the rotation. The ST7796 native size is 320 x 480, so the
 * landscape view needs the row and column exchange bit in MADCTL.
 * Orientation, color order and inversion must be confirmed by filling the screen with known colors.
 * No panel specific init table is given (no verified source), the chip runs on its defaults.
 */
#define BOARD_LCD_WIDTH             480U
#define BOARD_LCD_HEIGHT            320U
#define BOARD_LCD_PCLK_HZ           10000000U   /* conservative, the panel maximum is not verified */
#define BOARD_LCD_MAX_TRANSFER      30720U      /* bytes, 480 x 32 pixels of RGB565 */
#define BOARD_LCD_MADCTL            (SCREEN_HMI_MADCTL_MV | SCREEN_HMI_MADCTL_BGR)
#define BOARD_LCD_INVERT            true        /* session 15: with false the owner saw black and white swapped */
#define BOARD_BACKLIGHT_TIMER       0
#define BOARD_BACKLIGHT_CHANNEL     0
#define BOARD_BACKLIGHT_FREQ_HZ     5000U
#define BOARD_BACKLIGHT_RES_BITS    8U

/*
 * Touch, I2C, controller FT6336U. REF for the pins.
 * The interrupt pin of the reference (GPIO 7) is not used because the touch module polls.
 * DESIGN for the rest: I2C port 0, 100 kHz, no internal pull-ups (the board is assumed to have
 * external ones, not verified: if the controller does not answer, try enabling them),
 * and the coordinate swap (raw 320 x 480 view turned to 480 x 320). The Y mirror was found on the
 * board in session 16 (observed by tapping, not from a datasheet). X needs no mirror (observed).
 */
#define BOARD_PIN_TOUCH_SDA         6
#define BOARD_PIN_TOUCH_SCL         5
#define BOARD_TOUCH_I2C_PORT        0
#define BOARD_TOUCH_I2C_CLOCK_HZ    100000U
#define BOARD_TOUCH_I2C_PULLUP      false
#define BOARD_TOUCH_SWAP_XY         true
#define BOARD_TOUCH_MIRROR_X        false
#define BOARD_TOUCH_MIRROR_Y        true        /* session 16: with false the owner saw top and bottom swapped */

/*
 * microSD, SPI mode. REF for the pins.
 * DESIGN for the rest: SPI host 1 (SPI2_HOST of ESP-IDF, the LCD does not use an SPI host),
 * 10 MHz data clock (the sdcard module allows at most 25 MHz).
 * Wiring of the card slot (pull-up on MISO, card supply) is not verified.
 */
#define BOARD_PIN_SD_CS             41
#define BOARD_PIN_SD_MOSI           40
#define BOARD_PIN_SD_SCLK           39
#define BOARD_PIN_SD_MISO           38
#define BOARD_SD_SPI_HOST           1
#define BOARD_SD_CLOCK_HZ           10000000U

/*
 * Speaker, I2S send only, through the amplifier on the board. REF for the pins.
 * GPIO 35, 36 and 37 cannot be used if the module turns out to have octal PSRAM (not verified).
 * DESIGN: I2S port 0.
 */
#define BOARD_PIN_SPK_WS            35
#define BOARD_PIN_SPK_BCLK          36
#define BOARD_PIN_SPK_DOUT          37
#define BOARD_SPK_I2S_PORT          0

/*
 * Microphone INMP441, I2S receive only, on the extension header. GIVEN by the project owner:
 * SCK to GPIO 10, WS to GPIO 11, SD (data out of the microphone) to GPIO 12, L/R pin tied to GND
 * with a 10K resistor added by the owner. The 3.3 V supply of the microphone is not verified.
 * DESIGN: I2S port 1, so it does not share a port with the speaker.
 */
#define BOARD_PIN_MIC_BCLK          10
#define BOARD_PIN_MIC_WS            11
#define BOARD_PIN_MIC_DIN           12
#define BOARD_MIC_I2S_PORT          1

/* Sample rate of the speaker and the microphone. DESIGN: not measured, chosen for speech. */
#define BOARD_AUDIO_SAMPLE_RATE_HZ  16000U

/*
 * RS485 is not defined here: its pins have no verified source (see docs/HandOver.md).
 */

/* Return codes. Success is 0. */
#define BOARD_OK                    0
#define BOARD_ERR_STATE             (-100)   /* board_init not called, device already up, or a dependency is not up */
/*
 * Every other negative value is the code of the module that failed (screen_hmi, touch, speaker,
 * inmp441 or storage), passed through unchanged. The function that was called tells which module.
 */

/*
 * Prepares the board layer. All wiring values are constants fixed at build time, so this call only
 * marks the layer ready. It does NOT touch any hardware: no pin is configured and no bus is created.
 * Call it once before any board_init_xxx. Returns BOARD_OK.
 */
int board_init(void);

/*
 * Bring-up of one device each. app_start does not call them yet, so each device is brought up by
 * its own app when the owner decides, one at a time. All of them return BOARD_ERR_STATE when
 * board_init was not called or the device is already up. After a failure the call can be repeated.
 *
 * board_init_screen: brings up the LCD. The backlight stays OFF (duty 0), switch it on with
 *   screen_hmi_set_backlight. It also drives the reset line shared with the touch controller.
 * board_init_touch: brings up the touch controller. The reset line is shared with the display and
 *   driven by it, so this returns BOARD_ERR_STATE until board_init_screen succeeded.
 *   The shared reset sequence is not verified on this board.
 * board_init_speaker, board_init_mic: create the I2S channel only. The clock does not run until
 *   speaker_start or inmp441_start.
 * board_init_storage: only hands the configuration to the storage service (storage_init does not
 *   touch the card). The card is brought up by storage_mount, which the storage app calls.
 */
int board_init_screen(void);
int board_init_touch(void);
int board_init_speaker(void);
int board_init_mic(void);
int board_init_storage(void);

/*
 * Return the instance of a device, or NULL until its board_init_xxx succeeded. The instances are
 * static and owned by this layer. Access follows the rules of each module (not thread safe).
 * The SD card has no getter: use the storage_xxx functions.
 */
screen_hmi_t *board_get_screen(void);
touch_t *board_get_touch(void);
speaker_t *board_get_speaker(void);
inmp441_t *board_get_mic(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */