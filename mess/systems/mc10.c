#include "driver.h"
#include "cpu/m6800/m6800.h"
#include "vidhrdw/m6847.h"
#include "includes/mc10.h"
#include "devices/cassette.h"
#include "includes/dragon.h"		/* for coco_cassette_init() */
#include "inputx.h"


static MEMORY_READ_START( mc10_readmem )
	{ 0x0000, 0x001f, m6803_internal_registers_r },
	{ 0x0020, 0x007f, MRA_NOP }, /* unused */
	{ 0x0080, 0x00ff, MRA_RAM }, /* 6803 internal RAM */
	{ 0x0100, 0x3fff, MRA_NOP }, /* unused */
	{ 0x4000, 0x4fff, MRA_RAM },
//	{ 0x5000, 0xbffe, MRA_RAM }, /* expansion RAM */
	{ 0xbfff, 0xbfff, mc10_bfff_r },
//	{ 0xc000, 0xdfff, MWA_ROM }, /* expansion ROM */
	{ 0xe000, 0xffff, MRA_ROM }, /* ROM */
MEMORY_END

static MEMORY_WRITE_START( mc10_writemem )
	{ 0x0000, 0x001f, m6803_internal_registers_w },
	{ 0x0020, 0x007f, MWA_NOP }, /* unused */
	{ 0x0080, 0x00ff, MWA_RAM }, /* 6803 internal RAM */
	{ 0x0100, 0x3fff, MWA_NOP }, /* unused */
	{ 0x4000, 0x4fff, mc10_ram_w },
//	{ 0x5000, 0xbffe, MWA_RAM }, /* expansion RAM */
	{ 0xbfff, 0xbfff, mc10_bfff_w },
//	{ 0xc000, 0xdfff, MWA_ROM }, /* expansion ROM */
	{ 0xe000, 0xffff, MWA_ROM }, /* ROM */
MEMORY_END

static PORT_READ_START( mc10_readport )
	{ M6803_PORT1, M6803_PORT1, mc10_port1_r },
	{ M6803_PORT2, M6803_PORT2, mc10_port2_r },
PORT_END

static PORT_WRITE_START( mc10_writeport )
	{ M6803_PORT1, M6803_PORT1, mc10_port1_w },
	{ M6803_PORT2, M6803_PORT2, mc10_port2_w },
PORT_END

/* MC-10 keyboard

	   PB0 PB1 PB2 PB3 PB4 PB5 PB6 PB7
  PA6: Ctl N/c Brk N/c N/c N/c N/c Shift
  PA5: 8   9   :   ;   ,   -   .   /
  PA4: 0   1   2   3   4   5   6   7
  PA3: X   Y   Z   N/c N/c N/c Ent Space
  PA2: P   Q   R   S   T   U   V   W
  PA1: H   I   J   K   L   M   N   O
  PA0: @   A   B   C   D   E   F   G
 */
INPUT_PORTS_START( mc10 )
	PORT_START /* KEY ROW 0 */
	PORT_KEY1(0x01, IP_ACTIVE_LOW, "@", KEYCODE_ASTERISK, CODE_NONE,		'@')
	PORT_KEY1(0x02, IP_ACTIVE_LOW, "A", KEYCODE_A, CODE_NONE,				'A')
	PORT_KEY1(0x04, IP_ACTIVE_LOW, "B", KEYCODE_B, CODE_NONE,				'B')
	PORT_KEY1(0x08, IP_ACTIVE_LOW, "C", KEYCODE_C, CODE_NONE,				'C')
	PORT_KEY1(0x10, IP_ACTIVE_LOW, "D", KEYCODE_D, CODE_NONE,				'D')
	PORT_KEY1(0x20, IP_ACTIVE_LOW, "E", KEYCODE_E, CODE_NONE,				'E')
	PORT_KEY1(0x40, IP_ACTIVE_LOW, "F", KEYCODE_F, CODE_NONE,				'F')
	PORT_KEY1(0x80, IP_ACTIVE_LOW, "G", KEYCODE_G, CODE_NONE,				'G')

	PORT_START /* KEY ROW 1 */
	PORT_KEY1(0x01, IP_ACTIVE_LOW, "H", KEYCODE_H, CODE_NONE,				'H')
	PORT_KEY1(0x02, IP_ACTIVE_LOW, "I", KEYCODE_I, CODE_NONE,				'I')
	PORT_KEY1(0x04, IP_ACTIVE_LOW, "J", KEYCODE_J, CODE_NONE,				'J')
	PORT_KEY1(0x08, IP_ACTIVE_LOW, "K", KEYCODE_K, CODE_NONE,				'K')
	PORT_KEY1(0x10, IP_ACTIVE_LOW, "L", KEYCODE_L, CODE_NONE,				'L')
	PORT_KEY1(0x20, IP_ACTIVE_LOW, "M", KEYCODE_M, CODE_NONE,				'M')
	PORT_KEY1(0x40, IP_ACTIVE_LOW, "N", KEYCODE_N, CODE_NONE,				'N')
	PORT_KEY1(0x80, IP_ACTIVE_LOW, "O", KEYCODE_O, CODE_NONE,				'O')

	PORT_START /* KEY ROW 2 */
	PORT_KEY1(0x01, IP_ACTIVE_LOW, "P", KEYCODE_P, CODE_NONE,				'P')
	PORT_KEY1(0x02, IP_ACTIVE_LOW, "Q", KEYCODE_Q, CODE_NONE,				'Q')
	PORT_KEY1(0x04, IP_ACTIVE_LOW, "R", KEYCODE_R, CODE_NONE,				'R')
	PORT_KEY1(0x08, IP_ACTIVE_LOW, "S", KEYCODE_S, CODE_NONE,				'S')
	PORT_KEY1(0x10, IP_ACTIVE_LOW, "T", KEYCODE_T, CODE_NONE,				'T')
	PORT_KEY1(0x20, IP_ACTIVE_LOW, "U", KEYCODE_U, CODE_NONE,				'U')
	PORT_KEY1(0x40, IP_ACTIVE_LOW, "V", KEYCODE_V, CODE_NONE,				'V')
	PORT_KEY1(0x80, IP_ACTIVE_LOW, "W", KEYCODE_W, CODE_NONE,				'W')

	PORT_START /* KEY ROW 3 */
	PORT_KEY1(0x01, IP_ACTIVE_LOW, "X", KEYCODE_X, CODE_NONE,				'X')
	PORT_KEY1(0x02, IP_ACTIVE_LOW, "Y", KEYCODE_Y, CODE_NONE,				'Y')
	PORT_KEY1(0x04, IP_ACTIVE_LOW, "Z", KEYCODE_Z, CODE_NONE,				'Z')
	PORT_BITX(0x38, IP_ACTIVE_LOW, IPT_UNUSED, DEF_STR( Unused ), IP_KEY_NONE, IP_JOY_NONE)
	PORT_KEY1(0x40, IP_ACTIVE_LOW, "ENTER", KEYCODE_ENTER, IP_JOY_NONE,		13)
	PORT_KEY1(0x80, IP_ACTIVE_LOW, "SPACE", KEYCODE_SPACE, IP_JOY_NONE,		10)

	PORT_START /* KEY ROW 4 */
	PORT_KEY1(0x01, IP_ACTIVE_LOW, "0   ", KEYCODE_0, CODE_NONE,			'0')
	PORT_KEY2(0x02, IP_ACTIVE_LOW, "1  !", KEYCODE_1, CODE_NONE,			'1',	'!')
	PORT_KEY2(0x04, IP_ACTIVE_LOW, "2  \"", KEYCODE_2, CODE_NONE,			'2',	'\"')
	PORT_KEY2(0x08, IP_ACTIVE_LOW, "3  #", KEYCODE_3, CODE_NONE,			'3',	'#')
	PORT_KEY2(0x10, IP_ACTIVE_LOW, "4  $", KEYCODE_4, CODE_NONE,			'4',	'$')
	PORT_KEY2(0x20, IP_ACTIVE_LOW, "5  %", KEYCODE_5, CODE_NONE,			'5',	'%')
	PORT_KEY2(0x40, IP_ACTIVE_LOW, "6  &", KEYCODE_6, CODE_NONE,			'6',	'&')
	PORT_KEY2(0x80, IP_ACTIVE_LOW, "7  '", KEYCODE_7, CODE_NONE,			'7',	'\'')

	PORT_START /* KEY ROW 5 */
	PORT_KEY2(0x01, IP_ACTIVE_LOW, "8  (", KEYCODE_8, CODE_NONE,			'8',	'(')
	PORT_KEY2(0x02, IP_ACTIVE_LOW, "9  )", KEYCODE_9, CODE_NONE,			'9',	')')
	PORT_KEY2(0x04, IP_ACTIVE_LOW, ":  *", KEYCODE_COLON, CODE_NONE,		':',	'*')
	PORT_KEY2(0x08, IP_ACTIVE_LOW, ";  +", KEYCODE_QUOTE, CODE_NONE,		';',	'+')
	PORT_KEY2(0x10, IP_ACTIVE_LOW, ",  <", KEYCODE_COMMA, CODE_NONE,		',',	'<')
	PORT_KEY2(0x20, IP_ACTIVE_LOW, "-  =", KEYCODE_MINUS, CODE_NONE,		'-',	'=')
	PORT_KEY2(0x40, IP_ACTIVE_LOW, ".  >", KEYCODE_STOP, CODE_NONE,			'.',	'>')
	PORT_KEY2(0x80, IP_ACTIVE_LOW, "/  ?", KEYCODE_SLASH, CODE_NONE,		'/',	'?')

	PORT_START /* KEY ROW 6 */
	PORT_KEY0(0x01, IP_ACTIVE_LOW, "CONTROL", KEYCODE_LCONTROL, IP_JOY_NONE)
	PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_UNUSED, DEF_STR( Unused ), IP_KEY_NONE, IP_JOY_NONE)
	PORT_KEY1(0x04, IP_ACTIVE_LOW, "BREAK", KEYCODE_END, KEYCODE_ESC,		27)
	PORT_BITX(0x78, IP_ACTIVE_LOW, IPT_UNUSED, DEF_STR( Unused ), IP_KEY_NONE, IP_JOY_NONE)
	PORT_KEY1(0x80, IP_ACTIVE_LOW, "SHIFT", KEYCODE_LSHIFT, IP_JOY_NONE,	UCHAR_SHIFT_1)

	PORT_START /* 7 */
	PORT_DIPNAME( 0x80, 0x00, "16K RAM module" )
	PORT_DIPSETTING(	0x00, DEF_STR( No ))
	PORT_DIPSETTING(	0x80, DEF_STR( Yes ))
	PORT_DIPNAME( 0x40, 0x00, "DOS extension" )
	PORT_DIPSETTING(	0x00, DEF_STR( No ))
	PORT_DIPSETTING(	0x40, DEF_STR( Yes ))
	PORT_BIT(	  0x3c, 0x3c, IPT_UNUSED )
	PORT_DIPNAME( 0x03, 0x01, "Artifacting" )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x01, "Red" )
	PORT_DIPSETTING(    0x02, "Blue" )

INPUT_PORTS_END

static struct DACinterface mc10_dac_interface =
{
	1,
	{ 100 }
};

static MACHINE_DRIVER_START( mc10 )
	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M6803, 894886)        /* 0,894886 Mhz */
	MDRV_CPU_MEMORY(mc10_readmem, mc10_writemem)
	MDRV_CPU_PORTS(mc10_readport, mc10_writeport)
	MDRV_CPU_VBLANK_INT(m6847_vh_interrupt, M6847_INTERRUPTS_PER_FRAME)
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(0)

	/* video hardware */
	MDRV_M6847_NTSC( mc10 )

	/* sound hardware */
	MDRV_SOUND_ADD(DAC, mc10_dac_interface)
MACHINE_DRIVER_END

ROM_START(mc10)
	ROM_REGION(0x10000,REGION_CPU1,0)
	ROM_LOAD("mc10.rom", 0xE000, 0x2000,CRC( 0x11fda97e))
ROM_END

SYSTEM_CONFIG_START(mc10)
	CONFIG_DEVICE_CASSETTE(1, "cas\0", coco_cassette_init)
SYSTEM_CONFIG_END

/*	  YEAR	NAME	  PARENT	COMPAT	MACHINE   INPUT 	INIT	  CONFIG   COMPANY               FULLNAME */
COMP( 1983, mc10,     0,		0,		mc10,     mc10,     0,        mc10,    "Tandy Radio Shack",  "MC-10" )

