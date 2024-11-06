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

  /* Watch for the incoming cue from the other Pico */
  gpio_init( GPIO_P2_SIGNAL );  gpio_set_dir( GPIO_P2_SIGNAL, GPIO_IN );  gpio_pull_down( GPIO_P2_SIGNAL );

  /* Outgoing signal tells Pico1 when this Pico has the address bus set */
  gpio_init( GPIO_P2_LINKOUT ); gpio_set_dir( GPIO_P2_LINKOUT, GPIO_OUT ); gpio_put( GPIO_P2_LINKOUT, 0  );

  gpio_init( GPIO_ABUS_A0  );   gpio_set_dir( GPIO_ABUS_A0,  GPIO_IN );
  gpio_init( GPIO_ABUS_A1  );   gpio_set_dir( GPIO_ABUS_A1,  GPIO_IN );
  gpio_init( GPIO_ABUS_A2  );   gpio_set_dir( GPIO_ABUS_A2,  GPIO_IN );
  gpio_init( GPIO_ABUS_A3  );   gpio_set_dir( GPIO_ABUS_A3,  GPIO_IN );
  gpio_init( GPIO_ABUS_A4  );   gpio_set_dir( GPIO_ABUS_A4,  GPIO_IN );
  gpio_init( GPIO_ABUS_A5  );   gpio_set_dir( GPIO_ABUS_A5,  GPIO_IN );
  gpio_init( GPIO_ABUS_A6  );   gpio_set_dir( GPIO_ABUS_A6,  GPIO_IN );
  gpio_init( GPIO_ABUS_A7  );   gpio_set_dir( GPIO_ABUS_A7,  GPIO_IN );
  gpio_init( GPIO_ABUS_A8  );   gpio_set_dir( GPIO_ABUS_A8,  GPIO_IN );
  gpio_init( GPIO_ABUS_A9  );   gpio_set_dir( GPIO_ABUS_A9,  GPIO_IN );
  gpio_init( GPIO_ABUS_A10 );   gpio_set_dir( GPIO_ABUS_A10, GPIO_IN );
  gpio_init( GPIO_ABUS_A11 );   gpio_set_dir( GPIO_ABUS_A11, GPIO_IN );
  gpio_init( GPIO_ABUS_A12 );   gpio_set_dir( GPIO_ABUS_A12, GPIO_IN );
  gpio_init( GPIO_ABUS_A13 );   gpio_set_dir( GPIO_ABUS_A13, GPIO_IN );
  gpio_init( GPIO_ABUS_A14 );   gpio_set_dir( GPIO_ABUS_A14, GPIO_IN );
  gpio_init( GPIO_ABUS_A15 );   gpio_set_dir( GPIO_ABUS_A15, GPIO_IN );

  /* These are unused on this Pico and stay hi-Z */
  gpio_init( GPIO_Z80_MREQ );   gpio_set_dir( GPIO_Z80_MREQ, GPIO_IN );
  gpio_init( GPIO_Z80_RD   );   gpio_set_dir( GPIO_Z80_RD,   GPIO_IN );
  gpio_init( GPIO_Z80_CLK  );   gpio_set_dir( GPIO_Z80_CLK,  GPIO_IN );

  const uint LED_PIN = PICO_DEFAULT_LED_PIN;
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  while( 1 )
  {
    /* Signal from Pico1 is active high */
    while( gpio_get( GPIO_P2_SIGNAL ) == 0 );
    
    gpio_put( GPIO_P2_LINKOUT, 1  );

    gpio_put( GPIO_P2_BLIPPER, 1 );

    /* Put 0x4000 on the address bus */
    gpio_set_dir( GPIO_ABUS_A0,  GPIO_OUT ); gpio_put( GPIO_ABUS_A0, 0 );
    gpio_set_dir( GPIO_ABUS_A1,  GPIO_OUT ); gpio_put( GPIO_ABUS_A1, 0 );
    gpio_set_dir( GPIO_ABUS_A2,  GPIO_OUT ); gpio_put( GPIO_ABUS_A2, 0 );
    gpio_set_dir( GPIO_ABUS_A3,  GPIO_OUT ); gpio_put( GPIO_ABUS_A3, 0 );
    gpio_set_dir( GPIO_ABUS_A4,  GPIO_OUT ); gpio_put( GPIO_ABUS_A4, 0 );
    gpio_set_dir( GPIO_ABUS_A5,  GPIO_OUT ); gpio_put( GPIO_ABUS_A5, 0 );
    gpio_set_dir( GPIO_ABUS_A6,  GPIO_OUT ); gpio_put( GPIO_ABUS_A6, 0 );
    gpio_set_dir( GPIO_ABUS_A7,  GPIO_OUT ); gpio_put( GPIO_ABUS_A7, 0 );
    gpio_set_dir( GPIO_ABUS_A8,  GPIO_OUT ); gpio_put( GPIO_ABUS_A8, 0 );
    gpio_set_dir( GPIO_ABUS_A9,  GPIO_OUT ); gpio_put( GPIO_ABUS_A9, 0 );
    gpio_set_dir( GPIO_ABUS_A10, GPIO_OUT ); gpio_put( GPIO_ABUS_A10, 0 );
    gpio_set_dir( GPIO_ABUS_A11, GPIO_OUT ); gpio_put( GPIO_ABUS_A11, 0 );
    gpio_set_dir( GPIO_ABUS_A12, GPIO_OUT ); gpio_put( GPIO_ABUS_A12, 0 );
    gpio_set_dir( GPIO_ABUS_A13, GPIO_OUT ); gpio_put( GPIO_ABUS_A13, 0 );
    gpio_set_dir( GPIO_ABUS_A14, GPIO_OUT ); gpio_put( GPIO_ABUS_A14, 1 );
    gpio_set_dir( GPIO_ABUS_A15, GPIO_OUT ); gpio_put( GPIO_ABUS_A15, 0 );

    gpio_put( LED_PIN, 1 );

    /* Wait for the signal from the other Pico to stop, then put the address bus lines back to inputs */
    while( gpio_get( GPIO_P2_SIGNAL ) == 1 );

    gpio_put( LED_PIN, 0 );

    gpio_set_dir( GPIO_ABUS_A0,  GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A1,  GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A2,  GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A3,  GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A4,  GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A5,  GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A6,  GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A7,  GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A8,  GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A9,  GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A10, GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A11, GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A12, GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A13, GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A14, GPIO_IN );
    gpio_set_dir( GPIO_ABUS_A15, GPIO_IN );

    gpio_put( GPIO_P2_LINKOUT, 0  );

    gpio_put( GPIO_P2_BLIPPER, 0 );
  }

}
