


#include <16F877.h>
#fuses XT,WDT,NOPROTECT,PUT,BROWNOUT,NOLVP

#include <stdlib.h>

#use fast_io(A)
#use delay(clock=10000000)
#use rs232(baud=9600, xmit=PIN_C6, rcv=PIN_C7)
#use i2c(master,sda=PIN_C4,scl=PIN_C3)


#define use_portb_lcd TRUE
#include <lcdbs.c>

#define blue_keypad
#include <kbd.c>

#define SAA1064		0b01110000
#include "SAA1064.c"

/**********************************************************
/ PCF8583 RTC
/**********************************************************/

#define PCF8583_WRITE 0b10100010
#define PCF8583_READ 0b10100011
#define PCF8583_CURRENTTIME 0x1
#define PCF8583_ALARMTIME 0x9

#define PCF8583_MASK_DAY 0x00111111
#define PCF8583_MASK_YEAR 0x11000000
#define PCF8583_MASK_MONTH 0x00011111
#define PCF8583_MASK_WEEKDAY 0x11100000

struct atimestamp {
    int8 hundredth;
	int8 second;
	int8 minute;
    int8 hour;
	int8 day;
	int8 month;
    int8 weekday;
    int16 year;
	int1 leapyear;	
    int1 updated;
} CurrentTime;

int8 BCDdecode(int8 bcddata)
{
 return (bcddata/16)*10 + (bcddata%16);
}

int8 BCDencode(int8 decdata)
{
  return ((decdata / 10) << 4) | (decdata % 10);
}

unsigned int8 RTCRead(unsigned int8 address)
{
    int1 nack;
    unsigned int8 value = 0;

	i2c_start();
    nack = i2c_write(PCF8583_WRITE);
    if (nack) {
     i2c_stop();
     printf("rtcread nack 1\n\r");
    } else {
     nack = i2c_write(address);
     if (!nack) {
      i2c_start();
      nack = i2c_write(PCF8583_READ);
      if (!nack) {
       value  = i2c_read();
      } else      printf("rtcread nack 3\n\r");
     } else      printf("rtcread nack 2\n\r");
     i2c_stop();
    }
    return value;
}

void RTCWrite(unsigned int8 address, unsigned int8 data)
{
    int1 ack;
   // printf("\n\rdata: %u ten: %u one: %u value: %u",data,ten,one,value);
	i2c_start();
    ack = i2c_write(PCF8583_WRITE);
    if (ack) {
     i2c_stop();
     printf("rtcwrite nack 1 %X %X\n\r",address,data);

    } else {
     ack = i2c_write(address);
     if (!ack) {
      i2c_write(data);
     } else printf("rtcwrite nack 2 %X %X\n\r",address,data);
     i2c_stop();
    }
    delay_ms(100);
}

void RTCGetTime(int1 alarm, struct atimestamp* timestamp)
{
    int8 time_hun;
	int8 time_sec;
	int8 time_sec2;
	int8 time_min;
    int8 time_hour;
	int8 time_yearday;
	int8 time_weekdaymonth;
    int1 nack;

    int8 address;

    if (alarm) address = PCF8583_ALARMTIME;
    else address = PCF8583_CURRENTTIME;

	i2c_start();
    nack = i2c_write(PCF8583_WRITE);
    if (nack) {
     i2c_stop();
    } else {
     i2c_write(address);
     i2c_start();
     nack = i2c_write(PCF8583_READ);
     if (nack) {
      i2c_stop();
     } else {
      time_hun  = i2c_read();
      time_sec  = i2c_read();
      time_min  = i2c_read();
      time_hour = i2c_read();
      time_yearday  = i2c_read();
      time_weekdaymonth= i2c_read();
      i2c_stop();

	  time_sec2 = BCDdecode(time_sec);
	  if (timestamp->second != time_sec2) {
       timestamp->hundredth= BCDdecode(time_hun);
	   timestamp->second   = time_sec2;
	   timestamp->minute   = BCDdecode(time_min);
	   timestamp->hour     = BCDdecode(time_hour); 
	   timestamp->day      = BCDdecode(time_yearday & PCF8583_MASK_DAY); 
	   timestamp->month    = BCDdecode(time_weekdaymonth & PCF8583_MASK_MONTH);
       timestamp->weekday  = time_weekdaymonth & PCF8583_MASK_WEEKDAY; 
	   timestamp->leapyear = time_yearday ^ PCF8583_MASK_YEAR;
       timestamp->updated = 1;
      }
      delay_ms(10);
	 }
    }
}

void RTCSetTime(int1 alarm, struct atimestamp* timestamp)
{
 int8 address;
 if (alarm) address = PCF8583_ALARMTIME;
 else address = PCF8583_CURRENTTIME;
 RTCWrite(address,  BCDencode(timestamp->hundredth));
 RTCWrite(address+1,BCDencode(timestamp->second));
 RTCWrite(address+2,BCDencode(timestamp->minute));
 RTCWrite(address+3,BCDencode(timestamp->hour));
 
 RTCWrite(address+4,BCDencode(timestamp->day) & PCF8583_MASK_DAY);
 RTCWrite(address+5,BCDencode(timestamp->month & PCF8583_MASK_MONTH));
}


#define DCFPULSE PIN_A0
int1    lastdcflevel;
int1    dcflevel;

/**********************************************************
/ Common Timer
/**********************************************************/

int1 timeout100msec = 0;
int8 timer1sec = 10;
int1 timeout1sec = 0;

int8 dcfpulsetime;

#define TIMER1_50MSEC_OFF_10MHZ 30000
#define TIMER1_50MSEC_10MHZ 34285
#define TIMER1_100MSEC_10MHZ 3035

#INT_TIMER1
void timeproc()
{
  // set_timer1(3035); // 100msec at 10MHz
 //  set_timer1(TIMER1_50MSEC_10MHZ); // 50msec at 10MHz
   timeout100msec = 1;
   if (!(--timer1sec)) {
    timer1sec = 10;
    timeout1sec = 1;
   }
}

#INT_TIMER2
void timerproc2()
{
  dcfpulsetime++;
}

/**********************************************************
/ Serial Input Buffer
/**********************************************************/
#define BUFFER_SIZE 2
BYTE buffer[BUFFER_SIZE];
BYTE next_in = 0;
BYTE next_out = 0;

#INT_RDA
void serial_isr() {
   int t;

   buffer[next_in]=getc();
   t=next_in;
   next_in=(next_in+1) % BUFFER_SIZE;
   if(next_in==next_out)
     next_in=t;           // Buffer full !!
}

#define bkbhit (next_in!=next_out)

BYTE bgetc() {
   BYTE c;

   while(!bkbhit) ;
   c=buffer[next_out];
   next_out=(next_out+1) % BUFFER_SIZE;
   return(c);
}

/**********************************************************
/ Alarmclock Application
/**********************************************************/

// CONFIGURATION DEFAULT VALUES
#define MAGICNUMBER 239
#define DEFAULT_ALARMSTATE 0
#define DEFAULT_ALARMMODE 0
#define DEFAULT_ALARMDAYS 0b00011111;
#define DEFAULT_RADIOSTATE 0
#define DEFAULT_RADIOSTATION 8760
#define DEFAULT_RADIOVOLUME 2
#define DEFAULT_RADIOTIMEOUT 10 // minutes
#define DEFAULT_DISPLAYTIMEOUT 10 //seconds
#define DEFAULT_ALARMTIMEOUT 60 //seconds
#define DEFAULT_LEDTESTTIME 300 // msec

// CONFIGURATION EEPROM ADRESSES
#define EE_MAGICNUMBER 9       // IF != MAGICNUMBER -> INITIALIZE ALL
#define EE_ALARMSTATE 10        // 
#define EE_ALARMMODE 11
#define EE_ALARMMINUTE 20
#define EE_ALARMHOUR 21
#define EE_ALARMDAYS 22
#define EE_RADIOSTATE 12
#define EE_RADIOSTATION_01 13
#define EE_RADIOSTATION_02 14
#define EE_RADIOVOLUME 15
#define EE_RADIOTIMEOUT 16
#define EE_DISPLAYTIMEOUT 17
#define EE_ALARMTIMEOUT 18
#define EE_LEDTESTTIME 19


// EXTERNAL HARDWARE CONNECTIONS
#define ALARMSTATE_LED PIN_A2
#define RADIOSTATE_LED PIN_C6
#define LCD_BACKLIGHT PIN_C0
#define BUZZER PIN_A3

// STATE DEFINITIONS
#define TSTATE_DEFAULT 0
#define TSTATE_MENU01 1
#define TSTATE_EDITTIME 2

#define SERIALSTATE_DEFAULT 0

#define ALARMSTATE_OFF 0
#define ALARMSTATE_ON 1

#define ALARMMODE_BUZZER 0
#define ALARMMODE_RADIO 1
#define ALARMMODE_BOTH 2

#define RADIOMODE_OFF 0
#define RADIOMODE_ON 1
#define RADIOMODE_SLEEP 2

// CURRENT CONFIGURATION VALUES
int8  cfg_alarmstate;
int8  cfg_alarmmode;
int8  cfg_alarmminute;
int8  cfg_alarmhour;
int8  cfg_alarmdays;
int8  cfg_radiostate;
int16 cfg_radiostation;
int8  cfg_radiovolume;
int8  cfg_radiotimeout;
int8  cfg_displaytimeout;
int8  cfg_alarmtimeout;
int8  cfg_ledtesttime;

// RUNTIME STATES & COUNTERS
int8 ringtimeout = 30;
int8 terminaltimeout;
int8 serialstate = SERIALSTATE_DEFAULT;
int8 terminalstate = TSTATE_DEFAULT;
int8 editorposition = 0;
int1 editalarm = 0;
int1 ringstate = 0;
int8 ringcount = 0;
char timebuffer[6] = "";

// STRING CONSTANTS
const char Productname[] = "BUZZ 2000 Alarmclock";
const char Menu1[] = "1 Set Time";
const char Menu2[] = "2 Set Alarm";
const char Menu3[] = "able Alarm";
const char GoodMorning[] = "Good Morning!";
const char GoodNight[] = "Good Night!";
const char TimeFormat[] = "%02u:%02u"; 

// OTHER CONSTANTS
const int numbers[5] = {2,9,0,5,9}; //

// INITIALIZE CONFIGURATION
void initconfig()
{
  if (read_eeprom(EE_MAGICNUMBER) != MAGICNUMBER) 
  {
		cfg_alarmstate = DEFAULT_ALARMSTATE;
        cfg_alarmmode  = DEFAULT_ALARMMODE;
        cfg_radiostate = DEFAULT_RADIOSTATE;
        cfg_radiostation= DEFAULT_RADIOSTATION;
        cfg_radiovolume = DEFAULT_RADIOVOLUME;
        cfg_radiotimeout = DEFAULT_RADIOTIMEOUT;
        cfg_displaytimeout = DEFAULT_DISPLAYTIMEOUT;
        cfg_alarmtimeout = DEFAULT_ALARMTIMEOUT;
        cfg_ledtesttime  = DEFAULT_LEDTESTTIME;
        cfg_alarmminute = 0;
        cfg_alarmhour = 0;
        cfg_alarmdays = DEFAULT_ALARMDAYS;
        
        write_eeprom(EE_ALARMSTATE,cfg_alarmstate);
        write_eeprom(EE_ALARMMODE,cfg_alarmmode);
        write_eeprom(EE_ALARMMINUTE,cfg_alarmminute);
        write_eeprom(EE_ALARMHOUR,cfg_alarmhour);
        write_eeprom(EE_ALARMDAYS,cfg_alarmdays);
        write_eeprom(EE_RADIOSTATE,cfg_radiostate);
        write_eeprom(EE_RADIOSTATION_01,*(&cfg_radiostation));
        write_eeprom(EE_RADIOSTATION_02,*(&cfg_radiostation+1));
        write_eeprom(EE_RADIOVOLUME,cfg_radiovolume);
        write_eeprom(EE_RADIOTIMEOUT,cfg_radiotimeout);
        write_eeprom(EE_DISPLAYTIMEOUT,cfg_displaytimeout);
        write_eeprom(EE_ALARMTIMEOUT,cfg_alarmtimeout);
        write_eeprom(EE_LEDTESTTIME,cfg_ledtesttime);

        write_eeprom(EE_MAGICNUMBER,MAGICNUMBER);
  } else {
  		cfg_alarmstate = read_eeprom(EE_ALARMSTATE);
		cfg_alarmmode  = read_eeprom(EE_ALARMMODE);
		cfg_alarmminute= read_eeprom(EE_ALARMMINUTE);
		cfg_alarmhour  = read_eeprom(EE_ALARMHOUR);
		cfg_alarmdays  = read_eeprom(EE_ALARMDAYS);
		cfg_radiostate = read_eeprom(EE_RADIOSTATE);
		*(&cfg_radiostation)=read_eeprom(EE_RADIOSTATION_01);
		*(&cfg_radiostation+1)=read_eeprom(EE_RADIOSTATION_02);
        cfg_radiovolume   = read_eeprom(EE_RADIOVOLUME);
        cfg_radiotimeout  = read_eeprom(EE_RADIOTIMEOUT);
        cfg_displaytimeout= read_eeprom(EE_DISPLAYTIMEOUT);
        cfg_alarmtimeout   = read_eeprom(EE_ALARMTIMEOUT);
        cfg_ledtesttime   = read_eeprom(EE_LEDTESTTIME);
  }
}

/*

Sekunde
 00      : immer 0
 01 - 14 : codierung nach Bedarf
 15      : R 0=normale 1=Reserveantenne
 16      : Ankündigung des Wechsels von Sommer/Winterzeit
           eine Stunde lang vorher=1 sonst 0
 17 - 18 : Offset zu UTC,
           man kann auch sagen bei Sommerzeit ist Bit 17=1 im Winter Bit 18=1
 19      : Ankündigung einer Schaltsekunde
           eine Stunde lang vorher=1 sonst 0
 20      : Startbit, immer 1
 21 - 27 : Minuten BCD-codiert, Bit 21 ist LSB
 28      : Paritätsbit für Minuten
 29 - 34 : Stunden BCD-codiert
 35      : Paritätsbit für Stunden
 36 - 41 : Tag, BCD-codiert
 42 - 44 : Wochentag
 45 - 49 : Monat, BCD-codiert
 50 - 57 : Jahr BCD-codiert, letzte zwei Stellen
 58      : Paritätsbit für Bit 36-57
 59      : kein Impuls, beim nächsten fängt eine neue Minute an

;********************************************************************
;55555555 54444444 44433333 33333222 22222221 11111111
;87654321 09876543 21098765 43210987 65432109 87654321   Sekunde
;PJJJJJJJ JMMMMMWW WTTTTTTP SSSSSSPm mmmmmm1a ooaR0000   DCF-Bits
;
;JJJJJJJJ 000MMMMM 00tttttt 00ssssss 0mmmmmmm 1aooaR00 00000www
; Jahr     Monat    Tag      Stunde   Minute   STATUS   Wtag
;********************************************************************

       7        6        5        4        3        2        1        0
00000pJJ JJJJJJMM MMMwwwtt ttttphhh hhhpmmmm mmm1aooa Rxxxxxxx xxxxxxx0

dcfhour = bcd_decode( (dcfbuffer[4] & 0b00000111) << 3 | ((dcfbuffer[3] & 0b11100000) >> 5)); 
dcfmin = bcd_decode( (dcfbuffer[3] & 0b00001111) << 3 | ((dcfbuffer[2] & 0b11100000) >> 5));
hour = b
*/

int8 dcfbuffer[8];
int1 dcflastbit;
int8 dcfbitcount;
int1 dcfsequence = 0;

struct dcftimestamp {
 int8 min;
 int8 hour;
 int8 wday;
 int8 day;
 int8 month;
 int8 year;
 int8 utcoffset;
 int1 alert;
 int1 offsetchange;
 int1 leapsecond;
} dcf1, dcf2;

int1 do_dcf_decode(struct dcftimestamp* dcf)
{
  if ( (dcfbitcount == 59) && 
       (!bit_test(dcfbuffer[0],0)) &&
       ( bit_test(dcfbuffer[2],4)) ) {
// printf("-buf %X %X %X %X\r\n",dcfbuffer[0],dcfbuffer[1],dcfbuffer[2],dcfbuffer[3]);

   dcf->hour = bcddecode( (dcfbuffer[4] & 0b00000111) << 3 | ((dcfbuffer[3] & 0b11100000) >> 5)); 
   dcf->min = bcddecode( (dcfbuffer[3] & 0b00001111) << 3 | ((dcfbuffer[2] & 0b11100000) >> 5));
   dcf->day = bcddecode( (dcfbuffer[5] & 0b00000011) << 4 | ((dcfbuffer[4] & 0b11110000) >> 4));
   dcf->wday = bcddecode( (dcfbuffer[5] & 0b00011100) >>2 );
   dcf->month = bcddecode( (dcfbuffer[6] & 0b00000011) << 4 | ((dcfbuffer[5] & 0b11100000) >> 5));
   dcf->year = bcddecode( (dcfbuffer[7] & 0b00000011) << 7 | ((dcfbuffer[6] & 0b111111) >> 2));
   dcf->alert = dcfbuffer[1] & 0b10000000 >> 7;
   dcf->offsetchange = dcfbuffer[2] & 0b00000001;
   dcf->leapsecond = (dcfbuffer[2] & 0b00001000) >> 3;
   dcf->utcoffset =  (dcfbuffer[2] & 0b00000110) >> 1;

   printf("\r\nReceived: %02u:%02u %u, %02u.%02u.%02u\r\n",dcf->hour,dcf->min,dcf->wday,dcf->day,dcf->month,dcf->year);
   return 1;
  }
 return 0;
}


int8 pulse;
// QUERY DCF INPUT AND BUILD TIME
void do_dcf() 
{
  int8 i;
  

  dcflevel = input_state(DCFPULSE);
   if (dcflevel != lastdcflevel) {
  disable_interrupts(INT_TIMER2);
    lastdcflevel = dcflevel;
    if (!dcflevel) {  // negative flanke

     if (dcfpulsetime > 50) dcflastbit = 1; else dcflastbit = 0; 
     printf("-%u:%u",dcfbitcount,dcfpulsetime);
     if (dcflastbit) bit_set(dcfbuffer[dcfbitcount/8],dcfbitcount%8);
     dcfbitcount++;

	} else { // if (!dcflevel) // positive flanke

     if (dcfpulsetime > 150) {
      printf("-*:%u",dcfpulsetime);
      if (dcfsequence) {
       if (do_dcf_decode(&dcf2)) {
        if ( (dcf2.min - dcf1.min) == 1) {
         printf("SET TIME!\r\n");
         CurrentTime.Hour = dcf2.hour;
         CurrentTime.Minute = dcf2.min;
         CurrentTime.Second = 0;
         CurrentTime.Day = dcf2.day;
         CurrentTime.Weekday = dcf2.wday;
         CurrentTime.Month = dcf2.month;
         CurrentTime.Year = dcf2.year%4;
         RTCSetTime(0,&CurrentTime);
        } // if ( (dcf2.min
       } // if (do_dcf_decode
       dcfsequence = 0;
      } else { // if (dcfsequence)
       if (do_dcf_decode(&dcf1)) dcfsequence = 1;
      } // if (dcfsequence)
      dcfbitcount = 0;
      for (i=0; i<8; i++) dcfbuffer[i] = 0;
     } // if (dcfpulsetime > 150)

    } // if (!dcflevel)

    dcfpulsetime = 0;
enable_interrupts(INT_TIMER2);
   } // if (dcflevel != lastdcflevel)

}

int i;

#int_ccp1
void dcf_up()
{
	 dcfpulsetime = CCP_1 - CCP_2;
     if (dcfpulsetime > 150) {
      printf("-CP1:%u",dcfpulsetime);
      if (dcfsequence) {
       if (do_dcf_decode(&dcf2)) {
        if ( (dcf2.min - dcf1.min) == 1) {
         printf("SET TIME!\r\n");
         CurrentTime.Hour = dcf2.hour;
         CurrentTime.Minute = dcf2.min;
         CurrentTime.Second = 0;
         CurrentTime.Day = dcf2.day;
         CurrentTime.Weekday = dcf2.wday;
         CurrentTime.Month = dcf2.month;
         CurrentTime.Year = dcf2.year%4;
         RTCSetTime(0,&CurrentTime);
        } // if ( (dcf2.min
       } // if (do_dcf_decode
       dcfsequence = 0;
      } else { // if (dcfsequence)
       if (do_dcf_decode(&dcf1)) dcfsequence = 1;
      } // if (dcfsequence)
      dcfbitcount = 0;
      for (i=0; i<8; i++) dcfbuffer[i] = 0;
     } // if (dcfpulsetime > 150)
}

#int_ccp2
void dcf_down()
{
	 dcfpulsetime = CCP_2 - CCP_1;
     if (dcfpulsetime > 50) dcflastbit = 1; else dcflastbit = 0; 
     printf("-CP2-%u:%u",dcfbitcount,dcfpulsetime);
     if (dcflastbit) bit_set(dcfbuffer[dcfbitcount/8],dcfbitcount%8);
     dcfbitcount++;
}

// GET TIME FROM RTC AND DISPLAY TO 7SEG
void do_time() {
  int16 k3;

  RTCGetTime(0,&CurrentTime);
  if (CurrentTime.updated == 1) {
   CurrentTime.updated = 0;

   k3 = (int16)CurrentTime.hour*(int16)100 + (int16)CurrentTime.minute;
   initSAA1064();
   printSAA1064(k3,3);

   if (cfg_alarmstate) {
    if (   (CurrentTime.hour == cfg_alarmhour) 
       && (CurrentTime.minute == cfg_alarmminute)
       && (CurrentTime.second == 0) ) {
     ringstate = 1;
     ringtimeout = cfg_alarmtimeout;
     if (cfg_alarmmode) {
      cfg_radiostate = 1;
      output_high(RADIOSTATE_LED);
     }
    }
   }
  }
}

void printcr()
{
  printf("\n\r");
}

void printmenuserial()
{
  printcr();
  printcr();
  printf(Productname);
  printcr();
  printf(Menu1);
  printcr();
  printf(Menu2);
  printcr();
  printf(Menu3);
  printcr();
  printf("=> ");
}

void printmenulcd()
{
  lcd_cls();  
  lcd_gotoxy(1,1);
  printf(lcd_putc,Menu1);
  lcd_gotoxy(1,2);
  printf(lcd_putc,Menu2);
  lcd_gotoxy(1,3);
  if (cfg_alarmstate) printf(lcd_putc,"3 Dis");
  else printf(lcd_putc,"3 En");
  printf(lcd_putc,Menu3);
  lcd_gotoxy(1,4);
  printf(lcd_putc,"# Exit");
}

void printinfolcd()
{
 lcd_cls();  
 printf(lcd_putc,Productname);
 lcd_gotoxy(1,2);
 printf(lcd_putc,"Alarm ");
 printf(lcd_putc,TimeFormat,cfg_alarmhour,cfg_alarmminute);
 if (cfg_alarmstate) printf(lcd_putc," ENABLED");
 else printf(lcd_putc," OFF");
 lcd_gotoxy(1,3);
 if (CurrentTime.hour < 11) printf(lcd_putc,GoodMorning);
 else if(CurrentTime.hour > 21) printf(lcd_putc,GoodNight);
}

void printtimeeditor()
{
 lcd_cls();  
 lcd_gotoxy(1,2);
 if (!editalarm) {
  sprintf(timebuffer,TimeFormat, CurrentTime.hour,CurrentTime.minute);
  printf(lcd_putc,"Edit Time:  %S",timebuffer);
 }
 else {
  sprintf(timebuffer,TimeFormat, cfg_alarmhour,cfg_alarmminute);
  printf(lcd_putc,"Edit Alarm: %S",timebuffer);
 } 
 lcd_gotoxy(1,4);
 printf(lcd_putc,"# Exit    * Save");
 lcd_cursoron();
 editorposition = 0;
 lcd_gotoxy(13,2);
}

void printtime()
{
  printcr();
  printcr();
     printf("Time: %02u:%02u:%02u\n\rDate: %02u.%02u.%u",
       CurrentTime.hour,CurrentTime.minute,CurrentTime.second,
       CurrentTime.day,CurrentTime.month,CurrentTime.leapyear);
}

void main () {
  char key;

//  set_tris_C(0b11011000);
  
  output_low(BUZZER);
 
setup_adc(ADC_OFF);
setup_adc_ports(NO_ANALOGS);
output_a(0x00);
set_tris_a(0b00000001);

  setup_wdt(WDT_2304MS);

  enable_interrupts(global);
  enable_interrupts(int_rda);

   setup_ccp1(CCP_CAPTURE_RE);    // Configure CCP1 to capture rise
   setup_ccp2(CCP_CAPTURE_FE);    // Configure CCP2 to capture fall
   setup_timer_1(T1_INTERNAL);    // Start timer 1
   enable_interrupts(INT_TIMER1);
   enable_interrupts(INT_CCP2);   // Setup interrupt on falling edge
 
  dcfpulsetime = 0;
  dcflastbit = 0;
  dcfbitcount = 0;
//  setup_timer_2(T2_DIV_BY_16,0xFF,2);
//  enable_interrupts(INT_TIMER2);

  initconfig();

  terminaltimeout = cfg_displaytimeout; // 10sec to timeout backlight BUGBUG


  lcd_init();
  output_low(LCD_BACKLIGHT);
  printinfolcd();

  initSAA1064();
  testSAA1064();
  delay_ms(cfg_ledtesttime);
  initSAA1064();
 

  do_time();

 
  while (TRUE) {
    restart_wdt();


    // alle 100 ms 
    // 1. beim klingeln buzzerstatus ändern (MUSS an erster stelle stehen!!)
    if (timeout100msec) {
output_bit(PIN_A1,1);
     timeout100msec = 0;
     if (ringstate) {
      
      if (ringcount == 0) output_high(BUZZER);
      else if (ringcount == 1) output_low(BUZZER);
      else if (ringcount == 2) output_high(BUZZER);
      else if (ringcount == 3) output_low(BUZZER);
      else output_low(BUZZER);
      if (++ringcount==8) ringcount = 0;
     }

     // 2. dcf-decoderroutine ausführen
     do_dcf();

     
     // 3. zeit abholen und auf 7seg-display schieben
     do_time();

     // 4. sonstige interface-updates
     if (cfg_alarmstate) output_high(ALARMSTATE_LED);
     else output_low(ALARMSTATE_LED);
  
     if (cfg_radiostate) output_high(RADIOSTATE_LED);
     else output_low(RADIOSTATE_LED);

output_bit(PIN_A1,0);

    }

  

   if (ringstate) {
    if (timeout1sec) {
     if (--ringtimeout == 0) {
      ringstate = 0;
      output_low(BUZZER);
      terminaltimeout = cfg_displaytimeout;
     } else {
    //  printf("RRRRIIIING!!!!\n\r");
      output_low(LCD_BACKLIGHT);
     // output_high(BUZZER);
      timeout1sec = 0;
     }
    }
   } else if (!terminalstate) {
     if (timeout1sec) {
      timeout1sec = 0;



      if (--terminaltimeout == 0) 
      output_high(LCD_BACKLIGHT);
     }
    }

    if (bkbhit) {             // Serial Control
	 key = bgetc();
     printf("%c",key);

     if (!serialstate) {
      printmenuserial();
      serialstate = 1;
     }
     
     if (serialstate == 1) {
      if (key == '1') {
        printtime();
      }
      if (key == '4') {
        cfg_alarmstate = !cfg_alarmstate;
      }
      if (key == 'x') {
        write_eeprom(EE_MAGICNUMBER,0x0);
        reset_cpu();
      } 
      printmenuserial();
     }

    } // if bkbhit / serial input


    key=kbd_getc();
    if(key!=0) {              // Keypad Control

      if (ringstate) {
       ringstate = 0;
       output_low(BUZZER);
       terminaltimeout = cfg_displaytimeout;
      } else

      if (!terminalstate) {
       output_low(LCD_BACKLIGHT);
       terminaltimeout = cfg_displaytimeout;
       if(key=='*') {
        printmenulcd();
        terminalstate = TSTATE_MENU01;
       }
      }
        
      else if (terminalstate == TSTATE_MENU01) {
       if (key=='#') {
        printinfolcd();
        terminalstate = TSTATE_DEFAULT;
        terminaltimeout = cfg_displaytimeout;
       }
       else 
       if (key == '1') {
        terminalstate = TSTATE_EDITTIME;
        editalarm = 0;
        printtimeeditor();
       }
       else
       if (key == '2') {
        terminalstate = TSTATE_EDITTIME;
        editalarm = 1;
        printtimeeditor();
       }
       else
       if (key == '3') {
        if (cfg_alarmstate) cfg_alarmstate = 0;
        else cfg_alarmstate = 1;
        write_eeprom(EE_ALARMSTATE,cfg_alarmstate);
        printmenulcd();
       }
    }


      else if (terminalstate == TSTATE_EDITTIME) {
       if (key == '#') {
        lcd_cursoroff();
        printmenulcd();
        terminalstate = TSTATE_MENU01;
       }
       else 
       if (key == '*') {
        lcd_cursoroff();
        timebuffer[2] = 0;
        timebuffer[5] = 0;
        // printf("\n\rtimebuffer: %S %S",timebuffer,timebuffer+3);
        if (editalarm) {
         cfg_alarmhour = atoi(timebuffer);
         cfg_alarmminute = atoi(timebuffer + 3);
	     write_eeprom(EE_ALARMMINUTE,cfg_alarmminute);
     	 write_eeprom(EE_ALARMHOUR,cfg_alarmhour);
       } else { 
         CurrentTime.hour = atoi(timebuffer);
         CurrentTime.minute = atoi(timebuffer + 3);
         CurrentTime.second = 0;
         CurrentTime.hundredth = 0;
         RTCSetTime(0,&CurrentTime);
        }
        printmenulcd();
        terminalstate = TSTATE_MENU01;
       }
       else {
        if ( (key - '0') <= numbers[editorposition]) {
         if ((editorposition == 1) && (timebuffer[0] == '2') && (key > '3')) key = '3';
         if ((editorposition == 0) && (timebuffer[1] >  '3') && (key =='2')) {
          timebuffer[1] = '3';
          lcd_gotoxy(14,2);
          lcd_putc('3');
          lcd_gotoxy(13,2);
         } 
         lcd_putc(key);
         timebuffer[editorposition] = key;
         if (editorposition == 1) editorposition = 3;
         else if (editorposition==4) editorposition = 0;
         else editorposition++;
         lcd_gotoxy(13 + editorposition,2);
        }
       }
      }


   
    }


 


  }

}
