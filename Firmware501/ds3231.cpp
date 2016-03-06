
#include "main.h"

#include "ds3231.h"
#include "i2c_slave.h"

/**********************************************************
/ DS3231 RTC
/**********************************************************/

#define DS3231_WRITE B8(11010000)
#define DS3231_READ B8(11010001)

#define DS3231_CURRENTTIME 0x0
#define DS3231_ALARMTIME1 0x7
#define DS3231_ALARMTIME2 0xB

#define DS3231_MASK_WEEKDAY B8(00000111)
#define DS3231_MASK_DATE B8(00111111)
#define DS3231_MASK_HOUR B8(00111111)
#define DS3231_MASK_CENTURY B8(10000000)
#define DS3231_MASK_MONTH B8(00011111)

// int displayPulseTime = 0;

int8 BCDdecode(int8 bcddata)
{
 return (bcddata/16)*10 + (bcddata%16);
}

int8 BCDencode(int8 decdata)
{
  return ((decdata / 10) << 4) | (decdata % 10);
}

int8 RTCRead(int8 address)
{
  char out_msb = 0;

  i2cSlave * rtc = new i2cSlave(0,DS3231_WRITE);

  rtc->init();

  if (rtc->setStart())
  {
    if (rtc->setSLAW())
    {
      rtc->setData(address);
      rtc->setRestart();
      rtc->setSLAR();
      out_msb = rtc->readLastByte();

    } 
    rtc->setStop();
  }

  delete rtc;

  return out_msb;
}

int8 RTCWrite(int8 address, int8 data)
{
  i2cSlave * rtc = new i2cSlave(0,DS3231_WRITE);

  rtc->init();

  if (rtc->setStart())
  {
    if (rtc->setSLAW())
    {
      rtc->setData(address);
      rtc->setData(data);
  } 
    rtc->setStop();
  }

  delete rtc;
}

int16 RTCGetTemp()
{
    int16 upper = 0;
    int16 lower = 0;
    int16 i = 0;

    upper = RTCRead(0x11);
    lower = RTCRead(0x12);

    i = ((upper*100) + ((lower >> 6)*25)) / 10;

    return i;
}

void RTCGetTime(struct atimestamp* timestamp)
{
  int8 time_sec;
  int8 time_sec2;
  int8 time_min;
  int8 time_hour;
  int8 time_weekday;
  int8 time_day;
  int8 time_monthcentury;
  int8 time_year;
  int8 address;

  address  = DS3231_CURRENTTIME;

  i2cSlave * rtc = new i2cSlave(0,DS3231_WRITE);

  rtc->init();

  if (rtc->setStart())
  {
    if (rtc->setSLAW())
    {
      rtc->setData(address);
      rtc->setRestart();
      rtc->setSLAR();

      // time_hun   = i2c_read();
      time_sec      = rtc->readByte(); 
      time_min      = rtc->readByte();
      time_hour     = rtc->readByte();
      time_weekday  = rtc->readByte();
      time_day      = rtc->readByte();
      time_monthcentury  = rtc->readByte();
      time_year     = rtc->readLastByte();

      if (rtc->status() == I2C_STATUS_RECEIVE_NACK)
      {
        time_sec2 = BCDdecode(time_sec);

        if (timestamp->second != time_sec2) {

             timestamp->second   = BCDdecode(time_sec);
             timestamp->minute   = BCDdecode(time_min);
             timestamp->hour     = BCDdecode(time_hour & DS3231_MASK_HOUR); 
             timestamp->weekday = (time_weekday & DS3231_MASK_WEEKDAY);  
             timestamp->day     = BCDdecode(time_day);
             timestamp->month   = BCDdecode(time_monthcentury & DS3231_MASK_MONTH);
             timestamp->year    = BCDdecode(time_year);
             timestamp->updated = 1;

#if defined DEBUG_GENERAL
	debug_printf("RTCGetTime: %02u:%02u:%02u %u, %02u.%02u.%02u\n",timestamp->hour,
	                                                            timestamp->minute,
                                                                    timestamp->second,
                                                                    timestamp->weekday,
								timestamp->day,
								timestamp->month,
								timestamp->year);
#endif


        }

      }

    } 
    rtc->setStop();
  }

  delete rtc;

}

void RTCSetTime(struct atimestamp* timestamp)
{

#if defined DEBUG_GENERAL
	debug_printf("RTCSetTime: %02u:%02u:%02u %u, %02u.%02u.%02u\n",timestamp->hour,
	                                                            timestamp->minute,
                                                                    timestamp->second,
                                                                    timestamp->weekday,
								timestamp->day,
								timestamp->month,
								timestamp->year);
#endif

 i2cSlave * rtc = new i2cSlave(0,DS3231_WRITE);

  rtc->init();

  if (rtc->setStart())
  {
    if (rtc->setSLAW())
    {
      rtc->setData(DS3231_CURRENTTIME);
      rtc->setData(BCDencode(timestamp->second));
      rtc->setData(BCDencode(timestamp->minute));
      rtc->setData(BCDencode(timestamp->hour));
      rtc->setData(BCDencode(timestamp->weekday));
      rtc->setData(BCDencode(timestamp->day)); 
      rtc->setData(BCDencode(timestamp->month));
      rtc->setData(BCDencode(timestamp->year));

      if (rtc->status() == I2C_STATUS_RECEIVE_NACK)
      {
      }

    } 
    rtc->setStop();
  }

  delete rtc;

}

void initRTC()
{
    CurrentTime.hour = 0;
    CurrentTime.minute = 0;
    CurrentTime.second = 0;
    CurrentTime.weekday = 0;
    CurrentTime.day = 0;
    CurrentTime.month = 0;
    CurrentTime.year = 0;

    // READ CURRENT TIME
    RTCGetTime(&CurrentTime);

    // CONTROL REGISTER: DISABLE ALARMS, ENABLE 1HZ SQW OUTPUT
    RTCWrite(0x0E,0x00);
    
    // STATUS REGISTER: ENABLE 32KHZ OUTPUT
    RTCWrite(0x0F,0x08);

    // AGING OFFSET: TRIM FREQUENCY
    RTCWrite(0x10,0x0);
     
}


