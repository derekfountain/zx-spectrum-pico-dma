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
 * https://worldofspectrum.org/faq/reference/48kreference.htm
 *
 * The top border is 64 lines, each line being 224Ts.
 *
 * Spectrum top border time is 64 line times, which is 14336 T states. That's
 * 0.004096 of a second, or a smidge over 4ms. DMA in top border time needs to
 * run in that time, otherwise contention comes into play, not to mention the Z80
 * potentially writing to screen memory.
 *
 * The screen is 192 lines, each line being 224Ts.
 *
 * Lower border is also usable if I can synchronise the Pico to running
 * when the lower border starts. Lower border is 56 lines, which is another
 * 2.229ms, so 6.325ms in total: that's start of lower border to the start of
 * the top line of the display.
 *
 * So, to hit the start of lower border from /INT, I need 64 lines of top border,
 * plus 192 lines of screen, That's 228 lines from /INT to the start of the lower
 * border. At 224Ts per line, 228 lines is 64,512 Ts from /INT to the point I
 * want to start BUSREQ. That's 18.432ms from /INT.
 *
 * The screen is 6144+768 bytes, = 6912 bytes.
 */
int64_t alarm_callback( alarm_id_t id, void *user_data )
{
/* The overlapping interrupts (i.e. INT arriving while the DMA is is running)
 * isn't working. Not sure if the SDK alarm thing is smart enough. Skipping
 * every other INT makes it stable
 */
  static uint32_t  toggle = 0;
  toggle = ~toggle;
  if ( ~toggle )
    return 0;

  /* Assert bus request */
  gpio_put( GPIO_Z80_BUSREQ, 0 );

  /*
   * Spin waiting for Z80 to acknowledge. BUSACK goes active (low) on the 
   * rising edge of the clock - see fig8 in the Z80 manual
   *
   * The delay between BUSREQ being asserted and BUSACK acknowledging is
   * between 800ns and 2uS. Part of that would be the Z80 and whatever it's
   * up to, and part would be how this loop fits with the moment the ACK
   * line goes low.
   */
  while( gpio_get( GPIO_Z80_BUSACK ) == 1 );

  /* RD and IORQ lines are unused and stay inactive */
  gpio_set_dir( GPIO_Z80_RD,   GPIO_OUT ); gpio_put( GPIO_Z80_RD,   1 );
  gpio_set_dir( GPIO_Z80_IORQ, GPIO_OUT ); gpio_put( GPIO_Z80_IORQ, 1 );

  /* Blipper goes high while DMA process is active */
  /* Approx 500ns passes between BUSACK and here */
  gpio_put( GPIO_P1_BLIPPER, 1 );

  uint32_t byte_counter;
  for( byte_counter=0; byte_counter < 6912; byte_counter++ )
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

    /* Assert memory request */
    gpio_set_dir( GPIO_Z80_MREQ, GPIO_OUT ); gpio_put( GPIO_Z80_MREQ, 0 );

    /* Put 0x55 on the data bus */
    gpio_set_dir_out_masked( GPIO_DBUS_BITMASK );
    gpio_put_masked( GPIO_DBUS_BITMASK, 0x00000055);

    /* Assert the write line to write it */
    gpio_set_dir( GPIO_Z80_WR, GPIO_OUT ); gpio_put( GPIO_Z80_WR, 0 );

    /*
     * Spectrum RAM is rated 150ns which is 1.5e-07. Pico clock speed is
     * 125,000,000Hz, so one clock cycle is 8e-09. So that's 18.75
     * RP2040 clock cycles in one DRAM transaction time. NOP is T1, so it
     * takes one clock cycle, so 19 NOPs should guarantee a pause long
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

    /* Remove write and memory request */
    gpio_put( GPIO_Z80_WR,   1 );
    gpio_put( GPIO_Z80_MREQ, 1 ); 
    
    /*
     * Write cycle is complete. Clean everything up.
     *
     * Remove address bus (active high signal to other Pico).
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

  /* Don't reschedule this alarm function */
  return 0;
}

/*
 * This handler is called when the ULA pings the /INT line.
 * That's the top of the top border, but that doesn't give me
 * enough time to DMA the screen, so I need to start the DMA
 * the the top of the lower border. i.e. just after the last
 * line of the screen has been drawn.
 * The only marker point I have is /INT, so I need to time
 * 18.432ms from this point. I can't do it that precisely
 * using SDK on core, so I have to do my best until I can get
 * PIOs involved.
 */
void int_callback( uint gpio, uint32_t events ) 
{
  /*
   * The delay between the /INT line going low and this routine
   * running is a very consistent, and almost exact, 2uS. That
   * precision comes from the RP2040 internals, presumably.
   * 
   *  gpio_put( GPIO_P1_BLIPPER, 1 );
   */
  const uint32_t INT_TO_HANDLER_TIME_US = 2;

  /*
   * The delay between this alarm being scheduled to go off and the
   * handler actually running varies by about 2uS. That's measured
   * with the blipper pinging at the top of the handler. I can't use
   * the lower number because that will set the BUSREQ running while
   * the lowest part of the screen is still being drawn, which is
   * going to cause contention issues. It has to be consistently
   * /at least/ 18.432ms before BUSREQ is fired so I have to err on
   * the side of caution here.
   *
   * This is where using PIO to get the timing right will be of
   * benefit.
   */
  const uint32_t ALARM_TO_HANDLER_TIME_US = 3;

  const uint32_t INT_TO_LOWER_BORDER_TIME_US = 18432;

  add_alarm_in_us( INT_TO_LOWER_BORDER_TIME_US-
		   INT_TO_HANDLER_TIME_US-
		   ALARM_TO_HANDLER_TIME_US, alarm_callback, (void*)0, false );
}

void main( void )
{
  bi_decl(bi_program_description("ZX Spectrum DMA Pico1 Board Binary."));

  /* Blipper, for the scope */
  gpio_init( GPIO_P1_BLIPPER ); gpio_set_dir( GPIO_P1_BLIPPER, GPIO_OUT ); gpio_put( GPIO_P1_BLIPPER, 0 );

  /* Set up outgoing signal to the other Pico (active low, hold it low for initialisation) */
  gpio_init( GPIO_P1_REQUEST_SIGNAL );  gpio_set_dir( GPIO_P1_REQUEST_SIGNAL, GPIO_OUT );
  gpio_put( GPIO_P1_REQUEST_SIGNAL, 0 );

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

  /* Other side is waiting on this signal to go high. Init is complete, release other side */
  gpio_put( GPIO_P1_REQUEST_SIGNAL, 1 );
  sleep_ms(1);

  gpio_set_irq_enabled_with_callback( GPIO_Z80_INT, GPIO_IRQ_EDGE_FALL, true, &int_callback );

  while( 1 )
  {
    sleep_ms(5);
  }

}
