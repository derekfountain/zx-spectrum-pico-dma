/*
 * ZX DMA Firmware, a Raspberry Pi Pico based Spectrum DMA device
 * Copyright (C) 2024 Derek Fountain
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * cmake -DCMAKE_BUILD_TYPE=Debug ..
 * make -j10
 * sudo openocd -f interface/picoprobe.cfg -f target/rp2040.cfg -c "program ./pico1.elf verify reset exit"
 * sudo openocd -f interface/picoprobe.cfg -f target/rp2040.cfg
 * gdb-multiarch ./pico1.elf
 *  target remote localhost:3333
 *  load
 *  monitor reset init
 *  continue
 */

#define _GNU_SOURCE      /* Expose memmem() in string.h */

#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include <string.h>

#include "gpios.h"

static void test_blipper( void )
{
  gpio_put( GPIO_P1_BLIPPER, 1 );
  __asm volatile ("nop");
  __asm volatile ("nop");
  __asm volatile ("nop");
  __asm volatile ("nop");
  gpio_put( GPIO_P1_BLIPPER, 0 );
}

void main( void )
{
  bi_decl(bi_program_description("ZX Spectrum DMA Pico1 Board Binary."));

  /* Blipper, for the scope */
  gpio_init( GPIO_P1_BLIPPER ); gpio_set_dir( GPIO_P1_BLIPPER, GPIO_OUT ); gpio_put( GPIO_P1_BLIPPER, 0 );

  /* Set up outgoing signal to the other Pico (active high, so set low now) */
  gpio_init( GPIO_P1_SIGNAL );  gpio_set_dir( GPIO_P1_SIGNAL, GPIO_OUT ); gpio_put( GPIO_P1_SIGNAL, 0 );

  /* Set up incoming signal from the other Pico telling us that it's controlling the address bus */
  gpio_init( GPIO_P2_SIGNAL ); gpio_set_dir( GPIO_P2_SIGNAL, GPIO_IN );

  gpio_init( GPIO_DBUS_D0  );   gpio_set_dir( GPIO_DBUS_D0,  GPIO_IN );
  gpio_init( GPIO_DBUS_D1  );   gpio_set_dir( GPIO_DBUS_D1,  GPIO_IN );
  gpio_init( GPIO_DBUS_D2  );   gpio_set_dir( GPIO_DBUS_D2,  GPIO_IN );
  gpio_init( GPIO_DBUS_D3  );   gpio_set_dir( GPIO_DBUS_D3,  GPIO_IN );
  gpio_init( GPIO_DBUS_D4  );   gpio_set_dir( GPIO_DBUS_D4,  GPIO_IN );
  gpio_init( GPIO_DBUS_D5  );   gpio_set_dir( GPIO_DBUS_D5,  GPIO_IN );
  gpio_init( GPIO_DBUS_D6  );   gpio_set_dir( GPIO_DBUS_D6,  GPIO_IN );
  gpio_init( GPIO_DBUS_D7  );   gpio_set_dir( GPIO_DBUS_D7,  GPIO_IN );

  gpio_init( GPIO_Z80_BUSREQ ); gpio_set_dir( GPIO_Z80_BUSREQ, GPIO_OUT ); gpio_put( GPIO_Z80_BUSREQ, 1 );
  gpio_init( GPIO_Z80_BUSACK ); gpio_set_dir( GPIO_Z80_BUSACK, GPIO_IN  ); gpio_pull_up( GPIO_Z80_BUSACK );

  gpio_init( GPIO_Z80_MREQ );   gpio_set_dir( GPIO_Z80_MREQ, GPIO_IN );
  gpio_init( GPIO_Z80_IORQ );   gpio_set_dir( GPIO_Z80_IORQ, GPIO_IN );
  gpio_init( GPIO_Z80_RD   );   gpio_set_dir( GPIO_Z80_RD,   GPIO_IN );
  gpio_init( GPIO_Z80_WR   );   gpio_set_dir( GPIO_Z80_WR,   GPIO_IN );
  gpio_init( GPIO_Z80_M1   );   gpio_set_dir( GPIO_Z80_M1,   GPIO_IN );
  gpio_init( GPIO_Z80_CLK  );   gpio_set_dir( GPIO_Z80_CLK,  GPIO_IN );
  gpio_init( GPIO_Z80_INT  );   gpio_set_dir( GPIO_Z80_INT,  GPIO_IN );

  const uint LED_PIN = PICO_DEFAULT_LED_PIN;
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  while( 1 )
  {
    /* Wait for INT signal from Z80 - indicates top border time is starting */
    while( gpio_get( GPIO_Z80_INT ) == 1 );

    /* Approx 150ns to 200ns passes while the RP2040 spots the /INT */

    /* Assert bus request */
    gpio_set_dir( GPIO_Z80_BUSREQ, GPIO_OUT ); gpio_put( GPIO_Z80_BUSREQ, 0 );

    /*
     * Spin waiting for Z80 to acknowledge. BUSACK goes active (low) on the 
     * rising edge of the clock - see fig8 in the Z80 manual
     */
    while( gpio_get( GPIO_Z80_BUSACK ) == 1 );

    /* Blipper goes high while DMA process is active */
    gpio_put( GPIO_P1_BLIPPER, 1 );

    /* Approx 1.2uS passes while the RP2040 waits for the Z80 to BUSACK */

    /* RD and IORQ lines are unused and stay inactive */
    gpio_set_dir( GPIO_Z80_RD,   GPIO_OUT ); gpio_put( GPIO_Z80_RD,   1 );
    gpio_set_dir( GPIO_Z80_IORQ, GPIO_OUT ); gpio_put( GPIO_Z80_IORQ, 1 );

    /*
     * Moving on to the right hand side of fig7 in the Z80 manual.
     * We're at the start of T1.
     *
     * Set address bus (active high signal to other Pico), then wait for other
     * Pico to confirm it's done it
     */
    gpio_put( GPIO_P1_SIGNAL, 1 );
    while( gpio_get( GPIO_P2_SIGNAL ) == 0 );

    /*
     * We've just had the rising edge of Z80 clock T1. Wait for it to fall
     * which puts us halfway through T1
     */
    while( gpio_get( GPIO_Z80_CLK ) == 1 );    

    /* Assert memory request */
    gpio_set_dir( GPIO_Z80_MREQ, GPIO_OUT ); gpio_put( GPIO_Z80_MREQ, 0 );

    /* Put 0x55 on the data bus */
    gpio_set_dir( GPIO_DBUS_D0,  GPIO_OUT ); gpio_put( GPIO_DBUS_D0, 1 );
    gpio_set_dir( GPIO_DBUS_D1,  GPIO_OUT ); gpio_put( GPIO_DBUS_D1, 0 );
    gpio_set_dir( GPIO_DBUS_D2,  GPIO_OUT ); gpio_put( GPIO_DBUS_D2, 1 );
    gpio_set_dir( GPIO_DBUS_D3,  GPIO_OUT ); gpio_put( GPIO_DBUS_D3, 0 );
    gpio_set_dir( GPIO_DBUS_D4,  GPIO_OUT ); gpio_put( GPIO_DBUS_D4, 1 );
    gpio_set_dir( GPIO_DBUS_D5,  GPIO_OUT ); gpio_put( GPIO_DBUS_D5, 0 );
    gpio_set_dir( GPIO_DBUS_D6,  GPIO_OUT ); gpio_put( GPIO_DBUS_D6, 1 );
    gpio_set_dir( GPIO_DBUS_D7,  GPIO_OUT ); gpio_put( GPIO_DBUS_D7, 0 );

    /*
     * Wait for Z80 clock to rise and fall - that's the start of T2, then
     * at the clock low point halfway through T2
     */
    while( gpio_get( GPIO_Z80_CLK ) == 0 );    
    while( gpio_get( GPIO_Z80_CLK ) == 1 );    

    /* Assert the write line to write it */
    gpio_set_dir( GPIO_Z80_WR, GPIO_OUT ); gpio_put( GPIO_Z80_WR, 0 );

    /* Wait for Z80 clock rising edge, that's the start of T3 */
    while( gpio_get( GPIO_Z80_CLK ) == 0 );    

    /* Wait for Z80 clock falling edge, that's halfway through T3 */
    while( gpio_get( GPIO_Z80_CLK ) == 1 );    

    /* Remove write and memory request */
    gpio_put( GPIO_Z80_WR,   1 );
    gpio_put( GPIO_Z80_MREQ, 1 ); 
    
    /* Wait for Z80 clock rising edge, that's the end of T3 */
    while( gpio_get( GPIO_Z80_CLK ) == 0 );    

    /*
     * Write cycle is complete. Clean everything up.
     *
     * Remove address bus (active high signal to other Pico), then wait for
     * the other Pico to signal it's released the address bus
     */
    gpio_put( GPIO_P1_SIGNAL, 0 );
    while( gpio_get( GPIO_P2_SIGNAL ) == 1 );

    /* Put the data and control buses back to hi-Z */
    gpio_set_dir( GPIO_DBUS_D0,  GPIO_IN );
    gpio_set_dir( GPIO_DBUS_D1,  GPIO_IN );
    gpio_set_dir( GPIO_DBUS_D2,  GPIO_IN );
    gpio_set_dir( GPIO_DBUS_D3,  GPIO_IN );
    gpio_set_dir( GPIO_DBUS_D4,  GPIO_IN );
    gpio_set_dir( GPIO_DBUS_D5,  GPIO_IN );
    gpio_set_dir( GPIO_DBUS_D6,  GPIO_IN );
    gpio_set_dir( GPIO_DBUS_D7,  GPIO_IN );

    gpio_set_dir( GPIO_Z80_MREQ, GPIO_IN );
    gpio_set_dir( GPIO_Z80_WR,   GPIO_IN );
    gpio_set_dir( GPIO_Z80_IORQ, GPIO_IN );
    gpio_set_dir( GPIO_Z80_RD,   GPIO_IN );

    /*
     * DMA complete.
     *
     * Release bus request
     */
    gpio_put( GPIO_Z80_BUSREQ, 1 );

    /* Indicate DMA process complete */
    gpio_put( GPIO_P1_BLIPPER, 0 );

    /*
     * The INT signal from the ULA lasts 10uS. This current DMA experiment
     * code is way quicker than that, so INT will still be active (low).
     * So wait for it to finish and go high again.
     */
    while( gpio_get( GPIO_Z80_INT ) == 0 );
  }

}
