/******************************************************************************

	Game Driver for Nichibutsu Mahjong series.

	Mahjong Uranai Densetsu
	(c)1992 Nihon Bussan Co.,Ltd. / (c)1992 Yubis Co.,Ltd.

	Mahjong Koi no Magic Potion
	(c)1992 Nihon Bussan Co.,Ltd.

	Mahjong Pachinko Monogatari
	(c)1992 Nihon Bussan Co.,Ltd.

	Medal Mahjong Janjan Baribari
	(c)1992 Nihon Bussan Co.,Ltd. / (c)1992 Yubis Co.,Ltd. / (c)1992 AV JAPAN Co.,Ltd.

	Mahjong Bakuhatsu Junjouden
	(c)1991 Nihon Bussan Co.,Ltd.

	Ultra Maru-hi Mahjong
	(c)1993 Apple

	Mahjong Gal 10-renpatsu
	(c)1993 FUJIC Co.,Ltd.

	Mahjong Ren-ai Club
	(c)1993 FUJIC Co.,Ltd.

	Mahjong La Man
	(c)1993 Nihon Bussan Co.,Ltd. / (c)1993 AV JAPAN Co.,Ltd.

	Mahjong Keibaou
	(c)1993 Nihon Bussan Co.,Ltd.

	Medal Mahjong Pachi-Slot Tengoku (Medal Type)
	(c)1993 Nihon Bussan Co.,Ltd. / (c)1993 MIKI SYOUJI Co.,Ltd. / (c)1993 AV JAPAN Co.,Ltd.

	Mahjong Sailor Wars
	(c)1993 Nihon Bussan Co.,Ltd.

	Mahjong Sailor Wars-R (Medal type)
	(c)1993 Nihon Bussan Co.,Ltd.

	Bishoujo Janshi Pretty Sailor 18-kin
	(c)1994 SPHINX Co.,Ltd.

	Bishoujo Janshi Pretty Sailor 2
	(c)1994 SPHINX Co.,Ltd.

	Disco Mahjong Otachidai no Okite
	(c)1995 SPHINX Co.,Ltd.

	Nekketsu Grand-Prix Gal
	(c)1991 Nihon Bussan Co.,Ltd.

	Mahjong Gottsu ee-kanji
	(c)1991 Nihon Bussan Co.,Ltd.

	Mahjong Circuit no Mehyou
	(c)1992 Nihon Bussan Co.,Ltd. / (c)1992 Kawakusu Co.,Ltd.

	Medal Mahjong Circuit no Mehyou (Medal type)
	(c)1992 Nihon Bussan Co.,Ltd. / (c)1992 Kawakusu Co.,Ltd.

	Mahjong Koi Uranai
	(c)1992 Nihon Bussan Co.,Ltd.

	Mahjong Scout Man
	(c)1994 SPHINX Co.,Ltd. / (c)1994 AV JAPAN Co.,Ltd.

	Imekura Mahjong
	(c)1994 SPHINX Co.,Ltd. / (c)1994 AV JAPAN Co.,Ltd.

	Mahjong Erotica Golf
	(c)1994 FUJIC Co.,Ltd. / (c)1994 AV JAPAN Co.,Ltd.

	Driver by Takahiro Nogi <nogi@kt.rim.or.jp> 2000/03/13 -
	Special thanks to Tatsuyuki Satoh

******************************************************************************/
/******************************************************************************
Memo:

- Screen position sometimes be strange while frame skip != 0.

- In attract mode of otatidai, scroll position is strange after white fade.
  (problems in nb19010 busy flag emulation or main cpu clock?).

- Some games display "GFXROM BANK OVER!!" or "GFXROM ADDRESS OVER!!"
  in Debug build.

- Screen flip is not perfect.

******************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "machine/z80fmly.h"
#include "vidhrdw/generic.h"


#define	SIGNED_DAC	0		// 0:unsigned DAC, 1:signed DAC


VIDEO_UPDATE( sailorws );
VIDEO_START( sailorws );
VIDEO_START( mjkoiura );
VIDEO_UPDATE( mscoutm );
VIDEO_START( mscoutm );

READ_HANDLER( sailorws_palette_r );
WRITE_HANDLER( sailorws_palette_w );
READ_HANDLER( mscoutm_palette_r );
WRITE_HANDLER( mscoutm_palette_w );

WRITE_HANDLER( sailorws_gfxflag_0_w );
WRITE_HANDLER( sailorws_scrollx_0_w );
WRITE_HANDLER( sailorws_scrolly_0_w );
WRITE_HANDLER( sailorws_radr_0_w );
WRITE_HANDLER( sailorws_sizex_0_w );
WRITE_HANDLER( sailorws_sizey_0_w );
WRITE_HANDLER( sailorws_drawx_0_w );
WRITE_HANDLER( sailorws_drawy_0_w );

WRITE_HANDLER( sailorws_gfxflag_1_w );
WRITE_HANDLER( sailorws_scrollx_1_w );
WRITE_HANDLER( sailorws_scrolly_1_w );
WRITE_HANDLER( sailorws_radr_1_w );
WRITE_HANDLER( sailorws_sizex_1_w );
WRITE_HANDLER( sailorws_sizey_1_w );
WRITE_HANDLER( sailorws_drawx_1_w );
WRITE_HANDLER( sailorws_drawy_1_w );

void sailorws_gfxflag2_w(int data);
void sailorws_paltblnum_w(int data);
WRITE_HANDLER( sailorws_paltbl_0_w );
WRITE_HANDLER( sailorws_paltbl_1_w );
READ_HANDLER( sailorws_gfxbusy_0_r );
READ_HANDLER( sailorws_gfxbusy_1_r );
READ_HANDLER( sailorws_gfxrom_0_r );
READ_HANDLER( sailorws_gfxrom_1_r );


static int sailorws_inputport;
static int sailorws_dipswbitsel;
static int sailorws_outcoin_flag;
static int mscoutm_inputport;


static unsigned char *sailorws_nvram;
static size_t sailorws_nvram_size;


static NVRAM_HANDLER( sailorws )
{
	if (read_or_write)
		mame_fwrite(file, sailorws_nvram, sailorws_nvram_size);
	else
	{
		if (file)
			mame_fread(file, sailorws_nvram, sailorws_nvram_size);
		else
			memset(sailorws_nvram, 0, sailorws_nvram_size);
	}
}

static void sailorws_soundbank_w(int data)
{
	unsigned char *SNDROM = memory_region(REGION_CPU2);

	cpu_setbank(1, &SNDROM[0x08000 + (0x8000 * (data & 0x03))]);
}

static int sailorws_sound_r(int offset)
{
	return soundlatch_r(0);
}

static WRITE_HANDLER( sailorws_sound_w )
{
	soundlatch_w(0, data);
}

static void sailorws_soundclr_w(int offset, int data)
{
	soundlatch_clear_w(0, 0);
}

static void sailorws_outcoin_flag_w(int data)
{
	// bit0: coin in counter
	// bit1: coin out counter
	// bit2: hopper
	// bit3: coin lockout

	if (data & 0x04) sailorws_outcoin_flag ^= 1;
	else sailorws_outcoin_flag = 1;
}

static WRITE_HANDLER( sailorws_inputportsel_w )
{
	sailorws_inputport = (data ^ 0xff);
}

static int sailorws_dipsw_r(void)
{
	return ((((readinputport(0) & 0xff) | ((readinputport(1) & 0xff) << 8)) >> sailorws_dipswbitsel) & 0x01);
}

static void sailorws_dipswbitsel_w(int data)
{
	switch (data & 0xc0)
	{
		case	0x00:
			sailorws_dipswbitsel = 0;
			break;
		case	0x40:
			break;
		case	0x80:
			break;
		case	0xc0:
			sailorws_dipswbitsel = ((sailorws_dipswbitsel + 1) & 0x0f);
			break;
		default:
			break;
	}
}

static void mscoutm_inputportsel_w(int data)
{
	mscoutm_inputport = (data ^ 0xff);
}

static READ_HANDLER( mscoutm_dipsw_0_r )
{
	// DIPSW A
	return (((readinputport(0) & 0x01) << 7) | ((readinputport(0) & 0x02) << 5) |
	        ((readinputport(0) & 0x04) << 3) | ((readinputport(0) & 0x08) << 1) |
	        ((readinputport(0) & 0x10) >> 1) | ((readinputport(0) & 0x20) >> 3) |
	        ((readinputport(0) & 0x40) >> 5) | ((readinputport(0) & 0x80) >> 7));
}

static READ_HANDLER( mscoutm_dipsw_1_r )
{
	// DIPSW B
	return (((readinputport(1) & 0x01) << 7) | ((readinputport(1) & 0x02) << 5) |
	        ((readinputport(1) & 0x04) << 3) | ((readinputport(1) & 0x08) << 1) |
	        ((readinputport(1) & 0x10) >> 1) | ((readinputport(1) & 0x20) >> 3) |
	        ((readinputport(1) & 0x40) >> 5) | ((readinputport(1) & 0x80) >> 7));
}


/* TMPZ84C011 PIO emulation */

static unsigned char pio_dir[5*2], pio_latch[5*2];

static int tmpz84c011_pio_r(int offset)
{
	int portdata;

	if ((!strcmp(Machine->gamedrv->name, "mscoutm")) ||
	    (!strcmp(Machine->gamedrv->name, "imekura")) ||
	    (!strcmp(Machine->gamedrv->name, "mjegolf")))
	{
		switch (offset)
		{
			case	0:			/* PA_0 */
				// COIN IN, ETC...
				portdata = readinputport(2);
				break;
			case	1:			/* PB_0 */
				// PLAYER1 KEY, DIPSW A/B
				switch (mscoutm_inputport)
				{
					case	0x01:
						portdata = readinputport(3);
						break;
					case	0x02:
						portdata = readinputport(4);
						break;
					case	0x04:
						portdata = readinputport(5);
						break;
					case	0x08:
						portdata = readinputport(6);
						break;
					case	0x10:
						portdata = readinputport(7);
						break;
					default:
						portdata = 0xff;
						break;
				}
				break;
			case	2:			/* PC_0 */
				// PLAYER2 KEY
				portdata = 0xff;
				break;
			case	3:			/* PD_0 */
				portdata = 0xff;
				break;
			case	4:			/* PE_0 */
				portdata = 0xff;
				break;

			case	5:			/* PA_1 */
				portdata = 0xff;
				break;
			case	6:			/* PB_1 */
				portdata = 0xff;
				break;
			case	7:			/* PC_1 */
				portdata = 0xff;
				break;
			case	8:			/* PD_1 */
				portdata = sailorws_sound_r(0);
				break;
			case	9:			/* PE_1 */
				portdata = 0xff;
				break;

			default:
				logerror("PC %04X: TMPZ84C011_PIO Unknown Port Read %02X\n", activecpu_get_pc(), offset);
				portdata = 0xff;
				break;
		}
	}
	else
	{
		switch (offset)
		{
			case	0:			/* PA_0 */
				// COIN IN, ETC...
				portdata = ((readinputport(2) & 0xfe) | sailorws_outcoin_flag);
				break;
			case	1:			/* PB_0 */
				// PLAYER1 KEY, DIPSW A/B
				switch (sailorws_inputport)
				{
					case	0x01:
						portdata = readinputport(3);
						break;
					case	0x02:
						portdata = readinputport(4);
						break;
					case	0x04:
						portdata = readinputport(5);
						break;
					case	0x08:
						portdata = readinputport(6);
						break;
					case	0x10:
						portdata = ((readinputport(7) & 0x7f) | (sailorws_dipsw_r() << 7));
						break;
					default:
						portdata = 0xff;
						break;
				}
				break;
			case	2:			/* PC_0 */
				// PLAYER2 KEY
				portdata = 0xff;
				break;
			case	3:			/* PD_0 */
				portdata = 0xff;
				break;
			case	4:			/* PE_0 */
				portdata = 0xff;
				break;

			case	5:			/* PA_1 */
				portdata = 0xff;
				break;
			case	6:			/* PB_1 */
				portdata = 0xff;
				break;
			case	7:			/* PC_1 */
				portdata = 0xff;
				break;
			case	8:			/* PD_1 */
				portdata = sailorws_sound_r(0);
				break;
			case	9:			/* PE_1 */
				portdata = 0xff;
				break;

			default:
				logerror("PC %04X: TMPZ84C011_PIO Unknown Port Read %02X\n", activecpu_get_pc(), offset);
				portdata = 0xff;
				break;
		}
	}

	return portdata;
}

static void tmpz84c011_pio_w(int offset, int data)
{
	if ((!strcmp(Machine->gamedrv->name, "imekura")) ||
	    (!strcmp(Machine->gamedrv->name, "mscoutm")) ||
	    (!strcmp(Machine->gamedrv->name, "mjegolf")))
	{
		switch (offset)
		{
			case	0:			/* PA_0 */
				mscoutm_inputportsel_w(data);	// NB22090
				break;
			case	1:			/* PB_0 */
				break;
			case	2:			/* PC_0 */
				break;
			case	3:			/* PD_0 */
				sailorws_paltblnum_w(data);
				break;
			case	4:			/* PE_0 */
				sailorws_gfxflag2_w(data);	// NB22090
				break;

			case	5:			/* PA_1 */
				sailorws_soundbank_w(data);
				break;
			case	6:			/* PB_1 */
#if SIGNED_DAC
				DAC_1_signed_data_w(0, data);
#else
				DAC_1_data_w(0, data);
#endif
				break;
			case	7:			/* PC_1 */
#if SIGNED_DAC
				DAC_0_signed_data_w(0, data);
#else
				DAC_0_data_w(0, data);
#endif
				break;
			case	8:			/* PD_1 */
				break;
			case	9:			/* PE_1 */
				if (!(data & 0x01)) sailorws_soundclr_w(0, 0);
				break;

			default:
				logerror("PC %04X: TMPZ84C011_PIO Unknown Port Write %02X, %02X\n", activecpu_get_pc(), offset, data);
				break;
		}
	}
	else
	{
		switch (offset)
		{
			case	0:			/* PA_0 */
				break;
			case	1:			/* PB_0 */
				break;
			case	2:			/* PC_0 */
				sailorws_dipswbitsel_w(data);
				break;
			case	3:			/* PD_0 */
				sailorws_paltblnum_w(data);
				break;
			case	4:			/* PE_0 */
				sailorws_outcoin_flag_w(data);
				break;

			case	5:			/* PA_1 */
				sailorws_soundbank_w(data);
				break;
			case	6:			/* PB_1 */
#if SIGNED_DAC
				DAC_1_signed_data_w(0, data);
#else
				DAC_1_data_w(0, data);
#endif
				break;
			case	7:			/* PC_1 */
#if SIGNED_DAC
				DAC_0_signed_data_w(0, data);
#else
				DAC_0_data_w(0, data);
#endif
				break;
			case	8:			/* PD_1 */
				break;
			case	9:			/* PE_1 */
				if (!(data & 0x01)) sailorws_soundclr_w(0, 0);
				break;

			default:
				logerror("PC %04X: TMPZ84C011_PIO Unknown Port Write %02X, %02X\n", activecpu_get_pc(), offset, data);
				break;
		}
	}
}


/* CPU interface */

/* device 0 */
static READ_HANDLER( tmpz84c011_0_pa_r ) { return (tmpz84c011_pio_r(0) & ~pio_dir[0]) | (pio_latch[0] & pio_dir[0]); }
static READ_HANDLER( tmpz84c011_0_pb_r ) { return (tmpz84c011_pio_r(1) & ~pio_dir[1]) | (pio_latch[1] & pio_dir[1]); }
static READ_HANDLER( tmpz84c011_0_pc_r ) { return (tmpz84c011_pio_r(2) & ~pio_dir[2]) | (pio_latch[2] & pio_dir[2]); }
static READ_HANDLER( tmpz84c011_0_pd_r ) { return (tmpz84c011_pio_r(3) & ~pio_dir[3]) | (pio_latch[3] & pio_dir[3]); }
static READ_HANDLER( tmpz84c011_0_pe_r ) { return (tmpz84c011_pio_r(4) & ~pio_dir[4]) | (pio_latch[4] & pio_dir[4]); }

static WRITE_HANDLER( tmpz84c011_0_pa_w ) { pio_latch[0] = data; tmpz84c011_pio_w(0, data); }
static WRITE_HANDLER( tmpz84c011_0_pb_w ) { pio_latch[1] = data; tmpz84c011_pio_w(1, data); }
static WRITE_HANDLER( tmpz84c011_0_pc_w ) { pio_latch[2] = data; tmpz84c011_pio_w(2, data); }
static WRITE_HANDLER( tmpz84c011_0_pd_w ) { pio_latch[3] = data; tmpz84c011_pio_w(3, data); }
static WRITE_HANDLER( tmpz84c011_0_pe_w ) { pio_latch[4] = data; tmpz84c011_pio_w(4, data); }

static READ_HANDLER( tmpz84c011_0_dir_pa_r ) { return pio_dir[0]; }
static READ_HANDLER( tmpz84c011_0_dir_pb_r ) { return pio_dir[1]; }
static READ_HANDLER( tmpz84c011_0_dir_pc_r ) { return pio_dir[2]; }
static READ_HANDLER( tmpz84c011_0_dir_pd_r ) { return pio_dir[3]; }
static READ_HANDLER( tmpz84c011_0_dir_pe_r ) { return pio_dir[4]; }

static WRITE_HANDLER( tmpz84c011_0_dir_pa_w ) { pio_dir[0] = data; }
static WRITE_HANDLER( tmpz84c011_0_dir_pb_w ) { pio_dir[1] = data; }
static WRITE_HANDLER( tmpz84c011_0_dir_pc_w ) { pio_dir[2] = data; }
static WRITE_HANDLER( tmpz84c011_0_dir_pd_w ) { pio_dir[3] = data; }
static WRITE_HANDLER( tmpz84c011_0_dir_pe_w ) { pio_dir[4] = data; }

/* device 1 */
static READ_HANDLER( tmpz84c011_1_pa_r ) { return (tmpz84c011_pio_r(5) & ~pio_dir[5]) | (pio_latch[5] & pio_dir[5]); }
static READ_HANDLER( tmpz84c011_1_pb_r ) { return (tmpz84c011_pio_r(6) & ~pio_dir[6]) | (pio_latch[6] & pio_dir[6]); }
static READ_HANDLER( tmpz84c011_1_pc_r ) { return (tmpz84c011_pio_r(7) & ~pio_dir[7]) | (pio_latch[7] & pio_dir[7]); }
static READ_HANDLER( tmpz84c011_1_pd_r ) { return (tmpz84c011_pio_r(8) & ~pio_dir[8]) | (pio_latch[8] & pio_dir[8]); }
static READ_HANDLER( tmpz84c011_1_pe_r ) { return (tmpz84c011_pio_r(9) & ~pio_dir[9]) | (pio_latch[9] & pio_dir[9]); }

static WRITE_HANDLER( tmpz84c011_1_pa_w ) { pio_latch[5] = data; tmpz84c011_pio_w(5, data); }
static WRITE_HANDLER( tmpz84c011_1_pb_w ) { pio_latch[6] = data; tmpz84c011_pio_w(6, data); }
static WRITE_HANDLER( tmpz84c011_1_pc_w ) { pio_latch[7] = data; tmpz84c011_pio_w(7, data); }
static WRITE_HANDLER( tmpz84c011_1_pd_w ) { pio_latch[8] = data; tmpz84c011_pio_w(8, data); }
static WRITE_HANDLER( tmpz84c011_1_pe_w ) { pio_latch[9] = data; tmpz84c011_pio_w(9, data); }

static READ_HANDLER( tmpz84c011_1_dir_pa_r ) { return pio_dir[5]; }
static READ_HANDLER( tmpz84c011_1_dir_pb_r ) { return pio_dir[6]; }
static READ_HANDLER( tmpz84c011_1_dir_pc_r ) { return pio_dir[7]; }
static READ_HANDLER( tmpz84c011_1_dir_pd_r ) { return pio_dir[8]; }
static READ_HANDLER( tmpz84c011_1_dir_pe_r ) { return pio_dir[9]; }

static WRITE_HANDLER( tmpz84c011_1_dir_pa_w ) { pio_dir[5] = data; }
static WRITE_HANDLER( tmpz84c011_1_dir_pb_w ) { pio_dir[6] = data; }
static WRITE_HANDLER( tmpz84c011_1_dir_pc_w ) { pio_dir[7] = data; }
static WRITE_HANDLER( tmpz84c011_1_dir_pd_w ) { pio_dir[8] = data; }
static WRITE_HANDLER( tmpz84c011_1_dir_pe_w ) { pio_dir[9] = data; }


static void ctc0_interrupt(int state)
{
	cpu_set_irq_line_and_vector(0, 0, HOLD_LINE, Z80_VECTOR(0, state));
}

static void ctc1_interrupt(int state)
{
	cpu_set_irq_line_and_vector(1, 0, HOLD_LINE, Z80_VECTOR(0, state));
}

/* CTC of main cpu, ch0 trigger is vblank */
static INTERRUPT_GEN( ctc0_trg1 )
{
	z80ctc_0_trg1_w(0, 1);
	z80ctc_0_trg1_w(0, 0);
}

static z80ctc_interface ctc_intf =
{
	2,			/* 2 chip */
	{ 0, 1 },		/* clock */
	{ 0, 0 },		/* timer disables */
	{ ctc0_interrupt, ctc1_interrupt },	/* interrupt handler */
	{ 0, z80ctc_1_trg3_w },	/* ZC/TO0 callback ctc1.zc0 -> ctc1.trg3 */
	{ 0, 0 },		/* ZC/TO1 callback */
	{ 0, 0 },		/* ZC/TO2 callback */
};

static void tmpz84c011_init(void)
{
	int i;

	// initialize TMPZ84C011 PIO
	for (i = 0; i < (5 * 2); i++)
	{
		pio_dir[i] = pio_latch[i] = 0;
		tmpz84c011_pio_w(i, 0);
	}

	// initialize the CTC
	ctc_intf.baseclock[0] = Machine->drv->cpu[0].cpu_clock;
	ctc_intf.baseclock[1] = Machine->drv->cpu[1].cpu_clock;
	z80ctc_init(&ctc_intf);
}

static MACHINE_INIT( sailorws )
{
	//
}

static void initialize_driver(void)
{
	unsigned char *ROM = memory_region(REGION_CPU2);

	// sound program patch
	ROM[0x0213] = 0x00;			// DI -> NOP

	// initialize TMPZ84C011 PIO and CTC
	tmpz84c011_init();

	// initialize sound rom bank
	sailorws_soundbank_w(0);
}


static DRIVER_INIT( mjuraden ) { initialize_driver(); }
static DRIVER_INIT( koinomp ) { initialize_driver(); }
static DRIVER_INIT( patimono ) { initialize_driver(); }
static DRIVER_INIT( mmehyou ) { initialize_driver(); }
static DRIVER_INIT( gal10ren ) { initialize_driver(); }
static DRIVER_INIT( mjlaman ) { initialize_driver(); }
static DRIVER_INIT( mkeibaou ) { initialize_driver(); }
static DRIVER_INIT( pachiten ) { initialize_driver(); }
static DRIVER_INIT( mjanbari ) { initialize_driver(); }
static DRIVER_INIT( ultramhm ) { initialize_driver(); }
static DRIVER_INIT( sailorws ) { initialize_driver(); }
static DRIVER_INIT( sailorwr ) { initialize_driver(); }
static DRIVER_INIT( psailor1 ) { initialize_driver(); }
static DRIVER_INIT( psailor2 ) { initialize_driver(); }
static DRIVER_INIT( otatidai ) { initialize_driver(); }
static DRIVER_INIT( renaiclb ) { initialize_driver(); }
static DRIVER_INIT( ngpgal ) { initialize_driver(); }
static DRIVER_INIT( mjgottsu ) { initialize_driver(); }
static DRIVER_INIT( bakuhatu ) { initialize_driver(); }
static DRIVER_INIT( cmehyou ) { initialize_driver(); }
static DRIVER_INIT( mjkoiura ) { initialize_driver(); }
static DRIVER_INIT( mscoutm ) { initialize_driver(); }
static DRIVER_INIT( imekura ) { initialize_driver(); }
static DRIVER_INIT( mjegolf ) { initialize_driver(); }


static MEMORY_READ_START( readmem_sailorws )
	{ 0x0000, 0xefff, MRA_ROM },
	{ 0xf000, 0xf1ff, sailorws_palette_r },
	{ 0xf800, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( writemem_sailorws )
	{ 0x0000, 0xefff, MWA_ROM },
	{ 0xf000, 0xf1ff, sailorws_palette_w },
	{ 0xf800, 0xffff, MWA_RAM, &sailorws_nvram, &sailorws_nvram_size },
MEMORY_END

static MEMORY_READ_START( readmem_mjuraden )
	{ 0x0000, 0xefff, MRA_ROM },
	{ 0xf200, 0xf3ff, sailorws_palette_r },
	{ 0xf800, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( writemem_mjuraden )
	{ 0x0000, 0xefff, MWA_ROM },
	{ 0xf200, 0xf3ff, sailorws_palette_w },
	{ 0xf800, 0xffff, MWA_RAM },
MEMORY_END

static MEMORY_READ_START( readmem_koinomp )
	{ 0x0000, 0xdfff, MRA_ROM },
	{ 0xe000, 0xe1ff, sailorws_palette_r },
	{ 0xe800, 0xefff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( writemem_koinomp )
	{ 0x0000, 0xdfff, MWA_ROM },
	{ 0xe000, 0xe1ff, sailorws_palette_w },
	{ 0xe800, 0xefff, MWA_RAM, &sailorws_nvram, &sailorws_nvram_size },
MEMORY_END

static MEMORY_READ_START( readmem_ngpgal )
	{ 0x0000, 0xcfff, MRA_ROM },
	{ 0xd000, 0xd1ff, sailorws_palette_r },
	{ 0xd800, 0xdfff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( writemem_ngpgal )
	{ 0x0000, 0xcfff, MWA_ROM },
	{ 0xd000, 0xd1ff, sailorws_palette_w },
	{ 0xd800, 0xdfff, MWA_RAM },
MEMORY_END

static MEMORY_READ_START( readmem_mscoutm )
	{ 0x0000, 0xdfff, MRA_ROM },
	{ 0xe000, 0xe5ff, MRA_RAM },
	{ 0xe600, 0xebff, mscoutm_palette_r },
	{ 0xec00, 0xf1ff, MRA_RAM },
	{ 0xf200, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( writemem_mscoutm )
	{ 0x0000, 0xdfff, MWA_ROM },
	{ 0xe000, 0xe5ff, MWA_RAM },
	{ 0xe600, 0xebff, mscoutm_palette_w },
	{ 0xec00, 0xf1ff, MWA_RAM },
	{ 0xf200, 0xffff, MWA_RAM },
MEMORY_END

static MEMORY_READ_START( readmem_mjegolf )
	{ 0x0000, 0xdfff, MRA_ROM },
	{ 0xe000, 0xe5ff, mscoutm_palette_r },
	{ 0xe600, 0xebff, MRA_RAM },
	{ 0xec00, 0xf1ff, MRA_RAM },
	{ 0xf200, 0xffff, MRA_RAM },
MEMORY_END

static MEMORY_WRITE_START( writemem_mjegolf )
	{ 0x0000, 0xdfff, MWA_ROM },
	{ 0xe000, 0xe5ff, mscoutm_palette_w },
	{ 0xe600, 0xebff, MWA_RAM },
	{ 0xec00, 0xf1ff, MWA_RAM },
	{ 0xf200, 0xffff, MWA_RAM },
MEMORY_END

static MEMORY_READ_START( sound_readmem )
	{ 0x0000, 0x77ff, MRA_ROM },
	{ 0x7800, 0x7fff, MRA_RAM },
	{ 0x8000, 0xffff, MRA_BANK1 },
MEMORY_END

static MEMORY_WRITE_START( sound_writemem )
	{ 0x0000, 0x77ff, MWA_ROM },
	{ 0x7800, 0x7fff, MWA_RAM },
MEMORY_END


static PORT_READ_START( readport_mjuraden )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x80, 0x80, sailorws_gfxbusy_0_r },
	{ 0x81, 0x81, sailorws_gfxrom_0_r },
PORT_END

static PORT_WRITE_START( writeport_mjuraden )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x90, 0x9f, sailorws_paltbl_0_w },

	{ 0x80, 0x80, sailorws_gfxflag_0_w },
	{ 0x81, 0x82, sailorws_scrollx_0_w },
	{ 0x83, 0x84, sailorws_scrolly_0_w },
	{ 0x85, 0x87, sailorws_radr_0_w },
	{ 0x88, 0x88, sailorws_sizex_0_w },
	{ 0x89, 0x89, sailorws_sizey_0_w },
	{ 0x8a, 0x8b, sailorws_drawx_0_w },
	{ 0x8c, 0x8d, sailorws_drawy_0_w },
	{ 0x8f, 0x8f, IOWP_NOP },

	{ 0xb0, 0xb0, sailorws_sound_w },
	{ 0xb2, 0xb2, IOWP_NOP },
	{ 0xb4, 0xb4, IOWP_NOP },
	{ 0xb6, 0xb6, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_koinomp )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x80, 0x80, sailorws_gfxbusy_0_r },
	{ 0x81, 0x81, sailorws_gfxrom_0_r },
	{ 0xa0, 0xa0, sailorws_gfxbusy_1_r },
	{ 0xa1, 0xa1, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_koinomp )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x90, 0x9f, sailorws_paltbl_0_w },
	{ 0xb0, 0xbf, sailorws_paltbl_1_w },

	{ 0x80, 0x80, sailorws_gfxflag_0_w },
	{ 0x81, 0x82, sailorws_scrollx_0_w },
	{ 0x83, 0x84, sailorws_scrolly_0_w },
	{ 0x85, 0x87, sailorws_radr_0_w },
	{ 0x88, 0x88, sailorws_sizex_0_w },
	{ 0x89, 0x89, sailorws_sizey_0_w },
	{ 0x8a, 0x8b, sailorws_drawx_0_w },
	{ 0x8c, 0x8d, sailorws_drawy_0_w },
	{ 0x8f, 0x8f, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_gfxflag_1_w },
	{ 0xa1, 0xa2, sailorws_scrollx_1_w },
	{ 0xa3, 0xa4, sailorws_scrolly_1_w },
	{ 0xa5, 0xa7, sailorws_radr_1_w },
	{ 0xa8, 0xa8, sailorws_sizex_1_w },
	{ 0xa9, 0xa9, sailorws_sizey_1_w },
	{ 0xaa, 0xab, sailorws_drawx_1_w },
	{ 0xac, 0xad, sailorws_drawy_1_w },
	{ 0xaf, 0xaf, IOWP_NOP },

	{ 0xc0, 0xc0, sailorws_sound_w },
	{ 0xc2, 0xc2, IOWP_NOP },
	{ 0xc4, 0xc4, IOWP_NOP },
	{ 0xc6, 0xc6, sailorws_inputportsel_w },
	{ 0xcf, 0xcf, IOWP_NOP },
PORT_END


static PORT_READ_START( readport_patimono )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0xc0, 0xc0, sailorws_gfxbusy_0_r },
	{ 0xc1, 0xc1, sailorws_gfxrom_0_r },
	{ 0x80, 0x80, sailorws_gfxbusy_1_r },
	{ 0x81, 0x81, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_patimono )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x90, 0x9f, sailorws_paltbl_0_w },
	{ 0xd0, 0xdf, sailorws_paltbl_1_w },

	{ 0xc0, 0xc0, sailorws_gfxflag_0_w },
	{ 0xc1, 0xc2, sailorws_scrollx_0_w },
	{ 0xc3, 0xc4, sailorws_scrolly_0_w },
	{ 0xc5, 0xc7, sailorws_radr_0_w },
	{ 0xc8, 0xc8, sailorws_sizex_0_w },
	{ 0xc9, 0xc9, sailorws_sizey_0_w },
	{ 0xca, 0xcb, sailorws_drawx_0_w },
	{ 0xcc, 0xcd, sailorws_drawy_0_w },
	{ 0xcf, 0xcf, IOWP_NOP },

	{ 0x80, 0x80, sailorws_gfxflag_1_w },
	{ 0x81, 0x82, sailorws_scrollx_1_w },
	{ 0x83, 0x84, sailorws_scrolly_1_w },
	{ 0x85, 0x87, sailorws_radr_1_w },
	{ 0x88, 0x88, sailorws_sizex_1_w },
	{ 0x89, 0x89, sailorws_sizey_1_w },
	{ 0x8a, 0x8b, sailorws_drawx_1_w },
	{ 0x8c, 0x8d, sailorws_drawy_1_w },
	{ 0x8f, 0x8f, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_sound_w },
	{ 0xa4, 0xa8, IOWP_NOP },
	{ 0xa8, 0xa0, IOWP_NOP },
	{ 0xb0, 0xb8, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_mmehyou )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x80, 0x80, sailorws_gfxbusy_0_r },
	{ 0x81, 0x81, sailorws_gfxrom_0_r },
PORT_END

static PORT_WRITE_START( writeport_mmehyou )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x90, 0x9f, sailorws_paltbl_0_w },

	{ 0x80, 0x80, sailorws_gfxflag_0_w },
	{ 0x81, 0x82, sailorws_scrollx_0_w },
	{ 0x83, 0x84, sailorws_scrolly_0_w },
	{ 0x85, 0x87, sailorws_radr_0_w },
	{ 0x88, 0x88, sailorws_sizex_0_w },
	{ 0x89, 0x89, sailorws_sizey_0_w },
	{ 0x8a, 0x8b, sailorws_drawx_0_w },
	{ 0x8c, 0x8d, sailorws_drawy_0_w },
	{ 0x8f, 0x8f, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_sound_w },
	{ 0xa4, 0xa4, IOWP_NOP },
	{ 0xa8, 0xa8, IOWP_NOP },
	{ 0xb0, 0xb0, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_gal10ren )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x60, 0x60, sailorws_gfxbusy_0_r },
	{ 0x61, 0x61, sailorws_gfxrom_0_r },
	{ 0xa0, 0xa0, sailorws_gfxbusy_1_r },
	{ 0xa1, 0xa1, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_gal10ren )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x70, 0x7f, sailorws_paltbl_0_w },
	{ 0xb0, 0xbf, sailorws_paltbl_1_w },

	{ 0x60, 0x60, sailorws_gfxflag_0_w },
	{ 0x61, 0x62, sailorws_scrollx_0_w },
	{ 0x63, 0x64, sailorws_scrolly_0_w },
	{ 0x65, 0x67, sailorws_radr_0_w },
	{ 0x68, 0x68, sailorws_sizex_0_w },
	{ 0x69, 0x69, sailorws_sizey_0_w },
	{ 0x6a, 0x6b, sailorws_drawx_0_w },
	{ 0x6c, 0x6d, sailorws_drawy_0_w },
	{ 0x6f, 0x6f, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_gfxflag_1_w },
	{ 0xa1, 0xa2, sailorws_scrollx_1_w },
	{ 0xa3, 0xa4, sailorws_scrolly_1_w },
	{ 0xa5, 0xa7, sailorws_radr_1_w },
	{ 0xa8, 0xa8, sailorws_sizex_1_w },
	{ 0xa9, 0xa9, sailorws_sizey_1_w },
	{ 0xaa, 0xab, sailorws_drawx_1_w },
	{ 0xac, 0xad, sailorws_drawy_1_w },
	{ 0xaf, 0xaf, IOWP_NOP },

	{ 0xc0, 0xc0, sailorws_sound_w },
	{ 0xc8, 0xc8, IOWP_NOP },
	{ 0xd0, 0xd0, IOWP_NOP },
	{ 0xd8, 0xd8, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_renaiclb )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x60, 0x60, sailorws_gfxbusy_1_r },
	{ 0x61, 0x61, sailorws_gfxrom_1_r },
	{ 0xe0, 0xe0, sailorws_gfxbusy_0_r },
	{ 0xe1, 0xe1, sailorws_gfxrom_0_r },
PORT_END

static PORT_WRITE_START( writeport_renaiclb )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x70, 0x7f, sailorws_paltbl_1_w },
	{ 0xf0, 0xff, sailorws_paltbl_0_w },

	{ 0x60, 0x60, sailorws_gfxflag_1_w },
	{ 0x61, 0x62, sailorws_scrollx_1_w },
	{ 0x63, 0x64, sailorws_scrolly_1_w },
	{ 0x65, 0x67, sailorws_radr_1_w },
	{ 0x68, 0x68, sailorws_sizex_1_w },
	{ 0x69, 0x69, sailorws_sizey_1_w },
	{ 0x6a, 0x6b, sailorws_drawx_1_w },
	{ 0x6c, 0x6d, sailorws_drawy_1_w },
	{ 0x6f, 0x6f, IOWP_NOP },

	{ 0xe0, 0xe0, sailorws_gfxflag_0_w },
	{ 0xe1, 0xe2, sailorws_scrollx_0_w },
	{ 0xe3, 0xe4, sailorws_scrolly_0_w },
	{ 0xe5, 0xe7, sailorws_radr_0_w },
	{ 0xe8, 0xe8, sailorws_sizex_0_w },
	{ 0xe9, 0xe9, sailorws_sizey_0_w },
	{ 0xea, 0xeb, sailorws_drawx_0_w },
	{ 0xec, 0xed, sailorws_drawy_0_w },
	{ 0xef, 0xef, IOWP_NOP },

	{ 0x20, 0x20, sailorws_sound_w },
	{ 0x24, 0x24, IOWP_NOP },
	{ 0x28, 0x28, IOWP_NOP },
	{ 0x2c, 0x2c, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_mjlaman )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x80, 0x80, sailorws_gfxbusy_0_r },
	{ 0x81, 0x81, sailorws_gfxrom_0_r },
	{ 0xe0, 0xe0, sailorws_gfxbusy_1_r },
	{ 0xe1, 0xe1, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_mjlaman )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x90, 0x9f, sailorws_paltbl_0_w },
	{ 0xf0, 0xff, sailorws_paltbl_1_w },

	{ 0x80, 0x80, sailorws_gfxflag_0_w },
	{ 0x81, 0x82, sailorws_scrollx_0_w },
	{ 0x83, 0x84, sailorws_scrolly_0_w },
	{ 0x85, 0x87, sailorws_radr_0_w },
	{ 0x88, 0x88, sailorws_sizex_0_w },
	{ 0x89, 0x89, sailorws_sizey_0_w },
	{ 0x8a, 0x8b, sailorws_drawx_0_w },
	{ 0x8c, 0x8d, sailorws_drawy_0_w },
	{ 0x8f, 0x8f, IOWP_NOP },

	{ 0xe0, 0xe0, sailorws_gfxflag_1_w },
	{ 0xe1, 0xe2, sailorws_scrollx_1_w },
	{ 0xe3, 0xe4, sailorws_scrolly_1_w },
	{ 0xe5, 0xe7, sailorws_radr_1_w },
	{ 0xe8, 0xe8, sailorws_sizex_1_w },
	{ 0xe9, 0xe9, sailorws_sizey_1_w },
	{ 0xea, 0xeb, sailorws_drawx_1_w },
	{ 0xec, 0xed, sailorws_drawy_1_w },
	{ 0xef, 0xef, IOWP_NOP },

	{ 0x20, 0x20, sailorws_sound_w },
	{ 0x22, 0x22, IOWP_NOP },
	{ 0x24, 0x24, IOWP_NOP },
	{ 0x26, 0x26, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_mkeibaou )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x80, 0x80, sailorws_gfxbusy_0_r },
	{ 0x81, 0x81, sailorws_gfxrom_0_r },
	{ 0xa0, 0xa0, sailorws_gfxbusy_1_r },
	{ 0xa1, 0xa1, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_mkeibaou )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x90, 0x9f, sailorws_paltbl_0_w },
	{ 0xb0, 0xbf, sailorws_paltbl_1_w },

	{ 0x80, 0x80, sailorws_gfxflag_0_w },
	{ 0x81, 0x82, sailorws_scrollx_0_w },
	{ 0x83, 0x84, sailorws_scrolly_0_w },
	{ 0x85, 0x87, sailorws_radr_0_w },
	{ 0x88, 0x88, sailorws_sizex_0_w },
	{ 0x89, 0x89, sailorws_sizey_0_w },
	{ 0x8a, 0x8b, sailorws_drawx_0_w },
	{ 0x8c, 0x8d, sailorws_drawy_0_w },
	{ 0x8f, 0x8f, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_gfxflag_1_w },
	{ 0xa1, 0xa2, sailorws_scrollx_1_w },
	{ 0xa3, 0xa4, sailorws_scrolly_1_w },
	{ 0xa5, 0xa7, sailorws_radr_1_w },
	{ 0xa8, 0xa8, sailorws_sizex_1_w },
	{ 0xa9, 0xa9, sailorws_sizey_1_w },
	{ 0xaa, 0xab, sailorws_drawx_1_w },
	{ 0xac, 0xad, sailorws_drawy_1_w },
	{ 0xaf, 0xaf, IOWP_NOP },

	{ 0xd8, 0xd8, sailorws_sound_w },
	{ 0xda, 0xda, IOWP_NOP },
	{ 0xdc, 0xdc, IOWP_NOP },
	{ 0xde, 0xde, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_pachiten )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x60, 0x60, sailorws_gfxbusy_0_r },
	{ 0x61, 0x61, sailorws_gfxrom_0_r },
	{ 0xa0, 0xa0, sailorws_gfxbusy_1_r },
	{ 0xa1, 0xa1, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_pachiten )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x70, 0x7f, sailorws_paltbl_0_w },
	{ 0xb0, 0xbf, sailorws_paltbl_1_w },

	{ 0x60, 0x60, sailorws_gfxflag_0_w },
	{ 0x61, 0x62, sailorws_scrollx_0_w },
	{ 0x63, 0x64, sailorws_scrolly_0_w },
	{ 0x65, 0x67, sailorws_radr_0_w },
	{ 0x68, 0x68, sailorws_sizex_0_w },
	{ 0x69, 0x69, sailorws_sizey_0_w },
	{ 0x6a, 0x6b, sailorws_drawx_0_w },
	{ 0x6c, 0x6d, sailorws_drawy_0_w },
	{ 0x6f, 0x6f, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_gfxflag_1_w },
	{ 0xa1, 0xa2, sailorws_scrollx_1_w },
	{ 0xa3, 0xa4, sailorws_scrolly_1_w },
	{ 0xa5, 0xa7, sailorws_radr_1_w },
	{ 0xa8, 0xa8, sailorws_sizex_1_w },
	{ 0xa9, 0xa9, sailorws_sizey_1_w },
	{ 0xaa, 0xab, sailorws_drawx_1_w },
	{ 0xac, 0xad, sailorws_drawy_1_w },
	{ 0xaf, 0xaf, IOWP_NOP },

	{ 0xe0, 0xe0, sailorws_sound_w },
	{ 0xe2, 0xe2, IOWP_NOP },
	{ 0xe4, 0xe4, IOWP_NOP },
	{ 0xe6, 0xe6, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_sailorws )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x60, 0x60, sailorws_gfxbusy_0_r },
	{ 0x61, 0x61, sailorws_gfxrom_0_r },
	{ 0x80, 0x80, sailorws_gfxbusy_1_r },
	{ 0x81, 0x81, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_sailorws )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x70, 0x7f, sailorws_paltbl_0_w },
	{ 0x90, 0x9f, sailorws_paltbl_1_w },

	{ 0x60, 0x60, sailorws_gfxflag_0_w },
	{ 0x61, 0x62, sailorws_scrollx_0_w },
	{ 0x63, 0x64, sailorws_scrolly_0_w },
	{ 0x65, 0x67, sailorws_radr_0_w },
	{ 0x68, 0x68, sailorws_sizex_0_w },
	{ 0x69, 0x69, sailorws_sizey_0_w },
	{ 0x6a, 0x6b, sailorws_drawx_0_w },
	{ 0x6c, 0x6d, sailorws_drawy_0_w },
	{ 0x6f, 0x6f, IOWP_NOP },

	{ 0x80, 0x80, sailorws_gfxflag_1_w },
	{ 0x81, 0x82, sailorws_scrollx_1_w },
	{ 0x83, 0x84, sailorws_scrolly_1_w },
	{ 0x85, 0x87, sailorws_radr_1_w },
	{ 0x88, 0x88, sailorws_sizex_1_w },
	{ 0x89, 0x89, sailorws_sizey_1_w },
	{ 0x8a, 0x8b, sailorws_drawx_1_w },
	{ 0x8c, 0x8d, sailorws_drawy_1_w },
	{ 0x8f, 0x8f, IOWP_NOP },

	{ 0xf0, 0xf0, sailorws_sound_w },
	{ 0xf2, 0xf2, IOWP_NOP },
	{ 0xf4, 0xf4, IOWP_NOP },
	{ 0xf6, 0xf6, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_sailorwr )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x60, 0x60, sailorws_gfxbusy_0_r },
	{ 0x61, 0x61, sailorws_gfxrom_0_r },
	{ 0x80, 0x80, sailorws_gfxbusy_1_r },
	{ 0x81, 0x81, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_sailorwr )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x70, 0x7f, sailorws_paltbl_0_w },
	{ 0x90, 0x9f, sailorws_paltbl_1_w },

	{ 0x60, 0x60, sailorws_gfxflag_0_w },
	{ 0x61, 0x62, sailorws_scrollx_0_w },
	{ 0x63, 0x64, sailorws_scrolly_0_w },
	{ 0x65, 0x67, sailorws_radr_0_w },
	{ 0x68, 0x68, sailorws_sizex_0_w },
	{ 0x69, 0x69, sailorws_sizey_0_w },
	{ 0x6a, 0x6b, sailorws_drawx_0_w },
	{ 0x6c, 0x6d, sailorws_drawy_0_w },
	{ 0x6f, 0x6f, IOWP_NOP },

	{ 0x80, 0x80, sailorws_gfxflag_1_w },
	{ 0x81, 0x82, sailorws_scrollx_1_w },
	{ 0x83, 0x84, sailorws_scrolly_1_w },
	{ 0x85, 0x87, sailorws_radr_1_w },
	{ 0x88, 0x88, sailorws_sizex_1_w },
	{ 0x89, 0x89, sailorws_sizey_1_w },
	{ 0x8a, 0x8b, sailorws_drawx_1_w },
	{ 0x8c, 0x8d, sailorws_drawy_1_w },
	{ 0x8f, 0x8f, IOWP_NOP },

	{ 0xf8, 0xf8, sailorws_sound_w },
	{ 0xfa, 0xfa, IOWP_NOP },
	{ 0xfc, 0xfc, IOWP_NOP },
	{ 0xfe, 0xfe, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_psailor1 )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x60, 0x60, sailorws_gfxbusy_0_r },
	{ 0x61, 0x61, sailorws_gfxrom_0_r },
	{ 0xc0, 0xc0, sailorws_gfxbusy_1_r },
	{ 0xc1, 0xc1, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_psailor1 )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x70, 0x7f, sailorws_paltbl_0_w },
	{ 0xd0, 0xdf, sailorws_paltbl_1_w },

	{ 0x60, 0x60, sailorws_gfxflag_0_w },
	{ 0x61, 0x62, sailorws_scrollx_0_w },
	{ 0x63, 0x64, sailorws_scrolly_0_w },
	{ 0x65, 0x67, sailorws_radr_0_w },
	{ 0x68, 0x68, sailorws_sizex_0_w },
	{ 0x69, 0x69, sailorws_sizey_0_w },
	{ 0x6a, 0x6b, sailorws_drawx_0_w },
	{ 0x6c, 0x6d, sailorws_drawy_0_w },
	{ 0x6f, 0x6f, IOWP_NOP },

	{ 0xc0, 0xc0, sailorws_gfxflag_1_w },
	{ 0xc1, 0xc2, sailorws_scrollx_1_w },
	{ 0xc3, 0xc4, sailorws_scrolly_1_w },
	{ 0xc5, 0xc7, sailorws_radr_1_w },
	{ 0xc8, 0xc8, sailorws_sizex_1_w },
	{ 0xc9, 0xc9, sailorws_sizey_1_w },
	{ 0xca, 0xcb, sailorws_drawx_1_w },
	{ 0xcc, 0xcd, sailorws_drawy_1_w },
	{ 0xcf, 0xcf, IOWP_NOP },

	{ 0xf0, 0xf0, sailorws_sound_w },
	{ 0xf2, 0xf2, IOWP_NOP },
	{ 0xf4, 0xf4, IOWP_NOP },
	{ 0xf6, 0xf6, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_psailor2 )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x60, 0x60, sailorws_gfxbusy_0_r },
	{ 0x61, 0x61, sailorws_gfxrom_0_r },
	{ 0xa0, 0xa0, sailorws_gfxbusy_1_r },
	{ 0xa1, 0xa1, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_psailor2 )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x70, 0x7f, sailorws_paltbl_0_w },
	{ 0xb0, 0xbf, sailorws_paltbl_1_w },

	{ 0x60, 0x60, sailorws_gfxflag_0_w },
	{ 0x61, 0x62, sailorws_scrollx_0_w },
	{ 0x63, 0x64, sailorws_scrolly_0_w },
	{ 0x65, 0x67, sailorws_radr_0_w },
	{ 0x68, 0x68, sailorws_sizex_0_w },
	{ 0x69, 0x69, sailorws_sizey_0_w },
	{ 0x6a, 0x6b, sailorws_drawx_0_w },
	{ 0x6c, 0x6d, sailorws_drawy_0_w },
	{ 0x6f, 0x6f, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_gfxflag_1_w },
	{ 0xa1, 0xa2, sailorws_scrollx_1_w },
	{ 0xa3, 0xa4, sailorws_scrolly_1_w },
	{ 0xa5, 0xa7, sailorws_radr_1_w },
	{ 0xa8, 0xa8, sailorws_sizex_1_w },
	{ 0xa9, 0xa9, sailorws_sizey_1_w },
	{ 0xaa, 0xab, sailorws_drawx_1_w },
	{ 0xac, 0xad, sailorws_drawy_1_w },
	{ 0xaf, 0xaf, IOWP_NOP },

	{ 0xe0, 0xe0, sailorws_sound_w },
	{ 0xe2, 0xe2, IOWP_NOP },
	{ 0xe4, 0xe4, IOWP_NOP },
	{ 0xf6, 0xf6, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_otatidai )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x60, 0x60, sailorws_gfxbusy_0_r },
	{ 0x61, 0x61, sailorws_gfxrom_0_r },
	{ 0x80, 0x80, sailorws_gfxbusy_1_r },
	{ 0x81, 0x81, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_otatidai )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x70, 0x7f, sailorws_paltbl_0_w },
	{ 0x90, 0x9f, sailorws_paltbl_1_w },

	{ 0x60, 0x60, sailorws_gfxflag_0_w },
	{ 0x61, 0x62, sailorws_scrollx_0_w },
	{ 0x63, 0x64, sailorws_scrolly_0_w },
	{ 0x65, 0x67, sailorws_radr_0_w },
	{ 0x68, 0x68, sailorws_sizex_0_w },
	{ 0x69, 0x69, sailorws_sizey_0_w },
	{ 0x6a, 0x6b, sailorws_drawx_0_w },
	{ 0x6c, 0x6d, sailorws_drawy_0_w },
	{ 0x6f, 0x6f, IOWP_NOP },

	{ 0x80, 0x80, sailorws_gfxflag_1_w },
	{ 0x81, 0x82, sailorws_scrollx_1_w },
	{ 0x83, 0x84, sailorws_scrolly_1_w },
	{ 0x85, 0x87, sailorws_radr_1_w },
	{ 0x88, 0x88, sailorws_sizex_1_w },
	{ 0x89, 0x89, sailorws_sizey_1_w },
	{ 0x8a, 0x8b, sailorws_drawx_1_w },
	{ 0x8c, 0x8d, sailorws_drawy_1_w },
	{ 0x8f, 0x8f, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_sound_w },
	{ 0xa8, 0xa8, IOWP_NOP },
	{ 0xb0, 0xb0, IOWP_NOP },
	{ 0xb8, 0xb8, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_ngpgal )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0xc0, 0xc0, sailorws_gfxbusy_0_r },
	{ 0xc1, 0xc1, sailorws_gfxrom_0_r },
PORT_END

static PORT_WRITE_START( writeport_ngpgal )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0xd0, 0xdf, sailorws_paltbl_0_w },

	{ 0xc0, 0xc0, sailorws_gfxflag_0_w },
	{ 0xc1, 0xc2, sailorws_scrollx_0_w },
	{ 0xc3, 0xc4, sailorws_scrolly_0_w },
	{ 0xc5, 0xc7, sailorws_radr_0_w },
	{ 0xc8, 0xc8, sailorws_sizex_0_w },
	{ 0xc9, 0xc9, sailorws_sizey_0_w },
	{ 0xca, 0xcb, sailorws_drawx_0_w },
	{ 0xcc, 0xcd, sailorws_drawy_0_w },
	{ 0xcf, 0xcf, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_sound_w },
	{ 0xa4, 0xa4, IOWP_NOP },
	{ 0xa8, 0xa8, IOWP_NOP },
	{ 0xb0, 0xb0, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_mjgottsu )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x80, 0x80, sailorws_gfxbusy_0_r },
	{ 0x81, 0x81, sailorws_gfxrom_0_r },
PORT_END

static PORT_WRITE_START( writeport_mjgottsu )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x90, 0x9f, sailorws_paltbl_0_w },

	{ 0x80, 0x80, sailorws_gfxflag_0_w },
	{ 0x81, 0x82, sailorws_scrollx_0_w },
	{ 0x83, 0x84, sailorws_scrolly_0_w },
	{ 0x85, 0x87, sailorws_radr_0_w },
	{ 0x88, 0x88, sailorws_sizex_0_w },
	{ 0x89, 0x89, sailorws_sizey_0_w },
	{ 0x8a, 0x8b, sailorws_drawx_0_w },
	{ 0x8c, 0x8d, sailorws_drawy_0_w },
	{ 0x8f, 0x8f, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_sound_w },
	{ 0xa4, 0xa4, IOWP_NOP },
	{ 0xa8, 0xa8, IOWP_NOP },
	{ 0xb0, 0xb0, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_cmehyou )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0xc0, 0xc0, sailorws_gfxbusy_0_r },
	{ 0xc1, 0xc1, sailorws_gfxrom_0_r },
PORT_END

static PORT_WRITE_START( writeport_cmehyou )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0xd0, 0xdf, sailorws_paltbl_0_w },

	{ 0xc0, 0xc0, sailorws_gfxflag_0_w },
	{ 0xc1, 0xc2, sailorws_scrollx_0_w },
	{ 0xc3, 0xc4, sailorws_scrolly_0_w },
	{ 0xc5, 0xc7, sailorws_radr_0_w },
	{ 0xc8, 0xc8, sailorws_sizex_0_w },
	{ 0xc9, 0xc9, sailorws_sizey_0_w },
	{ 0xca, 0xcb, sailorws_drawx_0_w },
	{ 0xcc, 0xcd, sailorws_drawy_0_w },
	{ 0xcf, 0xcf, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_sound_w },
	{ 0xa8, 0xa8, IOWP_NOP },
	{ 0xb0, 0xb0, sailorws_inputportsel_w },
	{ 0xb4, 0xb4, IOWP_NOP },
PORT_END


static PORT_READ_START( readport_mjkoiura )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x80, 0x80, sailorws_gfxbusy_0_r },
	{ 0x81, 0x81, sailorws_gfxrom_0_r },
PORT_END

static PORT_WRITE_START( writeport_mjkoiura )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0x90, 0x9f, sailorws_paltbl_0_w },

	{ 0x80, 0x80, sailorws_gfxflag_0_w },
	{ 0x81, 0x82, sailorws_scrollx_0_w },
	{ 0x83, 0x84, sailorws_scrolly_0_w },
	{ 0x85, 0x87, sailorws_radr_0_w },
	{ 0x88, 0x88, sailorws_sizex_0_w },
	{ 0x89, 0x89, sailorws_sizey_0_w },
	{ 0x8a, 0x8b, sailorws_drawx_0_w },
	{ 0x8c, 0x8d, sailorws_drawy_0_w },
	{ 0x8f, 0x8f, IOWP_NOP },

	{ 0xa0, 0xa0, sailorws_sound_w },
	{ 0xa4, 0xa4, IOWP_NOP },
	{ 0xa8, 0xa8, IOWP_NOP },
	{ 0xb0, 0xb0, sailorws_inputportsel_w },
PORT_END


static PORT_READ_START( readport_mscoutm )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x80, 0x80, mscoutm_dipsw_1_r },
	{ 0x82, 0x82, mscoutm_dipsw_0_r },
	{ 0xc0, 0xc0, sailorws_gfxbusy_0_r },
	{ 0xc1, 0xc1, sailorws_gfxrom_0_r },
	{ 0xe0, 0xe0, sailorws_gfxbusy_1_r },
	{ 0xe1, 0xe1, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_mscoutm )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0xd0, 0xdf, sailorws_paltbl_0_w },
	{ 0xf0, 0xff, sailorws_paltbl_1_w },

	{ 0xa0, 0xa6, IOWP_NOP },			// nb22090 param ?

	{ 0xc0, 0xc0, sailorws_gfxflag_0_w },
	{ 0xc1, 0xc2, sailorws_scrollx_0_w },
	{ 0xc3, 0xc4, sailorws_scrolly_0_w },
	{ 0xc5, 0xc7, sailorws_radr_0_w },
	{ 0xc8, 0xc8, sailorws_sizex_0_w },
	{ 0xc9, 0xc9, sailorws_sizey_0_w },
	{ 0xca, 0xcb, sailorws_drawx_0_w },
	{ 0xcc, 0xcd, sailorws_drawy_0_w },
	{ 0xcf, 0xcf, IOWP_NOP },

	{ 0xe0, 0xe0, sailorws_gfxflag_1_w },
	{ 0xe1, 0xe2, sailorws_scrollx_1_w },
	{ 0xe3, 0xe4, sailorws_scrolly_1_w },
	{ 0xe5, 0xe7, sailorws_radr_1_w },
	{ 0xe8, 0xe8, sailorws_sizex_1_w },
	{ 0xe9, 0xe9, sailorws_sizey_1_w },
	{ 0xea, 0xeb, sailorws_drawx_1_w },
	{ 0xec, 0xed, sailorws_drawy_1_w },
	{ 0xef, 0xef, IOWP_NOP },

	{ 0x84, 0x84, sailorws_sound_w },
PORT_END


static PORT_READ_START( readport_imekura )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0x80, 0x80, mscoutm_dipsw_1_r },
	{ 0x82, 0x82, mscoutm_dipsw_0_r },
	{ 0xc0, 0xc0, sailorws_gfxbusy_0_r },
	{ 0xc1, 0xc1, sailorws_gfxrom_0_r },
	{ 0xe0, 0xe0, sailorws_gfxbusy_1_r },
	{ 0xe1, 0xe1, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_imekura )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0xd0, 0xdf, sailorws_paltbl_0_w },
	{ 0xf0, 0xff, sailorws_paltbl_1_w },

	{ 0xb0, 0xb6, IOWP_NOP },			// nb22090 param ?

	{ 0xc0, 0xc0, sailorws_gfxflag_0_w },
	{ 0xc1, 0xc2, sailorws_scrollx_0_w },
	{ 0xc3, 0xc4, sailorws_scrolly_0_w },
	{ 0xc5, 0xc7, sailorws_radr_0_w },
	{ 0xc8, 0xc8, sailorws_sizex_0_w },
	{ 0xc9, 0xc9, sailorws_sizey_0_w },
	{ 0xca, 0xcb, sailorws_drawx_0_w },
	{ 0xcc, 0xcd, sailorws_drawy_0_w },
	{ 0xcf, 0xcf, IOWP_NOP },

	{ 0xe0, 0xe0, sailorws_gfxflag_1_w },
	{ 0xe1, 0xe2, sailorws_scrollx_1_w },
	{ 0xe3, 0xe4, sailorws_scrolly_1_w },
	{ 0xe5, 0xe7, sailorws_radr_1_w },
	{ 0xe8, 0xe8, sailorws_sizex_1_w },
	{ 0xe9, 0xe9, sailorws_sizey_1_w },
	{ 0xea, 0xeb, sailorws_drawx_1_w },
	{ 0xec, 0xed, sailorws_drawy_1_w },
	{ 0xef, 0xef, IOWP_NOP },

	{ 0x84, 0x84, sailorws_sound_w },
PORT_END


static PORT_READ_START( readport_mjegolf )
	{ 0x10, 0x13, z80ctc_0_r },
	{ 0x50, 0x50, tmpz84c011_0_pa_r },
	{ 0x51, 0x51, tmpz84c011_0_pb_r },
	{ 0x52, 0x52, tmpz84c011_0_pc_r },
	{ 0x30, 0x30, tmpz84c011_0_pd_r },
	{ 0x40, 0x40, tmpz84c011_0_pe_r },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_r },

	{ 0xe0, 0xe0, mscoutm_dipsw_1_r },
	{ 0xe2, 0xe2, mscoutm_dipsw_0_r },
	{ 0xa0, 0xa0, sailorws_gfxbusy_0_r },
	{ 0xa1, 0xa1, sailorws_gfxrom_0_r },
	{ 0xc0, 0xc0, sailorws_gfxbusy_1_r },
	{ 0xc1, 0xc1, sailorws_gfxrom_1_r },
PORT_END

static PORT_WRITE_START( writeport_mjegolf )
	{ 0x10, 0x13, z80ctc_0_w },
	{ 0x50, 0x50, tmpz84c011_0_pa_w },
	{ 0x51, 0x51, tmpz84c011_0_pb_w },
	{ 0x52, 0x52, tmpz84c011_0_pc_w },
	{ 0x30, 0x30, tmpz84c011_0_pd_w },
	{ 0x40, 0x40, tmpz84c011_0_pe_w },
	{ 0x54, 0x54, tmpz84c011_0_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_0_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_0_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_0_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_0_dir_pe_w },

	{ 0xb0, 0xbf, sailorws_paltbl_0_w },
	{ 0xd0, 0xdf, sailorws_paltbl_1_w },

	{ 0x80, 0x86, IOWP_NOP },			// nb22090 param ?

	{ 0xa0, 0xa0, sailorws_gfxflag_0_w },
	{ 0xa1, 0xa2, sailorws_scrollx_0_w },
	{ 0xa3, 0xa4, sailorws_scrolly_0_w },
	{ 0xa5, 0xa7, sailorws_radr_0_w },
	{ 0xa8, 0xa8, sailorws_sizex_0_w },
	{ 0xa9, 0xa9, sailorws_sizey_0_w },
	{ 0xaa, 0xab, sailorws_drawx_0_w },
	{ 0xac, 0xad, sailorws_drawy_0_w },
	{ 0xaf, 0xaf, IOWP_NOP },

	{ 0xc0, 0xc0, sailorws_gfxflag_1_w },
	{ 0xc1, 0xc2, sailorws_scrollx_1_w },
	{ 0xc3, 0xc4, sailorws_scrolly_1_w },
	{ 0xc5, 0xc7, sailorws_radr_1_w },
	{ 0xc8, 0xc8, sailorws_sizex_1_w },
	{ 0xc9, 0xc9, sailorws_sizey_1_w },
	{ 0xca, 0xcb, sailorws_drawx_1_w },
	{ 0xcc, 0xcd, sailorws_drawy_1_w },
	{ 0xcf, 0xcf, IOWP_NOP },

	{ 0xe4, 0xe4, sailorws_sound_w },
PORT_END


static PORT_READ_START( sound_readport )
	{ 0x10, 0x13, z80ctc_1_r },
	{ 0x50, 0x50, tmpz84c011_1_pa_r },
	{ 0x51, 0x51, tmpz84c011_1_pb_r },
	{ 0x52, 0x52, tmpz84c011_1_pc_r },
	{ 0x30, 0x30, tmpz84c011_1_pd_r },
	{ 0x40, 0x40, tmpz84c011_1_pe_r },
	{ 0x54, 0x54, tmpz84c011_1_dir_pa_r },
	{ 0x55, 0x55, tmpz84c011_1_dir_pb_r },
	{ 0x56, 0x56, tmpz84c011_1_dir_pc_r },
	{ 0x34, 0x34, tmpz84c011_1_dir_pd_r },
	{ 0x44, 0x44, tmpz84c011_1_dir_pe_r },
PORT_END

static PORT_WRITE_START( sound_writeport )
	{ 0x10, 0x13, z80ctc_1_w },
	{ 0x50, 0x50, tmpz84c011_1_pa_w },
	{ 0x51, 0x51, tmpz84c011_1_pb_w },
	{ 0x52, 0x52, tmpz84c011_1_pc_w },
	{ 0x30, 0x30, tmpz84c011_1_pd_w },
	{ 0x40, 0x40, tmpz84c011_1_pe_w },
	{ 0x54, 0x54, tmpz84c011_1_dir_pa_w },
	{ 0x55, 0x55, tmpz84c011_1_dir_pb_w },
	{ 0x56, 0x56, tmpz84c011_1_dir_pc_w },
	{ 0x34, 0x34, tmpz84c011_1_dir_pd_w },
	{ 0x44, 0x44, tmpz84c011_1_dir_pe_w },

	{ 0x80, 0x80, YM3812_control_port_0_w },
	{ 0x81, 0x81, YM3812_write_port_0_w },
PORT_END


#define MJCTRL_SAILORWS_PORT1 \
	PORT_START	/* (3) PORT 1-0 */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 Kan", KEYCODE_LCONTROL, IP_JOY_NONE ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 M", KEYCODE_M, IP_JOY_NONE ) \
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 I", KEYCODE_I, IP_JOY_NONE ) \
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 E", KEYCODE_E, IP_JOY_NONE ) \
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 A", KEYCODE_A, IP_JOY_NONE ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define MJCTRL_SAILORWS_PORT2 \
	PORT_START	/* (4) PORT 1-1 */ \
	PORT_BITX(0x01, IP_ACTIVE_LOW, 0, "P1 Bet", KEYCODE_2, IP_JOY_NONE ) \
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 Reach", KEYCODE_LSHIFT, IP_JOY_NONE ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 N", KEYCODE_N, IP_JOY_NONE ) \
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 J", KEYCODE_J, IP_JOY_NONE ) \
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 F", KEYCODE_F, IP_JOY_NONE ) \
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 B", KEYCODE_B, IP_JOY_NONE ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define MJCTRL_SAILORWS_PORT3 \
	PORT_START	/* (5) PORT 1-2 */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 Ron", KEYCODE_Z, IP_JOY_NONE ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 Chi", KEYCODE_SPACE, IP_JOY_NONE ) \
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 K", KEYCODE_K, IP_JOY_NONE ) \
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 G", KEYCODE_G, IP_JOY_NONE ) \
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 C", KEYCODE_C, IP_JOY_NONE ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define MJCTRL_SAILORWS_PORT4 \
	PORT_START	/* (6) PORT 1-3 */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 Pon", KEYCODE_LALT, IP_JOY_NONE ) \
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 L", KEYCODE_L, IP_JOY_NONE ) \
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 H", KEYCODE_H, IP_JOY_NONE ) \
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 D", KEYCODE_D, IP_JOY_NONE ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define MJCTRL_SAILORWS_PORT5 \
	PORT_START	/* (7) PORT 1-4 */ \
	PORT_BITX(0x01, IP_ACTIVE_LOW, 0, "P1 Small", KEYCODE_BACKSPACE, IP_JOY_NONE ) \
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 Big", KEYCODE_ENTER, IP_JOY_NONE ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 Flip", KEYCODE_X, IP_JOY_NONE ) \
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 Double Up", KEYCODE_RSHIFT, IP_JOY_NONE ) \
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 Take Score", KEYCODE_RCONTROL, IP_JOY_NONE ) \
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 Last Chance", KEYCODE_RALT, IP_JOY_NONE ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define MJCTRL_MSCOUTM_PORT1 \
	PORT_START	/* (3) PORT 1-0 */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 Kan", KEYCODE_LCONTROL, IP_JOY_NONE ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 M", KEYCODE_M, IP_JOY_NONE ) \
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 I", KEYCODE_I, IP_JOY_NONE ) \
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 E", KEYCODE_E, IP_JOY_NONE ) \
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 A", KEYCODE_A, IP_JOY_NONE ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define MJCTRL_MSCOUTM_PORT2 \
	PORT_START	/* (4) PORT 1-1 */ \
	PORT_BITX(0x01, IP_ACTIVE_LOW, 0, "P1 Bet", KEYCODE_2, IP_JOY_NONE ) \
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 Reach", KEYCODE_LSHIFT, IP_JOY_NONE ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 N", KEYCODE_N, IP_JOY_NONE ) \
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 J", KEYCODE_J, IP_JOY_NONE ) \
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 F", KEYCODE_F, IP_JOY_NONE ) \
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 B", KEYCODE_B, IP_JOY_NONE ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define MJCTRL_MSCOUTM_PORT3 \
	PORT_START	/* (5) PORT 1-2 */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 Ron", KEYCODE_Z, IP_JOY_NONE ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 Chi", KEYCODE_SPACE, IP_JOY_NONE ) \
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 K", KEYCODE_K, IP_JOY_NONE ) \
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 G", KEYCODE_G, IP_JOY_NONE ) \
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 C", KEYCODE_C, IP_JOY_NONE ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define MJCTRL_MSCOUTM_PORT4 \
	PORT_START	/* (6) PORT 1-3 */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 Pon", KEYCODE_LALT, IP_JOY_NONE ) \
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 L", KEYCODE_L, IP_JOY_NONE ) \
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 H", KEYCODE_H, IP_JOY_NONE ) \
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 D", KEYCODE_D, IP_JOY_NONE ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

#define MJCTRL_MSCOUTM_PORT5 \
	PORT_START	/* (7) PORT 1-4 */ \
	PORT_BITX(0x01, IP_ACTIVE_LOW, 0, "P1 Small", KEYCODE_BACKSPACE, IP_JOY_NONE ) \
	PORT_BITX(0x02, IP_ACTIVE_LOW, 0, "P1 Big", KEYCODE_ENTER, IP_JOY_NONE ) \
	PORT_BITX(0x04, IP_ACTIVE_LOW, 0, "P1 Flip", KEYCODE_X, IP_JOY_NONE ) \
	PORT_BITX(0x08, IP_ACTIVE_LOW, 0, "P1 Double Up", KEYCODE_RSHIFT, IP_JOY_NONE ) \
	PORT_BITX(0x10, IP_ACTIVE_LOW, 0, "P1 Take Score", KEYCODE_RCONTROL, IP_JOY_NONE ) \
	PORT_BITX(0x20, IP_ACTIVE_LOW, 0, "P1 Last Chance", KEYCODE_RALT, IP_JOY_NONE ) \
	PORT_SERVICE( 0x40, IP_ACTIVE_LOW ) \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )


INPUT_PORTS_START( mjuraden )

	// I don't have manual for this game.

	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 1-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 1-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 1-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 1-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Character Display Test" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 1-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( koinomp )

	// I don't have manual for this game.

	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 1-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 1-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 1-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 1-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 1-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( patimono )

	// I don't have manual for this game.

	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 1-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 1-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 1-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 1-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 1-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( mjanbari )

	// I don't have manual for this game.

	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 1-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 1-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 1-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 1-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 1-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 1-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( mmehyou )

	// I don't have manual for this game.

	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 1-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 1-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 1-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 1-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 1-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 1-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 1-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 1-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( ultramhm )

	// I don't have manual for this game.

	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 1-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 1-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 1-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 1-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 1-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 1-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( gal10ren )

	// I don't have manual for this game.

	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 1-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 1-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 1-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 1-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 1-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 1-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( renaiclb )

	// I don't have manual for this game.

	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 1-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 1-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 1-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 1-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 1-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 1-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( mjlaman )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x04, 0x00, "Demo Sounds & Game Sounds" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Voices" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xe0, "1 (Easy)" )
	PORT_DIPSETTING(    0xc0, "2" )
	PORT_DIPSETTING(    0xa0, "3" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0x60, "5" )
	PORT_DIPSETTING(    0x40, "6" )
	PORT_DIPSETTING(    0x20, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hard)" )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( mkeibaou )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Game Sounds" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( pachiten )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x07, 0x07, "Game Out" )
	PORT_DIPSETTING(    0x07, "90% (Easy)" )
	PORT_DIPSETTING(    0x06, "85%" )
	PORT_DIPSETTING(    0x05, "80%" )
	PORT_DIPSETTING(    0x04, "75%" )
	PORT_DIPSETTING(    0x03, "70%" )
	PORT_DIPSETTING(    0x02, "65%" )
	PORT_DIPSETTING(    0x01, "60%" )
	PORT_DIPSETTING(    0x00, "55% (Hard)" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, "Last Chance" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Last chance needs 1credit" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Bet1 Only" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, "Bet Min" )
	PORT_DIPSETTING(    0x18, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x60, 0x00, "Bet Max" )
	PORT_DIPSETTING(    0x60, "8" )
	PORT_DIPSETTING(    0x40, "10" )
	PORT_DIPSETTING(    0x20, "12" )
	PORT_DIPSETTING(    0x00, "20" )
	PORT_DIPNAME( 0x80, 0x80, "Score Pool" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( sailorws )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x04, 0x04, "Infinite Bra" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Game Sounds" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( sailorwr )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x07, 0x07, "Game Out" )
	PORT_DIPSETTING(    0x07, "90% (Easy)" )
	PORT_DIPSETTING(    0x06, "85%" )
	PORT_DIPSETTING(    0x05, "80%" )
	PORT_DIPSETTING(    0x04, "75%" )
	PORT_DIPSETTING(    0x03, "70%" )
	PORT_DIPSETTING(    0x02, "65%" )
	PORT_DIPSETTING(    0x01, "60%" )
	PORT_DIPSETTING(    0x00, "55% (Hard)" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, "Last Chance" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Last chance needs 1credit" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 1-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Character Display Test" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "Bet1 Only" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x18, 0x18, "Bet Min" )
	PORT_DIPSETTING(    0x18, "1" )
	PORT_DIPSETTING(    0x10, "2" )
	PORT_DIPSETTING(    0x08, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x60, 0x00, "Bet Max" )
	PORT_DIPSETTING(    0x60, "8" )
	PORT_DIPSETTING(    0x40, "10" )
	PORT_DIPSETTING(    0x20, "12" )
	PORT_DIPSETTING(    0x00, "20" )
	PORT_DIPNAME( 0x80, 0x80, "Score Pool" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( psailor1 )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Game Sounds" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xe0, "1 (Easy)" )
	PORT_DIPSETTING(    0xc0, "2" )
	PORT_DIPSETTING(    0xa0, "3" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0x60, "5" )
	PORT_DIPSETTING(    0x40, "6" )
	PORT_DIPSETTING(    0x20, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hard)" )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x03, 0x03, "Start Score" )
	PORT_DIPSETTING(    0x00, "5000" )
	PORT_DIPSETTING(    0x01, "3000" )
	PORT_DIPSETTING(    0x02, "2000" )
	PORT_DIPSETTING(    0x03, "1000" )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( psailor2 )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x00, "Game Sounds" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0xe0, 0xe0, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xe0, "1 (Easy)" )
	PORT_DIPSETTING(    0xc0, "2" )
	PORT_DIPSETTING(    0xa0, "3" )
	PORT_DIPSETTING(    0x80, "4" )
	PORT_DIPSETTING(    0x60, "5" )
	PORT_DIPSETTING(    0x40, "6" )
	PORT_DIPSETTING(    0x20, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hard)" )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x03, 0x03, "Start Score" )
	PORT_DIPSETTING(    0x00, "5000" )
	PORT_DIPSETTING(    0x01, "3000" )
	PORT_DIPSETTING(    0x02, "2000" )
	PORT_DIPSETTING(    0x03, "1000" )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( otatidai )

	// I don't have manual for this game.

	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "1 (Easy)" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hard)" )
	PORT_DIPNAME( 0x08, 0x00, "Game Sounds" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_3C ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x03, 0x03, "Start Score" )
	PORT_DIPSETTING(    0x00, "5000" )
	PORT_DIPSETTING(    0x01, "3000" )
	PORT_DIPSETTING(    0x02, "2000" )
	PORT_DIPSETTING(    0x03, "1000" )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Sound Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( ngpgal )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "1 (Easy)" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hard)" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 1-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 1-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( mjgottsu )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Game Sounds" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( bakuhatu )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x08, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "Game Sounds" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( cmehyou )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "1 (Easy)" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hard)" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 1-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 1-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( mjkoiura )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "1 (Easy)" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hard)" )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_2C ) )
	PORT_DIPNAME( 0x10, 0x10, "Character Display Test" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 1-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "DIPSW 2-7" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "DIPSW 2-8" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )		// COIN OUT
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_SERVICE( 0x10, IP_ACTIVE_LOW )			// TEST
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN1 )		// COIN1
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )		// COIN2
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE1 )		// SERVICE

	MJCTRL_SAILORWS_PORT1
	MJCTRL_SAILORWS_PORT2
	MJCTRL_SAILORWS_PORT3
	MJCTRL_SAILORWS_PORT4
	MJCTRL_SAILORWS_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( mscoutm )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "1 (Easy)" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hard)" )
	PORT_DIPNAME( 0x18, 0x18, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Game Sounds" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x80, DEF_STR( On ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER

	MJCTRL_MSCOUTM_PORT1
	MJCTRL_MSCOUTM_PORT2
	MJCTRL_MSCOUTM_PORT3
	MJCTRL_MSCOUTM_PORT4
	MJCTRL_MSCOUTM_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( imekura )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "1 (Easy)" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hard)" )
	PORT_DIPNAME( 0x08, 0x00, "Game Sounds" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_3C ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER

	MJCTRL_MSCOUTM_PORT1
	MJCTRL_MSCOUTM_PORT2
	MJCTRL_MSCOUTM_PORT3
	MJCTRL_MSCOUTM_PORT4
	MJCTRL_MSCOUTM_PORT5
INPUT_PORTS_END

INPUT_PORTS_START( mjegolf )
	PORT_START	/* (0) DIPSW-A */
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x07, "1 (Easy)" )
	PORT_DIPSETTING(    0x06, "2" )
	PORT_DIPSETTING(    0x05, "3" )
	PORT_DIPSETTING(    0x04, "4" )
	PORT_DIPSETTING(    0x03, "5" )
	PORT_DIPSETTING(    0x02, "6" )
	PORT_DIPSETTING(    0x01, "7" )
	PORT_DIPSETTING(    0x00, "8 (Hard)" )
	PORT_DIPNAME( 0x08, 0x00, "Game Sounds" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_3C ) )

	PORT_START	/* (1) DIPSW-B */
	PORT_DIPNAME( 0x01, 0x01, "DIPSW 2-1" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "DIPSW 2-2" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, "DIPSW 2-3" )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, "DIPSW 2-4" )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, "DIPSW 2-5" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "DIPSW 2-6" )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, "Character Display Test" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, "Graphic ROM Test" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* (2) PORT 0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START3 )		// CREDIT CLEAR
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE3 )		// MEMORY RESET
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE2 )		// ANALYZER

	MJCTRL_MSCOUTM_PORT1
	MJCTRL_MSCOUTM_PORT2
	MJCTRL_MSCOUTM_PORT3
	MJCTRL_MSCOUTM_PORT4
	MJCTRL_MSCOUTM_PORT5
INPUT_PORTS_END


static Z80_DaisyChain daisy_chain_main[] =
{
	{ z80ctc_reset, z80ctc_interrupt, z80ctc_reti, 0 },	/* device 0 = CTC_0 */
	{ 0, 0, 0, -1 }		/* end mark */
};

static Z80_DaisyChain daisy_chain_sound[] =
{
	{ z80ctc_reset, z80ctc_interrupt, z80ctc_reti, 1 },	/* device 0 = CTC_1 */
	{ 0, 0, 0, -1 }		/* end mark */
};


static struct YM3812interface ym3812_interface =
{
	1,				/* 1 chip */
	4000000,			/* 4.00 MHz */
	{ 70 }
};

static struct DACinterface dac_interface =
{
	2,				/* 2 channels */
	{ 70, 100 },
};


static MACHINE_DRIVER_START( NBMJDRV1 )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main",Z80,12000000/2)		/* TMPZ84C011, 6.00 MHz */
	MDRV_CPU_CONFIG(daisy_chain_main)
	MDRV_CPU_MEMORY(readmem_sailorws, writemem_sailorws)
	MDRV_CPU_PORTS(readport_sailorws, writeport_sailorws)
	MDRV_CPU_VBLANK_INT(ctc0_trg1, 1)	/* vblank is connect to ctc triggfer */

	MDRV_CPU_ADD(Z80,8000000/1)			/* TMPZ84C011, 8.00 MHz */
	MDRV_CPU_FLAGS(CPU_AUDIO_CPU)
	MDRV_CPU_CONFIG(daisy_chain_sound)
	MDRV_CPU_MEMORY(sound_readmem,sound_writemem)
	MDRV_CPU_PORTS(sound_readport,sound_writeport)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(sailorws)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_UPDATE_AFTER_VBLANK | VIDEO_PIXEL_ASPECT_RATIO_1_2)
	MDRV_SCREEN_SIZE(1024, 512)
	MDRV_VISIBLE_AREA(0, 640-1, 0, 240-1)
	MDRV_PALETTE_LENGTH(256)

	MDRV_VIDEO_START(sailorws)
	MDRV_VIDEO_UPDATE(sailorws)

	/* sound hardware */
	MDRV_SOUND_ADD(YM3812, ym3812_interface)
	MDRV_SOUND_ADD(DAC, dac_interface)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( NBMJDRV2 )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_PIXEL_ASPECT_RATIO_1_2)
	MDRV_VIDEO_START(mjkoiura)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( NBMJDRV3 )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_PIXEL_ASPECT_RATIO_1_2)
	MDRV_PALETTE_LENGTH(512)

	MDRV_VIDEO_START(mscoutm)
	MDRV_VIDEO_UPDATE(mscoutm)
MACHINE_DRIVER_END


//-------------------------------------------------------------------------

static MACHINE_DRIVER_START( mjuraden )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_mjuraden,writemem_mjuraden)
	MDRV_CPU_PORTS(readport_mjuraden,writeport_mjuraden)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( koinomp )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_koinomp,writemem_koinomp)
	MDRV_CPU_PORTS(readport_koinomp,writeport_koinomp)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( patimono )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PORTS(readport_patimono,writeport_patimono)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( mjanbari )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PORTS(readport_patimono,writeport_patimono)

	MDRV_NVRAM_HANDLER(sailorws)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( mmehyou )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_koinomp,writemem_koinomp)
	MDRV_CPU_PORTS(readport_mmehyou,writeport_mmehyou)

	MDRV_NVRAM_HANDLER(sailorws)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( ultramhm )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_koinomp,writemem_koinomp)
	MDRV_CPU_PORTS(readport_koinomp,writeport_koinomp)

	MDRV_NVRAM_HANDLER(sailorws)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( gal10ren )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PORTS(readport_gal10ren,writeport_gal10ren)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( renaiclb )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PORTS(readport_renaiclb,writeport_renaiclb)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( mjlaman )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PORTS(readport_mjlaman,writeport_mjlaman)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( mkeibaou )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PORTS(readport_mkeibaou,writeport_mkeibaou)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( pachiten )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PORTS(readport_pachiten,writeport_pachiten)

	MDRV_NVRAM_HANDLER(sailorws)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( sailorws )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( sailorwr )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PORTS(readport_sailorwr,writeport_sailorwr)

	MDRV_NVRAM_HANDLER(sailorws)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( psailor1 )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PORTS(readport_psailor1,writeport_psailor1)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( psailor2 )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PORTS(readport_psailor2,writeport_psailor2)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( otatidai )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV1 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PORTS(readport_otatidai,writeport_otatidai)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( ngpgal )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV2 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_ngpgal,writemem_ngpgal)
	MDRV_CPU_PORTS(readport_ngpgal,writeport_ngpgal)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( mjgottsu )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV2 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_ngpgal,writemem_ngpgal)
	MDRV_CPU_PORTS(readport_mjgottsu,writeport_mjgottsu)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( bakuhatu )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV2 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_ngpgal,writemem_ngpgal)
	MDRV_CPU_PORTS(readport_mjgottsu,writeport_mjgottsu)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( cmehyou )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV2 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_ngpgal,writemem_ngpgal)
	MDRV_CPU_PORTS(readport_cmehyou,writeport_cmehyou)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( mjkoiura )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV2 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_mjuraden,writemem_mjuraden)
	MDRV_CPU_PORTS(readport_mjkoiura,writeport_mjkoiura)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( mscoutm )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV3 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_mscoutm,writemem_mscoutm)
	MDRV_CPU_PORTS(readport_mscoutm,writeport_mscoutm)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( imekura )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV3 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_mjegolf,writemem_mjegolf)
	MDRV_CPU_PORTS(readport_imekura,writeport_imekura)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( mjegolf )

	/* basic machine hardware */
	MDRV_IMPORT_FROM( NBMJDRV3 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_MEMORY(readmem_mjegolf,writemem_mjegolf)
	MDRV_CPU_PORTS(readport_mjegolf,writeport_mjegolf)
MACHINE_DRIVER_END


ROM_START( mjuraden )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "1.7c",   0x00000,  0x10000, CRC(3b142791) SHA1(b5cf9e2c12967ad4ba035b7480419c91e412c753) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "2.13e",  0x00000,  0x20000, CRC(3a230c22) SHA1(13aa18dcd320039bca2530c62d1033e4e3335697) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "3.1h",   0x000000, 0x80000, CRC(6face365) SHA1(ef42dbdac04069affb1b5841d6738b40b98f2dac) )
	ROM_LOAD( "4.3h",   0x080000, 0x80000, CRC(6b7b0518) SHA1(5f7ea874a872b8bd6d3a256e700cf9fc6c7b0a4a) )
	ROM_LOAD( "5.5h",   0x100000, 0x80000, CRC(43396517) SHA1(a6ba557a94e7e844d36798f73869209ffb4f3015) )
	ROM_LOAD( "6.6h",   0x180000, 0x80000, CRC(32cd3450) SHA1(8b0dd858d5d6f2b436fa8ffcb79975624525735b) )
	ROM_LOAD( "9.11h",  0x240000, 0x20000, CRC(585998bd) SHA1(5df6af1c33038eb3868fa9a2626396c3080077cc) )
	ROM_LOAD( "10.12h", 0x260000, 0x20000, CRC(58220c2a) SHA1(6a42c3ec3f7d01efdbe18a20976e6499885b1932) )
ROM_END

ROM_START( koinomp )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "1.7c",   0x00000,  0x10000, CRC(e4d626fc) SHA1(46bd9b494c7de4eae3e5dfb3bb2ce897ced0d6da) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "2.13e",  0x00000,  0x20000, CRC(4a5c814b) SHA1(8d06da85dd7f6a10e3cc81f0bb7bb60d07fb02a2) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "3.1h",   0x000000, 0x80000, CRC(1f16d3a1) SHA1(93018a9922c66fff6b4c558af474a79e3b90e46b) )
	ROM_LOAD( "4.3h",   0x080000, 0x80000, CRC(f00b1a11) SHA1(e0814ec823016f82c1614440e297a8c5337714e5) )
	ROM_LOAD( "5.5h",   0x100000, 0x80000, CRC(b1ae17b3) SHA1(48d73e004a2d06883a6e9910d06d1e78a986f69c) )
	ROM_LOAD( "6.6h",   0x180000, 0x80000, CRC(bb863b58) SHA1(a71ace4a1836c536b2e8562f46fe62d2f222a49d) )
	ROM_LOAD( "7.7h",   0x200000, 0x80000, CRC(2a3acd8c) SHA1(0bd85a2c4fcd563973485779ecabc64b08aeade4) )
	ROM_LOAD( "8.9h",   0x280000, 0x80000, CRC(595a643a) SHA1(1b909980eba798c720ac6f3ea2872a2e00bdb34d) )
	ROM_LOAD( "9.10h",  0x300000, 0x80000, CRC(28e68e7b) SHA1(a0587e7206dc9b9d5323f5b3c102b848eb58011f) )
	ROM_LOAD( "10.12h", 0x380000, 0x80000, CRC(a598f152) SHA1(634cf6977a5bb1747c512a5095324c40ada8cdd9) )
ROM_END

ROM_START( patimono )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "1.7c",   0x00000,  0x10000, CRC(e4860829) SHA1(192d89b7915153dea8b7a53e1756be83a07f78ec) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "2.13e",  0x00000,  0x20000, CRC(30770363) SHA1(a6af371ca833878361b4a15857fa216272650a19) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "3.1h",   0x000000, 0x80000, CRC(56cbf448) SHA1(b399934adf6df175b2632ef2766e019a40c6d339) )
	ROM_LOAD( "4.3h",   0x080000, 0x80000, CRC(4dd19093) SHA1(71d4c2755100851b6ad5f697d77ccd799cf6aa4d) )
	ROM_LOAD( "5.5h",   0x100000, 0x80000, CRC(63cdc4fe) SHA1(b85069cfef0b58b825d7b36c250873488d36d308) )
	ROM_LOAD( "6.6h",   0x180000, 0x80000, CRC(6057cb66) SHA1(cf12c8322b3034365367924de3b7ceb2b73f189a) )
	ROM_LOAD( "7.7h",   0x200000, 0x80000, CRC(309ea3d5) SHA1(5ff99bed43c6dc0af713897fea8d2b466da80c8d) )
	ROM_LOAD( "8.9h",   0x280000, 0x80000, CRC(6da16cdd) SHA1(295510405df7df4f09838d6a613a0de918648ccb) )
	ROM_LOAD( "9.10h",  0x300000, 0x80000, CRC(c6064b3b) SHA1(e65f7f5f3bd8f7a7d94bcc0b37393449af51a5bf) )
ROM_END

ROM_START( mjanbari )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "11.7c",  0x00000,  0x10000, CRC(1edde2ef) SHA1(fe0c23971cc25c8e2898ac697ce5111fda482f41) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "2.13e",  0x00000,  0x20000, CRC(30770363) SHA1(a6af371ca833878361b4a15857fa216272650a19) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "3.1h",   0x000000, 0x80000, CRC(0fb21d13) SHA1(f9ea15b2dce9c4c22e2095ede7522b980842d592) )
	ROM_LOAD( "4.3h",   0x080000, 0x80000, CRC(4dd19093) SHA1(71d4c2755100851b6ad5f697d77ccd799cf6aa4d) )
	ROM_LOAD( "5.5h",   0x100000, 0x80000, CRC(f5748587) SHA1(0325bd0e0851b56acaae12ce368fda51dc914ec0) )
	ROM_LOAD( "6.6h",   0x180000, 0x80000, CRC(9aaf6aa4) SHA1(1ffe5640c8758789549081db19328e152d5c36ac) )
	ROM_LOAD( "7.7h",   0x200000, 0x80000, CRC(34df5475) SHA1(f907a8240cc97bfc3f326036efb8c22b0eddb3d0) )
	ROM_LOAD( "8.9h",   0x280000, 0x80000, CRC(d4d74ec3) SHA1(81cdce6637b2fe18a5699584a74e50b74e0040be) )
	ROM_LOAD( "9.10h",  0x300000, 0x80000, CRC(f7958466) SHA1(0b1ec02b1b20d056a94c8591c05904cecee6e887) )
ROM_END

ROM_START( mmehyou )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "1.7c",  0x00000,  0x10000, CRC(29d51130) SHA1(266b63c9efa7aca9725173d30bccc39527bc7b55) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "2.13e", 0x00000,  0x20000, CRC(d193a2e1) SHA1(36e5e7c3b35daac96bb48883db0ae6ebd194acf3) )

	ROM_REGION( 0x260000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "3.1h",  0x000000, 0x80000, CRC(e4caab61) SHA1(10fc82edd46fddcfa0cea53eff631e5ac829e15d) )
	ROM_LOAD( "4.3h",  0x080000, 0x80000, CRC(bbb20aef) SHA1(d956f3ffa21b52c005cde13219c576e05ec9462f) )
	ROM_LOAD( "5.5h",  0x100000, 0x80000, CRC(ff59c4c9) SHA1(4c9d3f37c525366f5a43d42cdbf4f0814ab24d74) )
	ROM_LOAD( "6.6h",  0x180000, 0x80000, CRC(d20f9b92) SHA1(87aa76074611701de269d75f13d871bde23dcbef) )
	ROM_LOAD( "7.7h",  0x200000, 0x20000, CRC(d78dfbe2) SHA1(6353e6bab0453eb651f59a4333de4625783d4390) )
	ROM_LOAD( "8.9h",  0x220000, 0x20000, CRC(92160e9b) SHA1(4b8e4518646db5c83cc102ee99ea687c5039b132) )
	ROM_LOAD( "9.10h", 0x240000, 0x20000, CRC(18a72f2e) SHA1(6b4de4ed2befa4cdbce7822d7349024c14ecd4a0) )
ROM_END

ROM_START( ultramhm )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "1.7c",   0x00000,  0x10000, CRC(152811b1) SHA1(6f020848252f95468890c9c35dea3fb211caa9c9) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "2.12e",  0x00000,  0x20000, CRC(a26ba18b) SHA1(593abfda2e082e1dbea37dc8840bd4d63014cde6) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "3.1h",   0x000000, 0x80000, CRC(c0b2bb01) SHA1(5d7c2ae5ceff35a3a7eb747581d9c26ba77bb754) )
	ROM_LOAD( "4.3h",   0x080000, 0x80000, CRC(c9f0fe0f) SHA1(70b5338ca31891605d89531d0ddfafb6a557c5d0) )
	ROM_LOAD( "5.4h",   0x100000, 0x80000, CRC(ee9a449e) SHA1(4e630d633611d137c2cf43ae292162317eb855aa) )
	ROM_LOAD( "6.6h",   0x180000, 0x80000, CRC(0c1a8723) SHA1(b276ca47a5c6396fd8d529a2eae36df57c2753e4) )
ROM_END

ROM_START( gal10ren )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "gl10_01.bin",  0x00000,  0x10000, CRC(f63f81b4) SHA1(a805e472f01f2a6d4acb17770e2490d67c0b51ee) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "gl10_02.bin",  0x00000,  0x20000, CRC(1317b788) SHA1(0e12057c2849e733d88ca094d5382207cd34d79d) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "gl10_03.bin",  0x000000, 0x80000, CRC(ee7853ff) SHA1(5f1c395832437d70bda3928c0d9794b9d033a449) )
	ROM_LOAD( "gl10_04.bin",  0x080000, 0x80000, CRC(e17e4fb5) SHA1(dbb7fa896cf74a1fde00b137c8c220dfacf53cd2) )
	ROM_LOAD( "gl10_05.bin",  0x100000, 0x80000, CRC(0167f589) SHA1(d3fd38f0035e96c53752e4ea1924bcee692b12e4) )
	ROM_LOAD( "gl10_06.bin",  0x180000, 0x80000, CRC(a31a3ab8) SHA1(ac3768604b62808b0d75c20509f80a9684c68c8e) )
	ROM_LOAD( "gl10_07.bin",  0x200000, 0x80000, CRC(0d96419f) SHA1(a1eb0add44345552e3975489f4a569b51046989a) )
	ROM_LOAD( "gl10_08.bin",  0x280000, 0x80000, CRC(777857d0) SHA1(7bae33e61a67d264e56415b95187921dcbdda126) )
	ROM_LOAD( "gl10_09.bin",  0x300000, 0x80000, CRC(b1dba049) SHA1(d39dd7b320e1ed6492f2b6574ec49ead9650e8e9) )
	ROM_LOAD( "gl10_10.bin",  0x380000, 0x80000, CRC(a9806b00) SHA1(07dc60de6dcbe23e142d2438222b53b1d387abb8) )
ROM_END

ROM_START( renaiclb )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "1.7c",  0x00000,  0x10000, CRC(82f99130) SHA1(afb19d716b71c10ca3e8c014be9e9a6105aa660b) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "2.12e", 0x00000,  0x20000, CRC(9f6204a1) SHA1(8d20e0893bb8ceaf8f5076fb0ade24c040f4fc82) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "3.1h",  0x000000, 0x80000, CRC(3d205506) SHA1(f92511c8bff0f21e02587d01d5e046dbbb1dc342) )
	ROM_LOAD( "4.3h",  0x080000, 0x80000, CRC(d9c1af55) SHA1(1fb5e43c2d1bed652499e5f1a2c928d37860df29) )
	ROM_LOAD( "5.5h",  0x100000, 0x80000, CRC(3860cae7) SHA1(b1277acc567d9290ab6e4d69a58968f6a945a022) )
	ROM_LOAD( "6.6h",  0x180000, 0x80000, CRC(f5a43aaa) SHA1(c83e7f3891556428970021b220b42b1a416467a3) )
	ROM_LOAD( "7.7h",  0x200000, 0x80000, CRC(31676c54) SHA1(3127ce38763584e4832996c5cb7c999656c554d9) )
ROM_END

ROM_START( mjlaman )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "mlmn_01.bin",  0x00000,  0x10000, CRC(5974740d) SHA1(1856ccb91abcfbab53bbd23d41ecfe5a3ffa363d) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "mlmn_02.bin",  0x00000,  0x20000, CRC(90adede6) SHA1(ec08b095b894807a6f6d774a21e1e459f1d022b5) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "mlmn_03.bin",  0x000000, 0x80000, CRC(f9c4cda2) SHA1(3875fa6651c9c14e9edbc8c7e0d6ab79abc7cc6d) )
	ROM_LOAD( "mlmn_04.bin",  0x080000, 0x80000, CRC(576c54d4) SHA1(2985af027f56eec909a8be79486c6141c03c602d) )
	ROM_LOAD( "mlmn_05.bin",  0x100000, 0x80000, CRC(0318a070) SHA1(0173c5fa614c002e71475edc48699677874aea9f) )
	ROM_LOAD( "mlmn_06.bin",  0x180000, 0x80000, CRC(9ee76f86) SHA1(2cc52cb5ee47e51dc05bc194a0669ba480293764) )
ROM_END

ROM_START( mkeibaou )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "mkbo_01.bin",  0x00000,  0x10000, CRC(2e37b1fb) SHA1(e4fbfbe20fbc0ea204dc91ee0d133c9bd5bbefb5) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "mkbo_02.bin",  0x00000,  0x20000, CRC(c9a3109e) SHA1(615289fb9a7e11ceaa774eef8192ca7245e23449) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "mkbo_03.bin",  0x000000, 0x80000, CRC(671e2fd9) SHA1(d2062830d6fdfb91aafb5c17dd5865d3373799ea) )
	ROM_LOAD( "mkbo_04.bin",  0x080000, 0x80000, CRC(6ae5d3de) SHA1(13c44bea4e6052d91e7801e37a0e4540886688a8) )
	ROM_LOAD( "mkbo_05.bin",  0x100000, 0x80000, CRC(c57f4532) SHA1(497bff72e28f770be201e4e79ef4a7bdb6fa5d1e) )
	ROM_LOAD( "mkbo_06.bin",  0x180000, 0x80000, CRC(4b7edeea) SHA1(8d8f3fd5867e3ecebb7de7fadc8f0e830d8b4171) )
	ROM_LOAD( "mkbo_07.bin",  0x200000, 0x80000, CRC(6cb2e7f4) SHA1(3a7f1bd712a873561f80586f05e84cde26419f7f) )
	ROM_LOAD( "mkbo_08.bin",  0x280000, 0x80000, CRC(45ca7512) SHA1(28765da39da98fbdd2575f14145c13e2ed24bd5e) )
	ROM_LOAD( "mkbo_09.bin",  0x300000, 0x80000, CRC(abc47929) SHA1(9c83b8701fd7cfa11cf9b120350812ae371b4ea0) )
ROM_END

ROM_START( pachiten )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "pctn_01.bin",  0x00000,  0x10000, CRC(c033d7c6) SHA1(067cdff10819c8653e92981a5becdb73e52bbd70) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "pctn_02.bin",  0x00000,  0x20000, CRC(fe2f0dfa) SHA1(76a0ac1499bde3a05915711d882db132e720208e) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "pctn_03.bin",  0x000000, 0x80000, CRC(9d9c5956) SHA1(6ef318d3a1d4d4f48b893ea331f2db4d670efb3b) )
	ROM_LOAD( "pctn_04.bin",  0x080000, 0x80000, CRC(73765b76) SHA1(43ccdbfec63760f68cc6d7f97cf062862e7c4964) )
	ROM_LOAD( "pctn_05.bin",  0x100000, 0x80000, CRC(db929225) SHA1(499a485842b25fb02acfa5efd9f888e2b865818e) )
	ROM_LOAD( "pctn_06.bin",  0x180000, 0x80000, CRC(4c817293) SHA1(98a89e15b57c42181f525343b25006333a0602ac) )
	ROM_LOAD( "pctn_07.bin",  0x200000, 0x80000, CRC(34df5475) SHA1(f907a8240cc97bfc3f326036efb8c22b0eddb3d0) )
	ROM_LOAD( "pctn_08.bin",  0x280000, 0x80000, CRC(227a73e5) SHA1(721676a05fc8ab9e2255bda3b4d5582150dc4e0d) )
	ROM_LOAD( "pctn_09.bin",  0x300000, 0x80000, CRC(600c738f) SHA1(c8828e98aeddeeb542fd66968d6db4183e4c558e) )
ROM_END

ROM_START( sailorws )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "slws_01.bin",  0x00000,  0x10000, CRC(33191e48) SHA1(55481f09469b239d7eeafc6e53f7bb86f499fae7) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "slws_02.bin",  0x00000,  0x20000, CRC(582f3f29) SHA1(de8f716e069ce0137f610746e477bb7cc841fc72) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "slws_03.bin",  0x000000, 0x80000, CRC(7fe44b0f) SHA1(7400457e5b175f30717d2c55515ca119eec8b6fc) )
	ROM_LOAD( "slws_04.bin",  0x080000, 0x80000, CRC(8b78a009) SHA1(50de97a3ea500008ff9aa7d922c80792fae12550) )
	ROM_LOAD( "slws_05.bin",  0x100000, 0x80000, CRC(6408aa82) SHA1(f92a96bd50c3aad8a05e4737eb1a7955a52000be) )
	ROM_LOAD( "slws_06.bin",  0x180000, 0x80000, CRC(e01d17f5) SHA1(aaa3b0e6505a39013c30167952139d976812fc92) )
	ROM_LOAD( "slws_07.bin",  0x200000, 0x80000, CRC(f8f13876) SHA1(2589acd324f32d73ac5d68756d2f88c240ec09a7) )
	ROM_LOAD( "slws_08.bin",  0x280000, 0x80000, CRC(97ef333d) SHA1(217b9b37cb20459fd09cfe69850cd34a86e97901) )
	ROM_LOAD( "slws_09.bin",  0x300000, 0x80000, CRC(06cadf34) SHA1(a6cdf2f84d1ea2f14bb23b301e9efc78266f1702) )
	ROM_LOAD( "slws_10.bin",  0x380000, 0x80000, CRC(dd944b9c) SHA1(cc828605c4c39b4f3e5f34bc6b1ca103a7dfc719) )
ROM_END

ROM_START( sailorwr )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "slwr_01.bin",  0x00000,  0x10000, CRC(a0d65cd5) SHA1(3c617231309fd6919fd63c95ec0e3540c4b48a76) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "slws_02.bin",  0x00000,  0x20000, CRC(582f3f29) SHA1(de8f716e069ce0137f610746e477bb7cc841fc72) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "slwr_03.bin",  0x000000, 0x80000, CRC(03c865ae) SHA1(44ec3b2ace96a1aac2d816cfb585105b5056e814) )
	ROM_LOAD( "slws_04.bin",  0x080000, 0x80000, CRC(8b78a009) SHA1(50de97a3ea500008ff9aa7d922c80792fae12550) )
	ROM_LOAD( "slws_05.bin",  0x100000, 0x80000, CRC(6408aa82) SHA1(f92a96bd50c3aad8a05e4737eb1a7955a52000be) )
	ROM_LOAD( "slws_06.bin",  0x180000, 0x80000, CRC(e01d17f5) SHA1(aaa3b0e6505a39013c30167952139d976812fc92) )
	ROM_LOAD( "slwr_07.bin",  0x200000, 0x80000, CRC(2ee65c0b) SHA1(ca1959db37183a8e304fe649dab494e6828b3dcf) )
	ROM_LOAD( "slwr_08.bin",  0x280000, 0x80000, CRC(fe72a7fb) SHA1(1c2d400ee4fcafdde55190af74faafc5d35fd5ac) )
	ROM_LOAD( "slwr_09.bin",  0x300000, 0x80000, CRC(149ec899) SHA1(315e850e0cd116c4e02f9f10feed94feb6796f62) )
	ROM_LOAD( "slwr_10.bin",  0x380000, 0x80000, CRC(0cf3da5a) SHA1(9e3c615da436dee5918f970712b8f20fce22f9ec) )
ROM_END

ROM_START( psailor1 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "pts1_01.bin",  0x00000,  0x10000, CRC(a93dab87) SHA1(c4172bd8ed485b80c4fb7a617d8dc017b4fa01a1) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "pts1_02.bin",  0x00000,  0x20000, CRC(0bcc1a89) SHA1(386a979e9a482061fadb4c0bcf2808ad58caa0e6) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "pts1_03.bin",  0x000000, 0x80000, CRC(4f1c2726) SHA1(08090be67ad1caf8d7f53989c988cb8d16e81ee8) )
	ROM_LOAD( "pts1_04.bin",  0x080000, 0x80000, CRC(52e813e0) SHA1(1cb7a7b29b374ee293b5343c1ce5b5048da4ee33) )
	ROM_LOAD( "pts1_05.bin",  0x100000, 0x80000, CRC(c7de2894) SHA1(485bc854629915057e3ab53bf679297d1efe7c5e) )
	ROM_LOAD( "pts1_06.bin",  0x180000, 0x80000, CRC(ba6617f1) SHA1(76795437c180330ae02bb5b7f87ee15147db02d0) )
	ROM_LOAD( "pts1_07.bin",  0x200000, 0x80000, CRC(a67fc71e) SHA1(687648fa1c2dc04dc16ef3d6d65c480be9644f7e) )
	ROM_LOAD( "pts1_08.bin",  0x280000, 0x80000, CRC(eb6e20b6) SHA1(621c8a35266221dde53b6a784082f95288cee213) )
	ROM_LOAD( "pts1_09.bin",  0x300000, 0x80000, CRC(ea05b513) SHA1(dd4f6baf8de94996487d5f2db5020a2e93fe6255) )
	ROM_LOAD( "pts1_10.bin",  0x380000, 0x80000, CRC(2e50d1e7) SHA1(1cfac75740c68b1fcb86e24d7949c916c7961e11) )
ROM_END

ROM_START( psailor2 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "pts2_01.bin",  0x00000,  0x10000, CRC(5a94677f) SHA1(ce8c4f99b619ba72fd3c53e512a83688c3dd54a4) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "pts2_02.bin",  0x00000,  0x20000, CRC(3432de51) SHA1(affd2c1207502a305ea350e20f9adaf6aadfcaec) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "pts2_03.bin",  0x000000, 0x80000, CRC(2b8c992e) SHA1(8ed3b5245077ef0fdd7a9bdfdaa2d856824805fa) )
	ROM_LOAD( "pts2_04.bin",  0x080000, 0x80000, CRC(fea2d719) SHA1(6e797f0a0215da99d7f32f886730328174f15981) )
	ROM_LOAD( "pts2_05.bin",  0x100000, 0x80000, CRC(bab4bcb5) SHA1(9c35bc05203760ddef5da671eb49d271c59a4739) )
	ROM_LOAD( "pts2_06.bin",  0x180000, 0x80000, CRC(0bc750e2) SHA1(102cd5d9e800b77a838583f575459abcdfe7e172) )
	ROM_LOAD( "pts2_07.bin",  0x200000, 0x80000, CRC(9a0f2cc5) SHA1(4f3b7bc947f1b221dc03c44a6e8a36ab6e72b40f) )
	ROM_LOAD( "pts2_08.bin",  0x280000, 0x80000, CRC(ed617dda) SHA1(a3dc58213e8e6875dc937e1cb52c13251b877759) )
	ROM_LOAD( "pts2_09.bin",  0x300000, 0x80000, CRC(7dded702) SHA1(a5a9ccb6267314ff6e582f9deadd3591225606dd) )
	ROM_LOAD( "pts2_10.bin",  0x380000, 0x80000, CRC(7c0863c7) SHA1(c14963e5151677c80dd0ba0e74c20e14067dce59) )
ROM_END

ROM_START( otatidai )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "otcd_01.bin",  0x00000,  0x10000, CRC(a68acf90) SHA1(85d8d7d37fbdd334a5f489ce59dd630383c4e73c) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "otcd_02.bin",  0x00000,  0x20000, CRC(30ed0e78) SHA1(c04185d7e236aeafac5ee5882bfe1ddbf12ba430) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "otcd_03.bin",  0x000000, 0x80000, CRC(bf2cfc6b) SHA1(b232167742670a62b50904e5ac14eb1fcaf6caea) )
	ROM_LOAD( "otcd_04.bin",  0x080000, 0x80000, CRC(76e9b597) SHA1(a8fdd0d268643e57beb1472e7027754ef09b0cb4) )
	ROM_LOAD( "otcd_05.bin",  0x100000, 0x80000, CRC(4e30e3b5) SHA1(a8943417b59b40a0f0d186987f46e6e67f23c94e) )
	ROM_LOAD( "otcd_06.bin",  0x180000, 0x80000, CRC(5523d26e) SHA1(ccb9038d3076a3315dba74d492fa3b51e67f7733) )
	ROM_LOAD( "otcd_07.bin",  0x200000, 0x80000, CRC(8e86cc54) SHA1(b5ba44d088e9bc6e45a1f3a82015912207b6cd4b) )
	ROM_LOAD( "otcd_08.bin",  0x280000, 0x80000, CRC(8f92bc5c) SHA1(abb2424dc5d2714324df1e9c50e0d3a57ae527fd) )
	ROM_LOAD( "otcd_09.bin",  0x300000, 0x80000, CRC(e1c6c345) SHA1(eb7743d14d4bc0f699cb3ad42ef6af2a7a40434b) )
	ROM_LOAD( "otcd_10.bin",  0x380000, 0x80000, CRC(20f74d5b) SHA1(aa7263ca436609f27226208e18a81f2cfe87292b) )
ROM_END

ROM_START( ngpgal )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "ngpg_01.bin",  0x00000,  0x10000, CRC(c766378b) SHA1(b221908eb14ebf5c87ae896c3c27d261b26b5146) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "ngpg_02.bin",  0x00000,  0x20000, CRC(d193a2e1) SHA1(36e5e7c3b35daac96bb48883db0ae6ebd194acf3) )

	ROM_REGION( 0x280000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "ngpg_03.bin",  0x000000, 0x20000, CRC(1f7bd813) SHA1(09f5d443944822d7d0c8d6c1bb149cd7ad6a171a) )
	ROM_LOAD( "ngpg_04.bin",  0x020000, 0x20000, CRC(4f5bd948) SHA1(afe206be1f992f58dd58c8f19de3ff3bc6c672eb) )
	ROM_LOAD( "ngpg_05.bin",  0x040000, 0x20000, CRC(ab65bcc9) SHA1(26509f34b451f88649d67341b9248a5e37fb3be2) )
	ROM_LOAD( "ngpg_06.bin",  0x060000, 0x20000, CRC(0f469db1) SHA1(ca3a437e73b940bd528a9bd9b8ce8f24b16b1867) )
	ROM_LOAD( "ngpg_07.bin",  0x080000, 0x20000, CRC(637098a9) SHA1(f534308dd6300940df17b0b757a63b5861e43937) )
	ROM_LOAD( "ngpg_08.bin",  0x0a0000, 0x20000, CRC(2452d06e) SHA1(55ca2f5c9b76302e989bf9bad709ee7e8bc7fa41) )
	ROM_LOAD( "ngpg_09.bin",  0x0c0000, 0x20000, CRC(da5dded0) SHA1(4ca1ebee5ff74a0acbb27fafb21ad653dc66d8df) )
	ROM_LOAD( "ngpg_10.bin",  0x0e0000, 0x20000, CRC(94201d03) SHA1(9ca2deadb221b23076e323c13e3e83570b157676) )
	ROM_LOAD( "ngpg_11.bin",  0x100000, 0x20000, CRC(2bfc5d06) SHA1(69d779d1df7db16d09f26c325311dca157985ca8) )
	ROM_LOAD( "ngpg_12.bin",  0x120000, 0x20000, CRC(a7e6ecc2) SHA1(ea491e187ec2b2c897d3f970272ab872214a70a9) )
	ROM_LOAD( "ngpg_13.bin",  0x140000, 0x20000, CRC(5c43e71b) SHA1(ef103caed4c56fd205b77e4affd81a7f16296fa1) )
	ROM_LOAD( "ngpg_14.bin",  0x160000, 0x20000, CRC(e8b6802f) SHA1(45d11feba70d9982cab290ed71f959a5a47d003b) )
	ROM_LOAD( "ngpg_15.bin",  0x180000, 0x20000, CRC(7294b5ee) SHA1(658a953b562f95e780335c92fa53d2d6784e8baa) )
	ROM_LOAD( "ngpg_16.bin",  0x1a0000, 0x20000, CRC(3a1f7366) SHA1(cc6caea616de502cdf1a4717b4dae5aced10b8ca) )
	ROM_LOAD( "ngpg_17.bin",  0x1c0000, 0x20000, CRC(0b44f64e) SHA1(72c5f0d51f3de81371c7246d03f800bed43ea450) )
ROM_END

ROM_START( mjgottsu )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "mgek_01.bin",  0x00000,  0x10000, CRC(949676d7) SHA1(2dfa4576c126e1b4f1d4c9ceb2c89905a700f20c) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "2.8d",  0x00000,  0x20000, CRC(52c6a1a1) SHA1(8beb94870b890c05e78f95dba4c7e11f6300542b) )

	ROM_REGION( 0x280000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "3.1k",         0x000000, 0x20000, CRC(58528909) SHA1(ee481ded7a3dbae01e16fd12f9fe806976e87a7f) )
	ROM_LOAD( "4.2k",         0x020000, 0x20000, CRC(d09ad54d) SHA1(0489d9b0755356dd14a05469228cc44784dd7c91) )
	ROM_LOAD( "5.3k",         0x040000, 0x20000, CRC(40346785) SHA1(cb1e51bbd56c68990fee1ba61c04949a65a1ffd1) )
	ROM_LOAD( "mgek_06.bin",  0x060000, 0x20000, CRC(e96635e1) SHA1(47b297fcd24ed0efc6230b6a38ab20297edb0d87) )
	ROM_LOAD( "mgek_07.bin",  0x080000, 0x20000, CRC(174d7ad6) SHA1(894666c5038c88a6532f9a66686d13118aafa82f) )
	ROM_LOAD( "mgek_08.bin",  0x0a0000, 0x20000, CRC(65fd9c90) SHA1(a2fca559453ab34184b909db48cfede6ad8e5833) )
	ROM_LOAD( "mgek_09.bin",  0x0c0000, 0x20000, CRC(417cd914) SHA1(bd167e40b48ef3a0eb8b1767ec89ab03ac82c7b1) )
	ROM_LOAD( "mgek_10.bin",  0x0e0000, 0x20000, CRC(1151414e) SHA1(a2ec6059be72cb85695c8938a0f628bc016166d4) )
	ROM_LOAD( "mgek_11.bin",  0x100000, 0x20000, CRC(2ffd55be) SHA1(20d291102c0194d74faba76aba9bbf0836b86355) )
	ROM_LOAD( "mgek_12.bin",  0x120000, 0x20000, CRC(7a731fa9) SHA1(55266d1b62dd84c5ed4653f5d57d0980e97257a8) )
	ROM_LOAD( "mgek_13.bin",  0x140000, 0x20000, CRC(6d4e56f7) SHA1(c29d0ff6b8ccde7c76d70d267f95004f18b9ca1e) )
	ROM_LOAD( "mgek_14.bin",  0x160000, 0x20000, CRC(de3a675c) SHA1(c4c65feb5697a070184fa72be4136fdf404d07b8) )
	ROM_LOAD( "mgek_15.bin",  0x180000, 0x20000, CRC(e1d6d504) SHA1(b14a5d67318e2ef3f664db3b0a9a2778f675be15) )
	ROM_LOAD( "mgek_16.bin",  0x1a0000, 0x20000, CRC(ca1bca8d) SHA1(3d4138dc9ad4cd366585bc9ae75f612917e5447d) )
	ROM_LOAD( "mgek_17.bin",  0x1c0000, 0x20000, CRC(a69973ad) SHA1(fda9046256ef33b668aa8b7d0a697acc32df2a83) )
	ROM_LOAD( "mgek_18.bin",  0x1e0000, 0x20000, CRC(d7ad46da) SHA1(aea84e04165c6cacb579a61d1adaad21658aa0ef) )
ROM_END

ROM_START( bakuhatu )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "1.4c",  0x00000,  0x10000, CRC(687900ed) SHA1(6cb950e72e4ff62cdee7343f612569f4fccfde46) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "2.8d",  0x00000,  0x20000, CRC(52c6a1a1) SHA1(8beb94870b890c05e78f95dba4c7e11f6300542b) )

	ROM_REGION( 0x280000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "3.1k",   0x000000, 0x20000, CRC(58528909) SHA1(ee481ded7a3dbae01e16fd12f9fe806976e87a7f) )
	ROM_LOAD( "4.2k",   0x020000, 0x20000, CRC(d09ad54d) SHA1(0489d9b0755356dd14a05469228cc44784dd7c91) )
	ROM_LOAD( "5.3k",   0x040000, 0x20000, CRC(40346785) SHA1(cb1e51bbd56c68990fee1ba61c04949a65a1ffd1) )
	ROM_LOAD( "6.4k",   0x060000, 0x20000, CRC(772a6753) SHA1(c35e5a5126c6694a64db4944b0b08109c63912d3) )
	ROM_LOAD( "7.5k",   0x080000, 0x20000, CRC(3ab6b0b5) SHA1(050306f67aa58f825e7f43d35cfe62dc95fdd7af) )
	ROM_LOAD( "8.6k",   0x0a0000, 0x20000, CRC(5b1ca742) SHA1(20ae246d9785cfe69f7b826629ab11c3a70374d1) )
	ROM_LOAD( "9.7k",   0x0c0000, 0x20000, CRC(f177fae1) SHA1(8b9f7fbd078053eb780297a38905e4b7ac9b7ffd) )
	ROM_LOAD( "10.8k",  0x0e0000, 0x20000, CRC(e9003e4d) SHA1(26ace6abbbb093c0d5d5bfbfae3eb60b23f9304c) )
	ROM_LOAD( "11.10k", 0x100000, 0x20000, CRC(c08d835e) SHA1(d2617ba8ec6685c72c53c5bb5b42b89edd9affae) )
	ROM_LOAD( "12.11k", 0x120000, 0x20000, CRC(ae3cbba7) SHA1(925672cdd362be0ec13db3189ec35aadde178045) )
	ROM_LOAD( "13.1l",  0x140000, 0x20000, CRC(1c402b12) SHA1(52004a8b0b72a751dcfa3462d56b81f6b044b88d) )
	ROM_LOAD( "14.2l",  0x160000, 0x20000, CRC(7bb49eaf) SHA1(4d3fe901ab89a4ba4f5a3c34d1594f558a7097ae) )
	ROM_LOAD( "15.3l",  0x180000, 0x20000, CRC(d0844179) SHA1(c3ad5dff2a8b364f321020a3b62adebcb7522f93) )
	ROM_LOAD( "16.4l",  0x1a0000, 0x20000, CRC(5fe47077) SHA1(3f666d79898f0ae12497a0564ed0065e481a4788) )
	ROM_LOAD( "17.5l",  0x1c0000, 0x20000, CRC(9eab0682) SHA1(62e4aebdc144a1944cebdd6c8bbf7089a0dbafef) )
	ROM_LOAD( "18.6l",  0x1e0000, 0x20000, CRC(2b14cd5e) SHA1(4457a2fb9767eb4217b72b18dcc336f13766656d) )
ROM_END

ROM_START( cmehyou )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "cmhy_01.bin",  0x00000,  0x10000, CRC(436dfa6c) SHA1(92882cbc8c3d607045b514faed6bb34303d421d7) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "cmhy_02.bin",  0x00000,  0x20000, CRC(d193a2e1) SHA1(36e5e7c3b35daac96bb48883db0ae6ebd194acf3) )

	ROM_REGION( 0x280000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "cmhy_03.bin",  0x000000, 0x20000, CRC(1f7bd813) SHA1(09f5d443944822d7d0c8d6c1bb149cd7ad6a171a) )
	ROM_LOAD( "cmhy_04.bin",  0x020000, 0x20000, CRC(bdb3de8b) SHA1(cf84034f2fd09c196cf36196b8c244114dc71dcf) )
	ROM_LOAD( "cmhy_05.bin",  0x040000, 0x20000, CRC(4f686de2) SHA1(1861659270d5ae588dacc1801e98891cda34a545) )
	ROM_LOAD( "cmhy_06.bin",  0x060000, 0x20000, CRC(ddd1ac23) SHA1(c0a1e305110a7a1fd06a3b422aa006de1fc44ef5) )
	ROM_LOAD( "cmhy_07.bin",  0x080000, 0x20000, CRC(f7c5367f) SHA1(5f029b159e7625165ec4c6754b2a1e8203ee0e95) )
	ROM_LOAD( "cmhy_08.bin",  0x0a0000, 0x20000, CRC(f8eecdb5) SHA1(f08bd7a9be7027696468b100f7c29c96a3dacbda) )
	ROM_LOAD( "cmhy_09.bin",  0x0c0000, 0x20000, CRC(11e2bbdf) SHA1(fa01514a242d0834d1efa5dc1d3823557165fc80) )
	ROM_LOAD( "cmhy_10.bin",  0x0e0000, 0x20000, CRC(bbe489ae) SHA1(4e1b63f73e96f0f7eed8262cc88e14005198bc32) )
	ROM_LOAD( "cmhy_11.bin",  0x100000, 0x20000, CRC(338efc1f) SHA1(9e3f93c463f137104e907c0e080dadef5e6bf024) )
	ROM_LOAD( "cmhy_12.bin",  0x120000, 0x20000, CRC(6d9f9359) SHA1(36058b65e36ac2bd36a5d921525d2858b1c4aa4c) )
	ROM_LOAD( "cmhy_13.bin",  0x140000, 0x20000, CRC(5c43e71b) SHA1(ef103caed4c56fd205b77e4affd81a7f16296fa1) )
	ROM_LOAD( "cmhy_14.bin",  0x160000, 0x20000, CRC(e8b6802f) SHA1(45d11feba70d9982cab290ed71f959a5a47d003b) )
	ROM_LOAD( "cmhy_15.bin",  0x180000, 0x20000, CRC(f7674a64) SHA1(e26fad2242c394bbf945c43702f54aac32139e06) )
	ROM_LOAD( "cmhy_16.bin",  0x1a0000, 0x20000, CRC(3a1f7366) SHA1(cc6caea616de502cdf1a4717b4dae5aced10b8ca) )
	ROM_LOAD( "cmhy_17.bin",  0x1c0000, 0x20000, CRC(1b8f6e4c) SHA1(1595c215cf4012c5bd7921ca0c6da95b3fc32adb) )
	ROM_LOAD( "cmhy_18.bin",  0x1e0000, 0x20000, CRC(fb86f955) SHA1(6f266d159b4017f9c8b939886c784403b612ebd8) )
	ROM_LOAD( "cmhy_19.bin",  0x200000, 0x20000, CRC(fc89fa4f) SHA1(23e65426bc86c746bd7169033fe802a693720d4c) )
ROM_END

ROM_START( mjkoiura )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "mjku_01.bin",  0x00000,  0x10000, CRC(ef9ae73e) SHA1(2e7fe8f55bc06b842d74efd246df04be647d17e3) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "mjku_02.bin",  0x00000,  0x20000, CRC(3a230c22) SHA1(13aa18dcd320039bca2530c62d1033e4e3335697) )

	ROM_REGION( 0x280000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "mjku_03.bin",  0x000000, 0x20000, CRC(59432ccf) SHA1(a06be02c3d233e607a70b5f79e81c5eb0ff81fd9) )
	ROM_LOAD( "mjku_04.bin",  0x020000, 0x20000, CRC(df5816cb) SHA1(9bbadfcecddc36d2c8109c025e74bb10c854e30c) )
	ROM_LOAD( "mjku_05.bin",  0x040000, 0x20000, CRC(bf01b952) SHA1(b15f1532e0a31269bc31e71d94882d1c00ee6359) )
	ROM_LOAD( "mjku_06.bin",  0x060000, 0x20000, CRC(2dea05ef) SHA1(4d2b44794767cab1d3ef8a3c2c3bd04d2bc93cfa) )
	ROM_LOAD( "mjku_07.bin",  0x080000, 0x20000, CRC(c7843126) SHA1(bd8d7a3cfb53907b058469a796aeabee3deb6e3a) )
	ROM_LOAD( "mjku_08.bin",  0x0a0000, 0x20000, CRC(c7f2fc2d) SHA1(ac135de9173a96c652d56c2b1a6b775f81faad5c) )
	ROM_LOAD( "mjku_09.bin",  0x0c0000, 0x20000, CRC(816b2a36) SHA1(a70bea19ef14e58a6805131c107aca618d2b672e) )
	ROM_LOAD( "mjku_10.bin",  0x0e0000, 0x20000, CRC(c417fe11) SHA1(590aadb6cff534d989128d06032307ffdf5dc6cb) )
	ROM_LOAD( "mjku_11.bin",  0x100000, 0x20000, CRC(9e1914e2) SHA1(f12c8b8dc0ab991c99493d17ab4ccc3dc9ab1759) )
	ROM_LOAD( "mjku_12.bin",  0x120000, 0x20000, CRC(03607cec) SHA1(0da5c38526f223dad1552935d6da43ca806fe4c9) )
	ROM_LOAD( "mjku_13.bin",  0x140000, 0x20000, CRC(18018e08) SHA1(4a98cd26c59e6f696d1b72afb37f085121f791aa) )
	ROM_LOAD( "mjku_14.bin",  0x160000, 0x20000, CRC(4e835fc0) SHA1(b8585a038653a6d188a31b666f521b81dae2e763) )
	ROM_LOAD( "mjku_15.bin",  0x180000, 0x20000, CRC(8fe50109) SHA1(7ba8a73c58471126a851e07f57c3d00b8fdf1459) )
	ROM_LOAD( "mjku_16.bin",  0x1a0000, 0x20000, CRC(dc5b8688) SHA1(749af90c0759a47d7c78cc07958da1b4f11f4d76) )
	ROM_LOAD( "mjku_17.bin",  0x1c0000, 0x20000, CRC(8579a7b8) SHA1(0509e2f2717c55ce58dc4c74d65db714faa00c2f) )
	ROM_LOAD( "mjku_18.bin",  0x1e0000, 0x20000, CRC(c5e330a4) SHA1(465794e4fa2879d2ae396f1e8624bf21f784bfa3) )
	ROM_LOAD( "mjku_21.bin",  0x240000, 0x20000, CRC(585998bd) SHA1(5df6af1c33038eb3868fa9a2626396c3080077cc) )
	ROM_LOAD( "mjku_22.bin",  0x260000, 0x20000, CRC(64af3e5d) SHA1(9b9e8e3eda9b1a1c9b6b464e6c9f30ac4a667dbf) )
ROM_END

ROM_START( mscoutm )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "mscm_01.bin",  0x00000,  0x10000, CRC(9840ccd8) SHA1(b416606508abddbed50c7ae840e44067fc27d531) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "mscm_02.bin",  0x00000,  0x20000, CRC(4d2cbcab) SHA1(ad5c9257d32129c473e7f7124f84e381e35bbaa3) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "mscm_03.bin",  0x000000, 0x80000, CRC(fae64c95) SHA1(1df5e848afa94f7a3a147dfbbc7cb3eaec3dc4ca) )
	ROM_LOAD( "mscm_04.bin",  0x080000, 0x80000, CRC(03c80712) SHA1(a16eeca380d949b7e662e3463410532d2c2a22ba) )
	ROM_LOAD( "mscm_05.bin",  0x100000, 0x80000, CRC(107659f3) SHA1(e8d9342a9c91b96f52fcb9aad9fa2a8c6b145c44) )
	ROM_LOAD( "mscm_06.bin",  0x180000, 0x80000, CRC(61f7fa86) SHA1(3e57b4f6199271e7c3d20c80b486f293974b05ea) )
	ROM_LOAD( "mscm_07.bin",  0x200000, 0x80000, CRC(10a71690) SHA1(b161c347a77735fe524d39358f2fc2b139352aca) )
	ROM_LOAD( "mscm_08.bin",  0x280000, 0x80000, CRC(3b55ef93) SHA1(29dc2ef5ec4254dad04aac64635bee1eaa809c10) )
	ROM_LOAD( "mscm_09.bin",  0x300000, 0x80000, CRC(5823d565) SHA1(d5a63b5bc10845cbc4d348f20e0a2a40425de9d7) )
	ROM_LOAD( "mscm_10.bin",  0x380000, 0x80000, CRC(c6d44c0e) SHA1(512ee27b340a2741034a08459ce5b217edbce205) )
ROM_END

ROM_START( imekura )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "1.103",  0x00000,  0x10000, CRC(3491083b) SHA1(db73baecb757261d40282b99c87e230b4654fca9) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "2.804",  0x00000,  0x20000, CRC(1ef3e8f0) SHA1(1356047317481dfb28b05707abf039df3dec8e77) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "03.602",  0x000000, 0x80000, CRC(1eb05df4) SHA1(01960928108ff5fe306758e6a4e3ba1d6cd99572) )
	ROM_LOAD( "04.603",  0x080000, 0x80000, CRC(48fefd7d) SHA1(9de6c1324622d815a33a3443cf96bfd9c5c23460) )
	ROM_LOAD( "05.604",  0x100000, 0x80000, CRC(934699a8) SHA1(0b20ee46ca35451016b2536bb975f965e3431e3c) )
	ROM_LOAD( "06.605",  0x180000, 0x80000, CRC(ef97182d) SHA1(0228277005103f41085283e0b045610c7a15732a) )
	ROM_LOAD( "07.606",  0x200000, 0x80000, CRC(e3c6e401) SHA1(0164be663ff8615316f2342a7776178a7e28bc7f) )
	ROM_LOAD( "08.607",  0x280000, 0x80000, CRC(08efb2bf) SHA1(c86d7d3101891d0852116786b7df5ed289cdec67) )
	ROM_LOAD( "09.608",  0x300000, 0x80000, CRC(94606c32) SHA1(064114582eeb051c4d4450117fc039248eda383b) )
	ROM_LOAD( "10.609",  0x380000, 0x80000, CRC(79958b86) SHA1(cd769081867b175c0aa84495c5af97be0de76d6d) )
ROM_END

ROM_START( mjegolf )
	ROM_REGION( 0x10000, REGION_CPU1, 0 ) /* main program */
	ROM_LOAD( "metg_01.bin",  0x00000,  0x10000, CRC(1d7c2fcc) SHA1(cc05036ca8ff0fa5c676049c705ec19b39eb433a) )

	ROM_REGION( 0x20000, REGION_CPU2, 0 ) /* sound program */
	ROM_LOAD( "metg_02.bin",  0x00000,  0x20000, CRC(99f419cf) SHA1(496e5c0afcef9fd7707d87d3588e3a83d7d7f871) )

	ROM_REGION( 0x400000, REGION_GFX1, 0 ) /* gfx */
	ROM_LOAD( "metg_03.bin",  0x000000, 0x80000, CRC(99097d30) SHA1(d6e8b1a746364389690b6f8c23519d01c2d12abd) )
	ROM_LOAD( "metg_04.bin",  0x080000, 0x80000, CRC(9f1822b8) SHA1(517f711ce90ef2863076b55c124c59e7f2f30837) )
	ROM_LOAD( "metg_05.bin",  0x100000, 0x80000, CRC(44b88726) SHA1(01a79fd48bbc43b830eededa120f15eacb21dd61) )
	ROM_LOAD( "metg_06.bin",  0x180000, 0x80000, CRC(59ad0d78) SHA1(2edf86ac6f8755883beaabdf2838e048d2dc4799) )
	ROM_LOAD( "metg_07.bin",  0x200000, 0x80000, CRC(2d8b02d6) SHA1(01ad5f181a4c50fca8235557cd480afe3815d3b6) )
	ROM_LOAD( "metg_08.bin",  0x280000, 0x80000, CRC(f64e16fb) SHA1(e37cf1d2a1499e09cce1c9c076f1341d0e1ea55a) )
	ROM_LOAD( "metg_09.bin",  0x300000, 0x80000, CRC(4231de76) SHA1(fa88ad00d7db9d0494764554baabadcc4be5b8a4) )
	ROM_LOAD( "metg_10.bin",  0x380000, 0x80000, CRC(e91c7adf) SHA1(62e71d4887e7f78a4533573427f09799c6ddc8c6) )
ROM_END


//     YEAR,     NAME,   PARENT,  MACHINE,    INPUT,     INIT,    MONITOR, COMPANY, FULLNAME, FLAGS
GAME( 1992, mjuraden, 0,        mjuraden, mjuraden, mjuraden, ROT0, "Nichibutsu/Yubis", "Mahjong Uranai Densetsu (Japan)" )
GAME( 1992, koinomp,  0,        koinomp,  koinomp,  koinomp,  ROT0, "Nichibutsu", "Mahjong Koi no Magic Potion (Japan)" )
GAME( 1992, patimono, 0,        patimono, patimono, patimono, ROT0, "Nichibutsu", "Mahjong Pachinko Monogatari (Japan)" )
GAME( 1992, mjanbari, 0,        mjanbari, mjanbari, mjanbari, ROT0, "Nichibutsu/Yubis/AV JAPAN", "Medal Mahjong Janjan Baribari [BET] (Japan)" )
GAME( 1992, mmehyou,  0,        mmehyou,  mmehyou,  mmehyou,  ROT0, "Nichibutsu/Kawakusu", "Medal Mahjong Circuit no Mehyou [BET] (Japan)" )
GAME( 1993, ultramhm, 0,        ultramhm, ultramhm, ultramhm, ROT0, "Apple", "Ultra Maru-hi Mahjong (Japan)" )
GAME( 1993, gal10ren, 0,        gal10ren, gal10ren, gal10ren, ROT0, "FUJIC", "Mahjong Gal 10-renpatsu (Japan)" )
GAME( 1993, renaiclb, 0,        renaiclb, renaiclb, renaiclb, ROT0, "FUJIC", "Mahjong Ren-ai Club (Japan)" )
GAME( 1993, mjlaman,  0,        mjlaman,  mjlaman,  mjlaman,  ROT0, "Nichibutsu/AV JAPAN", "Mahjong La Man (Japan)" )
GAME( 1993, mkeibaou, 0,        mkeibaou, mkeibaou, mkeibaou, ROT0, "Nichibutsu", "Mahjong Keibaou (Japan)" )
GAME( 1993, pachiten, 0,        pachiten, pachiten, pachiten, ROT0, "Nichibutsu/MIKI SYOUJI/AV JAPAN", "Medal Mahjong Pachi-Slot Tengoku [BET] (Japan)" )
GAME( 1993, sailorws, 0,        sailorws, sailorws, sailorws, ROT0, "Nichibutsu", "Mahjong Sailor Wars (Japan)" )
GAME( 1993, sailorwr, sailorws, sailorwr, sailorwr, sailorwr, ROT0, "Nichibutsu", "Mahjong Sailor Wars-R [BET] (Japan)" )
GAME( 1994, psailor1, 0,        psailor1, psailor1, psailor1, ROT0, "SPHINX", "Bishoujo Janshi Pretty Sailor 18-kin (Japan)" )
GAME( 1994, psailor2, 0,        psailor2, psailor2, psailor2, ROT0, "SPHINX", "Bishoujo Janshi Pretty Sailor 2 (Japan)" )
GAME( 1995, otatidai, 0,        otatidai, otatidai, otatidai, ROT0, "SPHINX", "Disco Mahjong Otachidai no Okite (Japan)" )

GAME( 1991, ngpgal,   0,        ngpgal,   ngpgal,   ngpgal,   ROT0, "Nichibutsu", "Nekketsu Grand-Prix Gal (Japan)" )
GAME( 1991, mjgottsu, 0,        mjgottsu, mjgottsu, mjgottsu, ROT0, "Nichibutsu", "Mahjong Gottsu ee-kanji (Japan)" )
GAME( 1991, bakuhatu, mjgottsu, bakuhatu, bakuhatu, bakuhatu, ROT0, "Nichibutsu", "Mahjong Bakuhatsu Junjouden (Japan)" )
GAME( 1992, cmehyou,  0,        cmehyou,  cmehyou,  cmehyou,  ROT0, "Nichibutsu/Kawakusu", "Mahjong Circuit no Mehyou (Japan)" )
GAME( 1992, mjkoiura, 0,        mjkoiura, mjkoiura, mjkoiura, ROT0, "Nichibutsu", "Mahjong Koi Uranai (Japan)" )

GAME( 1994, mscoutm,  0,        mscoutm,  mscoutm,  mscoutm,  ROT0, "SPHINX/AV JAPAN", "Mahjong Scout Man (Japan)" )
GAME( 1994, imekura,  0,        imekura,  imekura,  imekura,  ROT0, "SPHINX/AV JAPAN", "Imekura Mahjong (Japan)" )
GAME( 1994, mjegolf,  0,        mjegolf,  mjegolf,  mjegolf,  ROT0, "FUJIC/AV JAPAN", "Mahjong Erotica Golf (Japan)" )
