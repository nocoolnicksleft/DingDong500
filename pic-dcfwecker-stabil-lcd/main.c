


#include <16F877.h>
#fuses XT,NOWDT,NOPROTECT,PUT,BROWNOUT,NOLVP

#include <stdlib.h>

#use fast_io(A)
#use fast_io(B)
#use fast_io(D)
#use fast_io(E)
#use delay(clock=10000000)
#use rs232(stream=terminal, baud=9600, xmit=PIN_A0, rcv=PIN_A1, ERRORS)
#use rs232(stream=mp3player, baud=9600, xmit=PIN_C6, rcv=PIN_C7, ERRORS)
#use i2c(master,fast,sda=PIN_C4,scl=PIN_C3)

// EXTERNAL HARDWARE CONNECTIONS
#define ALARMSTATE_LED PIN_A2
#define MP3STATE_LED PIN_C5
#define LCD_BACKLIGHT PIN_C0
#define BUZZER PIN_A3
#define MP3_TX PIN_A0
#define MP3_RX PIN_A1
// DCF_INPUT_PIN_1 PIN_C1
// DCF_INPUT_PIN_2 PIN_C2
// ALARM_SWITCH 

//#define DEBUG_ALL

#if defined DEBUG_ALL
#define DEBUG_SAA1064
#define DEBUG_DCF77
#define DEBUG_RTC
#define DEBUG_MAIN
#endif

// #define DEBUG_SAA1064
// #define DEBUG_DCF77
// #define DEBUG_RTC
// #define DEBUG_MAIN

#define use_portb_lcd TRUE
#include <lcdbs.c>

#include <kbdbs.c>

#define SAA1064		0b01110000
#include "SAA1064.c"

/*
#define VFD_TX PIN_E0
#define VFD_CLK PIN_E1
#define VFD_BUSY PIN_E2
void vfd_putc(char data)
{
  int8 i;
  output_low(VFD_CLK);
  output_low(VFD_TX);
  while (input(VFD_BUSY)) { }
  delay_us(10);
  for (i=0;i<8;i++) {
//   disable_interrupts(global);
   output_bit(VFD_TX, shift_left(&data,1,0) );
   delay_cycles(4);
   output_high(VFD_CLK);
//   enable_interrupts(global);
   delay_us(10);
   output_low(VFD_CLK);
   delay_us(10);
  }
}


void vfd_init()
{
 vfd_putc(0x19);
 vfd_putc(0x1B);
 vfd_putc(0x42);
}
*/
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
    int8 year;
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
     //printf(term,"rtcread nack 1\n\r");
    } else {
     nack = i2c_write(address);
     if (!nack) {
      i2c_start();
      nack = i2c_write(PCF8583_READ);
      if (!nack) {
       value  = i2c_read(0);
      } 
#if defined DEBUG_RTC
      else fprintf(terminal,"rtcread nack 3\n\r");
#endif
     } 
#if defined DEBUG_RTC
     else fprintf(terminal,"rtcread nack 2\n\r");
#endif
     i2c_stop();
    }
    return value;
}

int1 RTCWrite(unsigned int8 address, unsigned int8 data)
{
    int1 nack;
#if defined DEBUG_RTC
   fprintf(term,"RTCWrite a: %u d: %X\n\r",address,data);
#endif
	i2c_start();
    nack = i2c_write(PCF8583_WRITE);
    if (nack) {
     i2c_stop();
#if defined DEBUG_RTC
    fprintf(terminal,"RTCWrite nack 1 %X %X\n\r",address,data);
#endif
    } else {
     nack = i2c_write(address);
     if (!nack) {
      i2c_write(data);
     }
#if defined DEBUG_RTC
     else fprintf(terminal,"RTCWrite nack 2 %X %X\n\r",address,data);
#endif
     i2c_stop();
    }
    return (!nack);
}

void RTCGetTime(struct atimestamp* timestamp)
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
    address  = PCF8583_CURRENTTIME;

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
      time_weekdaymonth= i2c_read(0);
      i2c_stop();

	  time_sec2 = BCDdecode(time_sec);
	  if (timestamp->second != time_sec2) {
       timestamp->hundredth= BCDdecode(time_hun);
	   timestamp->second   = time_sec2;
	   timestamp->minute   = BCDdecode(time_min);
	   timestamp->hour     = BCDdecode(time_hour); 

       timestamp->weekday = ((time_weekdaymonth & PCF8583_MASK_WEEKDAY) >> 5);  
 	   timestamp->day  = ((time_yearday & 0x30) >> 4)*10 + (time_yearday & 0x0F);
	   timestamp->month = ((time_weekdaymonth & 0x10) >> 4)*10 + (time_weekdaymonth & 0x0F);
	   timestamp->year = (time_yearday & 0xC0) >> 6;

      timestamp->updated = 1;
      }
	 }
    }
}

void RTCSetTime(struct atimestamp* timestamp)
{

#if defined DEBUG_RTC
 fprintf(terminal,"RTCSetTime\n\r");
#endif

 RTCWrite(PCF8583_CURRENTTIME,  BCDencode(timestamp->hundredth));
 RTCWrite(PCF8583_CURRENTTIME+1,BCDencode(timestamp->second));

 if (!RTCWrite(PCF8583_CURRENTTIME+2,BCDencode(timestamp->minute)))
  RTCWrite(PCF8583_CURRENTTIME+2,BCDencode(timestamp->minute));
 if (!RTCWrite(PCF8583_CURRENTTIME+3,BCDencode(timestamp->hour)))
  RTCWrite(PCF8583_CURRENTTIME+3,BCDencode(timestamp->hour));
 
 RTCWrite(PCF8583_CURRENTTIME+4,((timestamp->day / 10) << 4) + (timestamp->day % 10) + (timestamp->year << 6));
 RTCWrite(PCF8583_CURRENTTIME+5,((timestamp->month / 10) << 4) + (timestamp->month % 10) + (timestamp->weekday << 5));
}

/**********************************************************
/ Serial Input Buffer
/**********************************************************/
#define BUFFER_SIZE 8
BYTE buffer[BUFFER_SIZE];
BYTE next_in = 0;
BYTE next_out = 0;

#INT_RDA
void serial_isr() {
   int t;

   buffer[next_in]=fgetc(mp3player);
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

int1 timeout50msec = 0;
int8 timer1sec = 10;
int1 timeout1sec = 0;

int8 dcfpulsetime;

#define TIMER1_50MSEC_OFF_10MHZ 30000
#define TIMER1_50MSEC_10MHZ 34285
#define TIMER1_100MSEC_10MHZ 3035


// CONFIGURATION DEFAULT VALUES
#define MAGICNUMBER 239
#define DEFAULT_ALARMSTATE 0
#define DEFAULT_ALARMMODE 0b11 // mp3 und Summer
#define DEFAULT_ALARMDAYS 0b00011111;
#define DEFAULT_MP3STATE 0
#define DEFAULT_MP3TITLE 1
#define DEFAULT_MP3VOLUME 60
#define DEFAULT_MP3TIMEOUT 10 // minutes
#define DEFAULT_DISPLAYTIMEOUT 200 //50msec-cycles
#define DEFAULT_ALARMTIMEOUT 230 //50msec-cycles
#define DEFAULT_LEDTESTTIME 200 // msec

// CONFIGURATION EEPROM ADRESSES
#define EE_MAGICNUMBER 9       // IF != MAGICNUMBER -> INITIALIZE ALL
#define EE_ALARMSTATE 10        // 
#define EE_ALARMMODE 11
#define EE_ALARMMINUTE 23
#define EE_ALARMHOUR 24
#define EE_ALARMDAYS 25
#define EE_MP3STATE 12
#define EE_MP3TITLE_01 13
#define EE_MP3TITLE_02 14
#define EE_MP3VOLUME 15
#define EE_MP3TIMEOUT 16
#define EE_DISPLAYTIMEOUT 18
#define EE_ALARMTIMEOUT 20
#define EE_LEDTESTTIME 22
#define EE_YEAR 26



// STATE DEFINITIONS
#define TSTATE_DEFAULT 0
#define TSTATE_MENU01 1
#define TSTATE_EDITTIME 2

#define SERIALSTATE_DEFAULT 0

#define ALARMSTATE_OFF 0
#define ALARMSTATE_ON 1

#define ALARMMODE_BUZZER 0
#define ALARMMODE_MP3 1
#define ALARMMODE_BOTH 2

#define MP3MODE_OFF 0
#define MP3MODE_ON 1
#define MP3MODE_SLEEP 2

// CURRENT CONFIGURATION VALUES
int8  cfg_alarmstate;
int8  cfg_alarmmode;
int8  cfg_alarmminute;
int8  cfg_alarmhour;
int8  cfg_alarmdays;
int8  cfg_mp3state;
int16  cfg_mp3title;
int8  cfg_mp3volume;
int8  cfg_mp3timeout;
int8  cfg_displaytimeout;
int8  cfg_alarmtimeout;
int8  cfg_ledtesttime;
int8  cfg_year;

// RUNTIME STATES & COUNTERS
int16 ringtimeout = 30;
int8 terminaltimeout = 0;
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
//const char GoodMorning[] = "Good Morning!";
//const char GoodNight[] = "Good Night!";
const char Exit[] = "# Exit";
const char Save[] = "* Save";
const char TimeFormat[] = "%02u:%02u"; 
//char Days[] = "MoDiMiDoFrSaSo";

// OTHER CONSTANTS
const int numbers[5] = {2,9,0,5,9}; //

// INITIALIZE CONFIGURATION
void initconfig()
{
  if (read_eeprom(EE_MAGICNUMBER) != MAGICNUMBER) 
  {
		cfg_alarmstate = DEFAULT_ALARMSTATE;
        cfg_alarmmode  = DEFAULT_ALARMMODE;
        cfg_mp3state   = DEFAULT_MP3STATE;
        cfg_mp3title   = DEFAULT_MP3TITLE;
        cfg_mp3volume  = DEFAULT_MP3VOLUME;
        cfg_mp3timeout = DEFAULT_MP3TIMEOUT;
        cfg_displaytimeout = DEFAULT_DISPLAYTIMEOUT;
        cfg_alarmtimeout = DEFAULT_ALARMTIMEOUT;
        cfg_ledtesttime  = DEFAULT_LEDTESTTIME;
        cfg_alarmminute = 0;
        cfg_alarmhour = 0;
        cfg_alarmdays = DEFAULT_ALARMDAYS;
        cfg_year = 0;
        
        write_eeprom(EE_ALARMSTATE,cfg_alarmstate);
        write_eeprom(EE_ALARMMODE,cfg_alarmmode);
        write_eeprom(EE_ALARMMINUTE,cfg_alarmminute);
        write_eeprom(EE_ALARMHOUR,cfg_alarmhour);
        write_eeprom(EE_ALARMDAYS,cfg_alarmdays);
        write_eeprom(EE_MP3STATE,cfg_mp3state);
        write_eeprom(EE_MP3TITLE_01,*(&cfg_mp3title));
        write_eeprom(EE_MP3TITLE_02,*(&cfg_mp3title+1));
        write_eeprom(EE_MP3VOLUME,cfg_mp3volume);
        write_eeprom(EE_MP3TIMEOUT,*(cfg_mp3timeout));
        write_eeprom(EE_DISPLAYTIMEOUT,*(cfg_displaytimeout));
        write_eeprom(EE_ALARMTIMEOUT,*(cfg_alarmtimeout));
        write_eeprom(EE_LEDTESTTIME,cfg_ledtesttime);
        write_eeprom(EE_YEAR,cfg_year);

        write_eeprom(EE_MAGICNUMBER,MAGICNUMBER);
  } else {
  		cfg_alarmstate = read_eeprom(EE_ALARMSTATE);
		cfg_alarmmode  = read_eeprom(EE_ALARMMODE);
		cfg_alarmminute= read_eeprom(EE_ALARMMINUTE);
		cfg_alarmhour  = read_eeprom(EE_ALARMHOUR);
		cfg_alarmdays  = read_eeprom(EE_ALARMDAYS);
		cfg_mp3state      = read_eeprom(EE_MP3STATE);
		*(&cfg_mp3title)  = read_eeprom(EE_MP3TITLE_01);
		*(&cfg_mp3title+1)= read_eeprom(EE_MP3TITLE_02);
        cfg_mp3volume   = read_eeprom(EE_MP3VOLUME);
        cfg_mp3timeout  = read_eeprom(EE_MP3TIMEOUT);
        *(&cfg_displaytimeout) = read_eeprom(EE_DISPLAYTIMEOUT);
        *(&cfg_alarmtimeout)   = read_eeprom(EE_ALARMTIMEOUT);
        cfg_ledtesttime   = read_eeprom(EE_LEDTESTTIME);
        cfg_year   = read_eeprom(EE_YEAR);
  }
}

/**********************************************************
/ MP3 Control
/**********************************************************/
#define MP3_REPORT_BUFFER_LENGTH 20

#define MP3_STATUS_READY 0
#define MP3_CMD_QUERY_STATUS 1
#define MP3_CMD_SET_VOLUME 2
#define MP3_CMD_PLAY 3
#define MP3_CMD_STOP 4
#define MP3_STATUS_INIT_0 5
#define MP3_STATUS_INIT_1 6
#define MP3_STATUS_INIT_2 7
#define MP3_STATUS_INIT_3 8
#define MP3_CMD_OPEN_TITLEFILE 9
#define MP3_CMD_QUERY_TITLE 10
#define MP3_CMD_QUERY_ARTIST 11
#define MP3_CMD_OPEN_COUNTERFILE 12
#define MP3_CMD_QUERY_COUNTERS 13
#define MP3_CMD_CLOSE_TITLEFILE 14
#define MP3_CMD_CLOSE_COUNTERFILE 15
#define MP3_CMD_QUERY_LENGTH 16

#define MP3_DEFAULT_TIMEOUT 60

int16 mp3lasttitle = 0;
// int8 mp3artist;
int1 mp3playing = 0;
int8 mp3timeout = 0;
int8 mp3retries = 0;

BYTE mp3buffer[MP3_REPORT_BUFFER_LENGTH];
int8 mp3bufferptr = 0;
int8 mp3status = MP3_STATUS_READY;
int8 mp3completed = 0;


#define MP3_OFF 0
#define MP3_PLAYING 1
#define MP3_VOLUME_OFF 254
#define MP3_VOLUME_DEFAULT 64


void mp3sendcommand()
{

  if      (mp3status ==  MP3_CMD_QUERY_STATUS) fprintf(mp3player,"PC Z\r");
  else if (mp3status ==  MP3_CMD_PLAY) fprintf(mp3player,"PC F /A/A%06lu.mp3\r",cfg_mp3title);
  else if (mp3status ==  MP3_CMD_QUERY_TITLE) fprintf(mp3player,"FC R 1 %u %lu\r",MP3_REPORT_BUFFER_LENGTH,((long)75*cfg_mp3title) + (long)30);
  else if (mp3status ==  MP3_CMD_QUERY_ARTIST) fprintf(mp3player,"FC R 1 %u %lu\r",MP3_REPORT_BUFFER_LENGTH,((long)75*cfg_mp3title));
  else if (mp3status ==  MP3_CMD_QUERY_LENGTH) fprintf(mp3player,"FC R 1 %u %lu\r",MP3_REPORT_BUFFER_LENGTH,((long)75*cfg_mp3title) + (long)70);
  else if (mp3status ==  MP3_CMD_STOP) fprintf(mp3player,"PC S\r");
  else if (mp3status ==  MP3_CMD_SET_VOLUME) fprintf(mp3player,"ST V %u\r",cfg_mp3volume);
  else if (mp3status ==  MP3_CMD_OPEN_TITLEFILE) fprintf(mp3player,"FC O 1 R /V/t.txt\r");
  else if (mp3status ==  MP3_CMD_OPEN_COUNTERFILE) fprintf(mp3player,"FC O 2 R /V/c.txt\r");
  else if (mp3status ==  MP3_CMD_QUERY_COUNTERS) fprintf(mp3player,"FC R 2 6 0\r");
  else if (mp3status ==  MP3_CMD_CLOSE_TITLEFILE) fprintf(mp3player,"FC C 1\r");
  else if (mp3status ==  MP3_CMD_CLOSE_COUNTERFILE) fprintf(mp3player,"FC C 2\r");
  mp3timeout = MP3_DEFAULT_TIMEOUT;
}

void mp3docommand(int8 commandid)
{
 if (!mp3status) {
  mp3status = commandid;
  mp3retries = 5;
  mp3sendcommand();
 }
}

void mp3save()
{
  write_eeprom(EE_MP3TITLE_01,*(&cfg_mp3title));
  write_eeprom(EE_MP3TITLE_02,*(&cfg_mp3title+1));
}

void mp3setnextfile()
{
  if (cfg_mp3title == mp3lasttitle) cfg_mp3title = 0;
  else cfg_mp3title++;
  mp3save();
}

void mp3setprevfile()
{
  if (cfg_mp3title == 0) cfg_mp3title = mp3lasttitle;
  else cfg_mp3title--;
  mp3save();
}

void mp3putbufferlcd(int8 start)
{
   int8 i;
   for (i=start;i<mp3bufferptr;i++) lcd_putc(mp3buffer[i]); 
}

/**********************************************************
/ DCF Clock Signal
/**********************************************************/

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
int8 dcfbitcount = 0;
int1 dcfsequence = 0;
int1 dcfdecodenow = 0;
int1 dcfsavetimenow = 0;
int8 dcfsyncstate = 0;

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
// printf(term,"-buf %X %X %X %X\r\n",dcfbuffer[0],dcfbuffer[1],dcfbuffer[2],dcfbuffer[3]);

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

   return 1;
  }
 return 0;
}

void dcfsavetime()
{
     if ( (
          ( (dcf2.hour == dcf1.hour) && ((dcf2.min - dcf1.min) == 1) )
          || 
          ( ((dcf2.hour - dcf1.hour) == 1) && (dcf2.min == 0) ) 
         ) && (dcf2.day == dcf1.day) && (dcf2.month == dcf1.month) && (dcf2.year == dcf1.year) ) {
     // alles super, zwei konsistente zeiten empfangen...
     // also setzen und speichern!
#if defined DEBUG_DCF77
      fprintf(terminal,"Valid Time: %02u:%02u %u, %02u.%02u.%02u\r\n",dcf2.hour,dcf2.min,dcf2.wday,dcf2.day,dcf2.month,dcf2.year);
#endif
      CurrentTime.Hour = dcf2.hour;
      CurrentTime.Minute = dcf2.min;
      CurrentTime.Second = 0;
      CurrentTime.Hundredth = 1;  // ist immer zweite mainloop-runde seit letztem dcf-takt,
                                 // also ist die zeit schon 2x50msec alt!
      CurrentTime.Day = dcf2.day;
      CurrentTime.Weekday = dcf2.wday;
      CurrentTime.Month = dcf2.month;
      CurrentTime.Year = dcf2.year%4;
      RTCSetTime(&CurrentTime);
      if (cfg_year != dcf2.year) {
       cfg_year = dcf2.year;
	   write_eeprom(EE_YEAR,cfg_year);
      }
      dcfsyncstate = 3;
     } else { // if ( (dcf2.min 
      // zeit ist zwar gültig aber nicht konsistent mit der vorigen, 
      // also diese speichern und auf die nächste warten...
      dcfsyncstate = 2;
     }
  
     // immer den aktuellen satz in den ersten puffer schieben 
     memcpy(&dcf1,&dcf2,sizeof(dcftimestamp));
     dcfsequence = 1;
}


void dcfdecode()
{

#if defined DEBUG_DCF77
     fprintf(terminal,"Decoding Time\r\n");
#endif
     if (dcfsequence) {
      if (do_dcf_decode(&dcf2)) {
        // zweite formal gültige zeit empfangen, 
        // also in der nächsten runde kontrollieren und speichern
        dcfsavetimenow = 1;
      } // if (do_dcf_decode
      // zeit viel kaputt, alles von vorne...
      dcfsequence = 0;
     } else { // if (dcfsequence)
      if (do_dcf_decode(&dcf1)) {
       // erste formal gültige zeit empfangen,
       // also auf die zweite warten und dem benutzer hoffnung machen
       dcfsequence = 1;
       dcfsyncstate = 2;
      }
      // zeit ist schrott, nix machen, weiter warten 
      else dcfsyncstate = 0;
     } // if (dcfsequence)
     dcfbitcount = 0;  // mit erstem sekundenbit weitermachen
     // for (i=0; i<8; i++) dcfbuffer[i] = 0; // puffer löschen
     memset(dcfbuffer,0,sizeof(dcfbuffer));
}
// QUERY DCF INPUT AND BUILD TIME

#int_ccp1
void dcf_up()
{
  if (dcfpulsetime > 20) {
#if defined DEBUG_DCF77
    fprintf(terminal,"%u-UP:%u-\r\n",dcfbitcount,dcfpulsetime);
#endif
   if (dcfpulsetime > 100) {
    dcfdecodenow = 1;
   } // if (dcfpulsetime > 150)
   dcfpulsetime = 0;
  }
}

#int_ccp2
void dcf_down()
{
  if (dcfpulsetime > 2) {

   if (!dcfdecodenow) {
#if defined DEBUG_DCF77
     fprintf(terminal,"-DN-%u\r\n",dcfpulsetime);
#endif
     if (dcfbitcount < 59) {
      if (dcfpulsetime > 13) bit_set(dcfbuffer[dcfbitcount/8],dcfbitcount%8);
     }
     dcfbitcount++;
     if (!dcfsyncstate) dcfsyncstate = 1;
   }
   dcfpulsetime = 0;
  }
}

//**********************************************************
// GET TIME FROM RTC AND DISPLAY TO 7SEG
//**********************************************************
void do_time() {
  int16 k3;

  if (!dcfdecodenow) {
  RTCGetTime(&CurrentTime);
  if (CurrentTime.updated == 1) {
   CurrentTime.updated = 0;
#if defined DEBUG_MAIN
   fprintf(terminal,"%02u:%02u:%02u\r\n",CurrentTime.hour,CurrentTime.minute,CurrentTime.second);
#endif
   k3 = (int16)CurrentTime.hour*(int16)100 + (int16)CurrentTime.minute;
   printSAA1064(k3,3);

   if (cfg_alarmstate) {
    if (  (CurrentTime.hour == cfg_alarmhour) 
       && (CurrentTime.minute == cfg_alarmminute)
       && (CurrentTime.second == 0) ) {
#if defined DEBUG_MAIN
     fprintf(terminal," RING! TO: %u\r\n",cfg_alarmtimeout);
#endif
     ringstate = 1;
     ringtimeout = cfg_alarmtimeout;
     ringcount = 0;
     if (cfg_alarmmode) {
      cfg_mp3state = 1;
     }
    }
   }
  }

  }
}


//**********************************************************
// DISPLAY AND MENUS
//**********************************************************

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
  printf(lcd_putc,Exit);
}

void printinfolcd()
{
// lcd_cls();  
 lcd_gotoxy(1,1);
 printf(lcd_putc,Productname);
// printf(vfd_putc,Productname);
 lcd_gotoxy(1,2);
 printf(lcd_putc,"Alarm ");
 printf(lcd_putc,TimeFormat,cfg_alarmhour,cfg_alarmminute);
 if (cfg_alarmstate) printf(lcd_putc," ON");
 else printf(lcd_putc," OFF");
 //lcd_gotoxy(1,3);
 //if (CurrentTime.hour < 11) printf(lcd_putc,GoodMorning);
 //else if(CurrentTime.hour > 21) printf(lcd_putc,GoodNight);
}

void printtimeeditor()
{
 lcd_cls();  
 lcd_gotoxy(1,2);
 printf(lcd_putc,"Edit "); 
 if (!editalarm) {
  sprintf(timebuffer,TimeFormat, CurrentTime.hour,CurrentTime.minute);
  printf(lcd_putc,"Time:  %S",timebuffer);
 }
 else {
  sprintf(timebuffer,TimeFormat, cfg_alarmhour,cfg_alarmminute);
  printf(lcd_putc,"Alarm: %S",timebuffer);
 } 
 lcd_gotoxy(1,4);
 printf(lcd_putc,Exit);
 lcd_gotoxy(8,4);
 printf(lcd_putc,Save);
 lcd_cursoron();
 editorposition = 0;
 lcd_gotoxy(13,2);
}
/*
void printtime()
{
     fprintf(terminal,"\n\rTime: %02u:%02u:%02u\n\rDate: %c%c, %02u.%02u.20%02u",
       CurrentTime.hour,CurrentTime.minute,CurrentTime.second,
       *(Days + (CurrentTime.weekday*2)),  *(Days + (CurrentTime.weekday*2) + 1),
       CurrentTime.day,CurrentTime.month,cfg_year);
}
*/
void togglealarmstate()
{
        if (cfg_alarmstate) cfg_alarmstate = 0;
        else cfg_alarmstate = 1;
        write_eeprom(EE_ALARMSTATE,cfg_alarmstate);
}

void togglemp3state()
{
        if (cfg_mp3state) {
         cfg_mp3state = 0;
         mp3docommand(MP3_CMD_STOP);
        } else {
         cfg_mp3state = 1;
         mp3docommand(MP3_CMD_PLAY);
        }
        write_eeprom(EE_MP3STATE,cfg_mp3state);
}

/**********************************************************
/ Common Timer
/**********************************************************/


#INT_TIMER1
void timeproc()
{
  // set_timer1(3035); // 100msec at 10MHz
   set_timer1(TIMER1_50MSEC_10MHZ); // 50msec at 10MHz
   dcfpulsetime++;
   timeout50msec = 1;

   if (ringstate) {
    // wir klingeln, also in raffiniertem muster den summer umschalten!
    // mit absicht kein else-if um die anzahl der takte gleichzuhalten
    if (ringcount == 0) output_high(BUZZER);
    if (ringcount == 4) output_low(BUZZER);
    if (ringcount == 9) output_high(BUZZER);
    if (ringcount == 13) output_low(BUZZER);
    if (++ringcount==64) ringcount = 0;
   }

   if (!(--timer1sec)) {
    timer1sec = 20;
    timeout1sec = 1;
   }
}

/**********************************************************
/ MAIN
/**********************************************************/
void main () {
  char key;
//  int i;
//write_eeprom(EE_MAGICNUMBER,0x0);
  output_low(BUZZER);
 
  setup_adc(ADC_OFF);
  setup_adc_ports(NO_ANALOGS);
  output_a(0x00);
  set_tris_a(0b00000010);

  set_tris_e(0b00000100);

 // set_tris_c(0b11011110);

  // setup_wdt(WDT_2304MS);

  setup_ccp1(CCP_CAPTURE_RE);    // Configure CCP1 to capture rise
  setup_ccp2(CCP_CAPTURE_FE);    // Configure CCP2 to capture fall
  setup_timer_1(T1_INTERNAL);    // Start timer 1
  enable_interrupts(INT_TIMER1);
  enable_interrupts(INT_CCP1);   // Setup interrupt on rising edge
  enable_interrupts(INT_CCP2);   // Setup interrupt on falling edge
  enable_interrupts(INT_RDA);

  initconfig();

  terminaltimeout = cfg_displaytimeout; // 10sec to timeout backlight BUGBUG


//  vfd_init();
  lcd_init();
  output_low(LCD_BACKLIGHT);
  printinfolcd();

  initSAA1064();
  testSAA1064();
  delay_ms(cfg_ledtesttime);
  initSAA1064();

  do_time();

  enable_interrupts(global);

  mp3docommand(MP3_CMD_CLOSE_TITLEFILE);

 /**********************************************************
 / STATE LOOP
 /**********************************************************/
  for (;;) {
 //   restart_wdt();

    if ((timeout1sec) && (mp3playing)) {
      mp3docommand(MP3_CMD_QUERY_STATUS); 
    }

   // ***********************************
    // alle 50 ms 
    // ***********************************
    if (timeout50msec) {

     timeout50msec = 0;

     // zeit abholen und auf 7seg-display schieben
     // alarmzeit prüfen
     do_time();

     // sonstige interface-updates
     if (cfg_alarmstate) output_high(ALARMSTATE_LED);
     else output_low(ALARMSTATE_LED);
  
     if (cfg_mp3state) output_high(MP3STATE_LED);
     else output_low(MP3STATE_LED);

     if (!terminalstate) {
      lcd_gotoxy(9,1);
      printf(lcd_putc,"%u",dcfsyncstate);
	  lcd_gotoxy(16,2);
      printf(lcd_putc,"%04lu",mp3lasttitle);
     }

    if (mp3status) {
     if (!(--mp3timeout)) {
      mp3completed = 0;
      mp3bufferptr = 0;
      if (--mp3retries) mp3sendcommand();
      else mp3status = MP3_STATUS_READY;
     }
    }   

    } // if (timeout50msec)

    // ***********************************
    // dcf-zeit prüfen und sichern
    // ***********************************
    if (dcfsavetimenow) {
     dcfsavetime();
     dcfsavetimenow = 0;
    }

    // ***********************************
    // dcf-telegramm kopieren
    //   ACHTUNG: während das dcfdecodenow-bit gesetzt ist 
    //            ist die pufferung gesperrt, also schnel1!
    // ***********************************
    if (dcfdecodenow) {       
     dcfdecode();
     dcfdecodenow = 0; // pufferung wieder freigeben
    }  

    // ***********************************
    // ringstatus prüfen
    // ***********************************
    if (ringstate) {           // ringing
     if (timeout1sec) {        
      timeout1sec = 0;
      if (--ringtimeout == 0) { // wenn ringtimeout abgelaufen
      ringstate = 0;           // aufhören
      output_low(BUZZER);
      terminaltimeout = cfg_displaytimeout;
     } else {                     // sonst licht an!
      output_low(LCD_BACKLIGHT);
     }
    }
   } else {
     if (timeout1sec) {
      timeout1sec = 0;
      if (!terminalstate) {
       if (--terminaltimeout == 0) output_high(LCD_BACKLIGHT);
       if (dcfsyncstate == 1) dcfsyncstate = 0;
      }
      
     }
   
   }


    // ***********************************
    // keyboard input prüfen
    // ***********************************
    key=kbd_getc();
    if(key!=0) {              // Keypad Control
#if defined DEBUG_MAIN
      fprintf(terminal,"%c",key);
#endif
      if (ringstate) {        // wir klingeln, alle tasten schalten nur aus
       ringstate = 0;
       output_low(BUZZER);
       terminaltimeout = cfg_displaytimeout;

      } else


	  // statuslose tasten ausserhalb des keyboards
      // sind immer aktiv, unabhängig von statuswerten!
      if (key == 'A') {
       togglealarmstate();
       printinfolcd();
      }

      if (key == 'B') {
       togglemp3state();
       printinfolcd();
      }

      if (key == 'C') {
       mp3setnextfile();
       mp3docommand(MP3_CMD_PLAY);
      }

      if (key == 'D') {
       mp3setprevfile();
       mp3docommand(MP3_CMD_PLAY);
      }

      if (!terminalstate) {		// nichts aktiv 
       output_low(LCD_BACKLIGHT); // alle tasten machen das licht an...
       terminaltimeout = cfg_displaytimeout; // ...und setzen den timeout
       if(key=='*') { // sternchen öffnet das menü
        printmenulcd();
        terminalstate = TSTATE_MENU01;
       }
      }
        
      else if (terminalstate == TSTATE_MENU01) { // menü aktiv
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
        togglealarmstate();
        printmenulcd();
       }
/*       else 
       if (key == '9') {
		
        reset_cpu();
       } */
    }

      else if (terminalstate == TSTATE_EDITTIME) { // zeiteditor aktiv

       if (key == '#') { // escape, nichts ändern, nur raus
        lcd_cursoroff();
        printmenulcd();
        terminalstate = TSTATE_MENU01;

       } else if (key == '*') { // save, daten speichern und raus

        lcd_cursoroff();
        timebuffer[2] = 0;
        timebuffer[5] = 0;
        // fprintf(terminal,"\n\rtimebuffer: %S %S",timebuffer,timebuffer+3);
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
         RTCSetTime(&CurrentTime);
        }
        printmenulcd();
        terminalstate = TSTATE_MENU01;

       } else {  // zahlentasten 

        if ( (key - '0') <= numbers[editorposition]) {
         if ((editorposition == 1) && (timebuffer[0] == '2') && (key > '3')) 
          key = '3';
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
        } // if ( (key - '0')
       } //  if (key == '#')

      } // else if (terminalstate == TSTATE_EDITTIME


   
    }


    // ***********************************
    // seriellen input vom mp3-player prüfen
    // ***********************************

    // evtl anstehende folgeaktionen lostreten
    if (mp3completed == MP3_CMD_CLOSE_TITLEFILE) {
     mp3docommand(MP3_CMD_CLOSE_COUNTERFILE);
    } 

    if (mp3completed == MP3_CMD_CLOSE_COUNTERFILE) {
     mp3docommand(MP3_CMD_OPEN_TITLEFILE);
    } 

    if (mp3completed == MP3_CMD_OPEN_TITLEFILE) {
     mp3docommand(MP3_CMD_OPEN_COUNTERFILE);
    } 

    if (mp3completed == MP3_CMD_OPEN_COUNTERFILE) {
     mp3docommand(MP3_CMD_QUERY_COUNTERS);
    } 

    if (mp3completed == MP3_CMD_PLAY) {
     mp3docommand(MP3_CMD_QUERY_TITLE);
    } 

    if (mp3completed == MP3_CMD_QUERY_TITLE) {
     mp3docommand(MP3_CMD_QUERY_ARTIST);
    }
    // folgeaktionen sind fertig
    mp3completed = 0;


    if (cfg_mp3state) {
     output_low(LCD_BACKLIGHT);
     if (!mp3playing) mp3docommand(MP3_CMD_PLAY);
    } 

    // neuer eingang
    if (bkbhit) {
     key = bgetc();
  //   fprintf(mp3player,"%c",key); // echo to terminal
     if (key == '>') { // prompt, also meldung auswerten
      if ((mp3bufferptr) && (mp3buffer[0] == 'E')) { // fehlermeldung!!
       lcd_gotoxy(17,4);
	   mp3putbufferlcd(0);
       mp3completed = 0;

      } else {

       if (mp3status == MP3_CMD_PLAY) mp3playing = 1;

       if (mp3status == MP3_CMD_STOP) mp3playing = 0;

       if (mp3status == MP3_CMD_QUERY_COUNTERS) {
        mp3buffer[7] = 0;
        mp3lasttitle = atol( mp3buffer + 1 );
       }

       if (mp3status == MP3_CMD_QUERY_TITLE) {
        lcd_gotoxy(1,3);
 	    mp3putbufferlcd(1);
       }

       if (mp3status == MP3_CMD_QUERY_ARTIST) {
        lcd_gotoxy(1,4);
 	    mp3putbufferlcd(1);
       }

       if (mp3status == MP3_CMD_QUERY_STATUS) {
        if (mp3buffer[0] == 'S') {
         mp3playing = 0;
         mp3setnextfile();
        } // if (mp3buffer[0] == 'S')
       } // if (mp3status == MP3_STATUS_QUERIED)


      } // if ((mp3bufferptr) / else
       mp3completed = mp3status;  // status für folgeaktionen aufbewahren
      mp3status = MP3_STATUS_READY; // neues kommando ist zulässig
      mp3bufferptr = 0;
     } else { // if (key == '>') 
       // zeichen zur aktuellen meldung hinzufügen
      if (mp3bufferptr < MP3_REPORT_BUFFER_LENGTH) {
       mp3buffer[mp3bufferptr] = key;
       mp3bufferptr++;
      } //  if (mp3bufferptr 
     } // if (key == '>') 
     
    } // if (bkbhit) 





  } // for (;;)

} // main()
