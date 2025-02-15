/*
 * GPIOs for RP2350 Stamp XL
 *
 * These need to match the hardware design.
 */

#ifndef __GPIOS_H
#define GPIOS_H

/* Test signals to scope */
#define GPIO_BLIPPER1     46
#define GPIO_BLIPPER2     47

/* Take over the Spectrum's ROM */
#define GPIO_ROMCS        28

/* ERROR HERE, this pin is connected directly to the Z80 /RESET */
#define GPIO_Z80_RESET    36

/*
 * ERROR HERE, this pin is the output connected to the base of Q101.
 * With no transistor in place this doesn't do anything
 */
#define GPIO_RESET_Z80    43

/* Z80 Control bus */
#define GPIO_Z80_CLK      24
#define GPIO_Z80_INT      25
#define GPIO_Z80_RD       26
#define GPIO_Z80_WR       27
#define GPIO_Z80_MREQ     29
#define GPIO_Z80_IORQ     30
#define GPIO_Z80_BUSREQ   31
#define GPIO_Z80_BUSACK   32
#define GPIO_Z80_WAIT     33

/* Z80 Data bus */
#define GPIO_DBUS_D0      0
#define GPIO_DBUS_D1      1
#define GPIO_DBUS_D2      2
#define GPIO_DBUS_D3      3
#define GPIO_DBUS_D4      4
#define GPIO_DBUS_D5      5
#define GPIO_DBUS_D6      6
#define GPIO_DBUS_D7      7

/* Z80 Address bus */
#define GPIO_DBUS_A0      8
#define GPIO_DBUS_A1      9
#define GPIO_DBUS_A2      10
#define GPIO_DBUS_A3      11
#define GPIO_DBUS_A4      12
#define GPIO_DBUS_A5      13
#define GPIO_DBUS_A6      14
#define GPIO_DBUS_A7      15
#define GPIO_DBUS_A8      16
#define GPIO_DBUS_A9      17
#define GPIO_DBUS_A10     18
#define GPIO_DBUS_A11     19
#define GPIO_DBUS_A12     20
#define GPIO_DBUS_A13     21
#define GPIO_DBUS_A14     22
#define GPIO_DBUS_A15     23

#define GPIO_DBUS_BITMASK 0x000000FF
#define GPIO_ABUS_BITMASK 0x00FFFF00

#endif
