
#include "main.h"

#include "dcf.h"
#include "mp3.h"
#include "vfd.h"
#include "keyboard.h"


void rtc_write_config(char, char) {
}
void rtc_write_time() {
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

volatile char spi0_lock = 0;
volatile char key_pending = 0;
volatile char option1 = 0;
volatile char option2 = 0;

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

volatile int updateclocknow = 1;

void rtcISR(void) __attribute__ ((interrupt ("IRQ")));

void rtcISR(void)
{
  updateclocknow = 1;
  ILR = 0xFF;
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
/* INTERRUPT TIMER0 (FIQ)                                            */
/*********************************************************************/
static void timer0ISR(void) __attribute__ ((interrupt ("IRQ")));

static void
timer0ISR(void)
{

  if (++timer0Count % 2) IO1CLR = BIT_19;
  else IO1SET = BIT_19;

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

unsigned char last_key = 0;



static void keyboardISR(void) __attribute__ ((interrupt ("IRQ"))) __attribute__((optimize("O0")));

static void keyboardISR(void)   
{

  if (spi_lock(2)) {
   IOCLR = KBD_SS;
   some_delay(KBD_SS_DELAY);
   S0SPDR = 0x00;  
  } else key_pending++;

  EXTINT = BIT_02;
  VICVectAddr = 0;
}


static void keyISR(void) __attribute__ ((interrupt ("IRQ"))) __attribute__((optimize("O0"))) ;

static void keyISR(void)  
{
  if (spi0_lock == 2) {
   if ((S0SPSR & BIT_07) == BIT_07) {
    last_key = S0SPDR;
//    if (last_key == 0) last_key = KEY_1_MAKE;
   }
   IOSET = KBD_SS;
   spi_unlock(2);
  }
  
   S0SPINT = BIT_00;
   VICVectAddr = 0;
}

#pragma OPTIMIZE ON

int kbd_writebyte(char data)
{
  if (spi_lock(2)) {
   S0SPCR   = (BIT_05 | BIT_03 );
   IOCLR = KBD_SS;
   some_delay(KBD_SS_DELAY);
   S0SPDR = data;    
   while ((S0SPSR & BIT_07) != BIT_07);
   data = S0SPDR;
   IOSET = KBD_SS;
   S0SPCR   = (BIT_05 | BIT_03 | BIT_07);
   spi_unlock(2);
   return S0SPDR;
  }
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

  if (cfg_mp3state) IOSET = BIT_28;
  else IOCLR = BIT_28; 

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
/* PORT DEFINITIONS                                                  */
/*********************************************************************/

#define PO_BIT_SCL (0x1 << 2)
#define PO_BIT_SDA (0x1 << 3)

#define PO_BIT_UMP3_RX (0x1 << 9)
#define PO_BIT_UMP3_TX (0x1 << 8)

#define PO_BIT_VFD_MB (0x1 << 28)
#define PO_BIT_VFD_SS (0x1 << 29)
#define PO_BIT_VFD_RES (0x1 << 22)
#define PO_BIT_VFD_SOUT (0x1 << 18)
#define PO_BIT_VFD_SIN (0x1 << 19)
#define PO_BIT_VFD_SCK (0x1 << 17)

#define PO_BIT_KBD_SCK (0x1 << 4)
#define PO_BIT_KBD_SIN (0x1 << 6)
#define PO_BIT_KBD_SOUT (0x1 << 5)
#define PO_BIT_KBD_IRQ (0x1 << 30)
#define PO_BIT_KBD_SEL (0x1 << 11)
#define PO_BIT_KBD_CLR (0x1 << 10)

#define PO_BIT_DCF_SIG_1 (0x1 << 14)
#define PO_BIT_DCF_SIG_2 (0x1 << 16)

#define P0_BIT_LED_RDY (0x1 << 31)
#define P0_BIT_LED_1 (0x1 << 12)
#define P0_BIT_LED_2 (0x1 << 13)

#define P0_BIT_AMP_ON (0x1 << 21)
#define P0_BIT_AMP_STANDBY (0x1 << 25)

#define P1_BIT_CONFIG_1 (0x1 << 16)
#define P1_BIT_CONFIG_2 (0x1 << 17)

#define P1_BIT_MONITOR_1 (0x1 << 18)
#define P1_BIT_MONITOR_2 (0x1 << 19)

/*********************************************************************/
/* INIT ROUTINE                                                      */
/*********************************************************************/

void setup_ports(void)
{

  //PINSEL0 = ( SEL_P0_3_EINT1 | SEL_P0_8_TXD1 | SEL_P0_9_RXD1 | SEL_P0_SPI0 | SEL_P0_15_EINT2); 
  //PINSEL1 = ( SEL_P016_EINT0 | SEL_P0_SPI1 );
  PINSEL0 = (
              (0x1 << PINSEL0_P0_2_BIT)  | // P0.2 = SCL0
              (0x1 << PINSEL0_P0_3_BIT)  | // P0.3 = SDA0
              (0x1 << PINSEL0_P0_4_BIT)  | // P0.4 = SCK0
              (0x1 << PINSEL0_P0_5_BIT)  | // P0.5 = MISO0
              (0x1 << PINSEL0_P0_6_BIT)  | // P0.6 = MOSI0
              (0x1 << PINSEL0_P0_7_BIT)  | // P0.7 = SSEL0
              (0x1 << PINSEL0_P0_8_BIT)  | // P0.8 = TxD (UART1)
              (0x1 << PINSEL0_P0_9_BIT)  | // P0.9 = RxD (UART1)
              (0x2 << PINSEL0_P0_14_BIT)   // P0.14 = EINT1
            );

  PINSEL1 = (
              (0x1 << PINSEL1_P0_16_BIT)  | // P0.16 = EINT0
              (0x2 << PINSEL1_P0_17_BIT)  | // P0.17 = SCK0
              (0x2 << PINSEL1_P0_18_BIT)  | // P0.18 = MISO0
              (0x2 << PINSEL1_P0_19_BIT)  | // P0.19 = MOSI0
              (0x2 << PINSEL1_P0_20_BIT)  | // P0.20 = SSEL0
              (0x2 << PINSEL1_P0_30_BIT)    // P0.30 = EINT3
            );  

  // IODIR   = ( BIT_02 | BIT_04 | BIT_05 | BIT_06 | BIT_11 | BIT_13 | BIT_22 | BIT_27 | BIT_28 | BIT_29 | BIT_30 );
  // IODIR   = ( BIT_02 | BIT_04 | BIT_11 | BIT_13 | BIT_21 | BIT_22 | BIT_24 | BIT_25 | BIT_27 | BIT_28 | BIT_29 | BIT_30 );

  IODIR = ( PO_BIT_UMP3_TX |
            PO_BIT_VFD_MB |
            PO_BIT_VFD_SS |
            PO_BIT_VFD_RES |
            PO_BIT_VFD_SOUT |
            PO_BIT_VFD_SCK |
            PO_BIT_KBD_SEL |
            PO_BIT_KBD_CLR |
            PO_BIT_KBD_SOUT |
            PO_BIT_KBD_SCK |
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
  
  IOSET = KBD_SS                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          ;
  // IOCLR = RTC_SEL;

  //IO1DIR = (IO1DIR | BIT_18 | BIT_19 );
  //IO1DIR &= ~BIT_16;
  //IO1DIR &= ~BIT_17;

  S0SPCCR  = 0x0000000A;
  S0SPCR   = (BIT_05 | BIT_03 | BIT_07); // Master mode, sync on end, interrupt enabled
  
  //S1SPCCR  = 0x00000004; // Clock Divider, 4 min, even only!!
  // S1SPCCR  = 0x000000FF; // Clock Divider, 4 min, even only!!
  SSPCPSR  = 0x000000FF; // Clock Divider, 4 min, even only!!
  SSPCR1   = (BIT_02); // Master mode
  SSPCR0   = 0x7; // 8 Bit 


  /************************************************/
  /*  0  EINT1 (DCF PULSE)                        */
  /*  1  EINT0 (DCF PULSE)                        */
  /*  4  EINT2 (KEYBOARD)                         */
  /************************************************/

  //EXTMODE  = (BIT_00 | BIT_01 | BIT_02);
  EXTMODE  = (BIT_00 | BIT_01 | BIT_02);
  EXTPOLAR = (BIT_01 | BIT_02);
  EXTWAKE  = (BIT_02);
  EXTINT   = (BIT_00 | BIT_01 | BIT_02);

  /************************************************/
  /*  2  TIMER0                                   */
  /************************************************/

  T0TCR = 0; /* Reset timer 0 */
  T0PR = 252; // 10MHz: 20MHz: 504 / 14. MHz: 370 /* Set the timer 0 prescale counter */
  T0MR0 = 989; // 20/14. MHz: 989 /* Set timer 0 match register */
  T0MCR = 3; /* Generate interrupt and reset counter on match */
  T0TCR = 1; /* Start timer 0 */

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

  CCR = 0x2;
  PREINT = 761;
  PREFRAC = 30784;
  CIIR = 0x1;
  CCR = 0x1;

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

  U1LCR = 0x00000083; // line control 
  U1DLM = 0x0;  // divisor msb
  U1DLL = 0x28; // divisor lsb
  U1LCR = 0x00000003; // line control
  U1FCR = 0x41; // FIFO control
  U1IER = 0x1; // interupt enable (RBR=0x1, THRE=0x2)

}

void setup_interrupts(void)
{
  /************************************************/
  /* DEFAULT                                      */
  /************************************************/
  VICDefVectAddr = (unsigned int)defaultISR;

  /************************************************/
  /*  0  EINT1 (DCF PULSE)                        */
  /************************************************/
  VICIntSelect &= ~0x00008000;
  VICIntEnable = 0x00008000; /* Enable EINT1 interrupt */
  VICVectCntl0 =  0x0000002F; /* Use slot 3 for EINT1 interrupt */
  VICVectAddr0 = (unsigned int)dcfupISR; /* Set the address of ISR for slot 1 */

  /************************************************/
  /*  1  EINT0 (DCF PULSE)                        */
  /************************************************/
  VICIntSelect &= ~0x00004000; // IRQ interrupt 
  VICIntEnable = 0x00004000; /* Enable EINT0 interrupt */
  VICVectCntl1 = 0x0000002E; /* Use slot 2 for EINT0 interrupt */
  VICVectAddr1 = (unsigned int)dcfdownISR; /* Set the address of ISR for slot 1 */

  /************************************************/
  /*  2  TIMER0                                   */
  /************************************************/
  VICIntSelect &= ~0x10; /* Timer 0 interrupt is IRQ interrupt */
  VICIntEnable = 0x10; /* Enable timer 0 interrupt */
  VICVectCntl2 = 0x24; /* Use slot 0 for timer 0 interrupt */
  VICVectAddr2 = (unsigned int)timer0ISR; /* Set the address of ISR for slot 0 */

  /************************************************/
  /*  3  SPI0 (KEYBOARD/RTC)                      */
  /************************************************/
  VICIntSelect &= ~BIT_10;
  VICIntEnable = BIT_10; /* Enable SPI0 interrupt */
  VICVectCntl3 =  0x20 | 0xA; /* Use slot 5 for SPI0 interrupt */
  VICVectAddr3 = (unsigned int)keyISR; /* Set the address of ISR for slot  */

  /************************************************/
  /* 4  EINT2 (KEYBOARD)                          */
  /************************************************/
  VICIntSelect &= ~0x00010000;
  VICIntEnable = 0x00010000; /* Enable EINT2 interrupt */
  VICVectCntl4 = 0x20 | 0x10; /* Use slot 2 for EINT2 interrupt */
  VICVectAddr4 = (unsigned int)keyboardISR; /* Set the address of ISR for slot  */

  /************************************************/
  /*  5  UART1 (MP3 BOARD)                        */
  /************************************************/
  VICIntSelect &= ~(1 << 7); /* interrupt is an IRQ interrupt */
  VICIntEnable = 0x00000080; /* Enable UART1 interrupt */
  VICVectCntl5 = 0x20 | 0x00000007; /* Use slot 3 for UART1 interrupt */
  VICVectAddr5 = (unsigned int)serialISR; /* Set the address of ISR for slot 3 */

  /************************************************/
  /*  6  RTC (SECOND CHANGE)                      */
  /************************************************/
  VICIntSelect &= ~0x00002000; /* RTC interrupt is an IRQ interrupt */
  VICIntEnable = 0x00002000; /* Enable UART1 interrupt */
  VICVectCntl6 = 0x20 | 0x0000000D; /* Use slot 3 for UART1 interrupt */
  VICVectAddr6 = (unsigned int)rtcISR; /* Set the address of ISR for slot 3 */

 // __ARMLIB_enableFIQ();
  libarm_enable_irq();

}


void readconfig()
{
#ifdef DEBUG_MAIN
  debug_printf("-%X-",IO1PIN);
#endif
  if ((IO1PIN & BIT_16) ==  BIT_16) option1 = 1;
  if ((IO1PIN & BIT_17) ==  BIT_17) option2 = 1;
}




void setupPorts(void) {

  IOCLR = 0xFFFFFFFF;

  IODIR = (P0_BIT_LED_RDY | P0_BIT_LED_1 | P0_BIT_LED_2);

  PINSEL0 = 0x0;
  

}

int secondticks = 0;

void tick( void) {
  static int counter = 0;
  static int status = 0; 
  
  

  if (counter > secondticks) {
    counter = 0;

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

    
  }

  counter++;

}

int main( void) {

  setupPorts();

  secondticks = ctl_get_ticks_per_second() >> 1;

  libarm_set_irq(1);

  ctl_start_timer(tick);

  
}






