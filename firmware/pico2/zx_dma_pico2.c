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
 * sudo openocd -f interface/picoprobe.cfg -f target/rp2040.cfg -c "program ./pico2.elf verify reset exit"
 * sudo openocd -f interface/picoprobe.cfg -f target/rp2040.cfg
 * gdb-multiarch ./pico2.elf
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
  gpio_put( GPIO_P2_BLIPPER, 1 );
  __asm volatile ("nop");
  __asm volatile ("nop");
  __asm volatile ("nop");
  __asm volatile ("nop");
  gpio_put( GPIO_P2_BLIPPER, 0 );
}

void main( void )
{
  bi_decl(bi_program_description("ZX Spectrum DMA Pico2 Board Binary."));

  /* Blipper, for the scope */
  gpio_init( GPIO_P2_BLIPPER ); gpio_set_dir( GPIO_P2_BLIPPER, GPIO_OUT ); gpio_put( GPIO_P2_BLIPPER, 0 );

  /* Watch for the incoming cue from Pico1 into this Pico */
  gpio_init( GPIO_P1_REQUEST_SIGNAL );  gpio_set_dir( GPIO_P1_REQUEST_SIGNAL, GPIO_IN );  gpio_pull_up( GPIO_P1_REQUEST_SIGNAL );

  /* Outgoing signal tells Pico1 when this Pico has the address bus set */
  gpio_init( GPIO_P2_DRIVING_SIGNAL );  gpio_set_dir( GPIO_P2_DRIVING_SIGNAL, GPIO_OUT ); gpio_put( GPIO_P2_DRIVING_SIGNAL, 1  );

  /* Set up the GPIOs which drive the Z80's address bus */
  gpio_init_mask( GPIO_ABUS_BITMASK );
  gpio_set_dir_in_masked( GPIO_ABUS_BITMASK );

  /* These are unused on this Pico and stay hi-Z */
  gpio_init( GPIO_Z80_MREQ );   gpio_set_dir( GPIO_Z80_MREQ, GPIO_IN );
  gpio_init( GPIO_Z80_RD   );   gpio_set_dir( GPIO_Z80_RD,   GPIO_IN );
  gpio_init( GPIO_Z80_CLK  );   gpio_set_dir( GPIO_Z80_CLK,  GPIO_IN );

  /* This side just listens to this req/ack exchange so it knows when to drive the address bus */
  gpio_init( GPIO_Z80_BUSREQ ); gpio_set_dir( GPIO_Z80_BUSREQ, GPIO_IN );
  gpio_init( GPIO_Z80_BUSACK ); gpio_set_dir( GPIO_Z80_BUSACK, GPIO_IN );

  const uint LED_PIN = PICO_DEFAULT_LED_PIN;
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  /* Other side holds this signal low while it initialises. Ignore everything until it's ready */
  while( gpio_get( GPIO_P1_REQUEST_SIGNAL ) == 0 ); 

  while( 1 )
  {
    /*
     * When BUSACK goes low Pico1 has the Z80 bus. That's when we put the
     * address on the address bus.
     *
     * This scanning busy-loop takes about 200ns per iteration.
     */
    while( gpio_get( GPIO_Z80_BUSACK ) == 1 ) test_blipper(); 

    const uint32_t write_address = 0x4000;
    uint32_t write_counter = 0;

    /* OK, Pico 1 has requested this Pico drives the address bus */
    for( write_counter = 0; write_counter < 1; write_counter++ )
    {
      /*
       * If Pico1 has turned off the bus request we've been
       * too slow - that's bad, stop driving the address bus immediately
       * and hope for the best
       */
      if( gpio_get( GPIO_Z80_BUSREQ ) == 1 )
	break;

      /* Wait for Pico1 to request the address bus is set */
      while( gpio_get( GPIO_P1_REQUEST_SIGNAL ) == 1 ); 

      /* Put 0x4000 on the address bus */
      gpio_set_dir_out_masked( GPIO_ABUS_BITMASK );
      gpio_put_masked( GPIO_ABUS_BITMASK, write_address+write_counter );

      /* Address bus has correct content, assert indicator showing we're driving address bus */
      /* This blipper goes high 200ns to 300ns after Pico1 requests the address be placed */
      gpio_put( GPIO_P2_BLIPPER, 1 );
      gpio_put( GPIO_P2_DRIVING_SIGNAL, 0 );

      /* Wait for the other Pico to end its memory request, then put the address bus lines back to inputs */
      while( gpio_get( GPIO_P1_REQUEST_SIGNAL ) == 0 ); 
      gpio_set_dir_in_masked( GPIO_ABUS_BITMASK );

      /* The address bus is loaded for about 390ns per byte */
      gpio_put( GPIO_P2_DRIVING_SIGNAL, 1 );
      gpio_put( GPIO_P2_BLIPPER, 0 );
    }

    /*
     * Pico1 will turn off the bus requested, then we wait for the
     * Z80 to acknowledge the DMA has ended and it's taken the bus back
     */
    while( gpio_get( GPIO_Z80_BUSACK ) == 0 );
  }

}
