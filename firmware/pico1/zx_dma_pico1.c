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

/*
 * Callback here means INT signal from Z80 - indicates top border time is starting.
 *
 * https://worldofspectrum.org/faq/reference/48kreference.htm
 *
 * Spectrum top border time is 64 line times, which is 14336 T states. That's
 * 0.004096 of a second, or a smidge over 4ms. This routine needs to run in
 * that time, otherwise contention comes into play, not to mention the Z80
 * potentially writing to screen memory.
 *
 * The screen is 6144+768 bytes, = 6912 bytes.
 *
 ** As things stand, 6912 bytes takes about 6ms to fill in, which means
 ** it's taking longer than top border time and bad things are happening.
 ** Not quite sure what, TBH.
 **
 ** Solve problem 1: even if the destination address isn't incremented,
 ** it writes to lots of different screen bytes. Need to find out why.
 **
 ** When that's solved, disconnect from th Z80 timings, the DMA should be
 ** able to run as fast as the DRAM can go.
 */
void int_callback( uint gpio, uint32_t events ) 
{
#if 0
  gpio_put( GPIO_P1_BLIPPER, 1 );

  /* Assert bus request */
  gpio_put( GPIO_Z80_BUSREQ, 0 );

  while( gpio_get( GPIO_Z80_BUSACK ) == 1 );

  gpio_put( GPIO_Z80_BUSREQ, 1 );

  gpio_put( GPIO_P1_BLIPPER, 0 );

  return;
#endif










  /* Assert bus request */
  gpio_put( GPIO_Z80_BUSREQ, 0 );

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

  uint32_t byte_counter;
  for( byte_counter=0; byte_counter < 1; byte_counter++ )
  {
    /*
     * Moving on to the right hand side of fig7 in the Z80 manual.
     * We're at the start of T1.
     *
     * Set address bus (active low signal to other Pico), then wait for other
     * Pico to confirm it's done it
     */
    gpio_put( GPIO_P1_REQUEST_SIGNAL, 0 );
    while( gpio_get( GPIO_P2_DRIVING_SIGNAL ) == 1 );

    /*
     * We've just had the rising edge of Z80 clock T1. Wait for it to fall
     * which puts us halfway through T1
     */
    while( gpio_get( GPIO_Z80_CLK ) == 1 );    

    /* Assert memory request */
    gpio_set_dir( GPIO_Z80_MREQ, GPIO_OUT ); gpio_put( GPIO_Z80_MREQ, 0 );

    /* Put 0x55 on the data bus */
    gpio_set_dir_out_masked( GPIO_DBUS_BITMASK );
    gpio_put_masked( GPIO_DBUS_BITMASK, 0x00000055);

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
     * Remove address bus (active high signal to other Pico). This isn't
     * used at this point, the other Pico will have stopped driving the
     * address bus when the MREQ signal was turned off (just above). So
     * turning this signal off is just housekeeping at this point.
     */
    gpio_put( GPIO_P1_REQUEST_SIGNAL, 1 );
    while( gpio_get( GPIO_P2_DRIVING_SIGNAL ) == 0 );
  }

  /* Put the data and control buses back to hi-Z */
  gpio_set_dir_in_masked( GPIO_DBUS_BITMASK );

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
}

void main( void )
{
  bi_decl(bi_program_description("ZX Spectrum DMA Pico1 Board Binary."));

  /* Blipper, for the scope */
  gpio_init( GPIO_P1_BLIPPER ); gpio_set_dir( GPIO_P1_BLIPPER, GPIO_OUT ); gpio_put( GPIO_P1_BLIPPER, 0 );

  /* Set up outgoing signal to the other Pico (active low, so set high now) */
  gpio_init( GPIO_P1_REQUEST_SIGNAL );  gpio_set_dir( GPIO_P1_REQUEST_SIGNAL, GPIO_OUT ); gpio_put( GPIO_P1_REQUEST_SIGNAL, 1 );

  /* Set up incoming signal from the other Pico telling us that it's controlling the address bus */
  gpio_init( GPIO_P2_DRIVING_SIGNAL ); gpio_set_dir( GPIO_P2_DRIVING_SIGNAL, GPIO_IN );

  /* Initialise Z80 data bus GPIOs as inputs */
  gpio_init_mask( GPIO_DBUS_BITMASK );
  gpio_set_dir_in_masked( GPIO_DBUS_BITMASK );

  /* Set up Z80 control bus */
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

  /* Let the Spectrum do its RAM check before we start interfering */
  sleep_ms(4000);

  gpio_set_irq_enabled_with_callback( GPIO_Z80_INT, GPIO_IRQ_EDGE_FALL, true, &int_callback );

  while( 1 )
  {
    sleep_ms(5);
  }

}
