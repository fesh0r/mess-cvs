/***************************************************************************

Nintendo VS UniSystem and DualSystem - (c) 198? Nintendo of America

	Portions of this code are heavily based on
	Brad Oliver's MESS implementation of the NES.

RP2C04-001:
- Gradius
- Pinball
- Hogan's Alley
- Baseball
- Skykid
- Super Xevious
- Platoon

RP2C04-002:
- Ladies golf
- Mach Rider
- Stroke N' Match Golf
- Castlevania
- Slalom

RP2C04-003:
- Excite Bike
- Dr mario
- Soccer
- Goonies
- Tko boxing

RP2c05-004:
- Super Mario Bros
- Ice climber
- Clu Clu Land
- Rbi Baseball ( ????? )
- Top gun ( ? )

Rcp2c03b:
- Duckhunt
- Tennis
- Gumshoe (no rom dump)

***************************************************************************/
#include "driver.h"
#include "vidhrdw/generic.h"
#include "vidhrdw/ppu2c03b.h"
#include "cpu/z80/z80.h"
#include "machine/rp5h01.h"

/* clock frequency */
#define N2A03_DEFAULTCLOCK ( 21477272.724 / 12 )

/* from vidhrdw */
extern int vsnes_vh_start( void );
extern void vsnes_vh_stop( void );
extern void vsnes_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
extern void vsnes_vh_screenrefresh( struct osd_bitmap *bitmap, int full_refresh );
extern int vsdual_vh_start( void );
extern void vsdual_vh_screenrefresh( struct osd_bitmap *bitmap, int full_refresh );
extern void vsdual_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

/* from machine */
extern void vsnes_init_machine( void );
extern void vsdual_init_machine( void );
extern void init_vsnes( void );
extern void init_suprmrio( void );
extern void init_excitebk( void );
extern void init_vsnormal( void );
extern void init_duckhunt( void );
extern void init_hogalley( void );
extern void init_goonies( void );
extern void init_machridr( void );
extern void init_vsslalom( void );
extern void init_cstlevna( void );
extern void init_drmario( void);
extern void init_rbibb( void );
extern void init_tkoboxng( void );
extern void init_vstopgun( void );
extern void init_vsgradus( void );
extern void init_vspinbal( void );
extern void init_vsskykid( void );
extern void init_platoon(void);
extern void init_vstennis( void );
extern READ_HANDLER( vsnes_in0_r );
extern READ_HANDLER( vsnes_in1_r );
extern READ_HANDLER( vsnes_in0_1_r );
extern READ_HANDLER( vsnes_in1_1_r );
extern WRITE_HANDLER( vsnes_in0_w );
extern WRITE_HANDLER( vsnes_in0_1_w );

/******************************************************************************/

/* local stuff */
static UINT8 *work_ram, *work_ram_1;

static READ_HANDLER( mirror_ram_r )
{
	return work_ram[ offset & 0x7ff ];
}

static READ_HANDLER( mirror_ram_1_r )
{
	return work_ram[ offset & 0x7ff ];
}

static WRITE_HANDLER( mirror_ram_w )
{
	work_ram[ offset & 0x7ff ] = data;
}

static WRITE_HANDLER( mirror_ram_1_w )
{
	work_ram[ offset & 0x7ff ] = data;
}

static WRITE_HANDLER( sprite_dma_w )
{
	int source = ( data & 7 ) * 0x100;

	ppu2c03b_spriteram_dma( 0, &work_ram[source] );
}

static WRITE_HANDLER( sprite_dma_1_w )
{
	int source = ( data & 7 ) * 0x100;

	ppu2c03b_spriteram_dma( 1, &work_ram_1[source] );
}

/******************************************************************************/

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x0800, 0x1fff, mirror_ram_r },
	{ 0x2000, 0x3fff, ppu2c03b_0_r },
	{ 0x4000, 0x4015, NESPSG_0_r },
	{ 0x4016, 0x4016, vsnes_in0_r },
	{ 0x4017, 0x4017, vsnes_in1_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x07ff, MWA_RAM, &work_ram },
	{ 0x0800, 0x1fff, mirror_ram_w },
	{ 0x2000, 0x3fff, ppu2c03b_0_w },
	{ 0x4011, 0x4011, DAC_0_data_w },
	{ 0x4014, 0x4014, sprite_dma_w },
	{ 0x4000, 0x4015, NESPSG_0_w },
	{ 0x4016, 0x4016, vsnes_in0_w },
	{ 0x4017, 0x4017, MWA_NOP }, /* in 1 writes ignored */
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress readmem_1[] =
{
	{ 0x0000, 0x07ff, MRA_RAM },
	{ 0x0800, 0x1fff, mirror_ram_1_r },
	{ 0x2000, 0x3fff, ppu2c03b_1_r },
	{ 0x4000, 0x4015, NESPSG_0_r },
	{ 0x4016, 0x4016, vsnes_in0_1_r },
	{ 0x4017, 0x4017, vsnes_in1_1_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem_1[] =
{
	{ 0x0000, 0x07ff, MWA_RAM, &work_ram_1 },
	{ 0x0800, 0x1fff, mirror_ram_1_w },
	{ 0x2000, 0x3fff, ppu2c03b_1_w },
	{ 0x4011, 0x4011, DAC_1_data_w },
	{ 0x4014, 0x4014, sprite_dma_1_w },
	{ 0x4000, 0x4015, NESPSG_1_w },
	{ 0x4016, 0x4016, vsnes_in0_1_w },
	{ 0x4017, 0x4017, MWA_NOP }, /* in 1 writes ignored */
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

/******************************************************************************/

#define VS_CONTROLS \
	PORT_START	/* IN0 */ \
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 )					/* BUTTON A on a nes */ \
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 )					/* BUTTON B on a nes */ \
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_START1 )					/* SELECT on a nes */ \
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 )					/* START on a nes */ \
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) \
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) \
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) \
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) \
\
\
	PORT_START	/* IN1 */ \
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )	/* BUTTON A on a nes */ \
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )	/* BUTTON B on a nes */ \
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_START2 )					/* SELECT on a nes */ \
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER2 ) 	/* START on a nes */ \
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_PLAYER2 ) \
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_PLAYER2 ) \
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_PLAYER2 ) \
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 ) \
\
\
	PORT_START	/* IN2 */ \
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) /* serial pin from controller */ \
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN ) \
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE1 ) /* service credit? */ \
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )	/* bit 0 of dsw goes here */ \
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )	/* bit 1 of dsw goes here */ \
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 ) \
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 ) \
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )


#define VS_ZAPPER \
PORT_START	/* IN0 */ \
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED )	/* sprite hit */ \
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON1 )	/* gun trigger */ \
	\
	PORT_START	/* IN1 */ \
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_UNUSED ) \
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED ) \


INPUT_PORTS_START( vsnes )
	VS_CONTROLS

	PORT_START /* DSW0 - bit 0 and 1 read from bit 3 and 4 on $4016, rest of the bits read on $4017 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( vstennis )
	PORT_START	/* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 )					/* BUTTON A on a nes */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 )					/* BUTTON B on a nes */
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_START1 )					/* SELECT on a nes */
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 )					/* START on a nes */
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )

	PORT_START	/* IN1 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )	/* BUTTON A on a nes */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )	/* BUTTON B on a nes */
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_START2 )					/* SELECT on a nes */
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER2 ) 	/* START on a nes */
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )

	PORT_START	/* IN2 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) /* serial pin from controller */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE1 ) /* service credit? */
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )	/* bit 0 of dsw goes here */
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )	/* bit 1 of dsw goes here */
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED ) /* this bit masks irqs - dont change */

	PORT_START /* DSW0 - bit 0 and 1 read from bit 3 and 4 on $4016, rest of the bits read on $4017 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )

	PORT_START	/* IN3 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER3 )	/* BUTTON A on a nes */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER3 )	/* BUTTON B on a nes */
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_START3 )					/* SELECT on a nes */
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER3 ) 	/* START on a nes */
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_PLAYER3 )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_PLAYER3 )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_PLAYER3 )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 )

	PORT_START	/* IN4 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER4 )	/* BUTTON A on a nes */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER4 )	/* BUTTON B on a nes */
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_START4 )					/* SELECT on a nes */
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER4 ) 	/* START on a nes */
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_PLAYER4 )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_PLAYER4 )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_PLAYER4 )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 )

	PORT_START	/* IN5 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) /* serial pin from controller */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE2 ) /* service credit? */
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_COIN3 )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_COIN4 )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNUSED ) /* this bit masks irqs - dont change */
INPUT_PORTS_END

INPUT_PORTS_START( drmario )
	PORT_START	/* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2)					/* BUTTON A on a nes */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )					/* BUTTON B on a nes */
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_START1 )					/* SELECT on a nes */
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 | IPF_PLAYER2)					/* START on a nes */
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_PLAYER2)
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_PLAYER2)
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_PLAYER2)
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_PLAYER2)

	PORT_START	/* IN1 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2  )	/* BUTTON A on a nes */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 )	/* BUTTON B on a nes */
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_START2 )					/* SELECT on a nes */
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3  ) 	/* START on a nes */
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP  )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN  )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT  )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT  )

	PORT_START	/* IN2 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) /* serial pin from controller */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE1 ) /* service credit? */
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )	/* bit 0 of dsw goes here */
	PORT_BIT ( 0x10, IP_ACTIVE_HIGH, IPT_UNUSED )	/* bit 1 of dsw goes here */
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START /* DSW0 - bit 0 and 1 read from bit 3 and 4 on $4016, rest of the bits read on $4017 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x0c, 0x00, "Virus Level"  )
	PORT_DIPSETTING(	0x00, "1" )
	PORT_DIPSETTING(	0x04, "3" )
	PORT_DIPSETTING(	0x08, "5" )
	PORT_DIPSETTING(	0x0c, "7" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x40, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( btlecity )
	VS_CONTROLS

	PORT_START /* DSW0 - bit 0 and 1 read from bit 3 and 4 on $4016, rest of the bits read on $4017 */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x02, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0x00, "Color Palette" )
	PORT_DIPSETTING(	0x00, "1" )
	PORT_DIPSETTING(	0x40, "2" )
	PORT_DIPSETTING(	0x80, "3" )
	PORT_DIPSETTING(	0xc0, "4" )
INPUT_PORTS_END


INPUT_PORTS_START( cluclu )
	VS_CONTROLS

	PORT_START /* DSW0 - bit 0 and 1 read from bit 3 and 4 on $4016, rest of the bits read on $4017 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x05, DEF_STR( 3C_1C) )
	PORT_DIPSETTING(	0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x06, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(	0x07, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00,  "3"  )
	PORT_DIPSETTING(	0x20,  "5"  )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( cstlevna )
	VS_CONTROLS

	PORT_START /* DSW0 - bit 0 and 1 read from bit 3 and 4 on $4016, rest of the bits read on $4017 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(	0x05, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x06, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x07, DEF_STR( Free_Play ) )

	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00, "2" )
	PORT_DIPSETTING(	0x08, "1" )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( iceclimb )
	VS_CONTROLS

	PORT_START /* DSW0 - bit 0 and 1 read from bit 3 and 4 on $4016, rest of the bits read on $4017 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x06, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(	0x07, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x18, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(	0x00,  "2")
	PORT_DIPSETTING(	0x10,  "3")
	PORT_DIPSETTING(	0x08,  "4")
	PORT_DIPSETTING(	0x18,  "6")
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( excitebk )
	VS_CONTROLS

	PORT_START /* DSW0 - bit 0 and 1 read from bit 3 and 4 on $4016, rest of the bits read on $4017 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x06, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(	0x07, DEF_STR( Free_Play ) )

	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( ladygolf )
	VS_CONTROLS

	PORT_START /* DSW0 - bit 0 and 1 read from bit 3 and 4 on $4016, rest of the bits read on $4017 */
	PORT_DIPNAME( 0x07, 0x01, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x07, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x05, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( machridr )
	VS_CONTROLS

	PORT_START /* DSW0 - bit 0 and 1 read from bit 3 and 4 on $4016, rest of the bits read on $4017 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(	0x05, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x06, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(	0x07, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x18, 0x00, "Timer" )
	PORT_DIPSETTING(	0x00, "280" )
	PORT_DIPSETTING(	0x10, "250" )
	PORT_DIPSETTING(	0x08, "220" )
	PORT_DIPSETTING(	0x18, "200" )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )
INPUT_PORTS_END


INPUT_PORTS_START( suprmrio )
	VS_CONTROLS

	PORT_START /* DSW0 - bit 0 and 1 read from bit 3 and 4 on $4016, rest of the bits read on $4017 */
	PORT_DIPNAME( 0x07, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Lives) )
	PORT_DIPSETTING(	0x00, "3"	)
	PORT_DIPSETTING(	0x08, "2"	)
	PORT_DIPNAME(0x30, 0x00, DEF_STR(Bonus_Life) )
	PORT_DIPSETTING(	0x00, "100" )
	PORT_DIPSETTING(	0x20, "150" )
	PORT_DIPSETTING(	0x10, "200" )
	PORT_DIPSETTING(	0x30, "250" )
	PORT_DIPNAME(0x40, 0x00, "Timer")
	PORT_DIPSETTING(	0x00, "Slow")
	PORT_DIPSETTING(	0x40, "FAST")
	PORT_DIPNAME(0x80, 0x00, "Continue Lives" )
	PORT_DIPSETTING(	0x00, "4" )
	PORT_DIPSETTING(	0x80, "3" )
INPUT_PORTS_END


INPUT_PORTS_START( duckhunt )
	VS_ZAPPER

	PORT_START	/* IN2 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) /* serial pin from controller */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE1 ) /* service credit? */
	PORT_DIPNAME( 0x08, 0x00, "uh" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "uh2" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START /* IN3 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) /* serial pin from controller */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x40, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x80, DEF_STR( On ) )

	PORT_START	/* IN4 - FAKE - Gun X pos */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X, 70, 30, 0, 255 )

	PORT_START	/* IN5 - FAKE - Gun Y pos */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y, 50, 30, 0, 255 )

INPUT_PORTS_END


INPUT_PORTS_START( hogalley )
	VS_ZAPPER

	PORT_START	/* IN2 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) /* serial pin from controller */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE1 ) /* service credit? */
	PORT_DIPNAME( 0x08, 0x00, "uh" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "uh2" )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x10, DEF_STR( On ) )
	PORT_BIT ( 0x20, IP_ACTIVE_HIGH, IPT_COIN1 )
	PORT_BIT ( 0x40, IP_ACTIVE_HIGH, IPT_COIN2 )
	PORT_BIT ( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN )

	PORT_START /* IN3 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED ) /* serial pin from controller */
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(	0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(	0x04, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(	0x00, "easiest" )
	PORT_DIPSETTING(	0x08, "easy" )
	PORT_DIPSETTING(	0x00, "hard" )
	PORT_DIPSETTING(	0x10, "hardest")
	PORT_DIPNAME( 0x20, 0x00, "Misses per game" )
	PORT_DIPSETTING(	0x00, "3" )
	PORT_DIPSETTING(	0x20, "5" )
	PORT_DIPNAME( 0xc0, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(	0x00, "30000 " )
	PORT_DIPSETTING(	0x40, "50000 " )
	PORT_DIPSETTING(	0x80, "80000 " )
	PORT_DIPSETTING(	0xc0, "10000 " )

	PORT_START	/* IN4 - FAKE - Gun X pos */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_X, 70, 30, 0, 255 )

	PORT_START	/* IN5 - FAKE - Gun Y pos */
	PORT_ANALOG( 0xff, 0x80, IPT_AD_STICK_Y, 50, 30, 0, 255 )

INPUT_PORTS_END


static struct GfxDecodeInfo nes_gfxdecodeinfo[] =
{
	/* none, the ppu generates one */
	{ -1 } /* end of array */
};

static struct NESinterface nes_interface =
{
	1,
	{ REGION_CPU1 },
	{ 50 },
};

static struct DACinterface nes_dac_interface =
{
	1,
	{ 50 },
};

static struct NESinterface nes_dual_interface =
{
	2,
	{ REGION_CPU1, REGION_CPU2 },
	{ 25, 25 },
};

static struct DACinterface nes_dual_dac_interface =
{
	2,
	{ 25, 25 },
};

static struct MachineDriver machine_driver_vsnes =
{
	/* basic machine hardware */
	{
		{
			CPU_N2A03,
			N2A03_DEFAULTCLOCK,
			readmem,writemem,0,0,
			ignore_interrupt, 0	/* NMIs are triggered by the PPU */
								/* some carts also trigger IRQs */
		}
	},
	60, ( ( ( 1.0 / 60.0 ) * 1000000.0 ) / 262 ) * ( 262 - 239 ), /* frames per second, vblank duration */
	1,
	vsnes_init_machine,

	/* video hardware */
	32*8, 30*8,	{ 0*8, 32*8-1, 0*8, 30*8-1 },
	nes_gfxdecodeinfo,
	4*16, 4*8,
	vsnes_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	vsnes_vh_start,
	vsnes_vh_stop,
	vsnes_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_NES,
			&nes_interface
		},
		{
			SOUND_DAC,
			&nes_dac_interface
		}
	}
};

static struct MachineDriver machine_driver_vsdual =
{
	/* basic machine hardware */
	{
		{
			CPU_N2A03,
			N2A03_DEFAULTCLOCK,
			readmem,writemem,0,0,
			ignore_interrupt, 0	/* NMIs are triggered by the PPU */
								/* some carts also trigger IRQs */
		},
		{
			CPU_N2A03,
			N2A03_DEFAULTCLOCK,
			readmem_1,writemem_1,0,0,
			ignore_interrupt, 0	/* NMIs are triggered by the PPU */
								/* some carts also trigger IRQs */
		}
	},
	60, ( ( ( 1.0 / 60.0 ) * 1000000.0 ) / 262 ) * ( 262 - 239 ), /* frames per second, vblank duration */
	1,
	vsdual_init_machine,

	/* video hardware */
	32*8*2, 30*8, { 0*8, 32*8*2-1, 0*8, 30*8-1 },
	nes_gfxdecodeinfo,
	2*4*16, 2*4*8,
	vsdual_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_DUAL_MONITOR,
	0,
	vsdual_vh_start,
	vsnes_vh_stop,
	vsdual_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_NES,
			&nes_dual_interface
		},
		{
			SOUND_DAC,
			&nes_dual_dac_interface
		}
	}
};


/******************************************************************************/

/* RP2C04_0004 games */
ROM_START( suprmrio)
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "1d",  0x8000, 0x2000, 0xbe4d5436 )
	ROM_LOAD( "1c",  0xa000, 0x2000, 0x0011fc5a )
	ROM_LOAD( "1b",  0xc000, 0x2000, 0xb1b87893 )
	ROM_LOAD( "1a",  0xe000, 0x2000, 0x1abf053c )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "2b",  0x0000, 0x2000, 0x42418d40 )
	ROM_LOAD( "2a",  0x2000, 0x2000, 0x15506b86 )
ROM_END

ROM_START( iceclimb )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "ic-1d",  0x8000, 0x2000, 0x65e21765 )
	ROM_LOAD( "ic-1c",  0xa000, 0x2000, 0xa7909c51 )
	ROM_LOAD( "ic-1b",  0xc000, 0x2000, 0x7fb3cc21 )
	ROM_LOAD( "ic-1a",  0xe000, 0x2000, 0xbf196bf7 )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "ic-2b",  0x0000, 0x2000, 0x331460b4 )
	ROM_LOAD( "ic-2a",  0x2000, 0x2000, 0x4ec44fb3 )
ROM_END

/* Gun games */
ROM_START( duckhunt )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "1d",  0x8000, 0x2000, 0x3f51f0ed )
	ROM_LOAD( "1c",  0xa000, 0x2000, 0x8bc7376c )
	ROM_LOAD( "1b",  0xc000, 0x2000, 0xa042b6e1 )
	ROM_LOAD( "1a",  0xe000, 0x2000, 0x1906e3ab )

	ROM_REGION( 0x4000, REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "2b",  0x0000, 0x2000, 0x0c52ec28 )
	ROM_LOAD( "2a",  0x2000, 0x2000, 0x3d238df3 )
ROM_END

ROM_START( hogalley)
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "1d",  0x8000, 0x2000, 0x2089e166 )
	ROM_LOAD( "1c",  0xa000, 0x2000, 0xa85934ae )
	ROM_LOAD( "1b",  0xc000, 0x2000, 0x718e25b3 )
	ROM_LOAD( "1a",  0xe000, 0x2000, 0xf9526852 )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "2b",  0x0000, 0x2000, 0x7623e954 )
	ROM_LOAD( "2a",  0x2000, 0x2000, 0x78c842b6 )
ROM_END

ROM_START( goonies )
	ROM_REGION( 0x20000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "prg.u7",  0x10000, 0x10000, 0x1e438d52 )

	ROM_REGION( 0x10000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "chr.u4",  0x0000, 0x10000, 0x4c4b61b0 )
ROM_END

ROM_START( vsgradus )
	ROM_REGION( 0x20000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "prg.u7",  0x10000, 0x10000, 0xd99a2087 )

	ROM_REGION( 0x10000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "chr.u4",  0x0000, 0x10000, 0x23cf2fc3 )
ROM_END

ROM_START( btlecity )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "bc.1d",  0x8000, 0x2000, 0x6aa87037 )
	ROM_LOAD( "bc.1c",  0xa000, 0x2000, 0xbdb317db )
	ROM_LOAD( "bc.1b",  0xc000, 0x2000, 0x1a0088b8 )
	ROM_LOAD( "bc.1a",  0xe000, 0x2000, 0x86307c89 )

	ROM_REGION( 0x4000, REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "bc.2b",  0x0000, 0x2000, 0x634f68bd )
	ROM_LOAD( "bc.2a",  0x2000, 0x2000, 0xa9b49a05 )
ROM_END

ROM_START( cluclu )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "cl.6d",  0x8000, 0x2000, 0x1e9f97c9 )
	ROM_LOAD( "cl.6c",  0xa000, 0x2000, 0xe8b843a7 )
	ROM_LOAD( "cl.6b",  0xc000, 0x2000, 0x418ee9ea )
	ROM_LOAD( "cl.6a",  0xe000, 0x2000, 0x5e8a8457 )

	ROM_REGION( 0x4000, REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "cl.8b",  0x0000, 0x2000, 0x960d9a6c )
	ROM_LOAD( "cl.8a",  0x2000, 0x2000, 0xe3139791 )
ROM_END

ROM_START( excitebk )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "eb-1d",  0x8000, 0x2000, 0x7e54df1d )
	ROM_LOAD( "eb-1c",  0xa000, 0x2000, 0x89baae91 )
	ROM_LOAD( "eb-1b",  0xc000, 0x2000, 0x4c0c2098 )
	ROM_LOAD( "eb-1a",  0xe000, 0x2000, 0xb9ab7110 )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "eb-2b",  0x0000, 0x2000, 0x80be1f50 )
	ROM_LOAD( "eb-2a",  0x2000, 0x2000, 0xa9b49a05 )
ROM_END

ROM_START( ladygolf)
	ROM_REGION( 0x10000,REGION_CPU1  ) /* 6502 memory */
	ROM_LOAD( "lg-1d",  0x8000, 0x2000, 0x8b2ab436 )
	ROM_LOAD( "lg-1c",  0xa000, 0x2000, 0xbda6b432 )
	ROM_LOAD( "lg-1b",  0xc000, 0x2000, 0xdcdd8220 )
	ROM_LOAD( "lg-1a",  0xe000, 0x2000, 0x26a3cb3b )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "lg-2b",  0x0000, 0x2000, 0x95618947 )
	ROM_LOAD( "lg-2a",  0x2000, 0x2000, 0xd07407b1 )
ROM_END

ROM_START( machridr )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "mr-1d",  0x8000, 0x2000, 0x379c44b9 )
	ROM_LOAD( "mr-1c",  0xa000, 0x2000, 0xcb864802 )
	ROM_LOAD( "mr-1b",  0xc000, 0x2000, 0x5547261f )
	ROM_LOAD( "mr-1a",  0xe000, 0x2000, 0xe3e3900d )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "mr-2b",  0x0000, 0x2000, 0x33a2b41a )
	ROM_LOAD( "mr-2a",  0x2000, 0x2000, 0x685899d8 )
ROM_END

ROM_START(vsgolf)
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "golf-1d",  0x8000, 0x2000, 0xa3e286d3 )
	ROM_LOAD( "golf-1c",  0xa000, 0x2000, 0xe477e48b )
	ROM_LOAD( "golf-1b",  0xc000, 0x2000, 0x7d80b511 )
	ROM_LOAD( "golf-1a",  0xe000, 0x2000, 0x7b767da6 )

	ROM_REGION( 0x4000, REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "golf-2b",  0x0000, 0x2000, 0x2782a3e5 )
	ROM_LOAD( "golf-2a",  0x2000, 0x2000, 0x6e93fdef )
ROM_END

ROM_START( vspinbal )
	ROM_REGION( 0x10000, REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "pb-6d",  0x8000, 0x2000, 0x69fc575e )
	ROM_LOAD( "pb-6c",  0xa000, 0x2000, 0xfa9472d2 )
	ROM_LOAD( "pb-6b",  0xc000, 0x2000, 0xf57d89c5 )
	ROM_LOAD( "pb-6a",  0xe000, 0x2000, 0x640c4741 )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "pb-8b",  0x0000, 0x2000, 0x8822ee9e )
	ROM_LOAD( "pb-8a",  0x2000, 0x2000, 0xcbe98a28 )
ROM_END


ROM_START( vsslalom )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "slalom.1d",  0x8000, 0x2000, 0x6240a07d )
	ROM_LOAD( "slalom.1c",  0xa000, 0x2000, 0x27c355e4 )
	ROM_LOAD( "slalom.1b",  0xc000, 0x2000, 0xd4825fbf )
	ROM_LOAD( "slalom.1a",  0xe000, 0x2000, 0x82333f80 )

	ROM_REGION( 0x2000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "slalom.2a",  0x0000, 0x2000, 0x977bb126 )

ROM_END

ROM_START( vssoccer )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "soccer1d",  0x8000, 0x2000, 0x0ac52145 )
	ROM_LOAD( "soccer1c",  0xa000, 0x2000, 0xf132e794 )
	ROM_LOAD( "soccer1b",  0xc000, 0x2000, 0x26bb7325 )
	ROM_LOAD( "soccer1a",  0xe000, 0x2000, 0xe731635a )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "soccer2b",  0x0000, 0x2000, 0x307b19ab )
	ROM_LOAD( "soccer2a",  0x2000, 0x2000, 0x7263613a )
ROM_END




ROM_START( drmario )
	ROM_REGION( 0x20000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "dm-uiprg",  0x10000, 0x10000, 0xd5d7eac4 )

	ROM_REGION( 0x8000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "dm-u3chr",  0x0000, 0x8000, 0x91871aa5 )
ROM_END

ROM_START( cstlevna )
	ROM_REGION( 0x30000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "mds-cv.prg",  0x10000, 0x20000, 0xffbef374 )

	/* No cart gfx - uses vram */
ROM_END


ROM_START( tkoboxng )
	ROM_REGION( 0x20000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "tkoprg.bin",  0x10000, 0x10000, 0x1e438d52 )

	ROM_REGION( 0x10000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "tkochr.bin",  0x0000, 0x10000, 0x4c4b61b0 )
ROM_END

/* not working yet */

ROM_START( topgun )
	ROM_REGION( 0x30000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "rc-003",  0x10000, 0x20000, 0xffbef374 )

	/* No cart gfx - uses vram */
ROM_END

ROM_START( rbibb )
	ROM_REGION( 0x20000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "rbi-prg",  0x10000, 0x10000, 0xd5d7eac4 )

	ROM_REGION( 0x8000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "rbi-cha",  0x0000, 0x8000, 0x91871aa5 )
ROM_END

ROM_START( vsskykid )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "sk-prg1",  0x08000, 0x08000, 0x1 )

	ROM_REGION( 0x8000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "sk-cha",  0x0000, 0x8000, 0x1 )
ROM_END

ROM_START( platoon )
	ROM_REGION( 0x30000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "prgver0.ic4",  0x10000, 0x20000, 0x1e438d52 )

	ROM_REGION( 0x20000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "chrver0.ic6",  0x0000, 0x20000, 0x4c4b61b0 )
ROM_END


/* Dual System */

ROM_START( vstennis )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "vst-1d",  0x08000, 0x02000, 0x1 )
	ROM_LOAD( "vst-1c",  0x0a000, 0x02000, 0x1 )
	ROM_LOAD( "vst-1b",  0x0c000, 0x02000, 0x1 )
	ROM_LOAD( "vst-1a",  0x0e000, 0x02000, 0x1 )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "vst-2b",  0x0000, 0x2000, 0x1 )
	ROM_LOAD( "vst-2a",  0x0000, 0x2000, 0x1 )

	ROM_REGION( 0x10000,REGION_CPU2 ) /* 6502 memory */
	ROM_LOAD( "vst-6d",  0x08000, 0x02000, 0x1 )
	ROM_LOAD( "vst-6c",  0x0a000, 0x02000, 0x1 )
	ROM_LOAD( "vst-6b",  0x0c000, 0x02000, 0x1 )
	ROM_LOAD( "vst-6a",  0x0e000, 0x02000, 0x1 )

	ROM_REGION( 0x4000,REGION_GFX2 ) /* PPU memory */
	ROM_LOAD( "vst-8b",  0x0000, 0x2000, 0x1 )
	ROM_LOAD( "vst-8a",  0x0000, 0x2000, 0x1 )
ROM_END


ROM_START( wrecking )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "wr.1d",  0x08000, 0x02000, 0x1 )
	ROM_LOAD( "wr.1c",  0x0a000, 0x02000, 0x1 )
	ROM_LOAD( "wr.1b",  0x0c000, 0x02000, 0x1 )
	ROM_LOAD( "wr.1a",  0x0e000, 0x02000, 0x1 )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "wr.2b",  0x0000, 0x2000, 0x1 )
	ROM_LOAD( "wr.2a",  0x0000, 0x2000, 0x1 )

	ROM_REGION( 0x10000,REGION_CPU2 ) /* 6502 memory */
	ROM_LOAD( "wr.6d",  0x08000, 0x02000, 0x1 )
	ROM_LOAD( "wr.6c",  0x0a000, 0x02000, 0x1 )
	ROM_LOAD( "wr.6b",  0x0c000, 0x02000, 0x1 )
	ROM_LOAD( "wr.6a",  0x0e000, 0x02000, 0x1 )

	ROM_REGION( 0x4000,REGION_GFX2 ) /* PPU memory */
	ROM_LOAD( "wr.8b",  0x0000, 0x2000, 0x1 )
	ROM_LOAD( "wr.8a",  0x0000, 0x2000, 0x1 )
ROM_END


ROM_START( balonfgt )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "bf.1d",  0x08000, 0x02000, 0x1 )
	ROM_LOAD( "bf.1c",  0x0a000, 0x02000, 0x1 )
	ROM_LOAD( "bf.1b",  0x0c000, 0x02000, 0x1 )
	ROM_LOAD( "bf.1a",  0x0e000, 0x02000, 0x1 )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "bf.2b",  0x0000, 0x2000, 0x1 )
	ROM_LOAD( "bf.2a",  0x0000, 0x2000, 0x1 )

	ROM_REGION( 0x10000,REGION_CPU2 ) /* 6502 memory */
	ROM_LOAD( "bf.6d",  0x08000, 0x02000, 0x1 )
	ROM_LOAD( "bf.6c",  0x0a000, 0x02000, 0x1 )
	ROM_LOAD( "bf.6b",  0x0c000, 0x02000, 0x1 )
	ROM_LOAD( "bf.6a",  0x0e000, 0x02000, 0x1 )

	ROM_REGION( 0x4000,REGION_GFX2 ) /* PPU memory */
	ROM_LOAD( "bf.8b",  0x0000, 0x2000, 0x1 )
	ROM_LOAD( "bf.8a",  0x0000, 0x2000, 0x1 )
ROM_END


ROM_START( mahjong )
	ROM_REGION( 0x10000,REGION_CPU1 ) /* 6502 memory */
	ROM_LOAD( "mj.1c",  0x08000, 0x02000, 0x1 )
	ROM_LOAD( "mj.1b",  0x0a000, 0x02000, 0x1 )
	ROM_LOAD( "mj.1a",  0x0c000, 0x02000, 0x1 )
	//ROM_LOAD( "wr.1a",  0x0e000, 0x02000, 0x1 )

	ROM_REGION( 0x4000,REGION_GFX1 ) /* PPU memory */
	ROM_LOAD( "mj.2b",  0x0000, 0x2000, 0x1 )
	//ROM_LOAD( "wr.2a",  0x0000, 0x2000, 0x1 )

	ROM_REGION( 0x10000,REGION_CPU2 ) /* 6502 memory */
	ROM_LOAD( "mj.6c",  0x08000, 0x02000, 0x1 )
	ROM_LOAD( "mj.6b",  0x0a000, 0x02000, 0x1 )
	ROM_LOAD( "mj.6a",  0x0c000, 0x02000, 0x1 )
	//ROM_LOAD( "wr.6a",  0x0e000, 0x02000, 0x1 )

	ROM_REGION( 0x4000,REGION_GFX2 ) /* PPU memory */
	ROM_LOAD( "mj.8b",  0x0000, 0x2000, 0x1 )
	//ROM_LOAD( "wr.8a",  0x0000, 0x2000, 0x1 )
ROM_END



/******************************************************************************/

/*    YEAR  NAME      PARENT  MACHINE  INPUT     INIT  	   MONITOR  */
GAME( 1985, btlecity, 0,	  vsnes,   btlecity, vsnormal, ROT0, "Namco",     "Battle City" )
GAME( 1987,	cstlevna, 0,	  vsnes,   cstlevna, cstlevna, ROT0, "Konami",    "Vs Castlevania" )
GAME( 1984, cluclu,   0,	  vsnes,   cluclu,   suprmrio, ROT0, "Nintendo",  "Clu Clu Land" )
GAME( 1990,	drmario,  0,	  vsnes,   drmario,  drmario,  ROT0, "Nintendo",  "Dr Mario" )
GAME( 1985, duckhunt, 0,      vsnes,   duckhunt, duckhunt, ROT0, "Nintendo",  "Duck Hunt" )
GAME( 1984, excitebk, 0,	  vsnes,   excitebk, excitebk, ROT0, "Nintendo",  "Excitebike")
GAME( 1985, hogalley, 0,      vsnes,   hogalley, hogalley, ROT0, "Nintendo",  "Hogan's Alley" )
GAME( 1984, iceclimb, 0,      vsnes,   iceclimb, suprmrio, ROT0, "Nintendo",  "Ice Climber" )
GAME( 1984, ladygolf, 0,      vsnes,   ladygolf, machridr, ROT0, "Nintendo",  "Lady Golf" )
GAME( 1985, machridr, 0,      vsnes,   machridr, machridr, ROT0, "Nintendo",  "Mach Rider" )
GAME( 1986, suprmrio, 0,      vsnes,   suprmrio, suprmrio, ROT0, "Nintendo",  "VS Super Mario Bros" )
GAME( 1984, vsgolf,   0,      vsnes,   vsnes,    machridr, ROT0, "Nintendo",  "VS Golf" )
GAME( 1984, vspinbal, 0,      vsnes,   vsnes,    vspinbal, ROT0, "Nintendo",  "Pinball" )
GAME( 1986, vsslalom, 0,      vsnes,   vsnes,    vsslalom, ROT0, "Nintendo",  "VS Slalom" )
GAME( 1985, vssoccer, 0,      vsnes,   vsnes,    excitebk, ROT0, "Nintendo",  "Unisystem Soccer" )
GAME( 1986,	goonies,  0,	  vsnes,   vsnes,    goonies,  ROT0, "Konami",	  "VS The Goonies" )
GAME( 1987, vsgradus, 0,      vsnes,   vsnes,    vsgradus, ROT0, "Nintendo",  "VS Gradius" )
GAME( 1987, tkoboxng, 0,      vsnes,   vsnes,    tkoboxng, ROT0, "Data East", "VS TKO Boxing" )

/* are these using the correct mappers? */
GAME( 1987, rbibb,    0,	  vsnes,   vsnes,    rbibb,    ROT0, "Nintendo",  "RBI Baseball" )
GAME( ????,	topgun,   0,	  vsnes,   vsnes,    vstopgun, ROT0, "Nintendo",  "VS Topgun" )
GAME( 1986,	vsskykid, 0,	  vsnes,   vsnes,    vsskykid, ROT0, "Namco",     "VS SkyKid" )

GAME( 1984, vstennis, 0,      vsdual,  vstennis, vstennis, ROT0, "Nintendo",  "VS Tennis" )
GAME( ????, wrecking, 0,      vsdual,  vstennis, vstennis, ROT0, "Nintendo",  "wrecking" )
GAME( ????, mahjong, 0,      vsdual,  vstennis, vstennis, ROT0, "Nintendo",  "mahjong" )
GAME( ????, balonfgt, 0,      vsdual,  vstennis, vstennis, ROT0, "Nintendo",  "balonfgt" )

GAME( ????, platoon,  0,     vsnes,   vsnes, platoon,  ROT0, "??????", 	"platoon?" )


