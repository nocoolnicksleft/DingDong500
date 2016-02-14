

#define BIT_00 0x1
#define BIT_01 0x2
#define BIT_02 0x4
#define BIT_03 0x8
#define BIT_04 0x10
#define BIT_05 0x20
#define BIT_06 0x40
#define BIT_07 0x80
#define BIT_08 0x100
#define BIT_09 0x200
#define BIT_10 0x400
#define BIT_11 0x800
#define BIT_12 0x1000
#define BIT_13 0x2000
#define BIT_14 0x4000
#define BIT_15 0x8000
#define BIT_16 0x10000
#define BIT_17 0x20000
#define BIT_18 0x40000
#define BIT_19 0x80000
#define BIT_20 0x100000
#define BIT_21 0x200000
#define BIT_22 0x400000
#define BIT_23 0x800000
#define BIT_24 0x1000000
#define BIT_25 0x2000000
#define BIT_26 0x4000000
#define BIT_27 0x8000000
#define BIT_28 0x10000000
#define BIT_29 0x20000000
#define BIT_30 0x40000000
#define BIT_31 0x80000000

#define B8(y)	( 0##y       &   1 \
		| 0##y >>  2 &   2 \
		| 0##y >>  4 &   4 \
		| 0##y >>  6 &   8 \
		| 0##y >>  8 &  16 \
		| 0##y >> 10 &  32 \
		| 0##y >> 12 &  64 \
		| 0##y >> 14 & 128 )

/*
int bit_test(int data, int bit)
{
  return (data & (1 << bit) == (1 << bit));
}
*/

#define SEL_P0_3_EINT1 0x000000C0
#define SEL_P0_7_EINT2 0x0000C000
#define SEL_P0_15_EINT2 0x80000000
#define SEL_P0_9_RXD1  0x00040000
#define SEL_P0_8_TXD1  0x00010000
#define SEL_P016_EINT0 0x00000001
#define SEL_P3_23_XCLK 0x00002000
#define SEL_P0_2_EINT0 0x0000000C
#define SEL_P0_SPI1    0x000002A8  // Enables SCK1, MISO1, MOSI1, SSEL1
#define SEL_P0_SPI1_N  (BIT_03 | BIT_05 | BIT_07 | BIT_09)   
#define SEL_P0_SPI0    0x00005500 // Enables SCK0, MISO0, MOSI0, SSEL0
#define SEL_P0_SPI0_N  (BIT_08 | BIT_10 | BIT_12 | BIT_14)    


#define PINSEL0 (*(volatile unsigned long *)0xE002C000)
#define PINSEL1 (*(volatile unsigned long *)0xE002C004)
#define PINSEL2 (*(volatile unsigned long *)0xE002C014)

#define S1SPCCR (*(volatile unsigned long *)0xE003000C)
#define S1SPCR (*(volatile unsigned long *)0xE0030000)
#define S1SPDR (*(volatile unsigned long *)0xE0030008)
#define S1SPSR (*(volatile unsigned long *)0xE0030004)
#define S1SPINT (*(volatile unsigned long *)0xE003001C)

#define S0SPCCR (*(volatile unsigned long *)0xE002000C)
#define S0SPCR (*(volatile unsigned long *)0xE0020000)
#define S0SPDR (*(volatile unsigned long *)0xE0020008)
#define S0SPSR (*(volatile unsigned long *)0xE0020004)
#define S0SPINT (*(volatile unsigned long *)0xE002001C)

#define IO1PIN (*(volatile unsigned long *)0xE0028010)
#define IO1SET (*(volatile unsigned long *)0xE0028014)
#define IO1DIR (*(volatile unsigned long *)0xE0028018)
#define IO1CLR (*(volatile unsigned long *)0xE002801C)


