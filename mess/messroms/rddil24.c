#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/io.h>
#include <fcntl.h>
#include <unistd.h>

/* simple linux utility (needs root privileg)
   dumps 24pin dil package 
   (gnd 12, +5v 24, d0..d2 9..11, d3..d7 13..17)
   ct eprop needed 

eprop
 a7		+5v
 a6		a8
 a5		a9
 a4		a11
 a3		a13
 a2		a10
 a1		a12
 a0		d7
 d0		d6
 d1		d5
 d2		d4
 gnd	d3
*/
   
// gcc -O
// important because of inb, outb

/* signetics c19082 notes
   first 0x2800 bytes zero
   0x800 data
   0x800 byte zero
   0x800 data

 a7		+5v
 a6		a8
 a5		a9
 a4		!hi (a11 eprop)
 a3		hi (a13 eprop)
 a2		a10
 a1		a11	(a12 eprop)
 a0		d7
 d0		d6
 d1		d5
 d2		d4
 gnd	d3

 commands to extract data from dump:
 dd if=dil24.bin of=lo bs=2048 count=1 skip=5
 dd if=dil24.bin of=hi bs=2048 count=1 skip=7
 cat lo hi >c19082.bin
 */
/* signetics c19081 notes
   first 0x2000 bytes zero
   0x800 data
   0x800 byte zero
   0x800 data
   0x800 byte zero

 a7		+5v
 a6		a8
 a5		a9
 a4		!lo (a11 eprop)
 a3		hi (a13 eprop)
 a2		a10
 a1		a11	(a12 eprop)
 a0		d7
 d0		d6
 d1		d5
 d2		d4
 gnd	d3

 commands to extract data from dump:
 dd if=dil24.bin of=lo bs=2048 count=1 skip=4
 dd if=dil24.bin of=hi bs=2048 count=1 skip=6
 cat lo hi >c19081.bin
 */

/* mos 6332 notes

2716 with 18 and 20 swapped

 a7		+5v
 a6		a8
 a5		a9
 a4		hi (cs?)
 a3		lo (!cs?)
 a2		a10
 a1		a11
 a0		d7
 d0		d6
 d1		d5
 d2		d4
 gnd	d3

 commands to extract data from dump:
 dd if=dil24.bin of=lo bs=2048 count=1 skip=1
 dd if=dil24.bin of=hi bs=2048 count=1 skip=3
 cat lo hi >mos6332.bin
 */

#define PORT_BASE 0x3e0

void wait_ticks(int c)
{
	struct timeval now, last;
	unsigned long long a,b;
	gettimeofday(&last,NULL);
	a=1000000ull*last.tv_sec+last.tv_usec;
	for (;;) {
		gettimeofday(&now,NULL);
		b=1000000ull*now.tv_sec+now.tv_usec;
		if (b-a>c) break;
	}
}

/*
  ct eprop
  8 bit socket

  1		A7
  2		A6
  3		A5
  4		A4
  5		A3
  6		A2
  7		A1
  8		A0
  9		D0
  10	D1
  11	D2
  12	GND

  13	D3
  14	D4
  15	D5
  16	D6
  17	D7
  18	/CE should be A11!
  19	A10
  20	/OE should be !CS
  21	A11 should be CS
  22	A9
  23	A8
  24	5V

 */

/*
  mos 6332
  4 kbyte 8bit rom

  1		A7
  2		A6
  3		A5
  4		A4
  5		A3
  6		A2
  7		A1
  8		A0
  9		D0
  10	D1
  11	D2
  12	GND

  13	D3
  14	D4
  15	D5
  16	D6
  17	D7
  18	A11
  19	A10
  20	CS1
  21	CS2
  22	A9
  23	A8
  24	5V

 */


int port[8]={0};

void out_port(int value, int off)
{
	port[off]=value;
	outb(value, PORT_BASE+off);
}

void out_addr(int addr)
{
	int temp;

	out_port(addr&0xff,0);

	temp=port[1];
	temp&=~0xf;
	temp|=(addr>>8)&0xf;
	out_port(temp, 1);

	temp=port[3];
	temp&=~0x41;
	if (addr&0x1000) temp|=0x40;
	if (addr&0x2000) temp|=1;
	out_port(temp,3);
}

int main(void)
{
	int value, i;
	unsigned char data[0x4000];
	FILE *file;

	ioperm(PORT_BASE,8,1);

	out_port(0,0);
	out_port(0,1); // CS on
	out_port(0,2); // 
	out_port(0,6); // vpp off

	out_port(4,3); // VCC on, VCC=5V, Socket=24pin, /OE=0, read

	wait_ticks (100000);

//	for (i=0; i<0x40; i++) {
	for (i=0; i<0x4000; i++) {
		out_addr(i);
		wait_ticks(100);
		value=inb(PORT_BASE+4);
		data[i]=value;
		if ((i&0xf)==0) printf("%.4x:",i);
		printf(" %.2x", value);
		if ((i&0xf)==0xf) printf("\n");
	}
	out_port(0xf8,2); // led off
	out_port(0,3); //VCC off
	out_port(0,6); //VPP off
	out_port(0,0);
	
	file=fopen("dil24.bin","wb");
	fwrite(data,1,sizeof(data),file);
	fclose(file);

	return 0;
}
