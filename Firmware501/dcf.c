
#include "main.h"
#include "dcf.h"

volatile unsigned int dcfbuffer[8];
volatile unsigned int dcfdeltatimer;
volatile unsigned int dcfbitcount = 0;
volatile unsigned int dcfsequence = 0;
volatile unsigned int dcfdecodenow = 0;
volatile unsigned int dcfsavetimenow = 0;
volatile unsigned short int dcfsignal = 0;
volatile unsigned short int dcfvalid = 0;
volatile unsigned short int dcfreceived = 0;
volatile unsigned short int dcfsync = 0;

struct dcftimestamp {
 unsigned int min;
 unsigned int hour;
 unsigned int wday;
 unsigned int day;
 unsigned int month;
 unsigned int year;
 unsigned int utcoffset;
 unsigned int alert;
 unsigned int offsetchange;
 unsigned int leapsecond;
} dcf1, dcf2;


/*********************************************************************/
/* INTERRUPT DCF DOWN (IRQ)                                          */
/*********************************************************************/

static void dcfdownISR(void)
{
 
  IOCLR = BIT_29;
  dcfsignal = 0;
  dcfvalid = 0;

  if ((dcfdeltatimer > 12) && (dcfdeltatimer<24)) {
   if (!dcfdecodenow) {
     dcfbuffer[(dcfbitcount / 8)] |= (1 << (dcfbitcount % 8)); // short for div/mod by 8
   }
   //debug_printf("1!\n");
 }

  /* Clear the external interrupt */
  EXTINT = 0x00000001;
  /* Dummy write VIC Vector Address to signal end of interrupt handling */
  VICVectAddr = 0;
}


/*********************************************************************/
/* INTERRUPT DCF UP (IRQ)                                            */
/*********************************************************************/

static void dcfupISR(void)
{
 // debug_printf("u %u ",dcfdeltatimer);
  
  IOSET = BIT_29;
  dcfsignal = 1;

  if ( ((dcfdeltatimer > 95) && (dcfdeltatimer < 105))  || 
       ((dcfdeltatimer > 195) && (dcfdeltatimer < 205)) ) {
   if (dcfdeltatimer > 195) {
    dcfdecodenow = 1;
   } 
   dcfvalid = 1;
   dcfdeltatimer = 0;

   if (dcfbitcount > 58) dcfbitcount = 0;
   else dcfbitcount++;
  // debug_printf("b %u\n",dcfbitcount);
   
  } 
  
  // debug_printf("xbit: %u timer: %u\n",dcfbitcount,dcfdeltatimer);

/* Clear the external interrupt */
  EXTINT = 0x00000002;
  /* Dummy write VIC Vector Address to signal end of interrupt handling */
  VICVectAddr = 0;
 
}

int bcddecode(int bcddata)
{
 return (bcddata >> 4)*10 + (bcddata%16);
}

int bcdencode(int decdata)
{
  return ((decdata / 10) << 4) | (decdata % 10);
}

int do_dcf_decode(struct dcftimestamp* dcf)
{

 // debug_printf("b %X %X %X %X c %u\n",dcfbuffer[0],dcfbuffer[1],dcfbuffer[2],dcfbuffer[3],dcfbitcount);
 //printf("V\r");

  if ( (dcfbitcount == 59) && 
       ( (dcfbuffer[0] & 1) == 0 ) &&
       ( (dcfbuffer[2] & (1 << 4)) == (1 << 4 ) ) ) {
   
   
   dcf->hour = bcddecode( (dcfbuffer[4] & 0x7 ) << 3 | ((dcfbuffer[3] & 0xE0 ) >> 5)); 
   dcf->min = bcddecode( (dcfbuffer[3] & 0xF ) << 3 | ((dcfbuffer[2] & 0xE0 ) >> 5));
   dcf->day = bcddecode( (dcfbuffer[5] & 0x3 ) << 4 | ((dcfbuffer[4] & 0xF0 ) >> 4));
   dcf->wday = bcddecode( (dcfbuffer[5] & 0x1C ) >>2 );
   dcf->month = bcddecode( ((dcfbuffer[6] & 0x3 ) << 3) | ((dcfbuffer[5] &  0xE0) >> 5) );
   dcf->year = bcddecode( (dcfbuffer[7] & 0x3 ) << 7 | ((dcfbuffer[6] & 0x3F) >> 2));
   dcf->alert = (dcfbuffer[1] & 0x80)  >> 7;
   dcf->offsetchange = (dcfbuffer[2] & 0x1) ;
   dcf->leapsecond = (dcfbuffer[2] & 0x8) >> 3;
   dcf->utcoffset =  (dcfbuffer[2] & 0x6) >> 1;

   return 1;
  }
 return 0;
}

static void dcfsavetime(void)
{
     if ( (
          ( (dcf2.hour == dcf1.hour) && ((dcf2.min - dcf1.min) == 1) )
          || 
          ( ((dcf2.hour - dcf1.hour) == 1) && (dcf2.min == 0) ) 
         ) && (dcf2.day == dcf1.day) && (dcf2.month == dcf1.month) && (dcf2.year == dcf1.year) ) {
     // alles super, zwei konsistente zeiten empfangen...
     // also setzen und speichern!
#ifdef DEBUG_MAIN    
      debug_printf("Valid Time: %02u:%02u %u, %02u.%02u.%02u\n",dcf2.hour,dcf2.min,dcf2.wday,dcf2.day,dcf2.month,dcf2.year);
#endif

      dcfsync = 1;
      CCR = 0x0;
      CCR = 0x2;
      CCR = 0x0;
      SEC = 0;
      MIN = dcf2.min;
      HOUR = dcf2.hour;
      DOM = dcf2.day;
      DOW = dcf2.wday;
      MONTH = dcf2.month;
      YEAR = 2000 + dcf2.year;
      CCR = 0x1;
     } else { // if ( (dcf2.min 
      // zeit ist zwar g�ltig aber nicht konsistent mit der vorigen, 
      // also diese speichern und auf die n�chste warten...
      //debug_printf("Decoded Time: %02u:%02u %u, %02u.%02u.%02u\n",dcf2.hour,dcf2.min,dcf2.wday,dcf2.day,dcf2.month,dcf2.year);
      dcfsync = 0;
     }
  
     // immer den aktuellen satz in den ersten puffer schieben 
     memcpy(&dcf1,&dcf2,sizeof(dcf1));
     dcfsequence = 1;
}


static void dcfdecode()
{

     if (dcfsequence) {
      if (do_dcf_decode(&dcf2)) {
        // zweite formal g�ltige zeit empfangen, 
        // also in der n�chsten runde kontrollieren und speichern
        dcfsavetimenow = 1;
      } 
      else {
       dcfreceived = 0;
       dcfsync = 0;
      }
      // zeit viel kaputt, alles von vorne...
      dcfsequence = 0;
     } else { // if (dcfsequence)
      if (do_dcf_decode(&dcf1)) {
       // erste formal g�ltige zeit empfangen,
       // also auf die zweite warten und dem benutzer hoffnung machen
       dcfsequence = 1;
       dcfreceived = 1;
      }
      // zeit ist schrott, nix machen, weiter warten 
      else dcfreceived = 0;
     } // if (dcfsequence)
     dcfbitcount = 0;  // mit erstem sekundenbit weitermachen
     memset((unsigned int *)dcfbuffer,0,sizeof(dcfbuffer)); // puffer l�schen
}
