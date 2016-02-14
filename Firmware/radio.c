#include <targets/LPC210x.h>
#include "common.h"
#include "radio.h"
#include "vfd.h"
#include "rtc.h"


/*********************************************************************/
/* CONFIGURATION                                                     */
/*********************************************************************/

#define MP3_COLUMN_LENGTH 21
#define MP3_BUFFERSIZE 60
#define MP3_QUEUE_LENGTH 30

#define MP3_STATUS_READY 0

#define MP3_DEFAULT_TIMEOUT 20

#define MP3_VOLUME_MINIMUM 0
#define MP3_VOLUME_MAXIMUM 20
#define MP3_VOLUME_DEFAULT 5
#define MP3_BUFFER_SIZE 100

#define RADIO_FRQ_MINIMUM 87500
#define RADIO_FRQ_DEFAULT 88050
#define RADIO_FRQ_MAXIMUM 107500

#define COMMAND_COUNT 26

#define COMMAND_FRQ 1
#define COMMAND_MOD 2
#define COMMAND_TUM 3
#define COMMAND_TUP 4
#define COMMAND_TDN 5
#define COMMAND_CHA 6
#define COMMAND_STA 7
#define COMMAND_MUT 8
#define COMMAND_SCA 9
#define COMMAND_LST 10
#define COMMAND_RST 11
#define COMMAND_BOO 12
#define COMMAND_RTO 13
#define COMMAND_RIN 14
#define COMMAND_RDU 15
#define COMMAND_QUA 16
#define COMMAND_PID 17
#define COMMAND_PTY 18
#define COMMAND_SID 19
#define COMMAND_DIS 20
#define COMMAND_ECH 21
#define COMMAND_CNT 22
#define COMMAND_STO 23
#define COMMAND_VOL 24
#define COMMAND_VUP 25
#define COMMAND_VDN 26

const char cmdtext[COMMAND_COUNT][4] = { 
"FRQ",
"MOD", 
"TUM", 
"TUP", 
"TDN", 
"CHA", 
"STA", 
"MUT", 
"SCA", 
"LST", 
"RST", 
"BOO", 
"RTO", 
"RIN", 
"RDU", 
"QUA", 
"PID", 
"PTY", 
"SID", 
"DIS",
"ECH",
"CNT",
"STO",
"VOL"
}; 

/*********************************************************************/
/* DATA                                                              */
/*********************************************************************/

static unsigned short int mp3message = 0;
static char mp3buffer[MP3_BUFFERSIZE];
static unsigned short int mp3bufferptr = 0;

unsigned short int mp3status = 0;
unsigned short int mp3completed = 0;

unsigned short int mp3lasttitle = 0;
unsigned short int mp3playing = 0;
unsigned short int mp3timeout = 0;
unsigned short int mp3retries = 0;
unsigned short int mp3fetch = 0;
unsigned short int mp3currenttime = 0;
unsigned short int mp3cardmissing = 0;

unsigned int radioFrequency = RADIO_FRQ_DEFAULT;
unsigned int radioVolume = 100;
char radioStationID[9] = "";
char radioPID[5] = "";
unsigned int radioQuality = 0;

char mp3command_queue[MP3_QUEUE_LENGTH];
unsigned short int  mp3command_queue_start = 0;
unsigned short int  mp3command_queue_stop = 0;
char mp3command_last;


/*********************************************************************/
/* AMPLIFIER COMMANDS                                                */
/*********************************************************************/
void amp_standby()
{
  IOSET = BIT_21;
  IOCLR = BIT_25;
}

void amp_off()
{
  IOCLR = BIT_21;
  IOCLR = BIT_25;
}

void amp_power()
{
  IOSET = BIT_21;
  IOSET = BIT_25;
}


/*********************************************************************/
/* INTERRUPT UART1 (IRQ)                                             */
/*********************************************************************/
static void serialISR(void) __attribute__ ((interrupt ("IRQ")));

static void serialISR(void)
{
  char c;
  int u;
  u = U1IIR;
  c = U1RBR;
  if (c == 0xD) {
   mp3message = 1;
   mp3buffer[mp3bufferptr] = 0;
  }
  else mp3buffer[mp3bufferptr] = c;
  if (mp3bufferptr < MP3_BUFFERSIZE) mp3bufferptr++;
//  else mp3bufferptr = 0;

  /* Update VIC priorities */
  VICVectAddr = 0;
}


/*********************************************************************/
/* UART Communication Commands                                       */
/*********************************************************************/
static void
UARTWriteChar(unsigned char ch)
{
  while ((U1LSR & BIT_05) == 0);
  U1THR = ch;
}

static unsigned int
UARTCharAvailable(void)
{
  return ((U1LSR & 0x01) == 1);
}

static unsigned char
UARTReadChar(void)
{
  while ((U1LSR & 0x01) == 0);
  return U1RBR;
}

void 
__putchar(int ch)
{
  UARTWriteChar(ch);
}



//**********************************************************
// QUEUE HANDLING
//**********************************************************
unsigned short int get_queue_length()
{
  if (mp3command_queue_start == mp3command_queue_stop) return 0;
  if (mp3command_queue_start < mp3command_queue_stop) return  (mp3command_queue_stop - mp3command_queue_start);
  else return  (MP3_QUEUE_LENGTH-mp3command_queue_start) + mp3command_queue_stop;
}

void mp3clearqueue()
{
 mp3command_queue_start = 0;
 mp3command_queue_stop = 0;
 mp3retries = 1;
 mp3timeout = 1;
}


void mp3commandpush(char c)
{
#ifdef DEBUG_MAIN
 //debug_printf("push %c qlen: %u qstart: %u qstop: %u \r\n",c,get_queue_length(),mp3command_queue_start,mp3command_queue_stop);
#endif
 
 if (c != mp3command_last) {
 mp3command_queue[mp3command_queue_stop] = c;
 if (mp3command_queue_stop == (MP3_QUEUE_LENGTH-1)) {
   if (mp3command_queue_start>0) {
    mp3command_queue_stop = 0;
   }
 } else {
  if (mp3command_queue_stop == (mp3command_queue_start - 1)) {
  } else {
   mp3command_queue_stop++;
  }
 } 
 mp3command_last = c;
 }
 
}

char mp3commandpop()
{
 char c = 0;
 if (mp3command_queue_start != mp3command_queue_stop) {
  c = mp3command_queue[mp3command_queue_start];
  if (mp3command_queue_start == (MP3_QUEUE_LENGTH-1)) mp3command_queue_start = 0;
  else {
    mp3command_queue_start++;
  }
 }
 if (mp3command_queue_start == mp3command_queue_stop) mp3command_last = 0;
 return c;
}

/**********************************************************
/ MP3 Control
/**********************************************************/
void mp3sendcommand()
{
  char commandstring[30];

  if      (mp3status ==  COMMAND_FRQ) sprintf(commandstring,"FRQ%06lu\r",radioFrequency);
  //if      (mp3status ==  COMMAND_FRQ) sprintf(commandstring,"FRQ088050\r");
  else if (mp3status ==  COMMAND_TUP) sprintf(commandstring,"TUP\r");
  else if (mp3status ==  COMMAND_TDN) sprintf(commandstring,"TDN\r");
  else if (mp3status ==  COMMAND_VOL) sprintf(commandstring,"VOL%u\r",cfg_mp3volume);
  else if (mp3status ==  COMMAND_VUP) sprintf(commandstring,"VUP\r");
  else if (mp3status ==  COMMAND_VDN) sprintf(commandstring,"VDN\r");
  else if (mp3status ==  COMMAND_ECH) sprintf(commandstring,"ECH0\r");
  else if (mp3status ==  COMMAND_STA) sprintf(commandstring,"STA\r");
  else if (mp3status ==  COMMAND_MOD) sprintf(commandstring,"MOD0\r");
  else if (mp3status ==  COMMAND_TUM) sprintf(commandstring,"TUM0\r");
  else if (mp3status ==  COMMAND_RDU) sprintf(commandstring,"RDU1\r");
  else if (mp3status ==  COMMAND_RIN) sprintf(commandstring,"RIN1\r");
  else if (mp3status ==  COMMAND_RTO) sprintf(commandstring,"RTO2\r");

#ifdef DEBUG_MAIN
   debug_printf("        %s\n",commandstring);
#endif
  printf(commandstring);
  mp3timeout = MP3_DEFAULT_TIMEOUT;
}

void mp3execute()
{
 if (get_queue_length()  && (!mp3message) && (!mp3status)) {
  mp3status = mp3commandpop();
  mp3retries = 3;
  mp3sendcommand();
 }
}

void radioclear()
{
    vfd_docmd4(VFD_CMD_AREA_CLEAR,0,16 + YOFFSET,126,64 + YOFFSET);
}

void radiodisplayfrequency()
{
     int tempfreq;
     vfd_putc(VFD_CMD_FONT_10X14);
     vfd_setcursor(6,38 + YOFFSET);
     tempfreq = radioFrequency / 10;
     vfd_printf(FrequencyFormat,tempfreq / 100, tempfreq % 100);
}

void radiodisplayvolume()
{
     //vfd_putc(VFD_CMD_FONT_MINI);
     //vfd_setcursor(0,54 + YOFFSET);
     //vfd_printf("%02u",radioVolume);
     vfd_make_progressbar(0,51 + YOFFSET,124,55 + YOFFSET,20,0);
     vfd_update_progressbar(radioVolume);
}

void radiodisplaystationdata()
{
 if (clockterminalstate != TSTATE_EDITFREQUENCY) {
    vfd_putc(VFD_CMD_FONT_5X7);
    vfd_setcursor(38,48 + YOFFSET);
    vfd_printf("%-8s",radioStationID);
    if (cfg_showdebug) {
         vfd_putc(VFD_CMD_FONT_MINI);
         vfd_setcursor(82,22 + YOFFSET);
         vfd_putstring(radioPID);
    }
 }
}

void radiodisplaydata()
{
        radiodisplayfrequency();
        radiodisplayvolume();
        radiodisplaystationdata();

         //vfd_make_progressbar(20,59,106,63,255,0);
         
         //vfd_docmd4(VFD_CMD_OUTLINE_SET,20,59,106,63);
         //vfd_docmd4(VFD_CMD_AREA_CLEAR,21,60,105,62);
}

int checksidcharacters(char * data) 
{
  int i = 0;
  while ( (i < 9) && (*(data + i) != 0) ) {
    if ( *(data + i) < 32) return 0;
    if ( *(data + i) > 90) return 0;
    i++;
  }
  if (i == 8) if ( *(data + i) != 0) return 0;
  if (i == 0) return 0;
  return 1;
}

void radiostart()
{
  mp3commandpush(COMMAND_ECH); // ECHO OFF
  mp3commandpush(COMMAND_MOD); // MODE FM RADIO
  mp3commandpush(COMMAND_TUM); // TUNING MODE DIRECT
  mp3commandpush(COMMAND_VOL); // SET VOLUME
  mp3commandpush(COMMAND_RIN); // SET 
  mp3commandpush(COMMAND_RTO); // SET RDS Tolerance
  mp3commandpush(COMMAND_FRQ); // SET FREQUENCY

  mp3commandpush(COMMAND_STA); // RETRIEVE COMPLETE STATUS
  radioStationID[0] = 0;
  radiodisplaydata();
}

//**********************************************************
// COMMAND POST-HANDLING
//**********************************************************
void domp3message()
{

   int i = 0;
   int commandid = 0;
   unsigned long int tempfreq = 0;

   if (cfg_mp3state)
   {

#ifdef DEBUG_MAIN
   debug_printf("%s\n",mp3buffer);
#endif
      if (mp3buffer[0] == 'e') { // fehlermeldung!!
       if (cfg_showdebug) {
        vfd_putc(VFD_CMD_FONT_MINI);
        vfd_setcursor(0,30);
        vfd_putstring(mp3buffer);
       }

       mp3completed = 0;

      } else if (mp3buffer[0] == 'r') { // RAW RDS DATA

      } else {

        for (i = 0; i < COMMAND_COUNT; i++)
        {
          if (mp3buffer[0] == cmdtext[i][0]) {
            if (mp3buffer[1] == cmdtext[i][1]) {
              if (mp3buffer[2] == cmdtext[i][2]) {
                commandid = i+1;
                continue;
              };
            };          
          };
        }

       /******************************************/
       /*                                        */
       /******************************************/
       if (commandid == COMMAND_FRQ) {
          tempfreq = atol(mp3buffer + 4);
          if ((tempfreq >= RADIO_FRQ_MINIMUM) && (tempfreq <= RADIO_FRQ_MAXIMUM)) {
            radioFrequency = tempfreq;
            radiodisplayfrequency();
            amp_power();
          }
       }

       /******************************************/
       /*                                        */
       /******************************************/
       if (commandid == COMMAND_PID) {
            strncpy(radioPID,mp3buffer + 4,4);
            radioPID[4] = 0;
            radiodisplaystationdata();
       }

       /******************************************/
       /*                                        */
       /******************************************/
       if (commandid == COMMAND_SID) {
          if (checksidcharacters(mp3buffer + 4)) {
            strncpy(radioStationID,mp3buffer + 4,8);
            radiodisplaystationdata();
          }
       }

       /******************************************/
       /*                                        */
       /******************************************/
       if (commandid == COMMAND_VOL) {
        tempfreq = atol( mp3buffer + 3 );
        if (tempfreq >= 0) {
          radioVolume = tempfreq;
          radiodisplayvolume();
          //vfd_update_progressbar(radioVolume);
          
        }
       }

    }
     
      mp3bufferptr = 0;
      mp3completed = mp3status;  // status für folgeaktionen aufbewahren
      mp3status = MP3_STATUS_READY; // neues kommando ist zulässig
    }
 }

