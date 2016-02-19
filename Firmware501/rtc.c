
#include "main.h"

#include "rtc.h"

/*********************************************************************/
/* DS1305 RTC                                                        */
/*********************************************************************/

static void some_delay(int d)
{     
  for(; d; --d);
}

char spi0_write_sync(char data)
{
   char buf = 0;
   char buf2 = 0;
   S0SPDR = data;   
   buf2 = S0SPSR;
   while ((buf2 & BIT_07) != BIT_07) {buf2 = S0SPSR;}
   //while ((S0SPSR & BIT_07) != BIT_07);
   buf = S0SPDR;
#ifdef DEBUG_MAIN
   //debug_printf("%X-%X-%X\n",data,buf,buf2);
#endif
   return buf;
}

void rtc_init()
{
  char buf;
  if (spi_lock(1)) {
   S0SPCR   = (BIT_05  | BIT_03  );
   IOSET = RTC_SEL;
   some_delay(DS1305_SEL_DELAY);

   spi0_write_sync(DS1305_CTRL_WR);
   spi0_write_sync(0x0);

   IOCLR = RTC_SEL;
   S0SPCR   = (BIT_05 | BIT_03 | BIT_07);
   spi_unlock(1);
  } 
  
}


int cbcdencode(char decdata)
{
  return ((decdata / 10) << 4) | (decdata % 10);
}

char rtc_write(char address, char data)
{
  char buf = 0;
  if (spi_lock(1)) {
   S0SPCR   = (BIT_05 | BIT_03);
   IOSET = RTC_SEL;
   some_delay(DS1305_SEL_DELAY);

   buf = spi0_write_sync(address);
   buf = spi0_write_sync(data);

   IOCLR = RTC_SEL;
   S0SPCR   = (BIT_05 | BIT_03 | BIT_07);
   spi_unlock(1);
  }
  return buf;

}


void rtc_write_time()
{
  if (spi_lock(1)) {
  
   S0SPCR   = (BIT_05 | BIT_03 );
   IOSET = RTC_SEL;
   some_delay(DS1305_SEL_DELAY);

   spi0_write_sync(DS1305_TIME_WR);
   spi0_write_sync(cbcdencode(SEC));
   spi0_write_sync(cbcdencode(MIN));
   spi0_write_sync(cbcdencode(HOUR));
   spi0_write_sync(cbcdencode(DOW));
   spi0_write_sync(cbcdencode(DOM));
   spi0_write_sync(cbcdencode(MONTH));
   spi0_write_sync(cbcdencode(YEAR-2000));

   IOCLR = RTC_SEL;
   S0SPCR   = (BIT_05 | BIT_03 | BIT_07);
   spi_unlock(1);
  }
}

int cbcddecode(char bcddata)
{
 return (bcddata >> 4)*10 + (bcddata%16);
}

char rtc_read_config(char id)
{
  char data = 0;
  if (spi_lock(1)) {
   S0SPCR   = (BIT_05 | BIT_03   );
   IOSET = RTC_SEL;
   some_delay(DS1305_SEL_DELAY);
   spi0_write_sync(id + DS1305_USER_RD);
   data = spi0_write_sync(0x0);
   IOCLR = RTC_SEL;
   S0SPCR   = (BIT_05 | BIT_03 | BIT_07);
   spi_unlock(1);
  }
  return data;
}

void rtc_write_config(char id, char data)
{
  if (spi_lock(1)) {
   S0SPCR   = (BIT_05 | BIT_03   );
   IOSET = RTC_SEL;
   some_delay(DS1305_SEL_DELAY);
   spi0_write_sync(id + DS1305_USER_WR);
   spi0_write_sync(data);
   IOCLR = RTC_SEL;
   S0SPCR   = (BIT_05 | BIT_03 | BIT_07);
   spi_unlock(1);
  }
}

void rtc_write_config_int(char id, int data)
{
  int i;
  char c;
  for (i = 0; i < 4; i++) {
    c = (char) (* (  ((char *)&data) + i));
    rtc_write_config(id + i, c);
  }
}

int rtc_read_config_int(char id)
{
  int result;
  int i;
  char c;
  for (i = 0; i < 4; i++) {
    c = rtc_read_config(id + i);
    (* ( ((char *)&result) + i)) = c;
  }
  return result;
}

void rtc_read_time()
{
  int rtc_sec = 0;
  int rtc_min = 0;
  int rtc_hour = 0;
  int rtc_dom = 0;
  int rtc_dow = 0;
  int rtc_month = 0;
  int rtc_year = 0;

  if (spi_lock(1)) {
   S0SPCR   = (BIT_05 | BIT_03   );
   IOSET = RTC_SEL;
   some_delay(DS1305_SEL_DELAY);

   rtc_sec = spi0_write_sync(DS1305_TIME_RD);
   rtc_sec = cbcddecode(spi0_write_sync(0x0));
   rtc_min = cbcddecode(spi0_write_sync(0x0));
   rtc_hour = cbcddecode(spi0_write_sync(0x0));
   rtc_dow = cbcddecode(spi0_write_sync(0x0));
   rtc_dom = cbcddecode(spi0_write_sync(0x0));
   rtc_month = cbcddecode(spi0_write_sync(0x0));
   rtc_year = cbcddecode(spi0_write_sync(0x0));
   IOCLR = RTC_SEL;

   CCR = 0x0;
   CCR = 0x2;
   CCR = 0x0;

   SEC = rtc_sec;
   MIN = rtc_min;
   HOUR = rtc_hour;
   DOW = rtc_dow;
   DOM = rtc_dom;
   MONTH = rtc_month;
   YEAR = rtc_year;

   CCR = 0x1;

#ifdef DEBUG_MAIN
 //  debug_printf("RTC Time: %X:%X:%X %X, %X.%X.%X\n",rtc_hour,rtc_min,rtc_sec,rtc_dow,rtc_dom,rtc_month,rtc_year);
   debug_printf("RTC Time: %02u:%02u:%02u %u, %02u.%02u.%02u\n",rtc_hour,rtc_min,rtc_sec,rtc_dow,rtc_dom,rtc_month,rtc_year);
#endif

   S0SPCR   = (BIT_05 | BIT_03 | BIT_07);
   spi_unlock(1);
  }
}

