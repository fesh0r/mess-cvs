/***************************************************************************

	Generic (PC-style) IDE controller implementation

***************************************************************************/

#include "idectrl.h"


/*************************************
 *
 *	Debugging
 *
 *************************************/

#define VERBOSE						0
#define PRINTF_IDE_COMMANDS			0

#if VERBOSE
#define LOG(x)	logerror x
#else
#define LOG(X)
#endif

#if (VERBOSE && PRINTF_IDE_COMMANDS)
#define LOGPRINT(x)	logerror x; printf x
#elif PRINTF_IDE_COMMANDS
#define LOGPRINT(x)	printf x
#else
#define LOGPRINT(X)
#endif



/*************************************
 *
 *	Constants
 *
 *************************************/

#define IDE_DISK_SECTOR_SIZE		512

#define TIME_PER_SECTOR				(TIME_IN_USEC(100))
#define TIME_PER_ROTATION			(TIME_IN_HZ(5400/60))

#define IDE_STATUS_ERROR			0x01
#define IDE_STATUS_HIT_INDEX		0x02
#define IDE_STATUS_BUFFER_READY		0x08
#define IDE_STATUS_SEEK_COMPLETE	0x10
#define IDE_STATUS_DRIVE_READY		0x40
#define IDE_STATUS_BUSY				0x80

#define IDE_CONFIG_REGISTERS		0x10

#define IDE_ADDR_CONFIG_UNK			0x034
#define IDE_ADDR_CONFIG_REGISTER	0x038
#define IDE_ADDR_CONFIG_DATA		0x03c

#define IDE_ADDR_DATA				0x1f0
#define IDE_ADDR_ERROR				0x1f1
#define IDE_ADDR_SECTOR_COUNT		0x1f2
#define IDE_ADDR_SECTOR_NUMBER		0x1f3
#define IDE_ADDR_CYLINDER_LSB		0x1f4
#define IDE_ADDR_CYLINDER_MSB		0x1f5
#define IDE_ADDR_HEAD_NUMBER		0x1f6
#define IDE_ADDR_STATUS_COMMAND		0x1f7

#define IDE_ADDR_STATUS_CONTROL		0x3f6

#define IDE_COMMAND_READ_MULTIPLE	0x20
#define IDE_COMMAND_WRITE_MULTIPLE	0x30
#define IDE_COMMAND_SET_CONFIG		0x91
#define IDE_COMMAND_READ_MULTIPLE_BLOCK	0xc4
#define IDE_COMMAND_WRITE_MULTIPLE_BLOCK 0xc5
#define IDE_COMMAND_SET_BLOCK_COUNT		0xc6
#define IDE_COMMAND_READ_DMA			0xc8
#define IDE_COMMAND_WRITE_DMA			0xca
#define IDE_COMMAND_GET_INFO		0xec
#define IDE_COMMAND_SET_FEATURES		0xef
#define IDE_COMMAND_UNKNOWN_F9		0xf9

#define IDE_ERROR_NONE				0x00
#define IDE_ERROR_DEFAULT			0x01
#define IDE_ERROR_UNKNOWN_COMMAND	0x04
#define IDE_ERROR_BAD_LOCATION		0x10
#define IDE_ERROR_BAD_SECTOR		0x80

#define IDE_BUSMASTER_STATUS_ACTIVE		0x01
#define IDE_BUSMASTER_STATUS_ERROR		0x02
#define IDE_BUSMASTER_STATUS_IRQ		0x04



/*************************************
 *
 *	Type definitions
 *
 *************************************/

struct ide_state
{
	UINT8	adapter_control;
	UINT8	status;
	UINT8	error;
	UINT8	command;
	UINT8	interrupt_pending;
	UINT8	precomp_offset;

	UINT8	buffer[IDE_DISK_SECTOR_SIZE];
	UINT8	features[IDE_DISK_SECTOR_SIZE];
	UINT16	buffer_offset;
	UINT16	sector_count;

	UINT16	block_count;
	UINT16	sectors_until_int;
	
	UINT8	dma_active;
	UINT8	dma_cpu;
	UINT8	dma_address_xor;
	UINT8	dma_last_buffer;
	offs_t	dma_address;
	offs_t	dma_descriptor;
	UINT32	dma_bytes_left;
	
	UINT8	bus_master_command;
	UINT8	bus_master_status;
	UINT32	bus_master_descriptor;

	UINT16	cur_cylinder;
	UINT8	cur_sector;
	UINT8	cur_head;
	UINT8	cur_head_reg;

	UINT16	num_cylinders;
	UINT8	num_sectors;
	UINT8	num_heads;

	UINT8	config_unknown;
	UINT8	config_register[IDE_CONFIG_REGISTERS];
	UINT8	config_register_num;

	struct ide_interface *intf;
	void *	disk;
	void *	last_status_timer;
	void *	reset_timer;
};



/*************************************
 *
 *	Local variables
 *
 *************************************/

static struct ide_state idestate[MAX_IDE_CONTROLLERS];



/*************************************
 *
 *	Prototypes
 *
 *************************************/

static void reset_callback(int param);

static void ide_build_features(struct ide_state *ide);

static void continue_read(struct ide_state *ide);
static void read_sector_done(int which);
static void read_next_sector(struct ide_state *ide);

static UINT32 ide_controller_read(struct ide_state *ide, offs_t offset, int size);
static void ide_controller_write(struct ide_state *ide, offs_t offset, int size, UINT32 data);



/*************************************
 *
 *	Interrupts
 *
 *************************************/

INLINE void signal_interrupt(struct ide_state *ide)
{
	LOG(("IDE interrupt assert\n"));

	/* signal an interrupt */
	if (ide->intf->interrupt)
		(*ide->intf->interrupt)(ASSERT_LINE);
	ide->interrupt_pending = 1;
	ide->bus_master_status |= IDE_BUSMASTER_STATUS_IRQ;
}


INLINE void clear_interrupt(struct ide_state *ide)
{
	LOG(("IDE interrupt clear\n"));

	/* clear an interrupt */
	if (ide->intf->interrupt)
		(*ide->intf->interrupt)(CLEAR_LINE);
	ide->interrupt_pending = 0;
}



/*************************************
 *
 *	Initialization & reset
 *
 *************************************/

int ide_controller_init_custom(int which, struct ide_interface *intf, void *diskhandle)
{
	struct ide_state *ide = &idestate[which];
	const struct hard_disk_header *header;

	/* NULL interface is immediate failure */
	if (!intf)
		return 1;

	/* reset the IDE state */
	memset(ide, 0, sizeof(*ide));
	ide->intf = intf;

	/* set MAME harddisk handle */
	ide->disk = diskhandle;

	/* get and copy the geometry */
	if (ide->disk)
	{
		header = hard_disk_get_header(ide->disk);
		ide->num_cylinders = header->cylinders;
		ide->num_sectors = header->sectors;
		ide->num_heads = header->heads;
		if (header->seclen != IDE_DISK_SECTOR_SIZE)
			/* wrong sector len */
			return 1;
#if PRINTF_IDE_COMMANDS
		printf("CHS: %d %d %d\n", ide->num_cylinders, ide->num_sectors, ide->num_heads);
#endif
	}

	/* build the features page */
	ide_build_features(ide);

	/* create a timer for timing status */
	ide->last_status_timer = timer_alloc(NULL);
	ide->reset_timer = timer_alloc(reset_callback);
	return 0;
}

int ide_controller_init(int which, struct ide_interface *intf)
{
	/* we only support one hard disk right now; get a handle to it */
	return ide_controller_init_custom(which, intf, get_disk_handle(0));
}


void ide_controller_reset(int which)
{
	struct ide_state *ide = &idestate[which];

	LOG(("IDE controller reset performed\n"));

	/* reset the drive state */
	ide->status = IDE_STATUS_DRIVE_READY | IDE_STATUS_SEEK_COMPLETE;
	ide->error = IDE_ERROR_DEFAULT;
	ide->buffer_offset = 0;
	clear_interrupt(ide);
}


UINT8 *ide_get_features(int which)
{
	struct ide_state *ide = &idestate[which];
	return ide->features;
}


static void reset_callback(int param)
{
	ide_controller_reset(param);
}



/*************************************
 *
 *	Convert offset/mem_mask to offset
 *	and size
 *
 *************************************/

INLINE int convert_to_offset_and_size(offs_t *offset, data32_t mem_mask)
{
	int size = 4;

	/* determine which real offset */
	if (mem_mask & 0x000000ff)
	{
		(*offset)++, size = 3;
		if (mem_mask & 0x0000ff00)
		{
			(*offset)++, size = 2;
			if (mem_mask & 0x00ff0000)
				(*offset)++, size = 1;
		}
	}

	/* determine the real size */
	if (!(mem_mask & 0xff000000))
		return size;
	size--;
	if (!(mem_mask & 0x00ff0000))
		return size;
	size--;
	if (!(mem_mask & 0x0000ff00))
		return size;
	size--;
	return size;
}



/*************************************
 *
 *	Advance to the next sector
 *
 *************************************/

INLINE void next_sector(struct ide_state *ide)
{
	/* LBA direct? */
	if (ide->cur_head_reg & 0x40)
	{
		ide->cur_sector++;
		if (ide->cur_sector == 0)
		{
			ide->cur_cylinder++;
			if (ide->cur_cylinder == 0)
				ide->cur_head++;
		}
	}

	/* standard CHS */
	else
	{
		/* sectors are 1-based */
		ide->cur_sector++;
		if (ide->cur_sector > ide->num_sectors)
		{
			/* heads are 0 based */
			ide->cur_sector = 1;
			ide->cur_head++;
			if (ide->cur_head >= ide->num_heads)
			{
				ide->cur_head = 0;
				ide->cur_cylinder++;
			}
		}
	}
}



/*************************************
 *
 *	Compute the LBA address
 *
 *************************************/

INLINE UINT32 lba_address(struct ide_state *ide)
{
	/* LBA direct? */
	if (ide->cur_head_reg & 0x40)
		return ide->cur_sector + ide->cur_cylinder * 256 + ide->cur_head * 16777216;

	/* standard CHS */
	else
		return (ide->cur_cylinder * ide->num_heads + ide->cur_head) * ide->num_sectors + ide->cur_sector - 1;
}



/*************************************
 *
 *	Build a features page
 *
 *************************************/

static void swap_strncpy(UINT8 *dst, const char *src, int field_size_in_words)
{
	int i;

	for (i = 0; i < field_size_in_words * 2 && src[i]; i++)
		dst[i ^ 1] = src[i];
	for ( ; i < field_size_in_words; i++)
		dst[i ^ 1] = ' ';
}


static void ide_build_features(struct ide_state *ide)
{
	int total_sectors = ide->num_cylinders * ide->num_heads * ide->num_sectors;

	memset(ide->buffer, 0, IDE_DISK_SECTOR_SIZE);

	/* basic geometry */
	ide->features[ 0*2+0] = 0x5a;						/*  0: configuration bits */
	ide->features[ 0*2+1] = 0x04;
	ide->features[ 1*2+0] = ide->num_cylinders & 0xff;	/*  1: logical cylinders */
	ide->features[ 1*2+1] = ide->num_cylinders >> 8;
	ide->features[ 2*2+0] = 0;							/*  2: reserved */
	ide->features[ 2*2+1] = 0;
	ide->features[ 3*2+0] = ide->num_heads & 0xff;		/*  3: logical heads */
	ide->features[ 3*2+1] = ide->num_heads >> 8;
	ide->features[ 4*2+0] = 0;							/*  4: vendor specific (obsolete) */
	ide->features[ 4*2+1] = 0;
	ide->features[ 5*2+0] = 0;							/*  5: vendor specific (obsolete) */
	ide->features[ 5*2+1] = 0;
	ide->features[ 6*2+0] = ide->num_sectors & 0xff;	/*  6: logical sectors per logical track */
	ide->features[ 6*2+1] = ide->num_sectors >> 8;
	ide->features[ 7*2+0] = 0;							/*  7: vendor-specific */
	ide->features[ 7*2+1] = 0;
	ide->features[ 8*2+0] = 0;							/*  8: vendor-specific */
	ide->features[ 8*2+1] = 0;
	ide->features[ 9*2+0] = 0;							/*  9: vendor-specific */
	ide->features[ 9*2+1] = 0;
	swap_strncpy(&ide->features[10*2+0], 				/* 10-19: serial number */
			"00000000000000000000", 10);
	ide->features[20*2+0] = 0;							/* 20: vendor-specific */
	ide->features[20*2+1] = 0;
	ide->features[21*2+0] = 0;							/* 21: vendor-specific */
	ide->features[21*2+1] = 0;
	ide->features[22*2+0] = 4;							/* 22: # of vendor-specific bytes on read/write long commands */
	ide->features[22*2+1] = 0;
	swap_strncpy(&ide->features[23*2+0], 				/* 23-26: firmware revision */
			"1.0", 4);
	swap_strncpy(&ide->features[27*2+0], 				/* 27-46: model number */
			"MAME Compressed Hard Disk", 20);
	ide->features[47*2+0] = 0x01;						/* 47: read/write multiple support */
	ide->features[47*2+1] = 0x80;
	ide->features[48*2+0] = 0;							/* 48: reserved */
	ide->features[48*2+1] = 0;
	ide->features[49*2+0] = 0x00;						/* 49: capabilities */
	ide->features[49*2+1] = 0x0f;
	ide->features[50*2+0] = 0;							/* 50: reserved */
	ide->features[50*2+1] = 0;
	ide->features[51*2+0] = 2;							/* 51: PIO data transfer cycle timing mode */
	ide->features[51*2+1] = 0;
	ide->features[52*2+0] = 2;							/* 52: single word DMA transfer cycle timing mode */
	ide->features[52*2+1] = 0;
	ide->features[53*2+0] = 3;							/* 53: field validity */
	ide->features[53*2+1] = 0;
	ide->features[54*2+0] = ide->num_cylinders & 0xff;	/* 54: number of current logical cylinders */
	ide->features[54*2+1] = ide->num_cylinders >> 8;
	ide->features[55*2+0] = ide->num_heads & 0xff;		/* 55: number of current logical heads */
	ide->features[55*2+1] = ide->num_heads >> 8;
	ide->features[56*2+0] = ide->num_sectors & 0xff;	/* 56: number of current logical sectors per track */
	ide->features[56*2+1] = ide->num_sectors >> 8;
	ide->features[57*2+0] = total_sectors & 0xff;		/* 57-58: number of current logical sectors per track */
	ide->features[57*2+1] = total_sectors >> 8;
	ide->features[58*2+0] = total_sectors >> 16;
	ide->features[58*2+1] = total_sectors >> 24;
	ide->features[59*2+0] = 0;							/* 59: multiple sector timing */
	ide->features[59*2+1] = 0;
	ide->features[60*2+0] = total_sectors & 0xff;		/* 60-61: total user addressable sectors */
	ide->features[60*2+1] = total_sectors >> 8;
	ide->features[61*2+0] = total_sectors >> 16;
	ide->features[61*2+1] = total_sectors >> 24;
	ide->features[62*2+0] = 0x07;						/* 62: single word dma transfer */
	ide->features[62*2+1] = 0x00;
	ide->features[63*2+0] = 0x07;						/* 63: multiword DMA transfer */
	ide->features[63*2+1] = 0x04;
	ide->features[64*2+0] = 0x03;						/* 64: flow control PIO transfer modes supported */
	ide->features[64*2+1] = 0x00;
	ide->features[65*2+0] = 0x78;						/* 65: minimum multiword DMA transfer cycle time per word */
	ide->features[65*2+1] = 0x00;
	ide->features[66*2+0] = 0x78;						/* 66: mfr's recommended multiword DMA transfer cycle time */
	ide->features[66*2+1] = 0x00;
	ide->features[67*2+0] = 0x4d;						/* 67: minimum PIO transfer cycle time without flow control */
	ide->features[67*2+1] = 0x01;
	ide->features[68*2+0] = 0x78;						/* 68: minimum PIO transfer cycle time with IORDY */
	ide->features[68*2+1] = 0x00;
}



/*************************************
 *
 *	Sector reading
 *
 *************************************/

static void continue_read(struct ide_state *ide)
{
	/* reset the totals */
	ide->buffer_offset = 0;

	/* clear the buffer ready flag */
	ide->status &= ~IDE_STATUS_BUFFER_READY;

	/* if there is more data to read, keep going */
	if (ide->sector_count > 0)
		ide->sector_count--;
	if (ide->sector_count > 0)
		read_next_sector(ide);
	else
	{
		ide->bus_master_status &= ~IDE_BUSMASTER_STATUS_ACTIVE;
		ide->dma_active = 0;
	}
}


static void write_buffer_to_dma(struct ide_state *ide)
{
	int bytesleft = IDE_DISK_SECTOR_SIZE;
	UINT8 *data = ide->buffer;
	
//	LOG(("Writing sector to %08X\n", ide->dma_address));

	/* loop until we've consumed all bytes */
	while (bytesleft--)
	{
		/* if we're out of space, grab the next descriptor */
		if (ide->dma_bytes_left == 0)
		{
			/* if we're out of buffer space, that's bad */
			if (ide->dma_last_buffer)
			{
				LOG(("DMA Out of buffer space!\n"));
				return;
			}
		
			/* fetch the address */
			ide->dma_address = cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor);
			ide->dma_address |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 8;
			ide->dma_address |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 16;
			ide->dma_address |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 24;
			ide->dma_address &= 0xfffffffe;

			/* fetch the length */
			ide->dma_bytes_left = cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor);
			ide->dma_bytes_left |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 8;
			ide->dma_bytes_left |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 16;
			ide->dma_bytes_left |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 24;
			ide->dma_last_buffer = (ide->dma_bytes_left >> 31) & 1;
			ide->dma_bytes_left &= 0xfffe;
			if (ide->dma_bytes_left == 0)
				ide->dma_bytes_left = 0x10000;
			
//			LOG(("New DMA descriptor: address = %08X  bytes = %04X  last = %d\n", ide->dma_address, ide->dma_bytes_left, ide->dma_last_buffer));
		}
		
		/* write the next byte */
		cpunum_write_byte(ide->dma_cpu, ide->dma_address++, *data++);
		ide->dma_bytes_left--;
	}
}


static void read_sector_done(int which)
{
	struct ide_state *ide = &idestate[which];
	int lba = lba_address(ide), count = 0;

	/* now do the read */
	if (ide->disk)
		count = hard_disk_read(ide->disk, lba, 1, ide->buffer);

	/* by default, mark the buffer ready and the seek complete */
	ide->status |= IDE_STATUS_BUFFER_READY;
	ide->status |= IDE_STATUS_SEEK_COMPLETE;

	/* and clear the busy adn error flags */
	ide->status &= ~IDE_STATUS_ERROR;
	ide->status &= ~IDE_STATUS_BUSY;

	/* if we succeeded, advance to the next sector and set the nice bits */
	if (count == 1)
	{
		/* advance the pointers */
		next_sector(ide);

		/* clear the error value */
		ide->error = IDE_ERROR_NONE;

		/* signal an interrupt */
		if (--ide->sectors_until_int == 0 || ide->sector_count == 1)
		{
			ide->sectors_until_int = ((ide->command == IDE_COMMAND_READ_MULTIPLE_BLOCK) ? ide->block_count : 1);
		signal_interrupt(ide);
	}

		/* keep going for DMA */
		if (ide->dma_active)
		{
			write_buffer_to_dma(ide);
			continue_read(ide);
		}
	}

	/* if we got an error, we need to report it */
	else
	{
		/* set the error flag and the error */
		ide->status |= IDE_STATUS_ERROR;
		ide->error = IDE_ERROR_BAD_SECTOR;
		ide->bus_master_status |= IDE_BUSMASTER_STATUS_ERROR;
		ide->bus_master_status &= ~IDE_BUSMASTER_STATUS_ACTIVE;

		/* signal an interrupt */
		signal_interrupt(ide);
	}
}


static void read_next_sector(struct ide_state *ide)
{
	/* just set a timer and mark ourselves busy */
	timer_set(TIME_PER_SECTOR, ide - idestate, read_sector_done);
	ide->status |= IDE_STATUS_BUSY;
}



/*************************************
 *
 *	Sector writing
 *
 *************************************/

static void write_sector_done(int which);

static void continue_write(struct ide_state *ide)
{
	/* reset the totals */
	ide->buffer_offset = 0;

	/* clear the buffer ready flag */
	ide->status &= ~IDE_STATUS_BUFFER_READY;

	/* set a timer to do the write */
	timer_set(TIME_PER_SECTOR, ide - idestate, write_sector_done);
	ide->status |= IDE_STATUS_BUSY;
}


static void read_buffer_from_dma(struct ide_state *ide)
{
	int bytesleft = IDE_DISK_SECTOR_SIZE;
	UINT8 *data = ide->buffer;
	
//	LOG(("Reading sector from %08X\n", ide->dma_address));

	/* loop until we've consumed all bytes */
	while (bytesleft--)
	{
		/* if we're out of space, grab the next descriptor */
		if (ide->dma_bytes_left == 0)
		{
			/* if we're out of buffer space, that's bad */
			if (ide->dma_last_buffer)
			{
				LOG(("DMA Out of buffer space!\n"));
				return;
			}
		
			/* fetch the address */
			ide->dma_address = cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor);
			ide->dma_address |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 8;
			ide->dma_address |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 16;
			ide->dma_address |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 24;
			ide->dma_address &= 0xfffffffe;

			/* fetch the length */
			ide->dma_bytes_left = cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor);
			ide->dma_bytes_left |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 8;
			ide->dma_bytes_left |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 16;
			ide->dma_bytes_left |= cpunum_read_byte(ide->dma_cpu, ide->dma_descriptor++ ^ ide->dma_address_xor) << 24;
			ide->dma_last_buffer = (ide->dma_bytes_left >> 31) & 1;
			ide->dma_bytes_left &= 0xfffe;
			if (ide->dma_bytes_left == 0)
				ide->dma_bytes_left = 0x10000;
			
//			LOG(("New DMA descriptor: address = %08X  bytes = %04X  last = %d\n", ide->dma_address, ide->dma_bytes_left, ide->dma_last_buffer));
		}
		
		/* read the next byte */
		*data++ = cpunum_read_byte(ide->dma_cpu, ide->dma_address++);
		ide->dma_bytes_left--;
	}
}


static void write_sector_done(int which)
{
	struct ide_state *ide = &idestate[which];
	int lba = lba_address(ide), count = 0;

	/* now do the write */
	if (ide->disk)
		count = hard_disk_write(ide->disk, lba, 1, ide->buffer);

	/* by default, mark the buffer ready and the seek complete */
	ide->status |= IDE_STATUS_BUFFER_READY;
	ide->status |= IDE_STATUS_SEEK_COMPLETE;

	/* and clear the busy adn error flags */
	ide->status &= ~IDE_STATUS_ERROR;
	ide->status &= ~IDE_STATUS_BUSY;

	/* if we succeeded, advance to the next sector and set the nice bits */
	if (count == 1)
	{
		/* advance the pointers */
		next_sector(ide);

		/* clear the error value */
		ide->error = IDE_ERROR_NONE;

		/* signal an interrupt */
		if (--ide->sectors_until_int == 0 || ide->sector_count == 1)
		{
			ide->sectors_until_int = ((ide->command == IDE_COMMAND_WRITE_MULTIPLE_BLOCK) ? ide->block_count : 1);
			signal_interrupt(ide);
		}
		
		/* signal an interrupt if there's more data needed */
		if (ide->sector_count > 0)
			ide->sector_count--;
		if (ide->sector_count == 0)
			ide->status &= ~IDE_STATUS_BUFFER_READY;
		
		/* keep going for DMA */
		if (ide->dma_active && ide->sector_count != 0)
		{
			read_buffer_from_dma(ide);
			continue_write(ide);
		}
		else
			ide->dma_active = 0;
	}

	/* if we got an error, we need to report it */
	else
	{
		/* set the error flag and the error */
		ide->status |= IDE_STATUS_ERROR;
		ide->error = IDE_ERROR_BAD_SECTOR;
		ide->bus_master_status |= IDE_BUSMASTER_STATUS_ERROR;
		ide->bus_master_status &= ~IDE_BUSMASTER_STATUS_ACTIVE;

		/* signal an interrupt */
		signal_interrupt(ide);
	}
}



/*************************************
 *
 *	Handle IDE commands
 *
 *************************************/

void handle_command(struct ide_state *ide, UINT8 command)
{
	/* implicitly clear interrupts here */
	clear_interrupt(ide);

	ide->command = command;
	switch (command)
	{
		case IDE_COMMAND_READ_MULTIPLE:
			LOGPRINT(("IDE Read multiple: C=%d H=%d S=%d LBA=%d count=%d\n",
				ide->cur_cylinder, ide->cur_head, ide->cur_sector, lba_address(ide), ide->sector_count));

			/* reset the buffer */
			ide->buffer_offset = 0;
			ide->sectors_until_int = 1;
			ide->dma_active = 0;

			/* start the read going */
			read_next_sector(ide);
			break;

		case IDE_COMMAND_READ_MULTIPLE_BLOCK:
			LOGPRINT(("IDE Read multiple block: C=%d H=%d S=%d LBA=%d count=%d\n",
				ide->cur_cylinder, ide->cur_head, ide->cur_sector, lba_address(ide), ide->sector_count));

			/* reset the buffer */
			ide->buffer_offset = 0;
			ide->sectors_until_int = ide->block_count;
			ide->dma_active = 0;

			/* start the read going */
			read_next_sector(ide);
			break;

		case IDE_COMMAND_READ_DMA:
			LOGPRINT(("IDE Read multiple DMA: C=%d H=%d S=%d LBA=%d count=%d\n",
				ide->cur_cylinder, ide->cur_head, ide->cur_sector, lba_address(ide), ide->sector_count));

			/* reset the buffer */
			ide->buffer_offset = 0;
			ide->sectors_until_int = ide->sector_count;
			ide->dma_active = 1;

			/* start the read going */
			if (ide->bus_master_command & 1)
			read_next_sector(ide);
			break;

		case IDE_COMMAND_WRITE_MULTIPLE:
			LOGPRINT(("IDE Write multiple: C=%d H=%d S=%d LBA=%d count=%d\n",
				ide->cur_cylinder, ide->cur_head, ide->cur_sector, lba_address(ide), ide->sector_count));

			/* reset the buffer */
			ide->buffer_offset = 0;
			ide->sectors_until_int = 1;
			ide->dma_active = 0;

			/* mark the buffer ready */
			ide->status |= IDE_STATUS_BUFFER_READY;
			break;

		case IDE_COMMAND_WRITE_DMA:
			LOGPRINT(("IDE Write multiple DMA: C=%d H=%d S=%d LBA=%d count=%d\n",
				ide->cur_cylinder, ide->cur_head, ide->cur_sector, lba_address(ide), ide->sector_count));

			/* reset the buffer */
			ide->buffer_offset = 0;
			ide->sectors_until_int = ide->sector_count;
			ide->dma_active = 1;

			/* start the read going */
			if (ide->bus_master_command & 1)
			{
				read_buffer_from_dma(ide);
				continue_write(ide);
			}
			break;

		case IDE_COMMAND_GET_INFO:
			LOGPRINT(("IDE Read features\n"));

			/* reset the buffer */
			ide->buffer_offset = 0;
			ide->sector_count = 1;

			/* build the features page */
			memcpy(ide->buffer, ide->features, sizeof(ide->buffer));

			/* indicate everything is ready */
			ide->status |= IDE_STATUS_BUFFER_READY;
			ide->status |= IDE_STATUS_SEEK_COMPLETE;

			/* and clear the busy adn error flags */
			ide->status &= ~IDE_STATUS_ERROR;
			ide->status &= ~IDE_STATUS_BUSY;

			/* clear the error too */
			ide->error = IDE_ERROR_NONE;

			/* signal an interrupt */
			signal_interrupt(ide);
			break;

		case IDE_COMMAND_SET_CONFIG:
			LOGPRINT(("IDE Set configuration (%d heads, %d sectors)\n", ide->cur_head + 1, ide->sector_count));

			ide->num_sectors = ide->sector_count;
			ide->num_heads = ide->cur_head + 1;

			/* signal an interrupt */
			signal_interrupt(ide);
			break;

		case IDE_COMMAND_UNKNOWN_F9:
			/* only used by Killer Instinct AFAICT */
			LOGPRINT(("IDE unknown command (F9)\n"));

			/* signal an interrupt */
			signal_interrupt(ide);
			break;

		case IDE_COMMAND_SET_FEATURES:
			LOGPRINT(("IDE Set features (%02X %02X %02X %02X %02X)\n", ide->precomp_offset, ide->sector_count & 0xff, ide->cur_sector, ide->cur_cylinder & 0xff, ide->cur_cylinder >> 8));

			/* signal an interrupt */
			signal_interrupt(ide);
			break;
		
		case IDE_COMMAND_SET_BLOCK_COUNT:
			LOGPRINT(("IDE Set block count (%02X)\n", ide->sector_count));

			ide->block_count = ide->sector_count;

			/* signal an interrupt */
			signal_interrupt(ide);
			break;

		default:
			LOGPRINT(("IDE unknown command (%02X)\n", command));
#ifdef MAME_DEBUG
{
	extern int debug_key_pressed;
	debug_key_pressed = 1;
}
#endif
			break;
	}
}



/*************************************
 *
 *	IDE controller read
 *
 *************************************/

static UINT32 ide_controller_read(struct ide_state *ide, offs_t offset, int size)
{
	UINT32 result = 0;

	/* logit */
	if (offset != IDE_ADDR_DATA && offset != IDE_ADDR_STATUS_COMMAND && offset != IDE_ADDR_STATUS_CONTROL)
		LOG(("%08X:IDE read at %03X, size=%d\n", activecpu_get_previouspc(), offset, size));

	switch (offset)
	{
		/* unknown config register */
		case IDE_ADDR_CONFIG_UNK:
			return ide->config_unknown;

		/* active config register */
		case IDE_ADDR_CONFIG_REGISTER:
			return ide->config_register_num;

		/* data from active config register */
		case IDE_ADDR_CONFIG_DATA:
			if (ide->config_register_num < IDE_CONFIG_REGISTERS)
				return ide->config_register[ide->config_register_num];
			return 0;

		/* read data if there's data to be read */
		case IDE_ADDR_DATA:
			if (ide->status & IDE_STATUS_BUFFER_READY)
			{
				/* fetch the correct amount of data */
				result = ide->buffer[ide->buffer_offset++];
				if (size > 1)
					result |= ide->buffer[ide->buffer_offset++] << 8;
				if (size > 2)
				{
					result |= ide->buffer[ide->buffer_offset++] << 16;
					result |= ide->buffer[ide->buffer_offset++] << 24;
				}

				/* if we're at the end of the buffer, handle it */
				if (ide->buffer_offset >= IDE_DISK_SECTOR_SIZE)
					continue_read(ide);
			}
			break;

		/* return the current error */
		case IDE_ADDR_ERROR:
			return ide->error;

		/* return the current sector count */
		case IDE_ADDR_SECTOR_COUNT:
			return ide->sector_count;

		/* return the current sector */
		case IDE_ADDR_SECTOR_NUMBER:
			return ide->cur_sector;

		/* return the current cylinder LSB */
		case IDE_ADDR_CYLINDER_LSB:
			return ide->cur_cylinder & 0xff;

		/* return the current cylinder MSB */
		case IDE_ADDR_CYLINDER_MSB:
			return ide->cur_cylinder >> 8;

		/* return the current head */
		case IDE_ADDR_HEAD_NUMBER:
			return ide->cur_head_reg;

		/* return the current status and clear any pending interrupts */
		case IDE_ADDR_STATUS_COMMAND:
		/* return the current status but don't clear interrupts */
		case IDE_ADDR_STATUS_CONTROL:
			result = ide->status;
			if (timer_timeelapsed(ide->last_status_timer) > TIME_PER_ROTATION)
			{
				result |= IDE_STATUS_HIT_INDEX;
				timer_adjust(ide->last_status_timer, TIME_NEVER, 0, 0);
			}

			/* clear interrutps only when reading the real status */
			if (offset == IDE_ADDR_STATUS_COMMAND)
			{
				if (ide->interrupt_pending)
					clear_interrupt(ide);
			}
			
			/* take a bit of time to speed up people who poll hard */
			activecpu_adjust_icount(-100);
			break;

		/* log anything else */
		default:
			logerror("%08X:unknown IDE read at %03X, size=%d\n", activecpu_get_previouspc(), offset, size);
			break;
	}

	/* return the result */
	return result;
}



/*************************************
 *
 *	IDE controller write
 *
 *************************************/

static void ide_controller_write(struct ide_state *ide, offs_t offset, int size, UINT32 data)
{
	/* logit */
	if (offset != IDE_ADDR_DATA)
		LOG(("%08X:IDE write to %03X = %08X, size=%d\n", activecpu_get_previouspc(), offset, data, size));

	switch (offset)
	{
		/* unknown config register */
		case IDE_ADDR_CONFIG_UNK:
			ide->config_unknown = data;
			break;

		/* active config register */
		case IDE_ADDR_CONFIG_REGISTER:
			ide->config_register_num = data;
			break;

		/* data from active config register */
		case IDE_ADDR_CONFIG_DATA:
			if (ide->config_register_num < IDE_CONFIG_REGISTERS)
				ide->config_register[ide->config_register_num] = data;
			break;

		/* write data */
		case IDE_ADDR_DATA:
			if (ide->status & IDE_STATUS_BUFFER_READY)
			{
				/* store the correct amount of data */
				ide->buffer[ide->buffer_offset++] = data;
				if (size > 1)
					ide->buffer[ide->buffer_offset++] = data >> 8;
				if (size > 2)
				{
					ide->buffer[ide->buffer_offset++] = data >> 16;
					ide->buffer[ide->buffer_offset++] = data >> 24;
				}

				/* if we're at the end of the buffer, handle it */
				if (ide->buffer_offset >= IDE_DISK_SECTOR_SIZE)
					continue_write(ide);
			}
			break;

		/* precompensation offset?? */
		case IDE_ADDR_ERROR:
			ide->precomp_offset = data;
			break;

		/* sector count */
		case IDE_ADDR_SECTOR_COUNT:
			ide->sector_count = data ? data : 256;
			break;

		/* current sector */
		case IDE_ADDR_SECTOR_NUMBER:
			ide->cur_sector = data;
			break;

		/* current cylinder LSB */
		case IDE_ADDR_CYLINDER_LSB:
			ide->cur_cylinder = (ide->cur_cylinder & 0xff00) | (data & 0xff);
			break;

		/* current cylinder MSB */
		case IDE_ADDR_CYLINDER_MSB:
			ide->cur_cylinder = (ide->cur_cylinder & 0x00ff) | ((data & 0xff) << 8);
			break;

		/* current head */
		case IDE_ADDR_HEAD_NUMBER:
			ide->cur_head = data & 0x0f;
			ide->cur_head_reg = data;
			// drive index = data & 0x10
			// LBA mode = data & 0x40
			break;

		/* command */
		case IDE_ADDR_STATUS_COMMAND:
			handle_command(ide, data);
			break;

		/* adapter control */
		case IDE_ADDR_STATUS_CONTROL:
			ide->adapter_control = data;

			/* handle controller reset */
			if (data == 0x04)
			{
				ide->status |= IDE_STATUS_BUSY;
				ide->status &= ~IDE_STATUS_DRIVE_READY;
				timer_adjust(ide->reset_timer, TIME_IN_MSEC(5), ide - idestate, 0);
			}
			break;
	}
}



/*************************************
 *
 *	Bus master read
 *
 *************************************/

static UINT32 ide_bus_master_read(struct ide_state *ide, offs_t offset, int size)
{
	LOG(("%08X:ide_bus_master_read(%d, %d)\n", activecpu_get_previouspc(), offset, size));

	/* command register */
	if (offset == 0)
		return ide->bus_master_command | (ide->bus_master_status << 16);
	
	/* descriptor table register */
	if (offset == 4)
		return ide->bus_master_descriptor;
	
	return 0xffffffff;
}



/*************************************
 *
 *	Bus master write
 *
 *************************************/

static void ide_bus_master_write(struct ide_state *ide, offs_t offset, int size, UINT32 data)
{
	LOG(("%08X:ide_bus_master_write(%d, %d, %08X)\n", activecpu_get_previouspc(), offset, size, data));

	/* command register */
	if (offset == 0)
	{
		UINT8 old = ide->bus_master_command;
		UINT8 val = data & 0xff;
		
		/* save the read/write bit and the start/stop bit */
		ide->bus_master_command = (old & 0xf6) | (val & 0x09);
		ide->bus_master_status = (ide->bus_master_status & ~IDE_BUSMASTER_STATUS_ACTIVE) | (val & 0x01);
		
		/* handle starting a transfer */
		if (!(old & 1) && (val & 1))
		{
			/* reset all the DMA data */
			ide->dma_bytes_left = 0;
			ide->dma_last_buffer = 0;
			ide->dma_descriptor = ide->bus_master_descriptor;
			ide->dma_cpu = cpu_getactivecpu();
			ide->dma_address_xor = (activecpu_endianess() == CPU_IS_LE) ? 0 : 3;
			
			/* if we're going live, start the pending read/write */
			if (ide->dma_active)
			{
				if (ide->bus_master_command & 8)
					read_next_sector(ide);
				else
				{
					read_buffer_from_dma(ide);
					continue_write(ide);
				}
			}
		}
	}
	
	/* status register */
	if (offset <= 2 && offset + size > 2)
	{
		UINT8 old = ide->bus_master_status;
		UINT8 val = data >> (8 * (2 - offset));
		
		/* save the DMA capable bits */
		ide->bus_master_status = (old & 0x9f) | (val & 0x60);
		
		/* clear interrupt and error bits */
		if (val & IDE_BUSMASTER_STATUS_IRQ)
			ide->bus_master_status &= ~IDE_BUSMASTER_STATUS_IRQ;
		if (val & IDE_BUSMASTER_STATUS_ERROR)
			ide->bus_master_status &= ~IDE_BUSMASTER_STATUS_ERROR;
	}
	
	/* descriptor table register */
	if (offset == 4)
		ide->bus_master_descriptor = data & 0xfffffffc;
}



/*************************************
 *
 *	IDE direct handlers (16-bit)
 *
 *************************************/

/*
	ide_bus_0_r()

	Read a 16-bit word from the IDE bus directly.

	select: 0->CS1Fx active, 1->CS3Fx active
	offset: register offset (state of DA2-DA0)
*/
int ide_bus_0_r(int select, int offset)
{
	/*int shift;*/

	offset += select ? 0x3f0 : 0x1f0;
	/*if (offset == 0x1f0)
	{
		return ide_controller32_0_r(offset >> 2, 0xffff0000);
	}
	else
	{
		shift = (offset & 3) * 8;
		return (ide_controller32_0_r(offset >> 2, ~ (0xff << shift)) >> shift);
	}*/
	return ide_controller_read(&idestate[0], offset, (offset == 0x1f0) ? 2 : 1);
}

/*
	ide_bus_0_w()

	Write a 16-bit word to the IDE bus directly.

	select: 0->CS1Fx active, 1->CS3Fx active
	offset: register offset (state of DA2-DA0)
	data: data written (state of D0-D15 or D0-D7)
*/
void ide_bus_0_w(int select, int offset, int data)
{
	/*int shift;*/

	offset += select ? 0x3f0 : 0x1f0;
	/*if (offset == 0x1f0)
	{
		ide_controller32_0_w(offset >> 2, data, 0xffff0000);
	}
	else
	{
		shift = (offset & 3) * 8;
		ide_controller32_0_w(offset >> 2, data << shift, ~ (0xff << shift));
	}*/
	if (offset == 0x1f0)
		ide_controller_write(&idestate[0], offset, 2, data);
	else
		ide_controller_write(&idestate[0], offset, 1, data & 0xff);
}



/*************************************
 *
 *	32-bit IDE handlers
 *
 *************************************/

READ32_HANDLER( ide_controller32_0_r )
{
	int size;

	offset *= 4;
	size = convert_to_offset_and_size(&offset, mem_mask);

	return ide_controller_read(&idestate[0], offset, size) << ((offset & 3) * 8);
}


WRITE32_HANDLER( ide_controller32_0_w )
{
	int size;

	offset *= 4;
	size = convert_to_offset_and_size(&offset, mem_mask);

	ide_controller_write(&idestate[0], offset, size, data >> ((offset & 3) * 8));
}


READ32_HANDLER( ide_bus_master32_0_r )
{
	int size;

	offset *= 4;
	size = convert_to_offset_and_size(&offset, mem_mask);

	return ide_bus_master_read(&idestate[0], offset, size) << ((offset & 3) * 8);
}


WRITE32_HANDLER( ide_bus_master32_0_w )
{
	int size;

	offset *= 4;
	size = convert_to_offset_and_size(&offset, mem_mask);

	ide_bus_master_write(&idestate[0], offset, size, data >> ((offset & 3) * 8));
}



/*************************************
 *
 *	16-bit IDE handlers
 *
 *************************************/

READ16_HANDLER( ide_controller16_0_r )
{
	int size;

	offset *= 2;
	size = convert_to_offset_and_size(&offset, mem_mask);

	return ide_controller_read(&idestate[0], offset, size) << ((offset & 1) * 8);
}


WRITE16_HANDLER( ide_controller16_0_w )
{
	int size;

	offset *= 2;
	size = convert_to_offset_and_size(&offset, mem_mask);

	ide_controller_write(&idestate[0], offset, size, data >> ((offset & 1) * 8));
}
