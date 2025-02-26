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

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"


#include "gpios.h"

//#define OVERCLOCK 270000

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
 * Local copy of the ZX display file. Pixel data is 256x192 pixels,
 * at 8 pixels per byte. Colour attributes are 32x24 bytes
 */
static uint8_t zx_screen_mirror[((256*192)/8) + (32*24)];
static uint16_t a_log[100000];
static uint32_t a_log_ctr = 0;

/*
 * This handler is called when the ULA pings the /INT line.
 *
 * Spectrum top border time is 64 line times, which is 14336 T states. That's
 * 0.004096 of a second, or a smidge over 4ms. DMA in top border time needs to
 * run in that time, otherwise contention comes into play, not to mention the Z80
 * potentially writing to screen memory.
 *
 * The top border is 64 lines, each line being 224Ts.
 */
void int_handler( uint gpio, uint32_t events ) 
{
  /*
   * Crude hack to let the ROM interrupt routine run, makes testing easier
   * because the Spectrum's keyboard scanning routine is in the interrupt
   * routine which runs at the same time as this DMA code.
   * If BASIC isn't running this isn't necessary. Even if BASIC is running
   * the Spectrum still works even without this. So I'm not quite sure how
   * necessary it is.
   */
  busy_wait_ms(1);

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
  for( byte_counter=0; byte_counter < 6912; byte_counter++ )
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
    gpio_put_masked( GPIO_ABUS_BITMASK, (write_address+byte_counter)<<GPIO_ABUS_A0 );

    /* Assert memory request */
    gpio_put( GPIO_Z80_MREQ, 0 );

    /* Put value on the data bus */
    gpio_put_masked( GPIO_DBUS_BITMASK, zx_screen_mirror[byte_counter]);

    /* Assert the write line to write it */
    gpio_put( GPIO_Z80_WR, 0 );

    /*
     * Spectrum RAM is rated 150ns which is 1.5e-07. RP2350 clock speed is
     * 150,000,000Hz, so one clock cycle is 6.66666666667e-09. So that's 22.5
     * RP2350 clock cycles in one DRAM transaction time. NOP is T1, so it
     * takes one clock cycle, so 23 NOPs should guarantee a pause long
     * enough for the 4116s to respond.
     */
#define USING_STATIC_RAM_MODULE 1
#if USING_STATIC_RAM_MODULE
  /*
   * This was developed on a Spectrum containing a static RAM-based lower memory
   * module. I thought it would be faster than the 4116s, so should work with
   * fewer than 23 NOPs. Turns out it doesn't. Empirical testing shows it needs 29,
   * which is 1.93e-07 seconds, or about 1.93 microseconds. It don't know why.
   * 
   * Update: it turns out that sometimes 29 is too few and the DMA doesn't work.
   * This appears to be related to the Spectrum's temperature. The cooler the
   * machine is the longer this delay needs to be - but again, this is with a
   * static RAM module.
   * 
   * I've currently go this at 35/150,000,000ths of a second. This seems
   * reliable. A transfer of 6,912 bytes at this speed takes 2.18ms.
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
#else
    /* Timings with 4116s go here */
#endif

    /* Remove write and memory request */
    gpio_put( GPIO_Z80_WR,   1 );
    gpio_put( GPIO_Z80_MREQ, 1 ); 
  }

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

int64_t start_dma_running( alarm_id_t id, void *user_data )
{
  gpio_set_irq_enabled_with_callback( GPIO_Z80_INT, GPIO_IRQ_EDGE_FALL, true, &int_handler );

  return 0;
}

void main( void )
{
  bi_decl(bi_program_description("ZX Spectrum DMA RP2350 Stamp XL Board Binary."));

#ifdef OVERCLOCK
  set_sys_clock_khz( OVERCLOCK, 1 );
#endif

  /* All interrupts off except the timers */
//  irq_set_mask_enabled( 0xFFFFFFFF, 0 );
//  irq_set_mask_enabled( 0x0000000F, 1 );

  /*
   * This is the Z80's /RESET signal, it's an input to this code.
   *
   * The internal pull up is required, but I don't know why. Without it the /RESET
   * line is held low and the Spectrum doesn't start up. With it the /RESET line
   * rises normally and the Spectrum starts as usual. Cutting the track from 
   * /RESET to the GPIO_Z80_RESET GPIO makes the problem go away, so it appears
   * the RP2350 GPIO is where the problem is, it's holding the line low. But I
   * don't really know. I discovered by accident that setting the RP2350's internal
   * pull up makes the problem go away. OK, I'll take it.
   */
  gpio_init( GPIO_Z80_RESET );  gpio_set_dir( GPIO_Z80_RESET, GPIO_IN ); gpio_pull_up( GPIO_Z80_RESET );

  /* GPIO_RESET_Z80 is the output which drives a reset to the Z80. Hold the Z80 in reset until everything is set up */
  gpio_init( GPIO_RESET_Z80 ); gpio_set_dir( GPIO_RESET_Z80, GPIO_OUT ); gpio_put( GPIO_RESET_Z80, 1 );
 
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

  /* Let the Spectrum run and do its RAM check before we start interferring */
  gpio_put( GPIO_RESET_Z80, 0 );

  add_alarm_in_ms( 3000, start_dma_running, NULL, 0 );

  /*
   * The IRQ handler stuff is nowhere near fast enough to handle this. The Z80's
   * write is finished long before the RP2350 even gets to call the handler function
   */
  while( 1 )
  {
    register uint64_t gpios = gpio_get_all64();

    const uint64_t WR_MREQ_MASK = (0x01 << GPIO_Z80_MREQ) | (0x01 << GPIO_Z80_WR);

    if( (gpios & WR_MREQ_MASK) == 0 )
    {
       gpio_put( GPIO_BLIPPER2, 1 );
      __asm volatile ("nop");
      __asm volatile ("nop");
      gpio_put( GPIO_BLIPPER2, 0 );

      /* It's a write to memory, find the address being written to */
      uint64_t address = (gpios & (0xFFFF << GPIO_ABUS_A0)) >> GPIO_ABUS_A0;

      const uint64_t display_first_byte = 0x4000;
      const uint64_t display_last_byte  = 0x5AFF;

      if( (address >= display_first_byte) && (address <= display_last_byte) )
      {
        uint8_t data = (gpios & (0xFF << GPIO_DBUS_D0)) >> GPIO_DBUS_D0;
        zx_screen_mirror[address-display_first_byte] = (data & 0xff);

       gpio_put( GPIO_BLIPPER2, 1 );
      __asm volatile ("nop");
      __asm volatile ("nop");
      gpio_put( GPIO_BLIPPER2, 0 );
      }

      /* Wait for the Z80 write to finish */
      while( (gpio_get_all64() & WR_MREQ_MASK) == 0 );
    }

  }

}
