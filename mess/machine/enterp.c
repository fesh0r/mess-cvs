/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include <stdarg.h>
#include "driver.h"
#include "cpu/z80/z80.h"
#include "includes/enterp.h"
#include "includes/basicdsk.h"

extern unsigned char *Enterprise_RAM;

void Enterprise_SetupPalette(void);

void enterprise_init_machine(void)
{
	/* allocate memory. */
	/* I am allocating it because I control how the ram is
	 * accessed. Mame will not allocate any memory because all
	 * the memory regions have been defined as MRA_BANK?
	*/
	/* 128k memory, 32k for dummy read/write operations
	 * where memory bank is not defined
	 */
	Enterprise_RAM = malloc((128*1024)+32768);
	if (!Enterprise_RAM) return;

	/* initialise the hardware */
	Enterprise_Initialise();
}

void enterprise_shutdown_machine(void)
{
	if (Enterprise_RAM != NULL)
		free(Enterprise_RAM);

	Enterprise_RAM = NULL;
}

int enterprise_floppy_init(int id)
{
        if (basicdsk_floppy_init(id)==INIT_OK)
        {
                basicdsk_set_geometry(id, 80, 2, 9, 512, 10, 3, 1);
                return INIT_OK;
        }

    return INIT_FAILED;
}
