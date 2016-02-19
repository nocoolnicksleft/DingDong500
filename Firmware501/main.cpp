
#include "main.h"



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
/* SPI LOCK FUNCTIONS                                                */
/*********************************************************************/

char spi0_lock = 0;

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




#define P0_BIT_LED_RDY (0x1 << 31)
#define P0_BIT_LED_1 (0x1 << 12)
#define P0_BIT_LED_2 (0x1 << 13)

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





