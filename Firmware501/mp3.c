
#include "main.h"
#include "mp3.h"
#include "vfd.h"
#include "rtc.h"



/*********************************************************************/
/* DATA                                                              */
/*********************************************************************/

unsigned short int mp3message = 0;
char mp3buffer[MP3_BUFFERSIZE];
unsigned short int mp3bufferptr = 0;

unsigned short int mp3status = 0;
unsigned short int mp3completed = 0;

unsigned short int mp3lasttitle = 0;
unsigned short int mp3playing = 0;
unsigned short int mp3timeout = 0;
unsigned short int mp3retries = 0;
unsigned short int mp3fetch = 0;
unsigned short int mp3currenttime = 0;
unsigned short int mp3cardmissing = 0;


struct mp3titledata mp3list[MP3_BUFFER_SIZE];

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


void serialISR(void)
{
  char c;
  int u;
  u = U1IIR;
  c = U1RBR;
  if (c == '>') {
   mp3message = 1;
   mp3buffer[mp3bufferptr] = 0;
  }
  else mp3buffer[mp3bufferptr] = c;
  if (mp3bufferptr < MP3_BUFFERSIZE) mp3bufferptr++;

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

  if      (mp3status ==  MP3_CMD_QUERY_STATUS) sprintf(commandstring,"PC Z\r");
  else if (mp3status ==  MP3_CMD_PLAY) sprintf(commandstring,"PC F /A/A%06u.mp3\r",cfg_mp3title);
  else if (mp3status ==  MP3_CMD_QUERY_ALL) sprintf(commandstring,"FC R 1 %u %u\r",(2*MP3_COLUMN_LENGTH + 15),((2*MP3_COLUMN_LENGTH + 15)*mp3fetch));
  else if (mp3status ==  MP3_CMD_STOP) sprintf(commandstring,"PC S\r");
  else if (mp3status ==  MP3_CMD_SET_VOLUME) sprintf(commandstring,"ST V %u\r",cfg_mp3volume);
  else if (mp3status ==  MP3_CMD_SET_BASS_ENHANCE) sprintf(commandstring,"ST B %u\r",7);
  else if (mp3status ==  MP3_CMD_OPEN_TITLEFILE) sprintf(commandstring,"FC O 1 R /V/t.txt\r");
  else if (mp3status ==  MP3_CMD_OPEN_COUNTERFILE) sprintf(commandstring,"FC O 2 R /V/c.txt\r");
  else if (mp3status ==  MP3_CMD_QUERY_COUNTERS) sprintf(commandstring,"FC R 2 6 0\r");
  else if (mp3status ==  MP3_CMD_CLOSE_TITLEFILE) sprintf(commandstring,"FC C 1\r");
  else if (mp3status ==  MP3_CMD_CLOSE_COUNTERFILE) sprintf(commandstring,"FC C 2\r");
  else if (mp3status ==  MP3_CMD_SET_SERIAL_SPEED) sprintf(commandstring,"ST D 4\r");
  else if (mp3status ==  MP3_CMD_DEVICE_INFO) sprintf(commandstring,"FC Q\r");
#ifdef DEBUG_MAIN
   debug_printf("%s\n",commandstring);
#endif
  printf(commandstring);
  mp3timeout = MP3_DEFAULT_TIMEOUT;
}


void mp3execute()
{
 if (get_queue_length() && (!mp3status)  && (!mp3message)) {
  mp3status = mp3commandpop();
  mp3retries = 3;
  mp3sendcommand();
 }
}

void mp3setnextfile()
{
  if (cfg_mp3title == mp3lasttitle) cfg_mp3title = 0;
  else cfg_mp3title++;
  rtc_write_config(RTC_MP3TITLE1,cfg_mp3title);
  rtc_write_config(RTC_MP3TITLE2,cfg_mp3title >> 8);
}

void mp3setprevfile()
{
  if (cfg_mp3title == 0) cfg_mp3title = mp3lasttitle;
  else cfg_mp3title--;
  rtc_write_config(RTC_MP3TITLE1,cfg_mp3title);
  rtc_write_config(RTC_MP3TITLE2,cfg_mp3title >> 8);
}


void mp3clear()
{
  vfd_docmd4(VFD_CMD_AREA_CLEAR,0,41,126,64);
}


void mp3displaytime()
{
  vfd_putc(VFD_CMD_FONT_MINI);
  vfd_setcursor(109,64);
  vfd_printf(TimeFormat,(unsigned short int)(mp3currenttime/60),(unsigned short int)(mp3currenttime%60));
}


void mp3displaydata(struct mp3titledata *data)
{
         vfd_putc(VFD_CMD_FONT_5X7);

         vfd_setcursor(0,50);
	 vfd_putstring(data->title);

         vfd_setcursor(0,58);
	 vfd_putstring(data->artist);

         vfd_putc(VFD_CMD_FONT_MINI);
         vfd_setcursor(0,64);
         vfd_printf(TimeFormat,(unsigned short int)(data->length/60),(unsigned short int)(data->length%60));

         vfd_make_progressbar(20,59,106,63,data->length,0);
         //vfd_docmd4(VFD_CMD_OUTLINE_SET,20,59,106,63);
         //vfd_docmd4(VFD_CMD_AREA_CLEAR,21,60,105,62);
}

void mp3displaycurrentdata() {
  //mp3displaydata(mp3list[cfg_mp3title]);
}


void mp3displayvolume()
{
     //vfd_putc(VFD_CMD_FONT_MINI);
     //vfd_setcursor(0,54 + YOFFSET);
     //vfd_printf("%02u",radioVolume);
     vfd_make_progressbar(0,51 + YOFFSET,124,55 + YOFFSET,20,0);
     vfd_update_progressbar(cfg_mp3volume);
}


void mp3start()
{
         mp3commandpush(MP3_CMD_QUERY_ALL);
         mp3commandpush(MP3_CMD_PLAY);
}

//**********************************************************
// COMMAND POST-HANDLING
//**********************************************************
void domp3message()
{

#ifdef DEBUG_MAIN
   debug_printf("%s\n",mp3buffer);
#endif
      if (mp3buffer[0] == 'E') { // fehlermeldung!!
       if (cfg_showdebug) {
        vfd_putc(VFD_CMD_FONT_MINI);
        vfd_setcursor(0,30);
        vfd_putstring(mp3buffer);
       }
       /******************************************/
       /*                                        */
       /******************************************/
       if ( 
               ((mp3buffer[1] == '0') && (mp3buffer[2] == '8')) // Fehler E08: Karte fehlt!
            || ((mp3buffer[1] == 'E') && (mp3buffer[2] == 'B')) // Fehler EEB: Handle ungültig, vermutlich Karte gewechselt
           ) { 

        if (!mp3cardmissing) {
         mp3cardmissing = 1;

         mp3clearqueue();
         mp3playing = 0;
         if (cfg_mp3state) {
          cfg_mp3title--;
          cfg_mp3state = 0;
         }
         amp_off();

         mp3clear();
         vfd_docmd4(VFD_CMD_OUTLINE_SET,0,41,126,41);
         vfd_putc(VFD_CMD_FONT_5X7);
         vfd_setcursor(0,58);
         vfd_putstring("PLEASE INSERT SD-CARD");
        }
       }

       mp3completed = 0;

      } else {


       /******************************************/
       /*  Wird nur nach Kartenfehler gesendet,  */
       /*  also Dateien öffnen und neu lesen     */
       /*                                        */
       /*                                        */
       /******************************************/
       if (mp3status == MP3_CMD_DEVICE_INFO) {
         if (mp3bufferptr > 1) {
          mp3cardmissing = 0;
          mp3clear();

          mp3commandpush(MP3_CMD_OPEN_TITLEFILE);
          mp3commandpush(MP3_CMD_OPEN_COUNTERFILE);
          mp3commandpush(MP3_CMD_QUERY_COUNTERS);
        }
       }

       /******************************************/
       /*                                        */
       /******************************************/
       if (mp3status == MP3_CMD_PLAY) {
         mp3playing = 1;
         vfd_docmd4(VFD_CMD_OUTLINE_SET,0,41,126,41);
         mp3displaydata(&mp3list[cfg_mp3title]);
         amp_power();
       }

       /******************************************/
       /*                                        */
       /******************************************/
       if (mp3status == MP3_CMD_STOP) {
         mp3playing = 0;
       }

       /******************************************/
       /*                                        */
       /******************************************/
       if (mp3status == MP3_CMD_QUERY_COUNTERS) {
        mp3buffer[7] = 0;
        mp3lasttitle = atol( mp3buffer + 1 );
        if (cfg_mp3title > mp3lasttitle) cfg_mp3title = mp3lasttitle;
        mp3fetch = 0;
        vfd_make_progressbar(10,30,110,40,mp3lasttitle-1,0);
        mp3commandpush(MP3_CMD_QUERY_ALL);
  
       }

       /******************************************/
       /*                                        */
       /******************************************/
       if (mp3status == MP3_CMD_QUERY_ALL) {
        strncpy(mp3list[mp3fetch].artist,mp3buffer + 1,MP3_COLUMN_LENGTH);
        strncpy(mp3list[mp3fetch].title,mp3buffer + 1 + MP3_COLUMN_LENGTH,MP3_COLUMN_LENGTH);
        mp3buffer[mp3bufferptr] = 0;
        mp3list[mp3fetch].length = atol(mp3buffer + 1 + (2*MP3_COLUMN_LENGTH + 10) );
        if (++mp3fetch < mp3lasttitle) {
         vfd_update_progressbar(mp3fetch);
         mp3commandpush(MP3_CMD_QUERY_ALL);
        } else vfd_clear_progressbar();
       }
  
       /******************************************/
       /*                                        */
       /******************************************/
       if (mp3status == MP3_CMD_QUERY_STATUS) {
        if (mp3buffer[0] == 'S') {
         if (mp3playing == 2) {
          mp3playing = 0;
          if (cfg_mp3state) {
           mp3setnextfile();
           mp3start();
          } else mp3clear();
         }
        } // if (mp3buffer[0] == 'S')
        else {
         mp3playing = 2;
         mp3buffer[mp3bufferptr-2] = 0;
         mp3currenttime = atol(mp3buffer + 2);
         mp3displaytime();
         vfd_update_progressbar(mp3currenttime);
        }
       } // if (mp3status == MP3_STATUS_QUERIED)

      } // if (mp3buffer[0] == 'E') / else

      mp3bufferptr = 0;
      mp3completed = mp3status;  // status für folgeaktionen aufbewahren
      mp3status = MP3_STATUS_READY; // neues kommando ist zulässig
     
 }

