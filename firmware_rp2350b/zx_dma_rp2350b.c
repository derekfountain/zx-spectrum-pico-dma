/*
 * ZX DMA Firmware, a Raspberry Pi RP2350b based Spectrum DMA device
 * Copyright (C) 2025 Derek Fountain
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
 * sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "program ./zx_dma_rp2350b.elf verify reset exit"
 * sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg
 * gdb-multiarch ./zx_dma_rp2350b.elf
 *  target remote localhost:3333
 *  load
 *  monitor reset init
 *  continue
 */

#define _GNU_SOURCE      /* Expose memmem() in string.h */

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include <string.h>

#include "gpios.h"

static void test_blipper( void )
{
  gpio_put( GPIO_BLIPPER1, 1 );
  __asm volatile ("nop");
  __asm volatile ("nop");
  __asm volatile ("nop");
  __asm volatile ("nop");
  gpio_put( GPIO_BLIPPER1, 0 );
}

/*
 * https://worldofspectrum.org/faq/reference/48kreference.htm
 */

/*
 * This handler is called when the ULA pings the /INT line.
 *
 * The top border is 64 lines, each line being 224Ts.
 *
 * Spectrum top border time is 64 line times, which is 14336 T states. That's
 * 0.004096 of a second, or a smidge over 4ms. DMA in top border time needs to
 * run in that time, otherwise contention comes into play, not to mention the Z80
 * potentially writing to screen memory.
 *
 * The screen is 192 lines, each line being 224Ts.
 */
#if 0
void int_callback( uint gpio, uint32_t events ) 
{
  /* Assert bus request */
  gpio_put( GPIO_Z80_BUSREQ, 0 );

  /*
   * Spin waiting for Z80 to acknowledge. BUSACK goes active (low) on the 
   * rising edge of the clock - see fig8 in the Z80 manual
   */
  while( gpio_get( GPIO_Z80_BUSACK ) == 1 );

  /* OK, we have the Z80's bus */

  /* RD and IORQ lines are unused by this DMA process and stay inactive */
  gpio_set_dir( GPIO_Z80_RD,   GPIO_OUT ); gpio_put( GPIO_Z80_RD,   1 );
  gpio_set_dir( GPIO_Z80_IORQ, GPIO_OUT ); gpio_put( GPIO_Z80_IORQ, 1 );

  /* Reset Z80 address and data bus GPIOs as outputs */
  gpio_set_dir_out_masked( GPIO_ABUS_BITMASK );
  gpio_set_dir_out_masked( GPIO_DBUS_BITMASK );

  /* Set directions of control signals to outputs */
  gpio_set_dir( GPIO_Z80_MREQ, GPIO_OUT ); gpio_put( GPIO_Z80_MREQ, 1 );
  gpio_set_dir( GPIO_Z80_WR,   GPIO_OUT ); gpio_put( GPIO_Z80_WR,   1 );

  /* Blipper goes high while DMA process is active */
  gpio_put( GPIO_BLIPPER1, 1 );

  const uint32_t write_address = 0x4000;

  uint32_t byte_counter;
  for( byte_counter=0; byte_counter < 1/*6912*/; byte_counter++ )
  {
    /*
     * With full Z80 synchronisation a 2048 byte DMA transfer takes 2.9ms.
     * Removing all the Z80 synchronisation stuff and putting in a fairly
     * precise pause tuned to the 150ns DRAM in the Spectrum results in a
     * 2048 byte DMA transfer in 1.850ms.
     * Top border time is 4.096ms, so the target for a 2048 transfer (a
     * third of the screen) is 4.096/3 which is 1.37ms.
     * Using the lower border, 6.325ms/3 is 2.108ms, which I'm now inside.
     *
     * So, use the lower border and my current performance is 5.55ms for
     * the whole screen, which is inside the 6.325 total border time.
     * In theory.
     */

    /* Set address of ZX byte to write to */
    gpio_put_masked( GPIO_ABUS_BITMASK, write_address+byte_counter );

    /* Put 0x55 on the data bus */
    gpio_put_masked( GPIO_DBUS_BITMASK, 0x00000001);

    /* Assert memory request */
    gpio_put( GPIO_Z80_MREQ, 0 );

    /* Assert the write line to write it */
    gpio_put( GPIO_Z80_WR, 0 );

    /*
     * Spectrum RAM is rated 150ns which is 1.5e-07. RP2350 clock speed is
     * 150,000,000Hz, so one clock cycle is 6.66666666667e-09. So that's 22.5
     * RP2350 clock cycles in one DRAM transaction time. NOP is T1, so it
     * takes one clock cycle, so 23 NOPs should guarantee a pause long
     * enough for the 4116s to respond.
     */
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    /* Remove write and memory request */
    gpio_put( GPIO_Z80_WR,   1 );
    gpio_put( GPIO_Z80_MREQ, 1 ); 

  }

  /* Address bus back to inputs */

  /* Put the address, data and control buses back to hi-Z */
  gpio_set_dir_in_masked( GPIO_ABUS_BITMASK );
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
  gpio_put( GPIO_BLIPPER1, 0 );

  return;
}
#endif

void main( void )
{
  bi_decl(bi_program_description("ZX Spectrum DMA RP2350 Stamp XL Board Binary."));

  /* Hold the Z80 in reset until everything is set up */
  /*
   * This line is connected directly to the Z80, which is
   * wrong.
   * No transistor present, so this doesn't do anything.
   * Setting the pin to input seems to hold the reset line
   * and the ZX doesn't start. Not sure if it's the E9
   * behaviour? I cut the track and now it's behaving
   * correctly.
   */
  gpio_init( GPIO_Z80_RESET ); gpio_set_dir( GPIO_Z80_RESET, GPIO_OUT ); gpio_put( GPIO_Z80_RESET, 1 );
 
  /*
   * This is the output pin to the base of Q101. This is the one which
   * should drive the reset line, but the board is wrong.
   * With no transistor in place, this doesn't do anything
   */
  gpio_init( GPIO_RESET_Z80   );   gpio_set_dir( GPIO_RESET_Z80,   GPIO_IN );

  /* Blippers, for the scope */
  gpio_init( GPIO_BLIPPER1 ); gpio_set_dir( GPIO_BLIPPER1, GPIO_OUT ); gpio_put( GPIO_BLIPPER1, 0 );
  gpio_init( GPIO_BLIPPER2 ); gpio_set_dir( GPIO_BLIPPER2, GPIO_OUT ); gpio_put( GPIO_BLIPPER2, 0 );
 
  /* Not taking over the ZX ROM for the time being */
  gpio_init( GPIO_ROMCS );    gpio_set_dir( GPIO_ROMCS, GPIO_IN );

  /* Set up Z80 control bus */
  
  gpio_init( GPIO_Z80_CLK  );   gpio_set_dir( GPIO_Z80_CLK,  GPIO_IN );
  gpio_init( GPIO_Z80_RD   );   gpio_set_dir( GPIO_Z80_RD,   GPIO_IN );
  gpio_init( GPIO_Z80_WR   );   gpio_set_dir( GPIO_Z80_WR,   GPIO_IN );
  gpio_init( GPIO_Z80_MREQ );   gpio_set_dir( GPIO_Z80_MREQ, GPIO_IN );
  gpio_init( GPIO_Z80_IORQ );   gpio_set_dir( GPIO_Z80_IORQ, GPIO_IN );
  gpio_init( GPIO_Z80_INT  );   gpio_set_dir( GPIO_Z80_INT,  GPIO_IN );
  gpio_init( GPIO_Z80_WAIT );   gpio_set_dir( GPIO_Z80_WAIT, GPIO_IN );
 

  gpio_init( GPIO_Z80_BUSREQ ); gpio_set_dir( GPIO_Z80_BUSREQ, GPIO_OUT ); gpio_put( GPIO_Z80_BUSREQ, 1 );
  gpio_init( GPIO_Z80_BUSACK ); gpio_set_dir( GPIO_Z80_BUSACK, GPIO_IN  );

  /* Initialise Z80 data bus GPIOs as inputs */
  gpio_init_mask( GPIO_DBUS_BITMASK );  gpio_set_dir_in_masked( GPIO_DBUS_BITMASK );

  /* Initialise Z80 address bus GPIOs as inputs */
  gpio_init_mask( GPIO_ABUS_BITMASK );  gpio_set_dir_in_masked( GPIO_ABUS_BITMASK );

  /* Let the Spectrum do its RAM check before we start interfering */
  sleep_ms(4000);

  //gpio_set_irq_enabled_with_callback( GPIO_Z80_INT, GPIO_IRQ_EDGE_FALL, true, &int_callback );

  while( 1 )
  {
    sleep_ms(5);
  }

}
