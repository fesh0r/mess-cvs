#include "driver.h"
#include "includes/europc.h"
#include "includes/pc.h"
#include "includes/pit8253.h"
#include "bcd.h"
#include <time.h>

/*
  europc
  fe107 bios checksum test
   memory test
  fe145
   irq vector init
  fe156
  fe169 fd774 // test of special europc registers 254 354
  fe16c fe817
  fe16f
   fec08 // test of special europc registers 800a rtc time or date error, rtc corrected
    fef66 0xf
    fdb3e 0x8..0xc

  fe172 fecc5 // 801a video setup error
   copyright output
  fe1b7
  fe1be di bits set mean output text!!!,
   (801a)
   0x8000 output
        1 rtc error
		2 rtc time or date error
		4 checksum error in setup
		8 rtc status corrected
	   10 video setup error
	   20 video ram bad
	   40 monitor type not recogniced
	   80 mouse port enabled
	  100 joystick port enabled

  fe1e2 fdc0c cpu speed is 4.77 mhz
  fe1e5 ff9c0 keyboard processor error
  fe1eb fc617 external lpt1 at 0x3bc
  fe1ee fe8ee external coms at

  routines:
  fc92d output text at bp
  fdb3e rtc read reg cl
  fe8ee piep
  fe95e rtc write reg cl
   polls until jim 0xa is zero,
   output cl at jim 0xa
   write ah hinibble as lownibble into jim 0xa
   write ah lownibble into jim 0xa
  fef66 rtc read reg cl
   polls until jim 0xa is zero,
   output cl at jim 0xa
   read low 4 nibble at jim 0xa
   read low 4 nibble at jim 0xa
   return first nibble<<4|second nibble in ah
  ffe87 0 -> ds

  469:
   bit 0: b0000 memory available
   bit 1: b8000 memory available
  46a: 00 jim 250 01 jim 350
 */

static struct {
	UINT8 data[16];
} europc_jim= { { 0 } } ;

/*
  250..253 write only 00 be 00 10

  252 write 0 b0000 memory activ
  252 write 0x10 b8000 memory activ

  254..257 r/w memory ? JIM asic? ram behaviour
*/
extern WRITE_HANDLER ( europc_jim_w )
{
	switch (offset) {
	case 0xa:
		europc_rtc_w(0, data);
		return;
	}
	logerror("jim write %.2x %.2x\n",offset,data);
	europc_jim.data[offset]=data;
}

extern READ_HANDLER ( europc_jim_r )
{
	int data=0;
	switch(offset) {
	case 4: case 5: case 6: case 7: data=europc_jim.data[offset];break;
	case 0: case 1: case 2: case 3: data=0;break;
	case 0xa: return europc_rtc_r(0);
	}
	return data;
}

/* port 2e0 polling!? at fd6e1 */

static struct {
	int port61; // bit 0,1 must be 0 for startup; reset?
} europc_pio= { 0 };

WRITE_HANDLER( europc_pio_w )
{
	switch (offset) {
	case 1:
		europc_pio.port61=data;
//		if (data==0x30) pc1640.port62=(pc1640.port65&0x10)>>4;
//		else if (data==0x34) pc1640.port62=pc1640.port65&0xf;
		pc_sh_speaker(data&3);
		pc_keyb_set_clock(data&0x40);
		break;
	}

	logerror("europc pio write %.2x %.2x\n",offset,data);
}


READ_HANDLER( europc_pio_r )
{
	int data=0;
	switch (offset) {
	case 0:
		if (!(europc_pio.port61&0x80))
			data = pc_keyb_read();
		break;
	case 1:
		data=europc_pio.port61;
		break;
	case 2:
		if (pit8253_get_output(0,2)) data|=0x20;
		break;
	}
	return data;
}

// realtime clock and nvram
static struct {
	/*
	   reg 0: seconds
	   reg 1: minutes
	   reg 2: hours
	   reg 3: day 1 based
	   reg 4: month 1 based
	   reg 5: year bcd (no century, values bigger 88? are handled as 1900, else 2000)
	   reg 6:
	   reg 7:
	   reg 8:
	   reg 9:
	   reg a:
	   reg b: 0x10 written
	    bit 0,1: 0 video startup mode: 0=specialadapter, 1=color40, 2=color80, 3=monochrom
		bit 2: internal video on
		bit 4: color
	   reg c:
	    bit 0,1: language/country
	   reg d: xor checksum
	   reg e:
	   reg 0f: 01 status ok, when not 01 written
	*/
	UINT8 data[0x10];
	int reg;
	int state;
	void *timer;
} europc_rtc;

void europc_rtc_set_time(void)
{
	time_t t;
	struct tm *tmtime;

	t=time(NULL);
	if (t==-1) return;

	tmtime=gmtime(&t);

	europc_rtc.data[0]=dec_2_bcd(tmtime->tm_sec);
	europc_rtc.data[1]=dec_2_bcd(tmtime->tm_min);
	europc_rtc.data[2]=dec_2_bcd(tmtime->tm_hour);

	europc_rtc.data[3]=dec_2_bcd(tmtime->tm_mday);
	europc_rtc.data[4]=dec_2_bcd(tmtime->tm_mon+1);
	europc_rtc.data[5]=dec_2_bcd(tmtime->tm_year%100);

	// freeing of gmtime??
}

static void europc_rtc_timer(int param)
{
	europc_rtc.data[0]=bcd_adjust(europc_rtc.data[0]+1);
	if (europc_rtc.data[0]>=0x60) {
		europc_rtc.data[0]=0;
		europc_rtc.data[1]=bcd_adjust(europc_rtc.data[1]+1);
		if (europc_rtc.data[1]>=0x60) {
			europc_rtc.data[1]=0;
			europc_rtc.data[2]=bcd_adjust(europc_rtc.data[2]+1);
			// different handling of hours
			if (europc_rtc.data[2]>=0x24) {
				europc_rtc.data[2]=0;
				europc_rtc.data[3]=bcd_adjust(europc_rtc.data[3]+1);
				// day in month overrun
				// month overrun
			}
		}
	}
}

void europc_rtc_init(void)
{
	memset(&europc_rtc,0,sizeof(europc_rtc));
	europc_rtc.data[0xf]=1;
	europc_rtc.timer=timer_pulse(1.0,0,europc_rtc_timer);
}

READ_HANDLER( europc_rtc_r )
{
	int data=0;
	switch (europc_rtc.state) {
	case 1:
		data=(europc_rtc.data[europc_rtc.reg]&0xf0)>>4;
		europc_rtc.state++;
		break;
	case 2:
		data=europc_rtc.data[europc_rtc.reg]&0xf;
		europc_rtc.state=0;
		break;
	}
	return data;
}

WRITE_HANDLER( europc_rtc_w )
{
	switch (europc_rtc.state) {
	case 0:
		europc_rtc.reg=data;
		europc_rtc.state=1;
		break;
	case 1:
		europc_rtc.data[europc_rtc.reg]=(europc_rtc.data[europc_rtc.reg]&~0xf0)|((data&0xf)<<4);
		europc_rtc.state++;
		break;
	case 2:
		europc_rtc.data[europc_rtc.reg]=(europc_rtc.data[europc_rtc.reg]&~0xf)|(data&0xf);
		europc_rtc.state=0;
		break;
	}
}

void europc_rtc_load_stream(void *file)
{
	osd_fread(file, europc_rtc.data, sizeof(europc_rtc.data));
}

void europc_rtc_save_stream(void *file)
{
	osd_fwrite(file, europc_rtc.data, sizeof(europc_rtc.data));
}

void europc_rtc_nvram_handler(void* file, int write)
{
	if (file==NULL) {
//		europc_set_time();
		// init only
	} else if (write) {
		europc_rtc_save_stream(file);
	} else {
		europc_rtc_load_stream(file);
	}
}

