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
  gpio_init( GPIO_P1_REQUEST_SIGNAL );  gpio_set_dir( GPIO_P1_REQUEST_SIGNAL, GPIO_IN );  gpio_pull_down( GPIO_P1_REQUEST_SIGNAL );

  /* Outgoing signal tells Pico1 when this Pico has the address bus set */
  gpio_init( GPIO_P2_DRIVING_SIGNAL );  gpio_set_dir( GPIO_P2_DRIVING_SIGNAL, GPIO_OUT ); gpio_put( GPIO_P2_DRIVING_SIGNAL, 0  );

  /* Set up the GPIOs which drive the Z80's address bus */
  gpio_init_mask( GPIO_ABUS_BITMASK );
  gpio_set_dir_in_masked( GPIO_ABUS_BITMASK );

  /* These are unused on this Pico and stay hi-Z */
  gpio_init( GPIO_Z80_MREQ );   gpio_set_dir( GPIO_Z80_MREQ, GPIO_IN );
  gpio_init( GPIO_Z80_RD   );   gpio_set_dir( GPIO_Z80_RD,   GPIO_IN );
  gpio_init( GPIO_Z80_CLK  );   gpio_set_dir( GPIO_Z80_CLK,  GPIO_IN );

  const uint LED_PIN = PICO_DEFAULT_LED_PIN;
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  while( 1 )
  {
    /* Signal from Pico1 into this Pico is active high. Wait for transition to low */
    /*
     * Ideally I could just wait here for BUSACK to go low. While that's low this
     * Pico needs to be driving the address bus. Right now the hardware doesn't
     * have that connection - this Pico can't see BUSACK - so I can't use it. But
     * it will be worth patching that if this works out.
     */
    while( gpio_get( GPIO_P1_REQUEST_SIGNAL ) == 0 );

    /* Pico 1 has requested this Pico drives the address bus */
    gpio_put( GPIO_P2_BLIPPER, 1 );
    gpio_put( GPIO_P2_DRIVING_SIGNAL,  1 );

/*
  create new loop here, say, 16 bytes - start with 1
  set directions at start of main loop, reset them at the end
  inner loop sets the incrementing value onto the address bus
  leaves it there until MREQ goes high - falling edge of clock at T3
  asserts each of 16 values in turn
  when the last one is complete, exit the inner loop, restore bus gpios
  wait for P2 signal to go high again, then go back to top
*/    

    /* Put 0x4000 on the address bus */
    gpio_set_dir_out_masked( GPIO_ABUS_BITMASK );
    gpio_put_masked( GPIO_ABUS_BITMASK, 0x00004000);

    /* Wait for the other Pico to end its memory request, then put the address bus lines back to inputs */
    while( gpio_get( GPIO_Z80_MREQ ) == 0 );

    gpio_set_dir_in_masked( GPIO_ABUS_BITMASK );

    gpio_put( GPIO_P2_DRIVING_SIGNAL,  0  );
    gpio_put( GPIO_P2_BLIPPER, 0 );
  }

}
