/* m6847.c -- Implementation of Motorola 6847 video hardware chip
 *
 * Nate Woods
 *
 * Originally based on src/mess/vidhrdw/dragon.c by Mathis Rosenhauer
 */

#include "m6847.h"
#include "vidhrdw/generic.h"
#include "includes/rstrbits.h"

/* The "Back doors" are declared here */
#include "includes/dragon.h"

struct m6847_state {
	struct m6847_init_params initparams;
	int modebits;
	int videooffset;
	int rowsize;
};

static struct m6847_state the_state;

enum {
	M6847_MODEBIT_AG		= 0x80,
	M6847_MODEBIT_AS		= 0x40,
	M6847_MODEBIT_INTEXT	= 0x20,
	M6847_MODEBIT_INV		= 0x10,
	M6847_MODEBIT_CSS		= 0x08,
	M6847_MODEBIT_GM2		= 0x04,
	M6847_MODEBIT_GM1		= 0x02,
	M6847_MODEBIT_GM0		= 0x01
};

#define MAX_VRAM 6144

#define LOG_M6847 0

static unsigned char palette[] = {
	0x00,0x00,0x00, /* BLACK */
	0x00,0xff,0x00, /* GREEN */
	0xff,0xff,0x00, /* YELLOW */
	0x00,0x00,0xff, /* BLUE */
	0xff,0x00,0x00, /* RED */
	0xff,0xff,0xff, /* BUFF */
	0x00,0xff,0xff, /* CYAN */
	0xff,0x00,0xff, /* MAGENTA */
	0xff,0x80,0x00, /* ORANGE */
	0x00,0x80,0x00, /* ARTIFACT GREEN/RED */
	0x00,0x80,0x00, /* ARTIFACT GREEN/BLUE */
	0xff,0x80,0x00, /* ARTIFACT BUFF/RED */
	0x00,0x80,0xff, /* ARTIFACT BUFF/BLUE */
	0x00,0x40,0x00,	/* ALPHANUMERIC DARK GREEN */
	0x00,0xff,0x00,	/* ALPHANUMERIC BRIGHT GREEN */
	0x40,0x10,0x00,	/* ALPHANUMERIC DARK ORANGE */
	0xff,0xc4,0x18,	/* ALPHANUMERIC BRIGHT ORANGE */
};

static unsigned char fontdata8x12[] =
{
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x02, 0x1a, 0x2a, 0x2a, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x14, 0x22, 0x22, 0x3e, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3c, 0x12, 0x12, 0x1c, 0x12, 0x12, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x20, 0x20, 0x20, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3c, 0x12, 0x12, 0x12, 0x12, 0x12, 0x3c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x20, 0x20, 0x3c, 0x20, 0x20, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x20, 0x20, 0x3c, 0x20, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1e, 0x20, 0x20, 0x26, 0x22, 0x22, 0x1e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x3e, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x22, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x24, 0x28, 0x30, 0x28, 0x24, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x36, 0x2a, 0x2a, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x32, 0x2a, 0x26, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3c, 0x22, 0x22, 0x3c, 0x20, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x2a, 0x24, 0x1a, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3c, 0x22, 0x22, 0x3c, 0x28, 0x24, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x10, 0x08, 0x04, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x2a, 0x2a, 0x36, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x14, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x20, 0x20, 0x10, 0x08, 0x04, 0x02, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x1c, 0x2a, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x3e, 0x10, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x14, 0x14, 0x36, 0x00, 0x36, 0x14, 0x14, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x1e, 0x20, 0x1c, 0x02, 0x3c, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x32, 0x32, 0x04, 0x08, 0x10, 0x26, 0x26, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x28, 0x28, 0x10, 0x2a, 0x24, 0x1a, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x10, 0x20, 0x20, 0x20, 0x10, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3e, 0x08, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x10, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x24, 0x24, 0x24, 0x24, 0x24, 0x18, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x02, 0x1c, 0x20, 0x20, 0x3e, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x02, 0x0c, 0x02, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x0c, 0x14, 0x3e, 0x04, 0x04, 0x04, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x20, 0x3c, 0x02, 0x02, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x20, 0x20, 0x3c, 0x22, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3e, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x1c, 0x22, 0x22, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x1e, 0x02, 0x02, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x08, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x24, 0x04, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,

	/* Lower case */
	0x00, 0x00, 0x00, 0x0C, 0x12, 0x10, 0x38, 0x10, 0x12, 0x3C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x02, 0x1E, 0x22, 0x1E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x20, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x3C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x20, 0x20, 0x20, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x02, 0x1E, 0x22, 0x22, 0x22, 0x1E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x3E, 0x20, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0C, 0x12, 0x10, 0x38, 0x10, 0x10, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x22, 0x22, 0x22, 0x1E, 0x02, 0x1C,
	0x00, 0x00, 0x00, 0x20, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x00, 0x18, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x24, 0x18,
	0x00, 0x00, 0x00, 0x20, 0x20, 0x24, 0x28, 0x38, 0x24, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x2A, 0x2A, 0x2A, 0x2A, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0x32, 0x22, 0x22, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x22, 0x3C, 0x20, 0x20,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x22, 0x22, 0x22, 0x1E, 0x02, 0x03,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x2C, 0x32, 0x20, 0x20, 0x20, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x20, 0x1C, 0x02, 0x3C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x10, 0x3C, 0x10, 0x10, 0x10, 0x12, 0x0C, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x26, 0x1A, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x14, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x2A, 0x2A, 0x1C, 0x14, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x14, 0x08, 0x14, 0x22, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x1E, 0x02, 0x1C,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x04, 0x08, 0x10, 0x3E, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x10, 0x10, 0x20, 0x10, 0x10, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x00, 0x08, 0x08, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x04, 0x04, 0x02, 0x04, 0x04, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x2A, 0x1C, 0x08, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x04, 0x3E, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00,

	/* Semigraphics 6 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0,
	0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f,
	0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0,
	0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0,
	0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff,
	0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff,
	0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f,
	0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0,
	0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
	0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00,
	0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff,
	0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f,
	0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0,
	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
	0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0,
	0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f,
	0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

	/* Block Graphics (Semigraphics 4 Graphics ) */
	0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00, 0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
	0x00,0x00,0x00,0x00,0x00,0x00, 0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0x00,0x00,0x00,0x00,0x00,0x00, 0xff,0xff,0xff,0xff,0xff,0xff,
	0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0x00,0x00,0x00,0x00,0x00,0x00,
	0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
	0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0xff,0xff,0xff,0xff,0xff,0xff,
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0, 0x00,0x00,0x00,0x00,0x00,0x00,
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0, 0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0, 0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0xf0,0xf0,0xf0,0xf0,0xf0,0xf0, 0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff, 0x00,0x00,0x00,0x00,0x00,0x00,
	0xff,0xff,0xff,0xff,0xff,0xff, 0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
	0xff,0xff,0xff,0xff,0xff,0xff, 0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
	0xff,0xff,0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,0xff,0xff
};

/* --------------------------------------------------
 * Initialization and termination
 * -------------------------------------------------- */

void m6847_vh_init_palette(unsigned char *sys_palette, unsigned short *sys_colortable,const unsigned char *color_prom)
{
	memcpy(sys_palette,palette,sizeof(palette));
}

int internal_m6847_vh_start(const struct m6847_init_params *params, int dirtyramsize)
{
	the_state.initparams = *params;
	the_state.modebits = 0;
	the_state.videooffset = 0;
	the_state.rowsize = 12;

	videoram_size = dirtyramsize;
	if (generic_vh_start())
		return 1;

	return 0;
}

int m6847_vh_start(const struct m6847_init_params *params)
{
	return internal_m6847_vh_start(params, MAX_VRAM);
}

void m6847_set_ram_size(int ramsize)
{
	the_state.initparams.ramsize = ramsize;
}

static UINT8 *mapper_alphanumeric(UINT8 *mem, int param, int *fg, int *bg, int *attr)
{
	UINT8 b;
	UINT8 *character;
	int bgc, fgc;
	int lowercase_hack = 0;

	b = *mem;

	/* Give the host machine a chance to pull our strings */
	the_state.initparams.charproc(b);

	/* HACKHACK - Need to know more about the M6847T1 to be accurate! */
	if (the_state.initparams.version == M6847_VERSION_M6847T1) {
		if (the_state.modebits & M6847_MODEBIT_INTEXT) {
			if (b < 0x80)
				lowercase_hack = 1;
		}
	}

	if (!lowercase_hack && (the_state.modebits & M6847_MODEBIT_AS)) {
		/* Semigraphics */
		bgc = 8;

		if ((the_state.modebits & M6847_MODEBIT_INTEXT) && (the_state.initparams.version != M6847_VERSION_M6847T1)) {
			/* Semigraphics 6 */
			character = &fontdata8x12[(96 + (b & 0x3f)) * 12];
			fgc = ((b >> 6) & 0x3);
			if (the_state.modebits & M6847_MODEBIT_CSS)
				fgc += 4;
		}
		else {
			/* Semigraphics 4 */
			switch(b & 0x0f) {
			case 0:
				bgc = 8;
				character = NULL;
				break;
			case 15:
				bgc = (b >> 4) & 0x7;
				character = NULL;
				break;
			default:
				bgc = 8;
				fgc = (b >> 4) & 0x7;
				character = &fontdata8x12[(160 + (b & 0x0f)) * 12];
				break;
			}
		}

	}
	else {
		/* Text */
		fgc = (the_state.modebits & M6847_MODEBIT_CSS) ? 15 : 13;

		/* Inverse the character, if appropriate */
		if (the_state.modebits & M6847_MODEBIT_INV)
			fgc ^= 1;

		if (lowercase_hack && (b < 0x20)) {
			b += 0x40;
			fgc ^= 1;
		}
		else {
			b &= 0x3f;
		}

		if (b == 0x20) {
			character = NULL;
		}
		else {
			character = &fontdata8x12[b * 12];
			if (param)
				character += 1;	/* Skew up */
		}
		bgc = fgc ^ 1;
	}

	*bg = bgc;
	*fg = fgc;
	return character;
}

/* This is a refresh function used by the Dragon/CoCo as well as the CoCo 3 when in lo-res
 * mode.  Internally, it treats the colors like a CoCo 3 and uses the 'metapalette' to
 * translate those colors to the proper palette.
 *
 * video_vmode
 *     bit 4	1=graphics, 0=text
 *     bit 3    resolution high bit
 *     bit 2    resolution low bit
 *     bit 1    1=b/w graphics, 0=color graphics
 *     bit 0	color set
 */
void internal_m6847_vh_screenrefresh(struct rasterbits_source *rs,
	struct rasterbits_videomode *rvm, struct rasterbits_frame *rf, int full_refresh,
	const int *metapalette, UINT8 *vrambase,
	int has_lowercase, int skew_up, int border_color, int wf, artifactproc artifact)
{
	static int rowheights[] = {
		12,		12,		12,		12,		12,		12,		12,		12,
		3,		3,		3,		2,		2,		1,		1,		1
	};

	rs->videoram = vrambase;
	rs->size = the_state.initparams.ramsize;
	rs->position = the_state.videooffset;
	rs->db = full_refresh ? NULL : dirtybuffer;
	rvm->height = 192 / the_state.rowsize;
	rvm->offset = 0;
	rf->width = 256 * wf;
	rf->height = 192;
	rf->total_scanlines = -1;
	rf->top_scanline = -1;
	rf->border_pen = (border_color == -1) ? -1 : Machine->pens[border_color];

	if (full_refresh) {
		/* Since we are not passing the dirty buffer to raster_bits(), we should clear it here */
		memset(dirtybuffer, 0, videoram_size);
	}

	if (the_state.modebits & M6847_MODEBIT_AG)
	{
		rvm->flags = RASTERBITS_FLAG_GRAPHICS;

		if (the_state.modebits & M6847_MODEBIT_GM0)
		{
			/* Resolution modes */
			rvm->bytesperrow = ((the_state.modebits & (M6847_MODEBIT_GM2|M6847_MODEBIT_GM1)) == (M6847_MODEBIT_GM2|M6847_MODEBIT_GM1)) ? 32 : 16;
			rvm->width = rvm->bytesperrow * 8;
			rvm->depth = 1;
			rvm->metapalette = &metapalette[the_state.modebits & M6847_MODEBIT_CSS ? 10 : 8];

			if (artifact && (rvm->bytesperrow == 32)) {
				/* I am here because we are doing PMODE 4 artifact colors */
				rvm->flags |= RASTERBITS_FLAG_ARTIFACT;
				rvm->u.artifact = artifact;
			}
		}
		else
		{
			/* Color modes */
			rvm->bytesperrow = ((the_state.modebits & (M6847_MODEBIT_GM2|M6847_MODEBIT_GM1)) != 0) ? 32 : 16;
			rvm->width = rvm->bytesperrow * 4;
			rvm->depth = 2;
			rvm->metapalette = &metapalette[the_state.modebits & M6847_MODEBIT_CSS ? 4: 0];
		}
	}
	else
	{
		rvm->flags = RASTERBITS_FLAG_TEXT | RASTERBITS_FLAG_TEXTMODULO;
		rvm->bytesperrow = 32;
		rvm->width = 32;
		rvm->depth = 8;
		rvm->metapalette = metapalette;
		rvm->u.text.mapper = mapper_alphanumeric;
		rvm->u.text.mapper_param = skew_up;
		rvm->u.text.fontheight = 12;
		rvm->u.text.underlinepos = -1;
	}
}


static void m6847_artifact_red(int *artifactcolors)
{
	switch(artifactcolors[3]) {
	case 1:
		artifactcolors[2] = 10;
		artifactcolors[1] = 9;
		break;
	case 5:
		artifactcolors[2] = 12;
		artifactcolors[1] = 11;
		break;
	}
}

static void m6847_artifact_blue(int *artifactcolors)
{
	switch(artifactcolors[3]) {
	case 1:
		artifactcolors[1] = 10;
		artifactcolors[2] = 9;
		break;
	case 5:
		artifactcolors[1] = 12;
		artifactcolors[2] = 11;
		break;
	}
}

int m6847_get_bordercolor(void)
{
	int bordercolor;

	if (the_state.modebits & M6847_MODEBIT_AG) {
		if (the_state.modebits & M6847_MODEBIT_CSS)
			bordercolor = M6847_BORDERCOLOR_WHITE;
		else
			bordercolor = M6847_BORDERCOLOR_GREEN;
	}
	else {
		if ((the_state.initparams.version == M6847_VERSION_M6847T1)
				&& ((the_state.modebits & (M6847_MODEBIT_GM2|M6847_MODEBIT_GM1)) == M6847_MODEBIT_GM2)) {
			/* We are on the new VDG; and we have a colored border */
			if (the_state.modebits & M6847_MODEBIT_CSS)
				bordercolor = M6847_BORDERCOLOR_ORANGE;
			else
				bordercolor = M6847_BORDERCOLOR_GREEN;
		}
		else {
			bordercolor = M6847_BORDERCOLOR_BLACK;
		}
	}
	return bordercolor;
}

static m6847_bordercolor(void)
{
	int pen = 0;

	switch(m6847_get_bordercolor()) {
	case M6847_BORDERCOLOR_BLACK:
		pen = 0;
		break;
	case M6847_BORDERCOLOR_GREEN:
		pen = 1;
		break;
	case M6847_BORDERCOLOR_WHITE:
		pen = 5;
		break;
	case M6847_BORDERCOLOR_ORANGE:
		pen = 16;
		break;
	}
	return pen;
}

void m6847_vh_update(struct osd_bitmap *bitmap,int full_refresh)
{
	static int m6847_metapalette[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 0, 5, 13, 14, 15, 16
	};
	static artifactproc artifacts[] = {
		NULL,
		m6847_artifact_red,
		m6847_artifact_blue
	};
	struct rasterbits_source rs;
	struct rasterbits_videomode rvm;
	struct rasterbits_frame rf;
	int artifact_value;

	artifact_value = (the_state.initparams.artifactdipswitch == -1) ? 0 : readinputport(the_state.initparams.artifactdipswitch);

	internal_m6847_vh_screenrefresh(&rs, &rvm, &rf,
		full_refresh, m6847_metapalette, the_state.initparams.ram,
		(the_state.initparams.version == M6847_VERSION_M6847T1), 0,
		(full_refresh ? m6847_bordercolor() : -1),
		1, artifacts[artifact_value & 3]);

	raster_bits(bitmap, &rs, &rvm, &rf, NULL);
}

/* --------------------------------------------------
 * Petty accessors
 * -------------------------------------------------- */

void m6847_set_video_offset(int offset)
{
#if LOG_M6847
	logerror("m6847_set_video_offset(): offset=$%04x\n", offset);
#endif

	offset %= the_state.initparams.ramsize;
	if (offset != the_state.videooffset) {
		the_state.videooffset = offset;
		schedule_full_refresh();
	}
}

int m6847_get_video_offset(void)
{
	return the_state.videooffset;
}

void m6847_touch_vram(int offset)
{
	offset -= the_state.videooffset;
	offset %= the_state.initparams.ramsize;

	if (offset < videoram_size)
		dirtybuffer[offset] = 1;
}

void m6847_set_row_height(int rowheight)
{
	if (rowheight != the_state.rowsize) {
		the_state.rowsize = rowheight;
		schedule_full_refresh();
	}
}

void m6847_set_cannonical_row_height(void)
{
	static const int graphics_rowheights[] = { 3, 3, 3, 2, 2, 1, 1, 1 };
	int rowheight;

	if (the_state.modebits & M6847_MODEBIT_AG) {
		rowheight = graphics_rowheights[the_state.modebits & (M6847_MODEBIT_GM2|M6847_MODEBIT_GM1|M6847_MODEBIT_GM0)];
	}
	else {
		rowheight = 12;
	}
	m6847_set_row_height(rowheight);
}

READ_HANDLER( m6847_ag_r )		{ return (the_state.modebits & M6847_MODEBIT_AG) ? 1 : 0; }
READ_HANDLER( m6847_as_r )		{ return (the_state.modebits & M6847_MODEBIT_AS) ? 1 : 0; }
READ_HANDLER( m6847_intext_r )	{ return (the_state.modebits & M6847_MODEBIT_INTEXT) ? 1 : 0; }
READ_HANDLER( m6847_inv_r )		{ return (the_state.modebits & M6847_MODEBIT_INV) ? 1 : 0; }
READ_HANDLER( m6847_css_r )		{ return (the_state.modebits & M6847_MODEBIT_CSS) ? 1 : 0; }
READ_HANDLER( m6847_gm2_r )		{ return (the_state.modebits & M6847_MODEBIT_GM2) ? 1 : 0; }
READ_HANDLER( m6847_gm1_r )		{ return (the_state.modebits & M6847_MODEBIT_GM1) ? 1 : 0; }
READ_HANDLER( m6847_gm0_r )		{ return (the_state.modebits & M6847_MODEBIT_GM0) ? 1 : 0; }

static void write_modebits(int data, int mask, int causesrefresh)
{
	int newmodebits;

	if (data)
		newmodebits = the_state.modebits | mask;
	else
		newmodebits = the_state.modebits & ~mask;

	if (newmodebits != the_state.modebits) {
		the_state.modebits = newmodebits;
		if (causesrefresh)
			schedule_full_refresh();
	}
}

WRITE_HANDLER( m6847_ag_w )		{ write_modebits(data, M6847_MODEBIT_AG, 1); }
WRITE_HANDLER( m6847_as_w )		{ write_modebits(data, M6847_MODEBIT_AS, 0); }
WRITE_HANDLER( m6847_intext_w )	{ write_modebits(data, M6847_MODEBIT_INTEXT, 0); }
WRITE_HANDLER( m6847_inv_w )	{ write_modebits(data, M6847_MODEBIT_INV, 0); }
WRITE_HANDLER( m6847_css_w )	{ write_modebits(data, M6847_MODEBIT_CSS, 1); }
WRITE_HANDLER( m6847_gm2_w )	{ write_modebits(data, M6847_MODEBIT_GM2, 1); }
WRITE_HANDLER( m6847_gm1_w )	{ write_modebits(data, M6847_MODEBIT_GM1, 1); }
WRITE_HANDLER( m6847_gm0_w )	{ write_modebits(data, M6847_MODEBIT_GM0, 1); }
