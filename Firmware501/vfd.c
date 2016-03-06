
#include "main.h"
#include "vfd.h"


#define P0_10 (1 << 10)

#define VFD_SCK 17
#define VFD_SOUT 18
#define VFD_SIN 19

#define VFD_MB 28
#define VFD_SS 29
#define VFD_RES 22

#define VFD_PRINTBUFFERSIZE 120

int vfd_modebits;
char vfd_turm[8]  = { 
130,
68,
0,
68,
130,
16,
16,
56};


static void vfd_delay(int d)
{     
  for(; d; --d);
}


char vfd_putc(char data)
{

  while (FIO0PIN & PO_BIT_VFD_MB); // Check if VFD is not busy 
  
  IOCLR = PO_BIT_VFD_SS;

  SSPDR = data; // Write byte
  data = SSPDR; // Dummy read to clear status

  while (!(SSPSR & SSPSR_TFE)); // Check Transmit FIFO Empty

  IOSET = PO_BIT_VFD_SS;

  return data;
}

void vfd_putstring(char *data)
{
   char start = 0;
   while (data[start]) vfd_putc(data[start++]);
}



int vfd_printf(const char *format, ...)
{
 char printbuffer[VFD_PRINTBUFFERSIZE];
 unsigned short int size;
 unsigned short int i;
 va_list ap;
 va_start(ap, format);
 size = vsnprintf(printbuffer,VFD_PRINTBUFFERSIZE,format,ap);
 for (i=0;printbuffer[i]!=0;i++) vfd_putc(printbuffer[i]);
 return size;
}

void vfd_setgraphics_macro(char id, char length, char data[])
{
  unsigned short int iy;
  vfd_putc(0x1B);
  vfd_putc(0x55);
  vfd_putc(0x1B);
  vfd_putc(id);
  vfd_putc(length+4);
  vfd_putc(VFD_CMD_SET_MODE);
  vfd_putc(VFD_CMD_MODE_HORIZ_VERT);
  vfd_putc(VFD_CMD_GRAPHIC_WRITE);
  vfd_putc(length);
  for (iy=0;iy<length;iy++) vfd_putc(data[iy]);
}


void vfd_setbrightness(char value)
{
 vfd_putc(0x1B);
 vfd_putc(value + 0xF8);
}

void vfd_setcursor(char x, char y)
{
 vfd_putc(0x10);
 vfd_putc(x);
 vfd_putc(y);
}

void vfd_setmode(char value)
{
 vfd_modebits = value;
 vfd_putc(0x1A);
 vfd_putc(vfd_modebits);
}

void vfd_gotoxy(char x, char y)
{
 vfd_setcursor(x*6,y*8);
}

unsigned int vfd_readbyte()
{
  unsigned short int i;
  unsigned  short int data = 0;
  while (IOPIN & PO_BIT_VFD_MB);
  IOCLR = PO_BIT_VFD_SS;
  while (SSPSR & SSPSR_BSY);
  data = SSPDR;
  IOSET = PO_BIT_VFD_SS;
  return data;
}

unsigned int vfd_readport()
{
 vfd_putc(0x1B);
 vfd_putc(0x52);
 vfd_delay(200);
 return vfd_readbyte();
}

void vfd_off()
{
 vfd_putc(0x1B);
 vfd_putc(0x46);
}

void vfd_on()
{
 vfd_putc(0x1B);
 vfd_putc(0x50);
}

void vfd_keyboardscan()
{
 vfd_putc(0x1B);
 vfd_putc(0x55);
 vfd_putc(0x1B);
 vfd_putc(0x4B);
}

void vfd_docmd4(char command, char x1, char y1, char x2, char y2)
{
 vfd_putc(command);
 vfd_putc(x1);
 vfd_putc(y1);
 vfd_putc(x2);
 vfd_putc(y2);
}


void vfd_init()
{
 IOSET = PO_BIT_VFD_SS;
 IOSET = PO_BIT_VFD_RES;
 vfd_putc(0x60); // use BCD mode
 vfd_putc(0x31); // 1
 vfd_putc(0x42); // B
 vfd_putc(0x60); // use BCD mode 
 vfd_putc(0x34); // 4  
 vfd_putc(0x32); // 2 --> 0x1B 0x42 = BCD mode off
 // now we're in binary mode
 vfd_putc(0x19); // reset & clear
 //vfd_putc(0x1A);         // set write mode
 //vfd_putc(vfd_modebits); // set write mode
}

unsigned short int vfd_pg_x1;
unsigned short int vfd_pg_x2;
unsigned short int vfd_pg_percentfont;
unsigned short int vfd_pg_percentx;
unsigned short int vfd_pg_y1;
unsigned short int vfd_pg_y2;
unsigned short int vfd_pg_maxvalue;
float vfd_pg_unit;

void vfd_make_progressbar(
unsigned short int x1,
unsigned short int y1,
unsigned short int x2,
unsigned short int y2,
unsigned short int maxvalue,
unsigned short int percentfont)
{
  vfd_pg_x1 = x1;
  vfd_pg_x2 = x2;
  vfd_pg_y1 = y1;
  vfd_pg_y2 = y2;
  vfd_pg_maxvalue = maxvalue;
  vfd_pg_percentfont = percentfont;

  if (vfd_pg_percentfont) {
   if (vfd_pg_percentfont == VFD_CMD_FONT_MINI) vfd_pg_percentx = ((vfd_pg_x2 - vfd_pg_x1) / 2) + vfd_pg_x1 - 6;
   if (vfd_pg_percentfont == VFD_CMD_FONT_5X7) vfd_pg_percentx = ((vfd_pg_x2 - vfd_pg_x1) / 2) + vfd_pg_x1 - 9;
   if (vfd_pg_percentfont == VFD_CMD_FONT_10X14) vfd_pg_percentx = ((vfd_pg_x2 - vfd_pg_x1) / 2) + vfd_pg_x1 - 15;
  };

  vfd_docmd4(VFD_CMD_OUTLINE_SET,x1,y1,x2,y2);
  vfd_docmd4(VFD_CMD_AREA_CLEAR,x1+1,y1+1,x2-1,y2-1);
  vfd_pg_unit = ( (float)(vfd_pg_x2 - vfd_pg_x1 - 2) / (float)maxvalue );
}


void vfd_update_progressbar(
unsigned short int value)
{
  vfd_docmd4(VFD_CMD_AREA_SET,vfd_pg_x1+1,vfd_pg_y1+1,vfd_pg_x1 + 1 + (int)( vfd_pg_unit * (float)value),vfd_pg_y2 - 1);
  if (vfd_pg_percentfont) {
   vfd_putc(vfd_pg_percentfont);
   vfd_setmode(0x03);
   vfd_setcursor(vfd_pg_percentx,vfd_pg_y2-1);
   vfd_printf("%2u%%",(100 * value) / vfd_pg_maxvalue );
   vfd_setmode(0x00);
  }
}

void vfd_clear_progressbar()
{
  vfd_docmd4(VFD_CMD_AREA_CLEAR,vfd_pg_x1,vfd_pg_y1,vfd_pg_x2,vfd_pg_y2);
}



