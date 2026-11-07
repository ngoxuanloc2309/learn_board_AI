#ifndef ZX3D50CE08S_V15_USRC_H
#define ZX3D50CE08S_V15_USRC_H

#ifdef __cplusplus
extern "C" {
#endif

/*Export IO Interface Description*/
#define EXT_IO1             GPIO_10
#define EXT_IO2             GPIO_11
#define EXT_IO3             GPIO_12
#define EXT_IO4             GPIO_13
#define EXT_IO5             GPIO_14
#define EXT_IO6             GPIO_21

/*Speaker Connector*/
#define SPK_POSITIVE        ?           /*扬声器正*/
#define SPK_NEGATIVE        ?           /*扬声器负*/

/*SDcard Interface*/
#define SD_CS               GPIO_41
#define SD_MOSI             GPIO_40
#define SD_CLK              GPIO_39
#define SD_MISO             GPIO_38

/*LCD Interface Description*/
#define BL_PWM              GPIO_45     /*Backlight control, active high*/
#define LCD_RESET           GPIO_4      /*LCD reset, multiplexed with touch reset*/
#define LCD_RS              GPIO_0      /*Command/Data selection*/
#define LCD_WR              GPIO_47     /*Write clock*/
#define LCD_TE              GPIO_48     /*Frame sync*/

/*LCD data interface, 8bit MCU (8080)*/
#define LCD_DB0             GPIO_9      
#define LCD_DB1             GPIO_46
#define LCD_DB2             GPIO_3
#define LCD_DB3             GPIO_8
#define LCD_DB4             GPIO_18
#define LCD_DB5             GPIO_17
#define LCD_DB6             GPIO_16
#define LCD_DB7             GPIO_15
/*LCD data interface, 8bit MCU (8080)*/

#define TP_INT              GPIO_7  /*Touch interrupt*/
#define TP_SDA              GPIO_6  /*Touch IIC data*/
#define TP_SCL              GPIO_5  /*Touch IIC clock*/
#define TP_RST              GPIO_4  /*Touch reset, multiplexed with LCD reset*/
/*LCD Interface Description*/

/*RS485*/
#define RXD                 GPIO_1
#define RTS                 GPIO_2
#define TXD                 GPIO_42

/*Audio amplifier*/
#define LRCK                GPIO_35
#define BCLK                GPIO_36
#define DOUT                GPIO_37

/*
    Hardware Peripherals

Audio amplifier             IIS, 2.5W @4R
LCD with touch panel        3.5 Inch capacitive touchscreen fully fit LCD screen, 480*320, 8080interface
SD card                     SD card using SPI interface
RS485                       Serial interface with flow control

    Specifications
LCD Param
Display type:               LCD
Drive IC model:             ST7796UI
Visual angle:               Full
Resolution:                 480*320
Interface:                  8bit MCU（8080）
Color:                      RGB565

Touch Param
Touchscreen type:           Capacitive touch
Drive IC model:             FT6336U
Interface:                  I2C
Touchscreen structure:      G+F
Touch mode:                 Single touch
Surface hardness:           >6H

*/

#ifdef __cplusplus
}
#endif

#endif