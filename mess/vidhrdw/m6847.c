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

struct m6847_state the_state;
static m6847_vblank_proc vblankproc;
static int artifact_dipswitch;

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
	0xff,0xff,0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,0xff,0xff,

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
	0x00, 0x00, 0x00, 0x08, 0x04, 0x3E, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00
};

static void calc_videoram_size(void)
{
	static int videoramsizes[] = {
		512,	512,	512,	512,	512,	512,	512,	512,
		1024,	1024,	2048,	1536,	3072,	3072,	6144,	6144
	};
	videoram_size = videoramsizes[the_state.video_vmode >> 1];
}

/* --------------------------------------------------
 * Initialization and termination
 * -------------------------------------------------- */

void m6847_vh_init_palette(unsigned char *sys_palette, unsigned short *sys_colortable,const unsigned char *color_prom)
{
	memcpy(sys_palette,palette,sizeof(palette));
}

int internal_m6847_vh_start(int maxvram)
{
	the_state.vram_mask = 0;
	the_state.video_offset = 0;
	the_state.video_gmode = 0;
	the_state.video_vmode = 0;
	vblankproc = NULL;
	artifact_dipswitch = -1;

	calc_videoram_size();

	videoram_size = maxvram;
	if (generic_vh_start())
		return 1;

	return 0;
}

int m6847_vh_start(void)
{
	return internal_m6847_vh_start(MAX_VRAM);
}

void m6847_set_vram(void *ram, int rammask)
{
	videoram = ram;
	the_state.vram_mask = rammask;
}

void m6847_set_vram_mask(int rammask)
{
	the_state.vram_mask = rammask;
}

void m6847_set_vblank_proc(m6847_vblank_proc proc)
{
	vblankproc = proc;
}

void m6847_set_artifact_dipswitch(int sw)
{
	artifact_dipswitch = sw;
}

static UINT8 *mapper_semigraphics6(UINT8 *mem, int param, int *fg, int *bg, int *attr)
{
	UINT8 b;

	b = *mem;
	*bg = 8;
	*fg = ((b >> 6) & 0x3) + param;
	return &fontdata8x12[(64 + (b & 0x3f)) * 12];
}

static UINT8 *mapper_text(UINT8 *mem, int param, int *fg, int *bg, int *attr)
{
	int fgc = 0, bgc = 0;
	int b;
	UINT8 *result;

	b = *mem;

	if (b >= 0x80) {
		/* Semigraphics 4 */
		switch(b & 0x0f) {
		case 0:
			bgc = 8;
			result = NULL;
			break;
		case 15:
			bgc = (b >> 4) & 0x7;
			result = NULL;
			break;
		default:
			bgc = 8;
			fgc = (b >> 4) & 0x7;
			result = &fontdata8x12[(128 + (b & 0x0f)) * 12];
			break;
		}
	}
	else {
		/* Text */
		bgc = (param & 0x01) ? 14 : 12;

		/* On the M6847T1 and the CoCo 3 GIME chip, bit 2 of video_gmode
		 * reversed the colors
		 *
		 * TODO: Find out what the normal M6847 did with bit 2
		 */
		if (param & 0x04)
			bgc ^= 1;

		/* Is this character lowercase or inverse? */
		if ((param & 0x02) && (b < 0x20)) {
			/* This character is lowercase */
			b += 144;
		}
		else if (b < 0x40) {
			/* This character is inverse */
			bgc ^= 1;
		}

		fgc = bgc;
		bgc ^= 1;
		if ((b & 0x3f) == 0x20)
			result = NULL;
		else
			result = &fontdata8x12[(b & 0xbf) * 12];
	}

	*fg = fgc;
	*bg = bgc;
	return result;
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
void internal_m6847_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh,
	const int *metapalette, UINT8 *vrambase, struct m6847_state *currentstate,
	int has_lowercase, int border_color, int wf, artifactproc artifact)
{
	int artifacting;
	int artifactpalette[4];
	int vrampos;
	int vramsize;
	int video_rowheight;

	struct rasterbits_source rs;
	struct rasterbits_videomode rvm;
	struct rasterbits_frame rf;

	static int rowheights[] = {
		12,		12,		12,		12,		12,		12,		12,		12,
		3,		3,		3,		2,		2,		1,		1,		1
	};

	video_rowheight = rowheights[currentstate->video_vmode >> 1];
	vrampos = currentstate->video_offset;
	vramsize = currentstate->vram_mask + 1;

	rs.videoram = vrambase;
	rs.size = vramsize;
	rs.position = vrampos;
	rs.db = dirtybuffer;
	rvm.height = 192 / video_rowheight;
	rf.width = 256 * wf;
	rf.height = 192;
	rf.border_pen = (border_color == -1) ? -1 : Machine->pens[border_color];

	if (full_refresh) {
		memset(dirtybuffer, 1, videoram_size);
	}

	if (currentstate->video_gmode & 0x10)
	{
		rvm.flags = RASTERBITS_FLAG_GRAPHICS;

		if ((currentstate->video_gmode & 0x02) && !(artifact && ((currentstate->video_gmode & 0x1e) == M6847_MODE_G4R)))
		{
			/* Resolution modes */
			rvm.bytesperrow = ((currentstate->video_gmode & 0x1e) == M6847_MODE_G4R) ? 32 : 16;
			rvm.width = rvm.bytesperrow * 8;
			rvm.depth = 1;
			rvm.metapalette = &metapalette[currentstate->video_gmode & 0x1 ? 10 : 8];
		}
		else
		{
			/* Color modes */
			rvm.bytesperrow = ((currentstate->video_gmode & 0x1e) != M6847_MODE_G1C) ? 32 : 16;

			/* Are we doing PMODE 4 artifact colors? */
			artifacting = ((currentstate->video_gmode & 0x0c) == 0x0c) && (currentstate->video_gmode & 0x02);
			if (artifacting) {
				/* I am here because we are doing PMODE 4 artifact colors */
				artifactpalette[0] = metapalette[currentstate->video_gmode & 0x1 ? 10: 8];
				artifactpalette[3] = metapalette[currentstate->video_gmode & 0x1 ? 11: 9];
				artifact(artifactpalette);

				rvm.width = rvm.bytesperrow * 8;
				rvm.depth = 1;
				rvm.metapalette = artifactpalette;
				rvm.flags |= RASTERBITS_FLAG_ARTIFACT;
			}
			else {
				/* If not, calculate offset normally */
				rvm.width = rvm.bytesperrow * 4;
				rvm.depth = 2;
				rvm.metapalette = &metapalette[currentstate->video_gmode & 0x1 ? 4: 0];
			}
		}
	}
	else
	{
		rvm.flags = RASTERBITS_FLAG_TEXT;
		rvm.bytesperrow = 32;
		rvm.width = 32;
		rvm.depth = 8;
		rvm.metapalette = metapalette;

		if (!has_lowercase && (currentstate->video_gmode & 0x02)) {
			/* Semigraphics 6 */
			rvm.u.text.mapper = mapper_semigraphics6;
			rvm.u.text.mapper_param = (currentstate->video_gmode & 0x1) ? 4 : 0;
		}
		else {
			/* Text (or Semigraphics 4) */
			rvm.u.text.mapper = mapper_text;
			rvm.u.text.mapper_param = currentstate->video_gmode & 0x7;
		}
		rvm.u.text.modulo = 12;
	}
	raster_bits(bitmap, &rs, &rvm, &rf, NULL);
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

static int m6847_bordercolor(void)
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
		pen = 8;
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
	int artifact_value;

	if (vblankproc)
		vblankproc();

	artifact_value = (artifact_dipswitch == -1) ? 0 : readinputport(artifact_dipswitch);

	internal_m6847_vh_screenrefresh(bitmap, full_refresh, m6847_metapalette, videoram,
		&the_state, FALSE, (full_refresh ? m6847_bordercolor() : -1),
		1, artifacts[artifact_value & 3]);
}

/* --------------------------------------------------
 * Petty accessors
 * -------------------------------------------------- */

void m6847_set_gmode(int mode)
{
#if LOG_M6847
	logerror("m6847_set_gmode(): offset=$%02x\n", mode);
#endif

	mode &= 0x1f;

	if (mode != the_state.video_gmode) {
		the_state.video_gmode = mode;
		schedule_full_refresh();
	}
}

void m6847_set_vmode(int mode)
{
#if LOG_M6847
	logerror("m6847_set_vmode(): offset=$%02x\n", mode);
#endif

	mode &= 0x1f;

	if (mode != the_state.video_vmode) {
		the_state.video_vmode = mode;
		calc_videoram_size();
		schedule_full_refresh();
	}
}

int m6847_get_gmode(void)
{
	return the_state.video_gmode;
}

int m6847_get_vmode(void)
{
	return the_state.video_vmode;
}

void m6847_set_video_offset(int offset)
{
#if LOG_M6847
	logerror("m6847_set_video_offset(): offset=$%04x\n", offset);
#endif

	offset &= the_state.vram_mask;
	if (offset != the_state.video_offset) {
		the_state.video_offset = offset;
		schedule_full_refresh();
	}
}

int m6847_get_video_offset(void)
{
	return the_state.video_offset;
}

void m6847_touch_vram(int offset)
{
	offset -= the_state.video_offset;
	offset &= the_state.vram_mask;

	if (offset < videoram_size)
		dirtybuffer[offset] = 1;
}

int m6847_get_bordercolor(void)
{
	/* TODO: Verify this table.  I am pretty sure that it is true
	 * for the CoCo 3 and M6847T1, but I'm not sure if it is true
	 * for plain M6847
	 */
	static int bordercolortable[] = {
		/* Text modes */
		M6847_BORDERCOLOR_BLACK,	M6847_BORDERCOLOR_BLACK,
		M6847_BORDERCOLOR_BLACK,	M6847_BORDERCOLOR_BLACK,
		M6847_BORDERCOLOR_BLACK,	M6847_BORDERCOLOR_BLACK,
		M6847_BORDERCOLOR_BLACK,	M6847_BORDERCOLOR_BLACK,
		M6847_BORDERCOLOR_BLACK,	M6847_BORDERCOLOR_BLACK,
		M6847_BORDERCOLOR_GREEN,	M6847_BORDERCOLOR_ORANGE,
		M6847_BORDERCOLOR_BLACK,	M6847_BORDERCOLOR_BLACK,
		M6847_BORDERCOLOR_BLACK,	M6847_BORDERCOLOR_BLACK,

		M6847_BORDERCOLOR_GREEN,	M6847_BORDERCOLOR_WHITE,
		M6847_BORDERCOLOR_GREEN,	M6847_BORDERCOLOR_WHITE,
		M6847_BORDERCOLOR_GREEN,	M6847_BORDERCOLOR_WHITE,
		M6847_BORDERCOLOR_GREEN,	M6847_BORDERCOLOR_WHITE,
		M6847_BORDERCOLOR_GREEN,	M6847_BORDERCOLOR_WHITE,
		M6847_BORDERCOLOR_GREEN,	M6847_BORDERCOLOR_WHITE,
		M6847_BORDERCOLOR_GREEN,	M6847_BORDERCOLOR_WHITE,
		M6847_BORDERCOLOR_GREEN,	M6847_BORDERCOLOR_WHITE
	};
	return bordercolortable[the_state.video_gmode];
}

void m6847_get_bordercolor_rgb(int *red, int *green, int *blue)
{
	switch(m6847_get_bordercolor()) {
	case M6847_BORDERCOLOR_BLACK:
		*red = 0;
		*green = 0;
		*blue = 0;
		break;

	case M6847_BORDERCOLOR_GREEN:
		*red = 0;
		*green = 255;
		*blue = 0;
		break;

	case M6847_BORDERCOLOR_WHITE:
		*red = 255;
		*green = 255;
		*blue = 255;
		break;

	case M6847_BORDERCOLOR_ORANGE:
		*red = 255;
		*green = 128;
		*blue = 0;
		break;
	}
}

int m6847_get_vram_size(void)
{
	calc_videoram_size();
	return videoram_size;
}

