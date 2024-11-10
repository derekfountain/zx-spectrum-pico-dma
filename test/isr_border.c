/*
 * zcc +zx -vn -clib=sdcc_iy -startup=31 isr_border.c -o isr_border -create-app
 * tap2sna.py isr_border.tap isr_border.z80
 * Z80onMDR_linux isr_border.z80
 *
 * The plan here was to change the border at the start of the INT routine,
 * then let the DMA run, then change the border colour back when the next
 * Z80 instruction runs, the red coloured band at the top of the screen
 * indicating how long the DMA is taking. But it's not easy to synchronise
 * the Z80's instructions with the DMA, so the idea doesn't really work.
 */

#pragma output REGISTER_SP = 0xD000

#include <arch/zx.h>
#include <intrinsic.h>
#include <im2.h>
#include <string.h>
#include <z80.h>

IM2_DEFINE_ISR(isr)
{
  zx_border( INK_RED );

  uint16_t i;
  for( i=0; i<500; i++ );
}

#define TABLE_HIGH_BYTE        ((unsigned int)0xD0)
#define JUMP_POINT_HIGH_BYTE   ((unsigned int)0xD1)

#define UI_256                 ((unsigned int)256)
#define TABLE_ADDR             ((void*)(TABLE_HIGH_BYTE*UI_256))
#define JUMP_POINT             ((unsigned char*)( (unsigned int)(JUMP_POINT_HIGH_BYTE*UI_256) + JUMP_POINT_HIGH_BYTE ))

int main( void )
{
  memset( TABLE_ADDR, JUMP_POINT_HIGH_BYTE, 257 );
  z80_bpoke( JUMP_POINT,   195 );
  z80_wpoke( JUMP_POINT+1, (unsigned int)isr );
  im2_init( TABLE_ADDR );
  intrinsic_ei();

  zx_cls(INK_WHITE);
  while(1)
  {
    zx_border( INK_WHITE );
  }
}
