/* Analog Devices ADSP-2106x SHARC emulator v2.0

   Written by Ville Linde
*/

#include "sharc.h"
#include "debugger.h"

static offs_t sharc_dasm(char *buffer, offs_t pc);
static void sharc_dma_exec(int channel);
static void check_interrupts(void);

enum
{
	SHARC_PC=1,		SHARC_PCSTK,	SHARC_MODE1,	SHARC_MODE2,
	SHARC_ASTAT,	SHARC_STKY,		SHARC_IRPTL,	SHARC_IMASK,
	SHARC_IMASKP,	SHARC_USTAT1,	SHARC_USTAT2,	SHARC_LCNTR,
	SHARC_R0,		SHARC_R1,		SHARC_R2,		SHARC_R3,
	SHARC_R4,		SHARC_R5,		SHARC_R6,		SHARC_R7,
	SHARC_R8,		SHARC_R9,		SHARC_R10,		SHARC_R11,
	SHARC_R12,		SHARC_R13,		SHARC_R14,		SHARC_R15,
	SHARC_SYSCON,	SHARC_SYSSTAT,	SHARC_MRF,		SHARC_MRB,
	SHARC_STSTKP,	SHARC_PCSTKP,	SHARC_LSTKP,
	SHARC_FADDR,	SHARC_DADDR,
	SHARC_I0,		SHARC_I1,		SHARC_I2,		SHARC_I3,
	SHARC_I4,		SHARC_I5,		SHARC_I6,		SHARC_I7,
	SHARC_I8,		SHARC_I9,		SHARC_I10,		SHARC_I11,
	SHARC_I12,		SHARC_I13,		SHARC_I14,		SHARC_I15,
	SHARC_M0,		SHARC_M1,		SHARC_M2,		SHARC_M3,
	SHARC_M4,		SHARC_M5,		SHARC_M6,		SHARC_M7,
	SHARC_M8,		SHARC_M9,		SHARC_M10,		SHARC_M11,
	SHARC_M12,		SHARC_M13,		SHARC_M14,		SHARC_M15,
	SHARC_L0,		SHARC_L1,		SHARC_L2,		SHARC_L3,
	SHARC_L4,		SHARC_L5,		SHARC_L6,		SHARC_L7,
	SHARC_L8,		SHARC_L9,		SHARC_L10,		SHARC_L11,
	SHARC_L12,		SHARC_L13,		SHARC_L14,		SHARC_L15,
	SHARC_B0,		SHARC_B1,		SHARC_B2,		SHARC_B3,
	SHARC_B4,		SHARC_B5,		SHARC_B6,		SHARC_B7,
	SHARC_B8,		SHARC_B9,		SHARC_B10,		SHARC_B11,
	SHARC_B12,		SHARC_B13,		SHARC_B14,		SHARC_B15,
};

typedef struct
{
	UINT32 i[8];
	UINT32 m[8];
	UINT32 b[8];
	UINT32 l[8];
} SHARC_DAG;

typedef union
{
	INT32 r;
	float f;
} SHARC_REG;

typedef struct
{
	UINT32 control;
	UINT32 int_index;
	UINT32 int_modifier;
	UINT32 int_count;
	UINT32 chain_ptr;
	UINT32 gen_purpose;
	UINT32 ext_index;
	UINT32 ext_modifier;
	UINT32 ext_count;
} DMA_REGS;

typedef struct
{
	UINT32 pc;
	SHARC_REG r[16];
	SHARC_REG reg_alt[16];
	UINT64 mrf;
	UINT64 mrb;

	UINT32 pcstack[32];
	UINT32 lcstack[6];
	UINT32 lastack[6];
	UINT32 lstkp;

	UINT32 faddr;
	UINT32 daddr;
	UINT32 pcstk;
	UINT32 pcstkp;
	UINT32 laddr;
	UINT32 curlcntr;
	UINT32 lcntr;

	/* Data Address Generator (DAG) */
	SHARC_DAG dag1;		// (DM bus)
	SHARC_DAG dag2;		// (PM bus)
	SHARC_DAG dag1_alt;
	SHARC_DAG dag2_alt;

	DMA_REGS dma[12];

	/* System registers */
	UINT32 mode1;
	UINT32 mode2;
	UINT32 astat;
	UINT32 stky;
	UINT32 irptl;
	UINT32 imask;
	UINT32 imaskp;
	UINT32 ustat1;
	UINT32 ustat2;

	UINT32 flag[4];

	UINT32 syscon;
	UINT32 sysstat;

	struct
	{
		UINT32 mode1;
		UINT32 astat;
	} status_stack[5];
	int status_stkp;

	UINT64 px;

	UINT16 *internal_ram;
	UINT16 *internal_ram_block0, *internal_ram_block1;
	int internal_ram_size;

	int (*irq_callback)(int irqline);
	void (*opcode_handler)(void);
	UINT64 opcode;
	UINT64 fetch_opcode;
	UINT64 decode_opcode;

	UINT32 nfaddr;

	int idle;
	int irq_active;
	int irq_active_num;

	SHARC_BOOT_MODE boot_mode;
} SHARC_REGS;


static SHARC_REGS sharc;
static int sharc_icount;

static void (* sharc_op[512])(void);



static UINT32 dmaop_src;
static UINT32 dmaop_dst;
static UINT32 dmaop_chain_ptr;
static int dmaop_src_modifier;
static int dmaop_dst_modifier;
static int dmaop_src_count;
static int dmaop_dst_count;
static int dmaop_pmode;
static int dmaop_cycles = 0;
static int dmaop_channel;
static int dmaop_chained_direction;



#define ROPCODE(pc)		((UINT64)(sharc.internal_ram[((pc-0x20000) * 3) + 0]) << 32) | \
						((UINT64)(sharc.internal_ram[((pc-0x20000) * 3) + 1]) << 16) | \
						((UINT64)(sharc.internal_ram[((pc-0x20000) * 3) + 2]) << 0)

INLINE void CHANGE_PC(UINT32 newpc)
{
	sharc.pc = newpc;
	sharc.daddr = newpc;
	sharc.faddr = newpc+1;
	sharc.nfaddr = newpc+2;

	// next instruction to be executed
	sharc.decode_opcode = ROPCODE(sharc.daddr);
	// next instruction to be decoded
	sharc.fetch_opcode = ROPCODE(sharc.faddr);
}

INLINE void CHANGE_PC_DELAYED(UINT32 newpc)
{
	sharc.nfaddr = newpc;
}





static int iop_latency_cycles = 0;
static int iop_latency_reg = 0;
static UINT32 iop_latency_data = 0;


static void add_iop_write_latency_effect(int iop_reg, UINT32 data, int latency)
{
	iop_latency_cycles = latency+1;
	iop_latency_reg = iop_reg;
	iop_latency_data = data;
}

static void iop_write_latency_effect(void)
{
	UINT32 data = iop_latency_data;

	switch (iop_latency_reg)
	{
		case 0x1c:
		{
			if (data & 0x1)
			{
				sharc_dma_exec(6);
			}
			break;
		}

		case 0x1d:
		{
			if (data & 0x1)
			{
				sharc_dma_exec(7);
			}
			break;
		}

		default:	fatalerror("SHARC: iop_write_latency_effect: unknown IOP register %02X", iop_latency_reg);
	}
}



/* IOP registers */
static UINT32 sharc_iop_r(UINT32 address)
{
	switch (address)
	{
		case 0x00: return 0;	// System configuration

		case 0x37:		// DMA status
		{
			UINT32 r = 0;
			if (dmaop_cycles > 0)
			{
				r |= 1 << dmaop_channel;
			}
			return r;
		}
		default:		fatalerror("sharc_iop_r: Unimplemented IOP reg %02X", address);
	}
	return 0;
}

static void sharc_iop_w(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x00: break;		// System configuration
		case 0x02: break;		// External Memory Wait State Configuration

		case 0x08: break;		// Message Register 0
		case 0x09: break;		// Message Register 1
		case 0x0a: break;		// Message Register 2
		case 0x0b: break;		// Message Register 3
		case 0x0c: break;		// Message Register 4
		case 0x0d: break;		// Message Register 5
		case 0x0e: break;		// Message Register 6
		case 0x0f: break;		// Message Register 7

		// DMA 6
		case 0x1c:
		{
			sharc.dma[6].control = data;
			add_iop_write_latency_effect(0x1c, data, 1);
			break;
		}

		case 0x20: break;

		case 0x40: sharc.dma[6].int_index = data; return;
		case 0x41: sharc.dma[6].int_modifier = data; return;
		case 0x42: sharc.dma[6].int_count = data; return;
		case 0x43: sharc.dma[6].chain_ptr = data; return;
		case 0x44: sharc.dma[6].gen_purpose = data; return;
		case 0x45: sharc.dma[6].ext_index = data; return;
		case 0x46: sharc.dma[6].ext_modifier = data; return;
		case 0x47: sharc.dma[6].ext_count = data; return;

		// DMA 7
		case 0x1d:
		{
			sharc.dma[7].control = data;
			add_iop_write_latency_effect(0x1d, data, 20);	// does this really need to be 20 cycles!!?
			break;
		}

		case 0x48: sharc.dma[7].int_index = data; return;
		case 0x49: sharc.dma[7].int_modifier = data; return;
		case 0x4a: sharc.dma[7].int_count = data; return;
		case 0x4b: sharc.dma[7].chain_ptr = data; return;
		case 0x4c: sharc.dma[7].gen_purpose = data; return;
		case 0x4d: sharc.dma[7].ext_index = data; return;
		case 0x4e: sharc.dma[7].ext_modifier = data; return;
		case 0x4f: sharc.dma[7].ext_count = data; return;

		default:		fatalerror("sharc_iop_w: Unimplemented IOP reg %02X, %08X", address, data);
	}
}


#include "sharcmem.c"
#include "sharcdma.c"
#include "sharcops.c"
#include "sharcops.h"



static void build_opcode_table(void)
{
	int i, j;
	int num_ops = sizeof(sharc_opcode_table) / sizeof(SHARC_OP);

	for (i=0; i < 512; i++)
	{
		sharc_op[i] = sharcop_unimplemented;
	}

	for (i=0; i < 512; i++)
	{
		UINT16 op = i << 7;

		for (j=0; j < num_ops; j++)
		{
			if ((sharc_opcode_table[j].op_mask & op) == sharc_opcode_table[j].op_bits)
			{
				if (sharc_op[i] != sharcop_unimplemented)
				{
					fatalerror("build_opcode_table: table already filled! (i=%04X, j=%d)\n", i, j);
				}
				else
				{
					sharc_op[i] = sharc_opcode_table[j].handler;
				}
			}
		}
	}
}

/*****************************************************************************/

void sharc_external_iop_write(UINT32 address, UINT32 data)
{
	if (address == 0x1c)
	{
		if (data != 0)
		{
			sharc.dma[6].control = data;
		}
	}
	else
	{
		mame_printf_debug("SHARC IOP write %08X, %08X\n", address, data);
		sharc_iop_w(address, data);
	}
}

void sharc_external_dma_write(UINT32 address, UINT64 data)
{
	switch ((sharc.dma[6].control >> 6) & 0x3)
	{
		case 2:			// 16/48 packing
		{
			int shift = address % 3;
			UINT64 r = pm_read48(sharc.dma[6].int_index);

			r &= ~((UINT64)(0xffff) << (shift*16));
			r |= (data & 0xffff) << (shift*16);

			pm_write48(sharc.dma[6].int_index, r);

			if (shift == 2)
			{

				sharc.dma[6].int_index += sharc.dma[6].int_modifier;
			}
			break;
		}
		default:
		{
			fatalerror("sharc_external_dma_write: unimplemented packing mode %d\n", (sharc.dma[6].control >> 6) & 0x3);
		}
	}
}

static offs_t sharc_dasm(char *buffer, offs_t pc)
{
	UINT64 op = 0;

	if (pc >= 0x20000 && pc < 0x30000)
	{
		op = ((UINT64)(sharc.internal_ram[((pc - 0x20000) * 3) + 0]) << 32) |
			 ((UINT64)(sharc.internal_ram[((pc - 0x20000) * 3) + 1]) << 16) |
			 ((UINT64)(sharc.internal_ram[((pc - 0x20000) * 3) + 2]) <<  0);
	}

#ifdef MAME_DEBUG
	sharc_dasm_one(buffer, pc, op);
#else
	sprintf(buffer, "$%04X%08X", (UINT32)((op >> 32) & 0xffff), (UINT32)(op));
#endif

	return 1;
}


static void sharc_init(int index, int clock, const void *config, int (*irqcallback)(int))
{
	const sharc_config *cfg = config;
	sharc.boot_mode = cfg->boot_mode;

	sharc.irq_callback = irqcallback;

	build_opcode_table();

	sharc.internal_ram = auto_malloc(2 * 0x10000 * sizeof(UINT16));		// 2x 128KB
	sharc.internal_ram_block0 = &sharc.internal_ram[0];
	sharc.internal_ram_block1 = &sharc.internal_ram[0x20000/2];
}

static void sharc_reset(void)
{
	memset(sharc.internal_ram, 0, 2 * 0x10000 * sizeof(UINT16));

	switch(sharc.boot_mode)
	{
		case BOOT_MODE_EPROM:
		{
			sharc.dma[6].int_index		= 0x20000;
			sharc.dma[6].int_modifier	= 1;
			sharc.dma[6].int_count		= 0x100;
			sharc.dma[6].ext_index		= 0x400000;
			sharc.dma[6].ext_modifier	= 1;
			sharc.dma[6].ext_count		= 0x600;
			sharc.dma[6].control		= 0x2a1;

			sharc_dma_exec(6);
			dma_op(dmaop_src, dmaop_dst, dmaop_src_modifier, dmaop_dst_modifier,
				   dmaop_src_count, dmaop_dst_count, dmaop_pmode);
			dmaop_cycles = 0;

			break;
		}

		case BOOT_MODE_HOST:
			break;

		default:
			fatalerror("SHARC: Unimplemented boot mode %d", sharc.boot_mode);
	}

	sharc.pc = 0x20004;
	sharc.daddr = sharc.pc + 1;
	sharc.faddr = sharc.daddr + 1;
	sharc.nfaddr = sharc.faddr+1;

	sharc.idle = 0;
	sharc.stky = 0x5400000;
}

static void sharc_exit(void)
{
	/* TODO */
}

static void sharc_get_context(void *dst)
{
	/* copy the context */
	if (dst)
	{
		*(SHARC_REGS *)dst = sharc;
	}
}

static void sharc_set_context(void *src)
{
	/* copy the context */
	if (src)
	{
		sharc = *(SHARC_REGS *)src;
	}

	change_pc(sharc.pc);
}

static void sharc_set_irq_line(int irqline, int state)
{
	if (state)
	{
		sharc.irq_active = 1;
		sharc.irq_active_num = 8-irqline;
	}
}

void sharc_set_flag_input(int flag_num, int state)
{
	if (flag_num >= 0 && flag_num < 4)
	{
		// Check if flag is set to input in MODE2 (bit == 0)
		if ((sharc.mode2 & (1 << (flag_num+15))) == 0)
		{
			sharc.flag[flag_num] = state ? 1 : 0;
		}
		else
		{
			fatalerror("sharc_set_flag_input: flag %d is set output!", flag_num);
		}
	}
}

static void check_interrupts(void)
{
	if ((sharc.imask & (1 << sharc.irq_active_num)) && (sharc.mode1 & MODE1_IRPTEN) && sharc.irq_active)
	{
		PUSH_PC(sharc.pc+1);

		sharc.irptl |= 1 << sharc.irq_active_num;

		if (sharc.irq_active_num >= 6 && sharc.irq_active_num <= 8)
		{
			PUSH_STATUS_STACK();
		}

		CHANGE_PC(0x20000 + (sharc.irq_active_num * 0x4));
		/* TODO: alter IMASKP */

		sharc.irq_active = 0;
	}
}

static int sharc_execute(int cycles)
{
	sharc_icount = cycles;

	if (sharc.idle && sharc.irq_active == 0)
	{
		// handle pending DMA transfers
		if (dmaop_cycles > 0)
		{
			dmaop_cycles -= cycles;
			if (dmaop_cycles <= 0)
			{
				dmaop_cycles = 0;
				dma_op(dmaop_src, dmaop_dst, dmaop_src_modifier, dmaop_dst_modifier, dmaop_src_count, dmaop_dst_count, dmaop_pmode);
				if (dmaop_chain_ptr != 0)
				{
					schedule_chained_dma_op(dmaop_channel, dmaop_chain_ptr, dmaop_chained_direction);
				}
			}
		}

		sharc_icount = 0;
		CALL_MAME_DEBUG;

		return cycles;
	}
	if (sharc.irq_active != 0)
	{
		sharc.idle = 0;
		check_interrupts();
	}

	// fill the initial pipeline

	// next executed instruction
	sharc.opcode = ROPCODE(sharc.daddr);
	sharc.opcode_handler = sharc_op[(sharc.opcode >> 39) & 0x1ff];

	// next decoded instruction
	sharc.fetch_opcode = ROPCODE(sharc.faddr);

	while (sharc_icount > 0 && !sharc.idle)
	{
		sharc.pc = sharc.daddr;
		sharc.daddr = sharc.faddr;
		sharc.faddr = sharc.nfaddr;
		sharc.nfaddr++;

		sharc.decode_opcode = sharc.fetch_opcode;

		// fetch next instruction
		sharc.fetch_opcode = ROPCODE(sharc.faddr);

		CALL_MAME_DEBUG;

		// handle looping
		if (sharc.pc == (sharc.laddr & 0xffffff))
		{
			switch (sharc.laddr >> 30)
			{
				case 0:		// arithmetic condition-based
				{
					int condition = (sharc.laddr >> 24) & 0x1f;

					if (DO_CONDITION_CODE(condition))
					{
						POP_LOOP();
						POP_PC();
					}
					else
					{
						CHANGE_PC(TOP_PC());
					}
					break;
				}
				case 1:		// counter-based, length 1
				{
					//fatalerror("SHARC: counter-based loop, length 1 at %08X", sharc.pc);
					//break;
				}
				case 2:		// counter-based, length 2
				{
					//fatalerror("SHARC: counter-based loop, length 2 at %08X", sharc.pc);
					//break;
				}
				case 3:		// counter-based, length >2
				{
					--sharc.lcstack[sharc.lstkp];
					--sharc.curlcntr;
					if (sharc.curlcntr == 0)
					{
						POP_LOOP();
						POP_PC();
					}
					else
					{
						CHANGE_PC(TOP_PC());
					}
				}
			}
		}

		// execute current instruction
		sharc.opcode_handler();

		// decode next instruction
		sharc.opcode = sharc.decode_opcode;
		sharc.opcode_handler = sharc_op[(sharc.opcode >> 39) & 0x1ff];




		// System register latency effect
		if (systemreg_latency_cycles > 0)
		{
			--systemreg_latency_cycles;
			if (systemreg_latency_cycles <= 0)
			{
				systemreg_write_latency_effect();
			}
		}

		// IOP register latency effect
		if (iop_latency_cycles > 0)
		{
			--iop_latency_cycles;
			if (iop_latency_cycles <= 0)
			{
				iop_write_latency_effect();
			}
		}

		// DMA transfer
		if (dmaop_cycles > 0)
		{
			--dmaop_cycles;
			if (dmaop_cycles <= 0)
			{
				dma_op(dmaop_src, dmaop_dst, dmaop_src_modifier, dmaop_dst_modifier, dmaop_src_count, dmaop_dst_count, dmaop_pmode);
				if (dmaop_chain_ptr != 0)
				{
					schedule_chained_dma_op(dmaop_channel, dmaop_chain_ptr, dmaop_chained_direction);
				}
			}
		}



		--sharc_icount;
	};

	return cycles - sharc_icount;
}

/*****************************************************************************/

/* Debugger definitions */

static UINT8 sharc_reg_layout[] =
{
	SHARC_PC,		SHARC_PCSTK,	-1,
	SHARC_FADDR,	SHARC_DADDR,	-1,
	SHARC_IMASK,	SHARC_ASTAT,	-1,
	SHARC_LCNTR,	SHARC_SYSSTAT,	-1,
	SHARC_R0,		SHARC_R8,		-1,
	SHARC_R1,		SHARC_R9,		-1,
	SHARC_R2,		SHARC_R10,		-1,
	SHARC_R3,		SHARC_R11,		-1,
	SHARC_R4,		SHARC_R12,		-1,
	SHARC_R5,		SHARC_R13,		-1,
	SHARC_R6,		SHARC_R14,		-1,
	SHARC_R7,		SHARC_R15,		0
};

static UINT8 sharc_win_layout[] =
{
	 0,16,34,17,	/* register window (top rows) */
	 0, 0,80,15,	/* disassembler window (left colums) */
	35,16,45, 2,	/* memory #2 window (right, lower middle) */
	35,19,45, 3,	/* memory #1 window (right, upper middle) */
	 0,23,80, 1,	/* command line window (bottom rows) */
};

/**************************************************************************
 * Generic set_info
 **************************************************************************/

static void sharc_set_info(UINT32 state, union cpuinfo *info)
{
	switch (state)
	{
		case CPUINFO_INT_PC:
		case CPUINFO_INT_REGISTER + SHARC_PC:			sharc.pc = info->i;						break;
		case CPUINFO_INT_REGISTER + SHARC_FADDR:		sharc.faddr = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_DADDR:		sharc.daddr = info->i;					break;

		case CPUINFO_INT_REGISTER + SHARC_R0:			sharc.r[0].r = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_R1:			sharc.r[1].r = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_R2:			sharc.r[2].r = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_R3:			sharc.r[3].r = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_R4:			sharc.r[4].r = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_R5:			sharc.r[5].r = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_R6:			sharc.r[6].r = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_R7:			sharc.r[7].r = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_R8:			sharc.r[8].r = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_R9:			sharc.r[9].r = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_R10:			sharc.r[10].r = info->i;				break;
		case CPUINFO_INT_REGISTER + SHARC_R11:			sharc.r[11].r = info->i;				break;
		case CPUINFO_INT_REGISTER + SHARC_R12:			sharc.r[12].r = info->i;				break;
		case CPUINFO_INT_REGISTER + SHARC_R13:			sharc.r[13].r = info->i;				break;
		case CPUINFO_INT_REGISTER + SHARC_R14:			sharc.r[14].r = info->i;				break;
		case CPUINFO_INT_REGISTER + SHARC_R15:			sharc.r[15].r = info->i;				break;

		case CPUINFO_INT_REGISTER + SHARC_I0:			sharc.dag1.i[0] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I1:			sharc.dag1.i[1] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I2:			sharc.dag1.i[2] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I3:			sharc.dag1.i[3] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I4:			sharc.dag1.i[4] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I5:			sharc.dag1.i[5] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I6:			sharc.dag1.i[6] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I7:			sharc.dag1.i[7] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I8:			sharc.dag2.i[0] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I9:			sharc.dag2.i[1] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I10:			sharc.dag2.i[2] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I11:			sharc.dag2.i[3] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I12:			sharc.dag2.i[4] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I13:			sharc.dag2.i[5] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I14:			sharc.dag2.i[6] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_I15:			sharc.dag2.i[7] = info->i;					break;

		case CPUINFO_INT_REGISTER + SHARC_M0:			sharc.dag1.m[0] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M1:			sharc.dag1.m[1] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M2:			sharc.dag1.m[2] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M3:			sharc.dag1.m[3] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M4:			sharc.dag1.m[4] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M5:			sharc.dag1.m[5] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M6:			sharc.dag1.m[6] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M7:			sharc.dag1.m[7] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M8:			sharc.dag2.m[0] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M9:			sharc.dag2.m[1] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M10:			sharc.dag2.m[2] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M11:			sharc.dag2.m[3] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M12:			sharc.dag2.m[4] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M13:			sharc.dag2.m[5] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M14:			sharc.dag2.m[6] = info->i;					break;
		case CPUINFO_INT_REGISTER + SHARC_M15:			sharc.dag2.m[7] = info->i;					break;
	}
}

#if (HAS_ADSP21062)
void adsp21062_set_info(UINT32 state, union cpuinfo *info)
{
	if (state >= CPUINFO_INT_INPUT_STATE && state <= CPUINFO_INT_INPUT_STATE + 2)
	{
		sharc_set_irq_line(state-CPUINFO_INT_INPUT_STATE, info->i);
		return;
	}
	else if (state >= CPUINFO_INT_INPUT_STATE + SHARC_INPUT_FLAG0 && state <= CPUINFO_INT_INPUT_STATE + SHARC_INPUT_FLAG3)
	{
		sharc_set_flag_input(state-(CPUINFO_INT_INPUT_STATE + SHARC_INPUT_FLAG0), info->i);
		return;
	}
	switch(state)
	{
		default:	sharc_set_info(state, info);		break;
	}
}
#endif


static int sharc_debug_read(int space, UINT32 offset, int size, UINT64 *value)
{
	offset >>= 2;

	if (space == ADDRESS_SPACE_PROGRAM)
	{
		if (offset >= 0x20000 && offset < 0x30000)
		{
			*value = pm_read48(offset);
		}
		else
		{
			*value = 0;
		}
	}
	else if (space == ADDRESS_SPACE_DATA)
	{
		if (offset >= 0x20000)
		{
			*value = dm_read32(offset/4);
		}
		else
		{
			offset = 0;
		}
	}
	return 1;
}

static int sharc_debug_readop(UINT32 offset, int size, UINT64 *value)
{
	offset >>= 2;

	if (offset >= 0x20000 && offset < 0x28000)
	{
		UINT64 op = ((UINT64)(sharc.internal_ram_block0[((offset-0x20000) * 3) + 0]) << 32) |
					((UINT64)(sharc.internal_ram_block0[((offset-0x20000) * 3) + 1]) << 16) |
					((UINT64)(sharc.internal_ram_block0[((offset-0x20000) * 3) + 2]) << 0);
		*value = op;
	}
	else if (offset >= 0x28000 && offset < 0x30000)
	{
		UINT64 op = ((UINT64)(sharc.internal_ram_block1[((offset-0x28000) * 3) + 0]) << 32) |
					((UINT64)(sharc.internal_ram_block1[((offset-0x28000) * 3) + 1]) << 16) |
					((UINT64)(sharc.internal_ram_block1[((offset-0x28000) * 3) + 2]) << 0);
		*value = op;
	}

	return 1;
}

// This is just used to stop the debugger from complaining about executing from I/O space
static ADDRESS_MAP_START( internal_pgm, ADDRESS_SPACE_PROGRAM, 32 )
	AM_RANGE(0x20000, 0x7ffff) AM_RAM
ADDRESS_MAP_END

void sharc_get_info(UINT32 state, union cpuinfo *info)
{
	switch(state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_CONTEXT_SIZE:					info->i = sizeof(sharc);				break;
		case CPUINFO_INT_INPUT_LINES:					info->i = 32;							break;
		case CPUINFO_INT_DEFAULT_IRQ_VECTOR:			info->i = 0;							break;
		case CPUINFO_INT_ENDIANNESS:					info->i = CPU_IS_LE;					break;
		case CPUINFO_INT_CLOCK_DIVIDER:					info->i = 1;							break;
		case CPUINFO_INT_MIN_INSTRUCTION_BYTES:			info->i = 8;							break;
		case CPUINFO_INT_MAX_INSTRUCTION_BYTES:			info->i = 8;							break;
		case CPUINFO_INT_MIN_CYCLES:					info->i = 1;							break;
		case CPUINFO_INT_MAX_CYCLES:					info->i = 40;							break;

		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_PROGRAM:	info->i = 32;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_PROGRAM: info->i = 24;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_PROGRAM: info->i = -2;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_DATA:	info->i = 32;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_DATA: 	info->i = 32;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_DATA: 	info->i = -2;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_IO:		info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_IO: 		info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_IO: 		info->i = 0;					break;

		case CPUINFO_INT_INPUT_STATE:			info->i = CLEAR_LINE;	break;

		case CPUINFO_INT_PREVIOUSPC:					/* not implemented */					break;

		case CPUINFO_INT_PC:
		case CPUINFO_INT_REGISTER + SHARC_PC:			info->i = sharc.pc;						break;
		case CPUINFO_INT_REGISTER + SHARC_PCSTK:		info->i = sharc.pcstk;					break;
		case CPUINFO_INT_REGISTER + SHARC_PCSTKP:		info->i = sharc.pcstkp;					break;
		case CPUINFO_INT_REGISTER + SHARC_LSTKP:		info->i = sharc.lstkp;					break;
		case CPUINFO_INT_REGISTER + SHARC_FADDR:		info->i = sharc.faddr;					break;
		case CPUINFO_INT_REGISTER + SHARC_DADDR:		info->i = sharc.daddr;					break;
		case CPUINFO_INT_REGISTER + SHARC_MODE1:		info->i = sharc.mode1;					break;
		case CPUINFO_INT_REGISTER + SHARC_MODE2:		info->i = sharc.mode2;					break;
		case CPUINFO_INT_REGISTER + SHARC_ASTAT:		info->i = sharc.astat;					break;
		case CPUINFO_INT_REGISTER + SHARC_IRPTL:		info->i = sharc.irptl;					break;
		case CPUINFO_INT_REGISTER + SHARC_IMASK:		info->i = sharc.imask;					break;
		case CPUINFO_INT_REGISTER + SHARC_USTAT1:		info->i = sharc.ustat1;					break;
		case CPUINFO_INT_REGISTER + SHARC_USTAT2:		info->i = sharc.ustat2;					break;
		case CPUINFO_INT_REGISTER + SHARC_STSTKP:		info->i = sharc.status_stkp;			break;

		case CPUINFO_INT_REGISTER + SHARC_R0:			info->i = sharc.r[0].r;					break;
		case CPUINFO_INT_REGISTER + SHARC_R1:			info->i = sharc.r[1].r;					break;
		case CPUINFO_INT_REGISTER + SHARC_R2:			info->i = sharc.r[2].r;					break;
		case CPUINFO_INT_REGISTER + SHARC_R3:			info->i = sharc.r[3].r;					break;
		case CPUINFO_INT_REGISTER + SHARC_R4:			info->i = sharc.r[4].r;					break;
		case CPUINFO_INT_REGISTER + SHARC_R5:			info->i = sharc.r[5].r;					break;
		case CPUINFO_INT_REGISTER + SHARC_R6:			info->i = sharc.r[6].r;					break;
		case CPUINFO_INT_REGISTER + SHARC_R7:			info->i = sharc.r[7].r;					break;
		case CPUINFO_INT_REGISTER + SHARC_R8:			info->i = sharc.r[8].r;					break;
		case CPUINFO_INT_REGISTER + SHARC_R9:			info->i = sharc.r[9].r;					break;
		case CPUINFO_INT_REGISTER + SHARC_R10:			info->i = sharc.r[10].r;				break;
		case CPUINFO_INT_REGISTER + SHARC_R11:			info->i = sharc.r[11].r;				break;
		case CPUINFO_INT_REGISTER + SHARC_R12:			info->i = sharc.r[12].r;				break;
		case CPUINFO_INT_REGISTER + SHARC_R13:			info->i = sharc.r[13].r;				break;
		case CPUINFO_INT_REGISTER + SHARC_R14:			info->i = sharc.r[14].r;				break;
		case CPUINFO_INT_REGISTER + SHARC_R15:			info->i = sharc.r[15].r;				break;

		case CPUINFO_INT_REGISTER + SHARC_I0:			info->i = sharc.dag1.i[0];					break;
		case CPUINFO_INT_REGISTER + SHARC_I1:			info->i = sharc.dag1.i[1];					break;
		case CPUINFO_INT_REGISTER + SHARC_I2:			info->i = sharc.dag1.i[2];					break;
		case CPUINFO_INT_REGISTER + SHARC_I3:			info->i = sharc.dag1.i[3];					break;
		case CPUINFO_INT_REGISTER + SHARC_I4:			info->i = sharc.dag1.i[4];					break;
		case CPUINFO_INT_REGISTER + SHARC_I5:			info->i = sharc.dag1.i[5];					break;
		case CPUINFO_INT_REGISTER + SHARC_I6:			info->i = sharc.dag1.i[6];					break;
		case CPUINFO_INT_REGISTER + SHARC_I7:			info->i = sharc.dag1.i[7];					break;
		case CPUINFO_INT_REGISTER + SHARC_I8:			info->i = sharc.dag2.i[0];					break;
		case CPUINFO_INT_REGISTER + SHARC_I9:			info->i = sharc.dag2.i[1];					break;
		case CPUINFO_INT_REGISTER + SHARC_I10:			info->i = sharc.dag2.i[2];					break;
		case CPUINFO_INT_REGISTER + SHARC_I11:			info->i = sharc.dag2.i[3];					break;
		case CPUINFO_INT_REGISTER + SHARC_I12:			info->i = sharc.dag2.i[4];					break;
		case CPUINFO_INT_REGISTER + SHARC_I13:			info->i = sharc.dag2.i[5];					break;
		case CPUINFO_INT_REGISTER + SHARC_I14:			info->i = sharc.dag2.i[6];					break;
		case CPUINFO_INT_REGISTER + SHARC_I15:			info->i = sharc.dag2.i[7];					break;

		case CPUINFO_INT_REGISTER + SHARC_M0:			info->i = sharc.dag1.m[0];					break;
		case CPUINFO_INT_REGISTER + SHARC_M1:			info->i = sharc.dag1.m[1];					break;
		case CPUINFO_INT_REGISTER + SHARC_M2:			info->i = sharc.dag1.m[2];					break;
		case CPUINFO_INT_REGISTER + SHARC_M3:			info->i = sharc.dag1.m[3];					break;
		case CPUINFO_INT_REGISTER + SHARC_M4:			info->i = sharc.dag1.m[4];					break;
		case CPUINFO_INT_REGISTER + SHARC_M5:			info->i = sharc.dag1.m[5];					break;
		case CPUINFO_INT_REGISTER + SHARC_M6:			info->i = sharc.dag1.m[6];					break;
		case CPUINFO_INT_REGISTER + SHARC_M7:			info->i = sharc.dag1.m[7];					break;
		case CPUINFO_INT_REGISTER + SHARC_M8:			info->i = sharc.dag2.m[0];					break;
		case CPUINFO_INT_REGISTER + SHARC_M9:			info->i = sharc.dag2.m[1];					break;
		case CPUINFO_INT_REGISTER + SHARC_M10:			info->i = sharc.dag2.m[2];					break;
		case CPUINFO_INT_REGISTER + SHARC_M11:			info->i = sharc.dag2.m[3];					break;
		case CPUINFO_INT_REGISTER + SHARC_M12:			info->i = sharc.dag2.m[4];					break;
		case CPUINFO_INT_REGISTER + SHARC_M13:			info->i = sharc.dag2.m[5];					break;
		case CPUINFO_INT_REGISTER + SHARC_M14:			info->i = sharc.dag2.m[6];					break;
		case CPUINFO_INT_REGISTER + SHARC_M15:			info->i = sharc.dag2.m[7];					break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_GET_CONTEXT:					info->getcontext = sharc_get_context;	break;
		case CPUINFO_PTR_SET_CONTEXT:					info->setcontext = sharc_set_context;	break;
		case CPUINFO_PTR_INIT:							info->init = sharc_init;				break;
		case CPUINFO_PTR_RESET:							info->reset = sharc_reset;				break;
		case CPUINFO_PTR_EXIT:							info->exit = sharc_exit;				break;
		case CPUINFO_PTR_EXECUTE:						info->execute = sharc_execute;			break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = sharc_dasm;			break;
		case CPUINFO_PTR_INSTRUCTION_COUNTER:			info->icount = &sharc_icount;			break;
		case CPUINFO_PTR_REGISTER_LAYOUT:				info->p = sharc_reg_layout;				break;
		case CPUINFO_PTR_WINDOW_LAYOUT:					info->p = sharc_win_layout;				break;
		case CPUINFO_PTR_READ:							info->read = sharc_debug_read;			break;
		case CPUINFO_PTR_READOP:						info->readop = sharc_debug_readop;		break;
		case CPUINFO_PTR_INTERNAL_MEMORY_MAP + ADDRESS_SPACE_PROGRAM: info->internal_map = construct_map_internal_pgm; break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_CORE_FAMILY:					strcpy(info->s = cpuintrf_temp_str(), "SHARC"); break;
		case CPUINFO_STR_CORE_VERSION:					strcpy(info->s = cpuintrf_temp_str(), "2.0"); break;
		case CPUINFO_STR_CORE_FILE:						strcpy(info->s = cpuintrf_temp_str(), __FILE__); break;
		case CPUINFO_STR_CORE_CREDITS:					strcpy(info->s = cpuintrf_temp_str(), "Copyright (C) 2004-2006 Ville Linde"); break;

		case CPUINFO_STR_FLAGS:							strcpy(info->s = cpuintrf_temp_str(), " "); break;

		case CPUINFO_STR_REGISTER + SHARC_PC:			sprintf(info->s = cpuintrf_temp_str(), "PC: %08X", sharc.pc); break;
		case CPUINFO_STR_REGISTER + SHARC_PCSTK:		sprintf(info->s = cpuintrf_temp_str(), "PCSTK: %08X", sharc.pcstk); break;
		case CPUINFO_STR_REGISTER + SHARC_PCSTKP:		sprintf(info->s = cpuintrf_temp_str(), "PCSTKP: %08X", sharc.pcstkp); break;
		case CPUINFO_STR_REGISTER + SHARC_LSTKP:		sprintf(info->s = cpuintrf_temp_str(), "LSTKP: %08X", sharc.lstkp); break;
		case CPUINFO_STR_REGISTER + SHARC_FADDR:		sprintf(info->s = cpuintrf_temp_str(), "FADDR: %08X", sharc.faddr); break;
		case CPUINFO_STR_REGISTER + SHARC_DADDR:		sprintf(info->s = cpuintrf_temp_str(), "DADDR: %08X", sharc.daddr); break;
		case CPUINFO_STR_REGISTER + SHARC_MODE1:		sprintf(info->s = cpuintrf_temp_str(), "MODE1: %08X", sharc.mode1); break;
		case CPUINFO_STR_REGISTER + SHARC_MODE2:		sprintf(info->s = cpuintrf_temp_str(), "MODE2: %08X", sharc.mode2); break;
		case CPUINFO_STR_REGISTER + SHARC_ASTAT:		sprintf(info->s = cpuintrf_temp_str(), "ASTAT: %08X", sharc.astat); break;
		case CPUINFO_STR_REGISTER + SHARC_IRPTL:		sprintf(info->s = cpuintrf_temp_str(), "IRPTL: %08X", sharc.irptl); break;
		case CPUINFO_STR_REGISTER + SHARC_IMASK:		sprintf(info->s = cpuintrf_temp_str(), "IMASK: %08X", sharc.imask); break;
		case CPUINFO_STR_REGISTER + SHARC_USTAT1:		sprintf(info->s = cpuintrf_temp_str(), "USTAT1: %08X", sharc.ustat1); break;
		case CPUINFO_STR_REGISTER + SHARC_USTAT2:		sprintf(info->s = cpuintrf_temp_str(), "USTAT2: %08X", sharc.ustat2); break;
		case CPUINFO_STR_REGISTER + SHARC_STSTKP:		sprintf(info->s = cpuintrf_temp_str(), "STSTKP: %08X", sharc.status_stkp); break;

		case CPUINFO_STR_REGISTER + SHARC_R0:			sprintf(info->s = cpuintrf_temp_str(), "R0: %08X", (UINT32)sharc.r[0].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R1:			sprintf(info->s = cpuintrf_temp_str(), "R1: %08X", (UINT32)sharc.r[1].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R2:			sprintf(info->s = cpuintrf_temp_str(), "R2: %08X", (UINT32)sharc.r[2].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R3:			sprintf(info->s = cpuintrf_temp_str(), "R3: %08X", (UINT32)sharc.r[3].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R4:			sprintf(info->s = cpuintrf_temp_str(), "R4: %08X", (UINT32)sharc.r[4].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R5:			sprintf(info->s = cpuintrf_temp_str(), "R5: %08X", (UINT32)sharc.r[5].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R6:			sprintf(info->s = cpuintrf_temp_str(), "R6: %08X", (UINT32)sharc.r[6].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R7:			sprintf(info->s = cpuintrf_temp_str(), "R7: %08X", (UINT32)sharc.r[7].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R8:			sprintf(info->s = cpuintrf_temp_str(), "R8: %08X", (UINT32)sharc.r[8].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R9:			sprintf(info->s = cpuintrf_temp_str(), "R9: %08X", (UINT32)sharc.r[9].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R10:			sprintf(info->s = cpuintrf_temp_str(), "R10: %08X", (UINT32)sharc.r[10].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R11:			sprintf(info->s = cpuintrf_temp_str(), "R11: %08X", (UINT32)sharc.r[11].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R12:			sprintf(info->s = cpuintrf_temp_str(), "R12: %08X", (UINT32)sharc.r[12].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R13:			sprintf(info->s = cpuintrf_temp_str(), "R13: %08X", (UINT32)sharc.r[13].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R14:			sprintf(info->s = cpuintrf_temp_str(), "R14: %08X", (UINT32)sharc.r[14].r); break;
		case CPUINFO_STR_REGISTER + SHARC_R15:			sprintf(info->s = cpuintrf_temp_str(), "R15: %08X", (UINT32)sharc.r[15].r); break;

		case CPUINFO_STR_REGISTER + SHARC_I0:			sprintf(info->s = cpuintrf_temp_str(), "I0: %08X", (UINT32)sharc.dag1.i[0]); break;
		case CPUINFO_STR_REGISTER + SHARC_I1:			sprintf(info->s = cpuintrf_temp_str(), "I1: %08X", (UINT32)sharc.dag1.i[1]); break;
		case CPUINFO_STR_REGISTER + SHARC_I2:			sprintf(info->s = cpuintrf_temp_str(), "I2: %08X", (UINT32)sharc.dag1.i[2]); break;
		case CPUINFO_STR_REGISTER + SHARC_I3:			sprintf(info->s = cpuintrf_temp_str(), "I3: %08X", (UINT32)sharc.dag1.i[3]); break;
		case CPUINFO_STR_REGISTER + SHARC_I4:			sprintf(info->s = cpuintrf_temp_str(), "I4: %08X", (UINT32)sharc.dag1.i[4]); break;
		case CPUINFO_STR_REGISTER + SHARC_I5:			sprintf(info->s = cpuintrf_temp_str(), "I5: %08X", (UINT32)sharc.dag1.i[5]); break;
		case CPUINFO_STR_REGISTER + SHARC_I6:			sprintf(info->s = cpuintrf_temp_str(), "I6: %08X", (UINT32)sharc.dag1.i[6]); break;
		case CPUINFO_STR_REGISTER + SHARC_I7:			sprintf(info->s = cpuintrf_temp_str(), "I7: %08X", (UINT32)sharc.dag1.i[7]); break;
		case CPUINFO_STR_REGISTER + SHARC_I8:			sprintf(info->s = cpuintrf_temp_str(), "I8: %08X", (UINT32)sharc.dag2.i[0]); break;
		case CPUINFO_STR_REGISTER + SHARC_I9:			sprintf(info->s = cpuintrf_temp_str(), "I9: %08X", (UINT32)sharc.dag2.i[1]); break;
		case CPUINFO_STR_REGISTER + SHARC_I10:			sprintf(info->s = cpuintrf_temp_str(), "I10: %08X", (UINT32)sharc.dag2.i[2]); break;
		case CPUINFO_STR_REGISTER + SHARC_I11:			sprintf(info->s = cpuintrf_temp_str(), "I11: %08X", (UINT32)sharc.dag2.i[3]); break;
		case CPUINFO_STR_REGISTER + SHARC_I12:			sprintf(info->s = cpuintrf_temp_str(), "I12: %08X", (UINT32)sharc.dag2.i[4]); break;
		case CPUINFO_STR_REGISTER + SHARC_I13:			sprintf(info->s = cpuintrf_temp_str(), "I13: %08X", (UINT32)sharc.dag2.i[5]); break;
		case CPUINFO_STR_REGISTER + SHARC_I14:			sprintf(info->s = cpuintrf_temp_str(), "I14: %08X", (UINT32)sharc.dag2.i[6]); break;
		case CPUINFO_STR_REGISTER + SHARC_I15:			sprintf(info->s = cpuintrf_temp_str(), "I15: %08X", (UINT32)sharc.dag2.i[7]); break;

		case CPUINFO_STR_REGISTER + SHARC_M0:			sprintf(info->s = cpuintrf_temp_str(), "M0: %08X", (UINT32)sharc.dag1.m[0]); break;
		case CPUINFO_STR_REGISTER + SHARC_M1:			sprintf(info->s = cpuintrf_temp_str(), "M1: %08X", (UINT32)sharc.dag1.m[1]); break;
		case CPUINFO_STR_REGISTER + SHARC_M2:			sprintf(info->s = cpuintrf_temp_str(), "M2: %08X", (UINT32)sharc.dag1.m[2]); break;
		case CPUINFO_STR_REGISTER + SHARC_M3:			sprintf(info->s = cpuintrf_temp_str(), "M3: %08X", (UINT32)sharc.dag1.m[3]); break;
		case CPUINFO_STR_REGISTER + SHARC_M4:			sprintf(info->s = cpuintrf_temp_str(), "M4: %08X", (UINT32)sharc.dag1.m[4]); break;
		case CPUINFO_STR_REGISTER + SHARC_M5:			sprintf(info->s = cpuintrf_temp_str(), "M5: %08X", (UINT32)sharc.dag1.m[5]); break;
		case CPUINFO_STR_REGISTER + SHARC_M6:			sprintf(info->s = cpuintrf_temp_str(), "M6: %08X", (UINT32)sharc.dag1.m[6]); break;
		case CPUINFO_STR_REGISTER + SHARC_M7:			sprintf(info->s = cpuintrf_temp_str(), "M7: %08X", (UINT32)sharc.dag1.m[7]); break;
		case CPUINFO_STR_REGISTER + SHARC_M8:			sprintf(info->s = cpuintrf_temp_str(), "M8: %08X", (UINT32)sharc.dag2.m[0]); break;
		case CPUINFO_STR_REGISTER + SHARC_M9:			sprintf(info->s = cpuintrf_temp_str(), "M9: %08X", (UINT32)sharc.dag2.m[1]); break;
		case CPUINFO_STR_REGISTER + SHARC_M10:			sprintf(info->s = cpuintrf_temp_str(), "M10: %08X", (UINT32)sharc.dag2.m[2]); break;
		case CPUINFO_STR_REGISTER + SHARC_M11:			sprintf(info->s = cpuintrf_temp_str(), "M11: %08X", (UINT32)sharc.dag2.m[3]); break;
		case CPUINFO_STR_REGISTER + SHARC_M12:			sprintf(info->s = cpuintrf_temp_str(), "M12: %08X", (UINT32)sharc.dag2.m[4]); break;
		case CPUINFO_STR_REGISTER + SHARC_M13:			sprintf(info->s = cpuintrf_temp_str(), "M13: %08X", (UINT32)sharc.dag2.m[5]); break;
		case CPUINFO_STR_REGISTER + SHARC_M14:			sprintf(info->s = cpuintrf_temp_str(), "M14: %08X", (UINT32)sharc.dag2.m[6]); break;
		case CPUINFO_STR_REGISTER + SHARC_M15:			sprintf(info->s = cpuintrf_temp_str(), "M15: %08X", (UINT32)sharc.dag2.m[7]); break;
	}
}

#if (HAS_ADSP21062)
void adsp21062_get_info(UINT32 state, union cpuinfo *info)
{
	switch(state)
	{
		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_SET_INFO:						info->setinfo = adsp21062_set_info;		break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s = cpuintrf_temp_str(), "ADSP21062"); break;

		default:	sharc_get_info(state, info);		break;
	}
}
#endif
