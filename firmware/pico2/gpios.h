/*
 * GPIOs for Pico2, the one on the left side as you look at the Spectrum,
 * with the address bus
 */

#ifndef __GPIOS_H
#define GPIOS_H

/* These need to match the hardware design */

/* Test signal to scope */
#define GPIO_P2_BLIPPER   19

#define GPIO_ABUS_A0       0
#define GPIO_ABUS_A1       1
#define GPIO_ABUS_A2       2
#define GPIO_ABUS_A3       3
#define GPIO_ABUS_A4       4
#define GPIO_ABUS_A5       5
#define GPIO_ABUS_A6       6
#define GPIO_ABUS_A7       7
#define GPIO_ABUS_A8       8
#define GPIO_ABUS_A9       9
#define GPIO_ABUS_A10     10
#define GPIO_ABUS_A11     11
#define GPIO_ABUS_A12     12
#define GPIO_ABUS_A13     13
#define GPIO_ABUS_A14     14
#define GPIO_ABUS_A15     15

#define GPIO_ABUS_BITMASK 0x0000FFFF

#define GPIO_Z80_MREQ     16
#define GPIO_Z80_RD       17
#define GPIO_Z80_CLK      18

/* These were soldered on for this project */
#define GPIO_Z80_BUSREQ  26
#define GPIO_Z80_BUSACK  28

/* Signal from Pico1 to Pico2, requesting Pico2 to set the address bus */
#define GPIO_P1_REQUEST_SIGNAL    20

/* Signal from Pico2 to Pico1, telling Pico1 that Pico2 is driving the address bus */
#define GPIO_P2_DRIVING_SIGNAL    22

#endif
