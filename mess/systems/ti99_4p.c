/*
	SNUG SGCPU (a.k.a. 99/4p) system (preliminary)

	This system is a reimplementation of the old ti99/4a console.  It is known
	both as the 99/4p ("peripheral box", since the system is a card to be
	inserted in the peripheral box, instead of a self contained console), and
	as the SGCPU ("Second Generation CPU", which was originally the name used
	in TI documentation to refer to either (or both) TI99/5 and TI99/8
	projects).

	The SGCPU was designed and built by the SNUG (System 99 Users Group),
	namely by Michael Becker for the hardware part and Harald Glaab for the
	software part.  It has no relationship with TI.

	The card is architectured around a 16-bit bus (vs. an 8-bit bus in every
	other TI99 system).  It includes 64kb of ROM, including a GPL interpreter,
	an internal DSR ROM which contains system-specific code, part of the TI
	extended Basic interpreter, and up to 1Mbyte of RAM.  It still includes a
	16-bit to 8-bit multiplexer in order to support extension cards designed
	for TI99/4a, but it can support 16-bit cards, too.  It does not include
	GROMs, video or sound: instead, it relies on the HSGPL and EVPC cards to
	do the job.

	TODO:
	* Test the system? Call Debug, Call XB16.
	* Implement MEM8 timings.
*/

#include "driver.h"
#include "inputx.h"
#include "vidhrdw/v9938.h"

#include "machine/ti99_4x.h"
#include "machine/tms9901.h"
#include "sndhrdw/spchroms.h"
#include "machine/99_peb.h"
#include "machine/994x_ser.h"
#include "machine/99_dsk.h"
#include "machine/99_ide.h"
#include "devices/mflopimg.h"
#include "devices/cassette.h"
#include "machine/smartmed.h"

static ADDRESS_MAP_START(memmap, ADDRESS_SPACE_PROGRAM, 16)

	AM_RANGE(0x0000, 0x1fff) AM_ROMBANK(1)								/*system ROM*/
	AM_RANGE(0x2000, 0x2fff) AM_RAMBANK(3)								/*lower 8kb of RAM extension: AMS bank 2*/
	AM_RANGE(0x3000, 0x3fff) AM_RAMBANK(4)								/*lower 8kb of RAM extension: AMS bank 3*/
	AM_RANGE(0x4000, 0x5fff) AM_READWRITE(ti99_4p_peb_r, ti99_4p_peb_w)	/*DSR ROM space*/
	AM_RANGE(0x6000, 0x7fff) AM_READWRITE(ti99_4p_cart_r,ti99_4p_cart_w)/*cartridge space (internal or hsgpl)*/
	AM_RANGE(0x8000, 0x83ff) AM_RAMBANK(2)								/*RAM PAD*/
	AM_RANGE(0x8400, 0x87ff) AM_READWRITE(ti99_nop_8_r, ti99_wsnd_w)	/*soundchip write*/
	AM_RANGE(0x8800, 0x8bff) AM_READWRITE(ti99_rv38_r, ti99_nop_8_w)	/*vdp read*/
	AM_RANGE(0x8C00, 0x8fff) AM_READWRITE(ti99_nop_8_r, ti99_wv38_w)	/*vdp write*/
	AM_RANGE(0x9000, 0x93ff) AM_READWRITE(ti99_nop_8_r, ti99_nop_8_w)	/*speech read - installed dynamically*/
	AM_RANGE(0x9400, 0x97ff) AM_READWRITE(ti99_nop_8_r, ti99_nop_8_w)	/*speech write - installed dynamically*/
	AM_RANGE(0x9800, 0x98ff) AM_READWRITE(ti99_4p_rgpl_r, ti99_nop_8_w)	/*GPL read*/
	AM_RANGE(0x9900, 0x9bff) AM_RAMBANK(11)								/*extra RAM for debugger*/
	AM_RANGE(0x9c00, 0x9fff) AM_READWRITE(ti99_nop_8_r, ti99_4p_wgpl_w)	/*GPL write*/
	AM_RANGE(0xa000, 0xafff) AM_RAMBANK(5)								/*upper 24kb of RAM extension: AMS bank 10*/
	AM_RANGE(0xb000, 0xbfff) AM_RAMBANK(6)								/*upper 24kb of RAM extension: AMS bank 11*/
	AM_RANGE(0xc000, 0xcfff) AM_RAMBANK(7)								/*upper 24kb of RAM extension: AMS bank 12*/
	AM_RANGE(0xd000, 0xdfff) AM_RAMBANK(8)								/*upper 24kb of RAM extension: AMS bank 13*/
	AM_RANGE(0xe000, 0xefff) AM_RAMBANK(9)								/*upper 24kb of RAM extension: AMS bank 14*/
	AM_RANGE(0xf000, 0xffff) AM_RAMBANK(10)								/*upper 24kb of RAM extension: AMS bank 15*/

ADDRESS_MAP_END

static ADDRESS_MAP_START(writecru, ADDRESS_SPACE_IO, 8)

	AM_RANGE(0x0000, 0x01ff) AM_WRITE(tms9901_0_cru_w)
	AM_RANGE(0x0200, 0x0fff) AM_WRITE(ti99_4p_peb_cru_w)

ADDRESS_MAP_END

static ADDRESS_MAP_START(readcru, ADDRESS_SPACE_IO, 8)

	AM_RANGE(0x0000, 0x003f) AM_READ(tms9901_0_cru_r)
	AM_RANGE(0x0040, 0x01ff) AM_READ(ti99_4p_peb_cru_r)

ADDRESS_MAP_END


/*
	Input ports, used by machine code for TI keyboard and joystick emulation.

	Since the keyboard microcontroller is not emulated, we use the TI99/4a 48-key keyboard,
	plus two optional joysticks.
*/

INPUT_PORTS_START(ti99_4p)

	/* 1 port for config */
	PORT_START	/* config */
		PORT_BIT( config_speech_mask << config_speech_bit, 1 << config_speech_bit, IPT_DIPSWITCH_NAME) PORT_NAME("Speech synthesis")
			PORT_DIPSETTING( 0x0000, DEF_STR( Off ) )
			PORT_DIPSETTING( 1 << config_speech_bit, DEF_STR( On ) )
		PORT_BIT( config_fdc_mask << config_fdc_bit, fdc_kind_hfdc << config_fdc_bit, IPT_DIPSWITCH_NAME) PORT_NAME("Floppy disk controller")
			PORT_DIPSETTING( fdc_kind_none << config_fdc_bit, DEF_STR( None ) )
			PORT_DIPSETTING( fdc_kind_TI << config_fdc_bit, "Texas Instruments SD" )
#if HAS_99CCFDC
			PORT_DIPSETTING( fdc_kind_CC << config_fdc_bit, "CorComp" )
#endif
			PORT_DIPSETTING( fdc_kind_BwG << config_fdc_bit, "SNUG's BwG" )
			PORT_DIPSETTING( fdc_kind_hfdc << config_fdc_bit, "Myarc's HFDC" )
		/*PORT_BIT( config_ide_mask << config_ide_bit, 1 << config_ide_bit, IPT_DIPSWITCH_NAME) PORT_NAME("Nouspickel's IDE card")
			PORT_DIPSETTING( 0x0000, DEF_STR( Off ) )
			PORT_DIPSETTING( 1 << config_ide_bit, DEF_STR( On ) )*/
		PORT_BIT( config_rs232_mask << config_rs232_bit, /*1 << config_rs232_bit*/0, IPT_DIPSWITCH_NAME) PORT_NAME("TI RS232 card")
			PORT_DIPSETTING( 0x0000, DEF_STR( Off ) )
			PORT_DIPSETTING( 1 << config_rs232_bit, DEF_STR( On ) )
		PORT_BIT( config_mecmouse_mask << config_mecmouse_bit, 0, IPT_DIPSWITCH_NAME) PORT_NAME("Mechatronics Mouse")
			PORT_DIPSETTING( 0x0000, DEF_STR( Off ) )
			PORT_DIPSETTING( 1 << config_mecmouse_bit, DEF_STR( On ) )
		PORT_BIT( config_usbsm_mask << config_usbsm_bit, 1 << config_usbsm_bit, IPT_DIPSWITCH_NAME) PORT_NAME("Nouspickel's USB-SM card")
			PORT_DIPSETTING( 0x0000, DEF_STR( Off ) )
			PORT_DIPSETTING( 1 << config_usbsm_bit, DEF_STR( On ) )


	/* 3 ports for mouse */
	PORT_START /* Mouse - X AXIS */
		PORT_BIT( 0xff, 0x00, IPT_TRACKBALL_X) PORT_SENSITIVITY(100) PORT_KEYDELTA(0) PORT_MINMAX(0,0) PORT_PLAYER(1)

	PORT_START /* Mouse - Y AXIS */
		PORT_BIT( 0xff, 0x00, IPT_TRACKBALL_Y) PORT_SENSITIVITY(100) PORT_KEYDELTA(0) PORT_MINMAX(0,0) PORT_PLAYER(1)

	PORT_START /* Mouse - buttons */
		PORT_BIT(0x0001, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_PLAYER(1)
		PORT_BIT(0x0002, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_PLAYER(1)


	/* 4 ports for keyboard and joystick */
	PORT_START	/* col 0 */
		PORT_BIT(0x0088, IP_ACTIVE_LOW, IPT_UNUSED)
		/* The original control key is located on the left, but we accept the
		right control key as well */
		PORT_BIT(0x0040, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("CTRL") PORT_CODE(KEYCODE_LCONTROL) PORT_CODE(KEYCODE_RCONTROL)
		PORT_BIT(0x0020, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("SHIFT") PORT_CODE(KEYCODE_LSHIFT) PORT_CHAR(UCHAR_SHIFT_1)
		/* TI99/4a has a second shift key which maps the same */
		PORT_BIT(0x0020, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("SHIFT") PORT_CODE(KEYCODE_RSHIFT) PORT_CHAR(UCHAR_SHIFT_1)
		/* The original control key is located on the right, but we accept the
		left function key as well */
		PORT_BIT(0x0010, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("FCTN") PORT_CODE(KEYCODE_RALT) PORT_CODE(KEYCODE_LALT) PORT_CHAR(UCHAR_SHIFT_2)
		PORT_BIT(0x0004, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("ENTER") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(13)
		PORT_BIT(0x0002, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("(SPACE)") PORT_CODE(KEYCODE_SPACE) PORT_CHAR(' ')
		PORT_BIT(0x0001, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("= + QUIT") PORT_CODE(KEYCODE_EQUALS) PORT_CHAR('=') PORT_CHAR('+')
				/* col 1 */
		PORT_BIT(0x8000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("x X (DOWN)") PORT_CODE(KEYCODE_X) PORT_CHAR('x') PORT_CHAR('X')
		PORT_BIT(0x4000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("w W ~") PORT_CODE(KEYCODE_W) PORT_CHAR('w') PORT_CHAR('W') PORT_CHAR('~')
		PORT_BIT(0x2000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("s S (LEFT)") PORT_CODE(KEYCODE_S) PORT_CHAR('s') PORT_CHAR('S')
		PORT_BIT(0x1000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("2 @ INS") PORT_CODE(KEYCODE_2) PORT_CHAR('2') PORT_CHAR('@')
		PORT_BIT(0x0800, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("9 ( BACK") PORT_CODE(KEYCODE_9) PORT_CHAR('9') PORT_CHAR('(')
		PORT_BIT(0x0400, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("o O '") PORT_CODE(KEYCODE_O) PORT_CHAR('o') PORT_CHAR('O') PORT_CHAR('\'')
		PORT_BIT(0x0200, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("l L") PORT_CODE(KEYCODE_L) PORT_CHAR('l') PORT_CHAR('L')
		PORT_BIT(0x0100, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(". >") PORT_CODE(KEYCODE_STOP) PORT_CHAR('.') PORT_CHAR('>')

	PORT_START	/* col 2 */
		PORT_BIT(0x0080, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("c C `") PORT_CODE(KEYCODE_C) PORT_CHAR('c') PORT_CHAR('C') PORT_CHAR('`')
		PORT_BIT(0x0040, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("e E (UP)") PORT_CODE(KEYCODE_E) PORT_CHAR('e') PORT_CHAR('E')
		PORT_BIT(0x0020, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("d D (RIGHT)") PORT_CODE(KEYCODE_D) PORT_CHAR('d') PORT_CHAR('D')
		PORT_BIT(0x0010, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("3 # ERASE") PORT_CODE(KEYCODE_3) PORT_CHAR('3') PORT_CHAR('#')
		PORT_BIT(0x0008, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("8 * REDO") PORT_CODE(KEYCODE_8) PORT_CHAR('8') PORT_CHAR('*')
		PORT_BIT(0x0004, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("i I ?") PORT_CODE(KEYCODE_I) PORT_CHAR('i') PORT_CHAR('I') PORT_CHAR('?')
		PORT_BIT(0x0002, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("k K") PORT_CODE(KEYCODE_K) PORT_CHAR('k') PORT_CHAR('K')
		PORT_BIT(0x0001, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(", <") PORT_CODE(KEYCODE_COMMA) PORT_CHAR(',') PORT_CHAR('<')
				/* col 3 */
		PORT_BIT(0x8000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("v V") PORT_CODE(KEYCODE_V) PORT_CHAR('v') PORT_CHAR('V')
		PORT_BIT(0x4000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("r R [") PORT_CODE(KEYCODE_R) PORT_CHAR('r') PORT_CHAR('R') PORT_CHAR('[')
		PORT_BIT(0x2000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("f F {") PORT_CODE(KEYCODE_F) PORT_CHAR('f') PORT_CHAR('F') PORT_CHAR('{')
		PORT_BIT(0x1000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("4 $ CLEAR") PORT_CODE(KEYCODE_4) PORT_CHAR('4') PORT_CHAR('$')
		PORT_BIT(0x0800, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("7 & AID") PORT_CODE(KEYCODE_7) PORT_CHAR('7') PORT_CHAR('&')
		PORT_BIT(0x0400, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("u U _") PORT_CODE(KEYCODE_U) PORT_CHAR('u') PORT_CHAR('U') PORT_CHAR('_')
		PORT_BIT(0x0200, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("j J") PORT_CODE(KEYCODE_J) PORT_CHAR('j') PORT_CHAR('J')
		PORT_BIT(0x0100, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("m M") PORT_CODE(KEYCODE_M) PORT_CHAR('m') PORT_CHAR('M')

	PORT_START	/* col 4 */
		PORT_BIT(0x0080, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("b B") PORT_CODE(KEYCODE_B) PORT_CHAR('b') PORT_CHAR('B')
		PORT_BIT(0x0040, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("t T ]") PORT_CODE(KEYCODE_T) PORT_CHAR('t') PORT_CHAR('T') PORT_CHAR(']')
		PORT_BIT(0x0020, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("g G }") PORT_CODE(KEYCODE_G) PORT_CHAR('g') PORT_CHAR('G') PORT_CHAR('}')
		PORT_BIT(0x0010, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("5 % BEGIN") PORT_CODE(KEYCODE_5) PORT_CHAR('5') PORT_CHAR('%')
		PORT_BIT(0x0008, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("6 ^ PROC'D") PORT_CODE(KEYCODE_6) PORT_CHAR('6') PORT_CHAR('^')
		PORT_BIT(0x0004, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("y Y") PORT_CODE(KEYCODE_Y) PORT_CHAR('y') PORT_CHAR('Y')
		PORT_BIT(0x0002, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("h H") PORT_CODE(KEYCODE_H) PORT_CHAR('h') PORT_CHAR('H')
		PORT_BIT(0x0001, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("n N") PORT_CODE(KEYCODE_N) PORT_CHAR('n') PORT_CHAR('N')
				/* col 5 */
		PORT_BIT(0x8000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("z Z \\") PORT_CODE(KEYCODE_Z) PORT_CHAR('z') PORT_CHAR('Z') PORT_CHAR('\\')
		PORT_BIT(0x4000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("q Q") PORT_CODE(KEYCODE_Q) PORT_CHAR('q') PORT_CHAR('Q')
		PORT_BIT(0x2000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("a A |") PORT_CODE(KEYCODE_A) PORT_CHAR('a') PORT_CHAR('A') PORT_CHAR('|')
		PORT_BIT(0x1000, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("1 ! DEL") PORT_CODE(KEYCODE_1) PORT_CHAR('1') PORT_CHAR('!')
		PORT_BIT(0x0800, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("0 )") PORT_CODE(KEYCODE_0) PORT_CHAR('0') PORT_CHAR(')')
		PORT_BIT(0x0400, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("p P \"") PORT_CODE(KEYCODE_P) PORT_CHAR('p') PORT_CHAR('P') PORT_CHAR('\"')
		PORT_BIT(0x0200, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("; :") PORT_CODE(KEYCODE_COLON) PORT_CHAR(';') PORT_CHAR(':')
		PORT_BIT(0x0100, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("/ -") PORT_CODE(KEYCODE_SLASH) PORT_CHAR('/') PORT_CHAR('-')

	PORT_START	/* col 6: "wired handset 1" (= joystick 1) */
		PORT_BIT(0x00E0, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x0010, IP_ACTIVE_LOW, IPT_JOYSTICK_UP/*, "(1UP)", CODE_NONE, OSD_JOY_UP*/) PORT_PLAYER(1)
		PORT_BIT(0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN/*, "(1DOWN)", CODE_NONE, OSD_JOY_DOWN, 0*/) PORT_PLAYER(1)
		PORT_BIT(0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT/*, "(1RIGHT)", CODE_NONE, OSD_JOY_RIGHT, 0*/) PORT_PLAYER(1)
		PORT_BIT(0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT/*, "(1LEFT)", CODE_NONE, OSD_JOY_LEFT, 0*/) PORT_PLAYER(1)
		PORT_BIT(0x0001, IP_ACTIVE_LOW, IPT_BUTTON1/*, "(1FIRE)", CODE_NONE, OSD_JOY_FIRE, 0*/) PORT_PLAYER(1)
			/* col 7: "wired handset 2" (= joystick 2) */
		PORT_BIT(0xE000, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x1000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP/*, "(2UP)", CODE_NONE, OSD_JOY2_UP, 0*/) PORT_PLAYER(2)
		PORT_BIT(0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN/*, "(2DOWN)", CODE_NONE, OSD_JOY2_DOWN, 0*/) PORT_PLAYER(2)
		PORT_BIT(0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT/*, "(2RIGHT)", CODE_NONE, OSD_JOY2_RIGHT, 0*/) PORT_PLAYER(2)
		PORT_BIT(0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT/*, "(2LEFT)", CODE_NONE, OSD_JOY2_LEFT, 0*/) PORT_PLAYER(2)
		PORT_BIT(0x0100, IP_ACTIVE_LOW, IPT_BUTTON1/*, "(2FIRE)", CODE_NONE, OSD_JOY2_FIRE, 0*/) PORT_PLAYER(2)

	PORT_START	/* one more port for Alpha line */
		PORT_BIT(0x0010, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Alpha Lock") PORT_CODE(KEYCODE_CAPSLOCK) PORT_TOGGLE

INPUT_PORTS_END


static const struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ -1 }		/* end of array */
};

/*
	SN76496(?) sound chip parameters.
*/
static struct SN76496interface tms9919interface =
{
	1,				/* one sound chip */
	{ 3579545 },	/* clock speed. connected to the TMS9918A CPUCLK pin... */
	{ 75 }			/* Volume.  I don't know the best value. */
};

static struct TMS5220interface tms5220interface =
{
	680000L,					/* 640kHz -> 8kHz output */
	50,							/* Volume.  I don't know the best value. */
	NULL,						/* no IRQ callback */
#if 1
	spchroms_read,				/* speech ROM read handler */
	spchroms_load_address,		/* speech ROM load address handler */
	spchroms_read_and_branch	/* speech ROM read and branch handler */
#endif
};

/*
	we use a DAC to emulate "audio gate", even thought
	a) there was no DAC in an actual TI99
	b) this is a 2-level output (whereas a DAC provides a 256-level output...)
*/
static struct DACinterface aux_sound_intf =
{
	1,				/* total number of DACs */
	{
		20			/* volume for audio gate*/
	}
};

/*
	2 tape units
*/
static struct Wave_interface tape_input_intf =
{
	2,
	{
		20,			/* Volume for CS1 */
		20			/* Volume for CS2 */
	}
};



/*
	machine description.
*/
static MACHINE_DRIVER_START(ti99_4p_60hz)

	/* basic machine hardware */
	/* TMS9900 CPU @ 3.0 MHz */
	MDRV_CPU_ADD(TMS9900, 3000000)
	/*MDRV_CPU_FLAGS(0)*/
	/*MDRV_CPU_CONFIG(0)*/
	MDRV_CPU_PROGRAM_MAP(memmap, 0)
	MDRV_CPU_IO_MAP(readcru, writecru)
	MDRV_CPU_VBLANK_INT(ti99_4ev_hblank_interrupt, 263)	/* 262.5 in 60Hz, 312.5 in 50Hz */
	/*MDRV_CPU_PERIODIC_INT(func, rate)*/

	MDRV_FRAMES_PER_SECOND(60)	/* or 50Hz */
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)
	/*MDRV_INTERLEAVE(interleave)*/

	MDRV_MACHINE_INIT( ti99 )
	MDRV_MACHINE_STOP( ti99 )
	/*MDRV_NVRAM_HANDLER( NULL )*/

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	/*MDRV_ASPECT_RATIO(num, den)*/
	MDRV_SCREEN_SIZE(512+32, (212+16)*2)
	MDRV_VISIBLE_AREA(0, 512+32 - 1, 0, (212+16)*2 - 1)

	/*MDRV_GFXDECODE(NULL)*/
	MDRV_PALETTE_LENGTH(512)
	MDRV_COLORTABLE_LENGTH(512)

	MDRV_PALETTE_INIT(v9938)
	MDRV_VIDEO_START(ti99_4ev)
	/*MDRV_VIDEO_EOF(name)*/
	MDRV_VIDEO_UPDATE(v9938)

	MDRV_SOUND_ATTRIBUTES(0)
	MDRV_SOUND_ADD(SN76496, tms9919interface)
	MDRV_SOUND_ADD(TMS5220, tms5220interface)
	MDRV_SOUND_ADD(DAC, aux_sound_intf)
	MDRV_SOUND_ADD(WAVE, tape_input_intf)

MACHINE_DRIVER_END


ROM_START(ti99_4p)
	/*CPU memory space*/
	ROM_REGION16_BE(region_cpu1_len_4p, REGION_CPU1, 0)
	ROM_LOAD16_BYTE("sgcpu_hb.bin", 0x0000, 0x8000, CRC(aa100730)) /* system ROMs */
	ROM_LOAD16_BYTE("sgcpu_lb.bin", 0x0001, 0x8000, CRC(2a5dc818)) /* system ROMs */

	/*DSR ROM space*/
	ROM_REGION(region_dsr_len, region_dsr, 0)
	ROM_LOAD_OPTIONAL("disk.bin", offset_fdc_dsr, 0x2000, CRC(8f7df93f)) /* TI disk DSR ROM */
#if HAS_99CCFDC
	ROM_LOAD_OPTIONAL("ccfdc.bin", offset_ccfdc_dsr, 0x4000, BAD_DUMP CRC(f69cc69d)) /* CorComp disk DSR ROM */
#endif
	ROM_LOAD_OPTIONAL("bwg.bin", offset_bwg_dsr, 0x8000, CRC(06f1ec89)) /* BwG disk DSR ROM */
	ROM_LOAD_OPTIONAL("hfdc.bin", offset_hfdc_dsr, 0x4000, CRC(66fbe0ed)) /* HFDC disk DSR ROM */
	ROM_LOAD_OPTIONAL("rs232.bin", offset_rs232_dsr, 0x1000, CRC(eab382fb)) /* TI rs232 DSR ROM */
	ROM_LOAD("evpcdsr.bin", offset_evpc_dsr, 0x10000, CRC(a062b75d)) /* evpc DSR ROM */

	/* HSGPL memory space */
	ROM_REGION(region_hsgpl_len, region_hsgpl, 0)

	/*TMS5220 ROM space*/
	ROM_REGION(0x8000, region_speech_rom, 0)
	ROM_LOAD("spchrom.bin", 0x0000, 0x8000, CRC(58b155f7)) /* system speech ROM */
ROM_END

SYSTEM_CONFIG_START(ti99_4p)
	CONFIG_DEVICE_CASSETTE			(2, NULL)
	CONFIG_DEVICE_FLOPPY			(4,	ti99)
	CONFIG_DEVICE_LEGACY			(IO_HARDDISK, 	4, "hd\0",	DEVICE_LOAD_RESETS_NONE,	OSD_FOPEN_RW_OR_READ,device_init_ti99_hd, NULL, device_load_ti99_hd, device_unload_ti99_hd, NULL)
	CONFIG_DEVICE_LEGACY			(IO_PARALLEL,	1, "",	DEVICE_LOAD_RESETS_NONE,	OSD_FOPEN_RW_CREATE_OR_READ,	NULL,	NULL,	device_load_ti99_4_pio,	device_unload_ti99_4_pio,		NULL)
	CONFIG_DEVICE_LEGACY			(IO_SERIAL,		1, "",	DEVICE_LOAD_RESETS_NONE,	OSD_FOPEN_RW_CREATE_OR_READ,	NULL,	NULL,	device_load_ti99_4_rs232,	device_unload_ti99_4_rs232,	NULL)
	/*CONFIG_DEVICE_LEGACY			(IO_QUICKLOAD,	1, "\0",	DEVICE_LOAD_RESETS_CPU,		OSD_FOPEN_RW_CREATE_OR_READ,	NULL,	NULL,	device_load_ti99_hsgpl,		device_unload_ti99_hsgpl,	NULL)*/
	CONFIG_DEVICE_LEGACY			(IO_MEMCARD,	1, "",	DEVICE_LOAD_RESETS_NONE,	OSD_FOPEN_RW_OR_READ,	device_init_smartmedia,	NULL,	device_load_smartmedia,	device_unload_smartmedia,	NULL)
SYSTEM_CONFIG_END

/*	  YEAR	NAME	  PARENT   COMPAT	MACHINE		�INPUT	  INIT	   CONFIG	COMPANY		FULLNAME */
COMP( 1996, ti99_4p,  0,	   0,		ti99_4p_60hz, ti99_4p, ti99_4p, ti99_4p,"snug",		"SGCPU (a.k.a. 99/4P)" )
