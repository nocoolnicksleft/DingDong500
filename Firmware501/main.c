
#include "main.h"

#include "dcf.h"
#include "mp3.h"
#include "vfd.h"
#include "keyboard.h"
#include "ds3231.h"

void rtc_write_config(char a, char b) {
}

char rtc_read_config(char a) {
 return 0;
}

void rtc_write_time() {
}

char rtc_read_time() {
 return 0;
}

void rtc_init() {
}


/*********************************************************************/
/* DEFINITIONS                                                       */
/*********************************************************************/
// #define DEBUG_MAIN
#define STARTUP_DELAY 10000
// STATE 
#define TSTATE_DEFAULT 0
#define TSTATE_MENU01 1
#define TSTATE_EDITTIME 2
#define TSTATE_EDITALARM 3
#define TSTATE_LIST_TITLE 4
#define TSTATE_LIST_ARTIST 5
#define TSTATE_EDITFREQUENCY 6



/*********************************************************************/
/* PUBLIC VARIABLES                                                  */
/*********************************************************************/

// CURRENT CONFIGURATION VALUES
unsigned short int cfg_alarmstate = 0;
unsigned short int cfg_alarmmode = 1;
unsigned short int cfg_alarmminute = 30;
unsigned short int cfg_alarmhour = 7;
unsigned short int cfg_alarmdays = 0;
unsigned short int cfg_mp3state = 0;
unsigned short int cfg_mp3title = 0;
unsigned short int cfg_mp3volume = 5;
unsigned short int cfg_mp3timeout = 60;
unsigned short int cfg_displaytimeout = 60;
unsigned short int cfg_showdebug = 0;
unsigned short int cfg_entrymode = 0;

// STRINGS
const char Exit[] = "# Exit";
const char Save[] = "* Save";
const char TimeFormat[] = "%02u:%02u"; 
const char FrequencyFormat[] = "%3u.%02u MHz"; 
const char Days[] = "MoDiMiDoFrSaSo";

/*********************************************************************/
/* LOCAL VARIABLES                                                   */
/*********************************************************************/

char spi0_lock = 0;
char key_pending = 0;
char option1 = 0;
char option2 = 0;

// RUNTIME STATES & COUNTERS
unsigned short int terminaltimeout = 0;
unsigned short int clockterminalstate = 0;
unsigned short int mp3terminalstate = 0;
unsigned short int editorposition;
char timebuffer[11] = "";

#define LIST_LENGTH 7
unsigned short int list_offset = 0;
unsigned short int list_current = 0;

static int timer0Count;

volatile unsigned short int timeout10msec = 0;
volatile unsigned short int timer400msec = 40;
volatile unsigned short int timeout400msec = 0;

/*********************************************************************/
/* LOCAL CONSTANTS                                                   */
/*********************************************************************/

const int numbers[5] = {2,9,0,5,9}; //


/*********************************************************************/
/* SOME DELAY                                                        */
/*********************************************************************/
static void some_delay(int d)
{     
  for(; d; --d);
}

/*********************************************************************/
/* SPI LOCK FUNCTIONS                                                */
/*********************************************************************/

char spi_lock(char id)
{
  
  if (spi0_lock == 0) {
   spi0_lock = id;
  } else id = 0; 

#ifdef DEBUG_MAIN
 //debug_printf("lock fail %u\n",id);
#endif
  return id;
}

void spi_unlock(char id)
{
  if (spi0_lock == id) spi0_lock = 0;
}


/*********************************************************************/
/* INTERRUPT RTC (ILR, IRQ)                                          */
/*********************************************************************/

static int updateclocknow = 1;

static void rtcISR(void) __attribute__ ((interrupt ("IRQ")));

void rtcISR(void)
{

  updateclocknow = 1;
  
  ILR |= 1; // Clear interrupt flag

  VICVectAddr = 0;
  
}


int defaultOccurred = 0;

/*********************************************************************/
/* DEFAULT INTERRUPT                                                 */
/*********************************************************************/
static void defaultISR(void) __attribute__ ((interrupt ("IRQ")));

static void
defaultISR(void)
{
  defaultOccurred = 1;

  /* Update VIC priorities */
  VICVectAddr = 0;
}

/*********************************************************************/
/* I2C INTERRUPT                                                     */
/*********************************************************************/
static void i2cISR(void) __attribute__ ((interrupt ("IRQ")));

void i2cISR(void)
{

  

  /* Update VIC priorities */
  VICVectAddr = 0;
}


/*********************************************************************/
/* INTERRUPT TIMER0 (FIQ)                                            */
/*********************************************************************/
static void timer0ISR(void) __attribute__ ((interrupt ("IRQ")));

static void
timer0ISR(void)
{

  if (++timer0Count % 2) IO1CLR = P1_BIT_MONITOR_2;
  else IO1SET = P1_BIT_MONITOR_2;

   if (dcfdeltatimer > 253) dcfdeltatimer = 0;
   else dcfdeltatimer++;

   timeout10msec = 1;

   if (!(--timer400msec)) {
    timer400msec = 40;
    timeout400msec = 1;
   }

  /* Clear the timer 0 interrupt */
  T0IR = 0xFF;
  /* Update VIC priorities */
  VICVectAddr = 0;
}


//**********************************************************
// GENERAL KEYBOARD HANDLING
//**********************************************************

#define KBD_SCK 4
#define KBD_SOUT 5
#define KBD_SIN 6
#define KBD_CLR 10
#define KBD_SS BIT_11
#define KBD_IRQ 15
#define KBD_SS_DELAY 1200

volatile unsigned char last_key = 0;



void keyboardISR(void) __attribute__ ((interrupt ("IRQ")));

void keyboardISR(void)   
{
  char d;
  d = S0SPSR;

  if (spi_lock(2)) {
  
   IOCLR = PO_BIT_KBD_SEL;
   some_delay(KBD_SS_DELAY);

   S0SPDR = 0xAA;  
  } else {
    key_pending++;
  }


  EXTINT = EXTINT_EINT3;
  VICVectAddr = 0;
  
}


static void keyISR(void) __attribute__ ((interrupt ("IRQ")));

static void keyISR(void)  
{
  
  if (spi0_lock == 2) {
   if (S0SPSR & S0SPSR_SPIF) {
    last_key = S0SPDR;
//    if (last_key == 0) last_key = KEY_1_MAKE;
   }
   IOSET = PO_BIT_KBD_SEL;
  spi_unlock(2);
  }
  
   S0SPINT = BIT_00; // Clear interrupt
   VICVectAddr = 0;

}


int kbd_writebyte(char data)
{
  if (spi_lock(2)) {
   S0SPCR   = (BIT_05 | BIT_03 );
   IOCLR = PO_BIT_KBD_SEL;
   some_delay(KBD_SS_DELAY);
   S0SPDR = data;    
 //  while ((S0SPSR & BIT_07) != BIT_07);
 some_delay(KBD_SS_DELAY);
   data = S0SPDR;
   IOSET = PO_BIT_KBD_SEL;
   S0SPCR   = (BIT_05 | BIT_03 | BIT_07);
   spi_unlock(2);
   return S0SPDR;
  }

/*
  //if (spi_lock(2)) {
   S0SPCR   = (S0SPCR_MSTR | S0SPCR_CPHA );
   //IOCLR = KBD_SS;
   //some_delay(KBD_SS_DELAY);
   S0SPDR = data;    
   while (!(S0SPSR & S0SPSR_SPIF));
   data = S0SPDR;
   //IOSET = KBD_SS;
   S0SPCR   = (S0SPCR_MSTR | S0SPCR_CPHA | S0SPCR_SPIE);
   //spi_unlock(2);
   return S0SPDR;
  //}
*/

}

char kbd_readbyte()
{
  char data;
  data =last_key;
  last_key = 0;
  return data;
}




/*********************************************************************/
/* DISPLAY ROUTINES                                                  */
/*********************************************************************/

void clearcenter()
{
  vfd_docmd4(VFD_CMD_AREA_CLEAR,0,16 + YOFFSET,126,40 + YOFFSET);
}

void cleardebug()
{
  vfd_docmd4(VFD_CMD_AREA_CLEAR,0,17 + YOFFSET,126,22 + YOFFSET);
}

void printupdatevfd()
{
 static short int last_dcfsync = 0;
 static short int last_mp3state = 0;

//  vfd_setcursor(54,11);
//  vfd_putc(VFD_CMD_PIXEL_CLEAR ^ dcfsync);

  if (last_dcfsync != dcfsync) {
   if (dcfsync) {
    vfd_setcursor(64,6 + YOFFSET);
    vfd_putc(0x1);
   }
   else vfd_docmd4(VFD_CMD_AREA_CLEAR,64,4 + YOFFSET,71,14 + YOFFSET);
   last_dcfsync = dcfsync;
  }

  vfd_setcursor(67,8 + YOFFSET);
  vfd_putc(VFD_CMD_PIXEL_CLEAR ^ dcfsignal);

  vfd_setcursor(67,9 + YOFFSET);
  vfd_putc(VFD_CMD_PIXEL_CLEAR ^ dcfvalid);

  vfd_setcursor(67,10 + YOFFSET);
  vfd_putc(VFD_CMD_PIXEL_CLEAR ^ dcfreceived);

  if (cfg_showdebug) {
   vfd_putc(VFD_CMD_FONT_MINI);
   vfd_setcursor(0,22 + YOFFSET);
   vfd_printf("%02u %02u %02u %03u %04lu %04lu",SEC,dcfbitcount,dcf2.month,cfg_mp3volume,cfg_mp3title,mp3lasttitle);
  }

   if (clockterminalstate != TSTATE_EDITALARM) {
    vfd_putc(VFD_CMD_FONT_5X7);
    vfd_setcursor(34,7 + YOFFSET);
    if (!cfg_alarmstate) vfd_printf("OFF  ");
    else vfd_printf(TimeFormat,cfg_alarmhour,cfg_alarmminute);
   }

  // sonstige interface-updates
/*
 TODO: Funktion rausfinden und anders lösen
  if (cfg_mp3state) IOSET = BIT_28;
  else IOCLR = BIT_28; 
*/

  if (last_mp3state != cfg_mp3state) {
   if (cfg_mp3state) kbd_writebyte(KEY_ID_LED | KEY_MAKE | 0x1);
   else kbd_writebyte(KEY_ID_LED | KEY_BREAK | 0x1);
   last_mp3state = cfg_mp3state;
  }
}

void printinfovfd()
{
  vfd_docmd4(VFD_CMD_OUTLINE_SET,0,15 + YOFFSET,126,15 + YOFFSET);
  vfd_putc(VFD_CMD_FONT_5X7);
  vfd_setcursor(0,7 + YOFFSET);
  vfd_printf("ALARM");
  printupdatevfd();

 //vfd_gotoxy(1,3);
 //if (CurrentTime.hour < 11) printf(vfd_putc,GoodMorning);
 //else if(CurrentTime.hour > 21) printf(vfd_putc,GoodNight);
}


void printtimecursor()
{
 static unsigned short int cursorstate = 0;
 if (clockterminalstate == TSTATE_EDITALARM) {
  vfd_putc(VFD_CMD_FONT_5X7);
  vfd_setcursor(34 + (editorposition * 6),7 + YOFFSET);
 } else
 if (clockterminalstate == TSTATE_EDITTIME)  {
  vfd_putc(VFD_CMD_FONT_10X14);
  vfd_setcursor(72 + (editorposition * 11),14 + YOFFSET);
 } else
 if (clockterminalstate == TSTATE_EDITFREQUENCY)  {
  vfd_putc(VFD_CMD_FONT_10X14);
  vfd_setcursor(6 + (editorposition * 11),38 + YOFFSET);
 }
 if (cursorstate) vfd_putc(timebuffer[editorposition]);
 else vfd_putc(0x5F);
 cursorstate = !cursorstate;
}

void printtimeeditor()
{
 clearcenter();
 vfd_setcursor(0,30 + YOFFSET);
 vfd_printf(Exit);
 vfd_putc(' ');
 vfd_printf(Save);
 editorposition = 0;
 if (clockterminalstate == TSTATE_EDITALARM) {
  vfd_putc(VFD_CMD_FONT_5X7);
  vfd_setcursor(34,7 + YOFFSET);
  vfd_printf(timebuffer);
 }
 printtimecursor();
}

void printfrequencyeditor()
{
 //clearcenter();
 vfd_setcursor(0,48 + YOFFSET);
 vfd_printf(Exit);
 vfd_putc(' ');
 vfd_printf(Save);
 editorposition = 0;
 printtimecursor();
}

void printlistcursor()
{
 vfd_docmd4(VFD_CMD_AREA_CLEAR,0,17 + YOFFSET,3,58 + YOFFSET);
 vfd_putc(VFD_CMD_FONT_MINI);
 vfd_setcursor(0,22+(6*list_current) + YOFFSET);
 vfd_putc('>');
}

void printlist()
{
 char i;
 vfd_docmd4(VFD_CMD_AREA_CLEAR,0,17 + YOFFSET,3,58 + YOFFSET);
 vfd_putc(VFD_CMD_FONT_MINI);
 for (i = 0; i < LIST_LENGTH; i++) {
  vfd_setcursor(5,22+(6*i) + YOFFSET);
  //BUGBUGvfd_printf("%s - %s",mp3list[i+list_offset].artist,mp3list[i+list_offset].title);
  vfd_putc(VFD_CMD_CLEAR_EOL);
 }
 printlistcursor();
}



void setalarmstate(unsigned short int newstate)
{
    cfg_alarmstate = newstate;
    if (cfg_alarmstate) kbd_writebyte(KEY_ID_LED | KEY_MAKE | 0x0);
    else kbd_writebyte(KEY_ID_LED | KEY_BREAK | 0x0);
    printinfovfd();
    rtc_write_config(RTC_ALARMSTATE,cfg_alarmstate);
}


void setmp3state(unsigned short int newstate)
{
   if (newstate) {
     cfg_mp3state = newstate;
     kbd_writebyte(KEY_ID_LED | KEY_MAKE | 0x1);
     if (!mp3playing) {
      amp_standby();
      mp3start();
     }
   } else {
     cfg_mp3state = newstate;
     mp3clear();
     kbd_writebyte(KEY_ID_LED | KEY_BREAK | 0x1);
     amp_off();
   }
}

void mp3save()
{
   rtc_write_config(RTC_MP3TITLE1,cfg_mp3title);
   rtc_write_config(RTC_MP3TITLE2,cfg_mp3title >> 8);
}


void setentrymode(int newmode)
{
  cfg_entrymode = newmode;
  if (cfg_entrymode)  kbd_writebyte(KEY_ID_LED | KEY_MAKE | 0x3);
  else kbd_writebyte(KEY_ID_LED | KEY_BREAK | 0x3);
}

void setmp3volume(int newv)
{
    cfg_mp3volume = newv;
    mp3commandpush(MP3_CMD_SET_VOLUME);
    rtc_write_config(RTC_MP3VOLUME,cfg_mp3volume);
    mp3displayvolume();
}

//**********************************************************
// KEY HANDLING
//**********************************************************
void dokeyboard()
{
  char key;
  int intbuffer;

    key = kbd_readbyte();

    if(key!=0) {              // Keypad Control
#if defined DEBUG_MAIN
     debug_printf("key: %X\n",key);
#endif

      // statuslose tasten ausserhalb des keyboards
      // sind immer aktiv, unabhängig von statuswerten!
      if (key == KEY_A_MAKE) {
       setalarmstate(!cfg_alarmstate);
      }

      if (key == KEY_B_MAKE) {
        setmp3state(!cfg_mp3state);
        //write_eeprom(EE_MP3STATE,cfg_mp3state);
      }

      if (key == KEY_D_MAKE) {
        setentrymode(!cfg_entrymode);
        //write_eeprom(EE_MP3STATE,cfg_mp3state);
      }
      
      /********************************************************/
      /* MP3 läuft                                            */
      /********************************************************/
      if (cfg_mp3state) {

        if (key == ROTARY_0_RIGHT) {
          if (cfg_mp3volume < MP3_VOLUME_MAXIMUM) 
            setmp3volume(cfg_mp3volume + 1);
        }
  
        if (key == ROTARY_0_LEFT) {
          if (cfg_mp3volume > MP3_VOLUME_MINIMUM)
            setmp3volume(cfg_mp3volume - 1);
        }
  
  
        if (key == ROTARY_1_RIGHT) {
          mp3setnextfile();
          mp3commandpush(MP3_CMD_PLAY);
        }
  
        if (key == ROTARY_1_LEFT) {
          mp3setprevfile();
          mp3commandpush(MP3_CMD_PLAY);
        }
              
     }

      /********************************************************/
      /* Kein Modus aktiv                                     */
      /********************************************************/
      if (!clockterminalstate) {		// nichts aktiv 

      /********************************************************/
      /* MP3 läuft nicht                                      */
      if (!cfg_mp3state) {
       if ((key == ROTARY_1_RIGHT) || (key == ROTARY_1_LEFT) ) {
        // wenn !listenmodus liste anzeigen, current als aktiv kennzeichnen, listemodus an
        if (clockterminalstate != TSTATE_LIST_TITLE) {
         clockterminalstate = TSTATE_LIST_TITLE;
         if (cfg_mp3title > (mp3lasttitle - LIST_LENGTH)) {
          list_offset = mp3lasttitle - LIST_LENGTH;
          list_current = LIST_LENGTH - (mp3lasttitle - cfg_mp3title);
         } else {
          list_offset = cfg_mp3title;
          list_current = 0;
         }
         clearcenter();
         mp3clear();
         vfd_putc(VFD_CMD_FONT_MINI);
         vfd_setcursor(0,64 + YOFFSET);
         vfd_printf("* to select   # to cancel");
         char i;
         for (i = 0; i < LIST_LENGTH; i++) {
          
         }

         printlist();
        }
       }
      }


       if (cfg_entrymode) {

         if (key == KEY_1_MAKE) {
            clockterminalstate = TSTATE_EDITALARM;
            sprintf(timebuffer,TimeFormat,cfg_alarmhour,cfg_alarmminute);
            printtimeeditor();
          
         } else if (key == KEY_2_MAKE) {

            clockterminalstate = TSTATE_EDITTIME;
            sprintf(timebuffer,TimeFormat,HOUR,MIN);
            printtimeeditor();
          
         } else if (key == KEY_3_MAKE) {
/*
            clockterminalstate = TSTATE_EDITFREQUENCY;
            sprintf(timebuffer,FrequencyFormat,radioFrequency / 1000, radioFrequency % 1000);
            printfrequencyeditor();
  */        
         } 
         else
         if (key == KEY_9_MAKE) {
           cfg_showdebug = !cfg_showdebug;
           if (!cfg_showdebug) cleardebug();
         }
         else
         if (key == KEY_4_MAKE) {
          amp_off();
         }
         else
         if (key == KEY_5_MAKE) {
          amp_standby();
         }
         else
         if (key == KEY_6_MAKE) {
          amp_power();
         }
         else
         if (key == KEY_7_MAKE) {
          vfd_off();
         }
         else
         if (key == KEY_8_MAKE) {
          vfd_on();
         }

       }

     } else if (clockterminalstate == TSTATE_LIST_TITLE)  {

       if (key == KEY_NUMBER_BREAK) { // escape, nichts ändern, nur raus
        clearcenter();
        mp3clear();
        clockterminalstate = 0;

       } else if (key == KEY_ASTERISK_BREAK) { // save, daten speichern und raus
        clearcenter();
        mp3clear();
        cfg_mp3title = list_offset + list_current;
        mp3save();
        clockterminalstate = 0;

       } else if (key == ROTARY_1_RIGHT) {
         // wenn ende nächsten block lesen und anzeigen, cursor nach oben
         // sonst markierung nach unten
         if (list_current == (LIST_LENGTH-1)) {
          if (list_offset < (mp3lasttitle - LIST_LENGTH + 1)) {
           list_offset++;
           printlist();
          }
         } else {
          if (list_current < (LIST_LENGTH-1)) {
           list_current++;
           printlistcursor();
          }
         }
         
        }
        else if (key == ROTARY_1_LEFT) {
         // wenn anfang dann vorigen block lesen und anzeigen, cursor nach unten
         // sonst markierung nach oben
         if (!list_current) {
          if (list_offset > 0) {
           list_offset--;
           printlist();
          }
         } else {
          list_current--;
          printlistcursor();
         }
         
        }

      }

        
      else if ((clockterminalstate == TSTATE_EDITTIME) || (clockterminalstate == TSTATE_EDITALARM)  || (clockterminalstate == TSTATE_EDITFREQUENCY)) { // zeiteditor aktiv

       if (key == KEY_NUMBER_BREAK) { // escape, nichts ändern, nur raus

        clockterminalstate = TSTATE_DEFAULT;
	clearcenter();
        mp3clear();
        if (cfg_mp3state) mp3displaycurrentdata();
        updateclocknow = 1;

       } else if (key == KEY_ASTERISK_BREAK) { // save, daten speichern und raus
        
        if (clockterminalstate == TSTATE_EDITALARM) {

         timebuffer[2] = 0;
         timebuffer[5] = 0;
         cfg_alarmhour = atoi(timebuffer);
         cfg_alarmminute = atoi(timebuffer + 3);
         rtc_write_config(RTC_ALARMMINUTE,cfg_alarmminute);
         rtc_write_config(RTC_ALARMHOUR,cfg_alarmhour);

        } else if (clockterminalstate == TSTATE_EDITTIME) {

         timebuffer[2] = 0;
         timebuffer[5] = 0;
         HOUR = atoi(timebuffer);
         MIN = atoi(timebuffer + 3);
         SEC= 0;
         rtc_write_time();
         updateclocknow = 1;

        } else {
/*
         timebuffer[3] = 0;
         timebuffer[6] = 0;
         intbuffer = (atoi(timebuffer)*100) + (atoi(timebuffer + 4) * 1);
         intbuffer *= 10;
         setradiofrequency(intbuffer);
*/
        }

        clockterminalstate = TSTATE_DEFAULT;
	clearcenter();
        mp3clear();
        if (cfg_mp3state) mp3displaycurrentdata();

       } else if ( ((key & KEY_ID) == 0x0) && ((key & KEY_MAKE) == KEY_MAKE) ) {  // zahlentasten 

        if (clockterminalstate == TSTATE_EDITALARM) vfd_putc(VFD_CMD_FONT_5X7);
        else  vfd_putc(VFD_CMD_FONT_10X14);


        if ((clockterminalstate == TSTATE_EDITTIME) || (clockterminalstate == TSTATE_EDITALARM)) {

            if ( (key - 16) <= numbers[editorposition]) {
    
             if ((editorposition == 1) && (timebuffer[0] == '2') && (key > (16+3))) 
              key = 16 + 3;
             if ((editorposition == 0) && (timebuffer[1] >  '3') && (key == (16+2))) {
              timebuffer[1] = '3';
              if (clockterminalstate == TSTATE_EDITALARM) vfd_setcursor(40,7 + YOFFSET);
              else  vfd_setcursor(83,14 + YOFFSET);
              vfd_putc('3');
             } 
    
             if (clockterminalstate == TSTATE_EDITALARM) vfd_setcursor(34+(editorposition*6),7 + YOFFSET);
             else vfd_setcursor(72 + (editorposition * 11),14 + YOFFSET);
    
             vfd_putc(key - 16 + '0');
             timebuffer[editorposition] = key -16 +'0';
             if (editorposition == 1) editorposition = 3;
             else if (editorposition==4) editorposition = 0;
             else editorposition++;
            } // if ( (key - '0')

        } else {


          if ((key >= 16) && (key <= 16+9)) {

             if ((editorposition == 0) && (key > (16 + 1) ) ) 
              key = 16 + 0;

         //    if ((editorposition == 1) && (timebuffer[0] == '1')) 
         //     key = 16 + 0;

             if ((editorposition == 1) && (timebuffer[0] == '0') && ((key < (16 + 8)) || (key > (16 + 9))) ) 
              key = 16 + 8;

             if ((editorposition == 2) && (timebuffer[1] == '8') && ((key < (16 + 7))) ) 
              key = 16 + 7;

             if ((editorposition == 5) && (key != (16 + 0) ) && (key != (16 + 5) )) 
              key = 16 + 0;

             vfd_setcursor(6 + (editorposition * 11), 38 + YOFFSET);
    
             vfd_putc(key - 16 + '0');

             timebuffer[editorposition] = key - 16 + '0';
             
             if (editorposition == 2) editorposition = 4;
             else if (editorposition == 5) editorposition = 0;
             else editorposition++;

          }

        }

       } //  if (key == '#')
        if (clockterminalstate != TSTATE_DEFAULT) printtimecursor();

      } // else if (clockterminalstate == TSTATE_EDITTIME  
    }
}


/*********************************************************************/
/* INIT ROUTINE                                                      */
/*********************************************************************/

void setup_ports(void)
{

  PINSEL0 = (
              (0x1 << PINSEL0_P0_2_BIT)  | // P0.2 = SCL0
              (0x1 << PINSEL0_P0_3_BIT)  | // P0.3 = SDA0
              (0x1 << PINSEL0_P0_4_BIT)  | // P0.4 = SCK0
              (0x1 << PINSEL0_P0_5_BIT)  | // P0.5 = MISO0
              (0x1 << PINSEL0_P0_6_BIT)  | // P0.6 = MOSI0
              // (0x1 << PINSEL0_P0_7_BIT)  | // P0.7 = SSEL0
              (0x1 << PINSEL0_P0_8_BIT)  | // P0.8 = TxD (UART1)
              (0x1 << PINSEL0_P0_9_BIT)  | // P0.9 = RxD (UART1)
              (0x2 << PINSEL0_P0_14_BIT)   // P0.14 = EINT1
            );

  PINSEL1 = (
              (0x1 << PINSEL1_P0_16_BIT)  | // P0.16 = EINT0
              (0x2 << PINSEL1_P0_17_BIT)  | // P0.17 = SCK1
              (0x2 << PINSEL1_P0_18_BIT)  | // P0.18 = MISO1
              (0x2 << PINSEL1_P0_19_BIT)  | // P0.19 = MOSI1
              (0x2 << PINSEL1_P0_20_BIT)  | // P0.20 = SSEL1
              (0x2 << PINSEL1_P0_30_BIT)    // P0.30 = EINT3
            );  

  IODIR = ( PO_BIT_UMP3_TX |
            PO_BIT_VFD_SS |
            PO_BIT_VFD_RES |
            PO_BIT_KBD_SEL |
            PO_BIT_KBD_CLR |
            P0_BIT_LED_RDY |
            P0_BIT_LED_1 |
            P0_BIT_LED_2 |
            P0_BIT_AMP_ON |
            P0_BIT_AMP_STANDBY
            );

  IO1DIR = ( P1_BIT_MONITOR_1 |
             P1_BIT_MONITOR_2 );

  IOCLR   = 0xFFFFFFFF;
  IO1CLR   = 0xFFFFFFFF;
  
  IOSET = PO_BIT_KBD_SEL;
  //IOSET = PO_BIT_VFD_RES;
  IOSET = PO_BIT_VFD_SS;

  /************************************************/
  /* SPI0 KEYBOARD                                */
  /************************************************/
  S0SPCR = 0x00;
  S0SPCCR  = 0x000000FF;
  S0SPCR   = (S0SPCR_MSTR | S0SPCR_CPHA | S0SPCR_SPIE); // Master mode, sync on end, interrupt enabled
  
  /************************************************/
  /* SSP VFD                                      */ 
  /************************************************/
  SSPCPSR  = 0xFF; // Clock Divider, 4 min, even only!!
  SSPCR0   = (0x3 << 8) | 0x7; // SCR=3, 8 Bit 
  SSPCR1   = SSPCR1_SSE; // ENABLE SSP


  /************************************************/
  /*  0  EINT1 (DCF PULSE)                        */
  /*  1  EINT0 (DCF PULSE)                        */
  /*  2  EINT2 (I2C)                              */
  /*  4  EINT3 (KEYBOARD)                         */
  /************************************************/

// TODO turn on EINT2 when needed

  EXTMODE  = (EXTMODE_EXTMODE0 | EXTMODE_EXTMODE1 | EXTMODE_EXTMODE2 | EXTMODE_EXTMODE3);
  EXTPOLAR = (EXTPOLAR_EXTPOLAR1 | EXTPOLAR_EXTPOLAR2 | EXTPOLAR_EXTPOLAR3);
  INTWAKE = (INTWAKE_EXTWAKE2);
  
  EXTINT = (EXTINT_EINT0 | EXTINT_EINT1 | EXTINT_EINT2 | EXTINT_EINT3); // Reset interrupt flags

  /************************************************/
  /*  2  TIMER0                                   */
  /************************************************/

  T0TCR = 0; // Reset timer 0 
  T0PR = 252; // 10MHz: 20MHz: 504 / 14. MHz: 370 /* Set the timer 0 prescale counter 
  T0MR0 = 989; // 20/14. MHz: 989 / Set timer 0 match register 
  T0MCR = 3; // Generate interrupt and reset counter on match 
  T0TCR = 1; // Start timer 0 

  /************************************************/
  /*  6  RTC (SECOND CHANGE)                      */
  /************************************************/

  //PREINT = int (pclk / 32768) - 1 = 304 
  //PREFRAC = pclk - ((PREINT +1) x 32768) = 5,760
  
  //PREINT = int (18431600 / 32768) - 1 = 561
  //PREFRAC = 18431600 - ((561 +1) x 32768) = 15,984

  // 14. MHz
  //PREINT = int (36863750 / 32768) - 1 = 1123
  //PREFRAC = 36863750 - ((1123 +1) x 32768) = 32,518

  // 20 MHz
  //PREINT = int (50000000 / 32768) - 1 = 1524
  //PREFRAC = 50000000 - ((1524 +1) x 32768) = 28,800

  // 10 MHz
  //PREINT = int (25000000 / 32768) - 1 = 761
  //PREFRAC = 25000000 - ((761 +1) x 32768) = 30,784

  CCR = CCR_CTCRST | CCR_CLKSRC; // RTC Disable/Reset, Clocksource=32kHz Crystal
  ILR = ILR_RTCCIF | ILR_RTCALF; // Clear interrupts
  CIIR = CIIR_IMSEC; // Interrupt every 1 SEC
  CCR = CCR_CLKEN | CCR_CLKSRC; // RTC Enable, Clocksource=32kHz Crystal
/*
  ILR |= 0x3; // Clear interrupts
  CCR = 0x2;
  PREINT = 761;
  PREFRAC = 30784;
  CIIR = 0x1;
  CCR = 0x1;
*/
  /************************************************/
  /*  5  UART1 (MP3 BOARD)                        */
  /************************************************/

  // Fosc = 14. MHz
  // xclk (measured) = 18,4316 MHz
  // pclk (calculated) = 18,431875 MHz
  // cclk (calculated) = 36,86375 MHz 
  //  Divisor: 36863750 / (16 * 19200) = 119,99918619791666666666666666667  ^  120 = 0x78 
  //  Divisor: 36863750 / (16 * 38400) = 59,999593098958333333333333333333  ^  60 = 0x3C 
  //  Divisor: 36863750 / (16 * 115200) = 19,999864366319444444444444444444  ^  20 = 0x14

  // Fosc = 20 MHz
  // xclk (measured) = 
  // pclk (calculated) = 25,00000 MHz
  // cclk (calculated) = 50,00000 MHz 
  //  Divisor: 50000000 / (16 * 19200) = ^   = 0x
  //  Divisor: 50000000 / (16 * 38400) =   ^  = 0x 
  //  Divisor: 50000000 / (16 * 115200) = 27,126706676750567761970744389383  ^  27 = 0x1B

  // Fosc = 10 MHz
  // xclk (measured) = 
  // pclk (calculated) = 10,00000 MHz
  // cclk (calculated) = 25,00000 MHz 
  //  Divisor: 25000000 / (16 * 19200) = ^   = 0x
  //  Divisor: 25000000 / (16 * 38400) = 40,690104166666666666666666666667 ^  41 = 0x28
  //  Divisor: 25000000 / (16 * 115200) = 13,563368055555555555555555555556  ^  14 = 0x0E
/*
  U1LCR = 0x00000083; // line control 
  U1DLM = 0x0;  // divisor msb
  U1DLL = 0x28; // divisor lsb
  U1LCR = 0x00000003; // line control
  U1FCR = 0x41; // FIFO control
  U1IER = 0x1; // interupt enable (RBR=0x1, THRE=0x2)
*/
}

#define IRQslot_en 0x20

void setup_interrupts(void)
{
  /************************************************/
  /* DEFAULT                                      */
  /************************************************/
  VICDefVectAddr = (unsigned int)defaultISR;

  // Clear Interrupt selections
  VICIntSelect = 0x00000000;

  // Clear all interrupts
  VICIntEnClr = 0xFFFFFFFF;

  /************************************************/
  /*  0  EINT1 (DCF PULSE)                        */
  /************************************************/
  
  VICIntSelect &= ~VICIntSelect_EINT1;     // Interrupt is IRQ 
  VICIntEnable = VICIntEnable_EINT1;       // Enable interrupt 
  VICVectCntl0 =  IRQslot_en | VICIntEnable_EINT1_BIT; // Enable vector for slot 0, select EINT1
  VICVectAddr0 = (unsigned int)dcfupISR;   // Set vector address 

  /************************************************/
  /*  1  EINT0 (DCF PULSE)                        */
  /************************************************/
  
  VICIntSelect &= ~VICIntSelect_EINT0;     // IRQ interrupt 
  VICIntEnable = VICIntEnable_EINT0;       // Enable interrupt 
  VICVectCntl1 = IRQslot_en | VICIntEnable_EINT0_BIT; // Enable vector for slot 01 select EINT0 
  VICVectAddr1 = (unsigned int)dcfdownISR; // Set vector address 
  

  /************************************************/
  /*  2  TIMER0                                   */
  /************************************************/
  
  VICIntSelect &= ~VICIntSelect_TIMER0;   // IRQ interrupt  
  VICIntEnable = VICIntEnable_TIMER0;     // Enable interrupt   
  VICVectCntl2 = IRQslot_en | VICIntEnable_TIMER0_BIT; // Enable vector for slot 2, select TIMER0
  VICVectAddr2 = (unsigned int)timer0ISR; // Set vector address 
  

  /************************************************/
  /*  3  SPI0 KEYBOARD                            */
  /************************************************/
  
  VICIntSelect &= ~VICIntSelect_SPI0;    // IRQ interrupt 
  VICIntEnable = VICIntEnable_SPI0;      // Enable interrupt   
  VICVectCntl3 = IRQslot_en | VICIntEnable_SPI0_BIT; // Enable vector for slot 3, select SPI0
  VICVectAddr3 = (unsigned int)keyISR;   // Set vector address 
  

  /************************************************/
  /* 4  EINT3 (KEYBOARD)                          */
  /************************************************/
  
  VICIntSelect &= ~VICIntSelect_EINT3;   // IRQ interrupt 
  VICIntEnable = VICIntEnable_EINT3;     // Enable interrupt 
  VICVectCntl4 = IRQslot_en | VICIntEnable_EINT3_BIT; // Enable vector for slot 4, select EINT2
  VICVectAddr4 = (unsigned int)keyboardISR; // Set vector address
  

  /************************************************/
  /*  5  UART1 (MP3 BOARD)                        */
  /************************************************/
  
  VICIntSelect &= ~VICIntSelect_UART1;  // IRQ interrupt 
  VICIntEnable = VICIntEnable_UART1;    // Enable interrupt
  VICVectCntl5 = IRQslot_en | VICIntEnable_UART1_BIT;  // Enable vector for slot 5, select UART1
  VICVectAddr5 = (unsigned int)serialISR; // Set vector address
  

  /************************************************/
  /*  6  RTC (SECOND CHANGE)                      */
  /************************************************/
  
  VICIntSelect &= ~VICIntSelect_RTC;    // IRQ interrupt 
  VICIntEnable = VICIntEnable_RTC; // Enable interrupt
  VICVectCntl6 = IRQslot_en | VICIntEnable_RTC_BIT; // Enable vector for slot 6, select RTC
  VICVectAddr6 = (unsigned int)rtcISR; // Set vector address
  

  /************************************************/
  /*  7 I2C                                       */
  /************************************************/
  /*
  VICIntSelect &= ~VICIntSelect_I2C0;    // IRQ interrupt 
  VICIntEnable = VICIntEnable_I2C0; // Enable interrupt
  VICVectCntl7 = IRQslot_en | VICIntEnable_I2C0_BIT; // Enable vector for slot 7, select I2C
  VICVectAddr7 = (unsigned int)i2cISR; // Set vector address
  */

 // __ARMLIB_enableFIQ();
 // ILR |= 1;

  libarm_enable_irq();

}


void readconfig()
{
#ifdef DEBUG_MAIN
  debug_printf("-%X-",IO1PIN);
#endif
  if (IO1PIN & P1_BIT_CONFIG_1) option1 = 1;
  if (IO1PIN & P1_BIT_CONFIG_2) option2 = 1;
}

  static int counter = 0;

int secondticks = 0;
void tick( void) {
  static int status = 0; 
//  if (counter > secondticks) {
//    counter = 0;
    IOSET = (P0_BIT_LED_RDY | P0_BIT_LED_1 | P0_BIT_LED_2);
    if (status == 0) {
      status = 1;
      IOCLR = P0_BIT_LED_RDY;  
    } else if (status == 1) {
      status = 2;
      IOCLR = P0_BIT_LED_1;  
    } else {
      status = 0;
      IOCLR = P0_BIT_LED_2;  
    }    
//  }
//  counter++;
}

int main( void) {
/*
  setupPorts();
  secondticks = ctl_get_ticks_per_second() >> 1;
  libarm_set_irq(1);
  ctl_start_timer(tick);
*/

  char c;

  some_delay(STARTUP_DELAY);

  setup_ports();



  amp_off();
  readconfig();

  vfd_init();
  vfd_setgraphics_macro(0x1,8,vfd_turm);


  rtc_init();
  rtc_read_time();

  if (rtc_read_config(RTC_MAGICNUMBER) == 218) {
   cfg_alarmstate = rtc_read_config(RTC_ALARMSTATE);
   cfg_alarmmode  = rtc_read_config(RTC_ALARMMODE);
   cfg_alarmminute = rtc_read_config(RTC_ALARMMINUTE);
   cfg_alarmhour = rtc_read_config(RTC_ALARMHOUR);
   cfg_alarmdays = rtc_read_config(RTC_ALARMDAYS);
   cfg_mp3volume = rtc_read_config(RTC_MP3VOLUME);
   cfg_mp3title  = rtc_read_config(RTC_MP3TITLE1);
   cfg_mp3title  |= ( rtc_read_config(RTC_MP3TITLE2) << 8);
   cfg_showdebug = rtc_read_config(RTC_SHOWDEBUG);
   //radioFrequency = rtc_read_config_int(RTC_FREQUENCY);
#ifdef DEBUG_MAIN
   debug_printf("read config\n");   
#endif
  } else {
   rtc_write_config(RTC_ALARMSTATE,cfg_alarmstate);
   rtc_write_config(RTC_ALARMMODE,cfg_alarmmode);
   rtc_write_config(RTC_ALARMMINUTE,cfg_alarmminute);
   rtc_write_config(RTC_ALARMHOUR,cfg_alarmhour);
   rtc_write_config(RTC_ALARMDAYS,cfg_alarmdays);
   rtc_write_config(RTC_MP3VOLUME,cfg_mp3volume);
   cfg_mp3title = 0;
   rtc_write_config(RTC_MP3TITLE1,cfg_mp3title);
   rtc_write_config(RTC_MP3TITLE2,cfg_mp3title >> 8);
   rtc_write_config(RTC_SHOWDEBUG,cfg_showdebug);
   //rtc_write_config_int(RTC_FREQUENCY,radioFrequency);
   rtc_write_config(RTC_MAGICNUMBER,218);
#ifdef DEBUG_MAIN
   debug_printf("fresh config\n");   
#endif
  }

  setup_interrupts();


  // initialize hardware interface
  setalarmstate(cfg_alarmstate);

  printinfovfd();

  

  // initialize radio board
  
  setmp3state(0);
  setentrymode(0);

  while(1)
  {
  
   if (key_pending > 0) {
    if (spi_lock(2)) {
     key_pending--;
     IOCLR = KBD_SS;
     some_delay(KBD_SS_DELAY);
     S0SPDR = 0xAA;  
    }
   }
  

    mp3execute();

    if (mp3message) {
#ifdef DEBUG_MAIN
   //  debug_printf("%s\n",mp3buffer);
#endif
     domp3message();
     mp3message = 0;
    }

    if (timeout400msec) {
      timeout400msec = 0;

/*
      // wenn mp3 playing: aktuelle zeit bei mp3-player abfragen
      if (mp3playing) {
       mp3commandpush(MP3_CMD_QUERY_STATUS); 
      }
      if (mp3cardmissing) {
       mp3commandpush(MP3_CMD_DEVICE_INFO); 
      }
*/
      // wenn editor aktiv: cursor blinken
      if ((clockterminalstate == TSTATE_EDITTIME) || (clockterminalstate == TSTATE_EDITALARM)  || (clockterminalstate == TSTATE_EDITFREQUENCY)) {
        printtimecursor();
      }
    }

    dokeyboard();

    if (timeout10msec) {
     timeout10msec = 0;

     if (!clockterminalstate) {
      //printupdatevfd();
      printinfovfd();
     }
/*
     if (mp3status) {
      if (--mp3timeout == 0) {
      
#ifdef DEBUG_MAIN
       debug_printf("T!\n");
#endif
       mp3completed = 0;
       mp3bufferptr = 0;
       if (--mp3retries) mp3sendcommand();
       else mp3status = MP3_STATUS_READY;
      }
     }
*/

    }

    if (updateclocknow) {
     tick();
  
     updateclocknow  = 0; 

     if (clockterminalstate != TSTATE_EDITTIME) {
      vfd_putc(VFD_CMD_FONT_10X14);
      vfd_setcursor(72,14 + YOFFSET);
      vfd_printf(TimeFormat,HOUR,MIN);
     }

     vfd_putc(VFD_CMD_FONT_MINI);
     vfd_setcursor(0,14 + YOFFSET);
     vfd_printf("%c%c %02u.%02u.%02u",
       *(Days + ((DOW-1)*2)),  
       *(Days + ((DOW-1)*2) + 1),
       DOM,
       MONTH,
       YEAR);

     if (cfg_alarmstate) {
       if ((cfg_alarmhour == HOUR) && (cfg_alarmminute == MIN) && (2 == SEC)) {
         if (!cfg_mp3state) setmp3state(1);
       }
     }

       //rtc_read_time();
    }

    if (dcfsavetimenow) {
     dcfsavetimenow = 0;
     dcfsavetime();
     rtc_write_time();

    }

    if (dcfdecodenow) {       
     dcfdecodenow = 0; // pufferung wieder freigeben
     dcfdecode();
    }  


  }


  
}






