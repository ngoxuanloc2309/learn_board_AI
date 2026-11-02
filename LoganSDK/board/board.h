#ifndef BOARD_H
#define BOARD_H

/*This is the board with HMI: WT32S3-WROVER - Model: ZX3D50CE08S-V15-USRC, 230208*/
typedef enum{
    BOARD_WT32S3_WROVER = 0,
    /*Other boards in the future*/
    BOARD_UNKNOWN
} BoardType_t;

#endif // BOARD_H