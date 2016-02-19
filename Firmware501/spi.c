
struct spiport {
 byte status;
 byte spistatus;
 byte lock;
 int reg_status;
 int reg_data;
};


struct spiconnection {
 byte id;
 spiport *port;
 byte selectport;
 byte selectdelay;
 byte clockactivelow;
 byte synconend;
 byte selectactivelow;
 byte buffer[8];
 byte bufptr;
 byte buflen;
 void *completionhandler;
};


struct spiport SPIPORT[2];

struct spiconnection SPICONNECTION[4];

BYTE spilastusedconnection = 0;

static void spi_delay(long d)
{     
  for(; d; --d);
}

BYTE spiport_open(BYTE number)
{
  // init structure
  SPIPORT[number].status = 0;
  SPIPORT[number].lock = 0;

  if (number == 0)
  {
   SPIPORT[number].reg_status = &S0SPSR;
   SPIPORT[number].reg_data = &S0SPDR;
  }

  if (number == 1)
  {
   SPIPORT[number].reg_status = S1SPSR;
   SPIPORT[number].reg_data = S1SPDR;
  }
  // register ISR
  
}

void spiconn_writeasync(BYTE id)
{
   __ARMLIB_disableIRQ();
   // check if connection free
   if (! SPICONNECTION[id].status)
   {
     SPICONNECTION[id].status = 1;
     
     // check if port free, wait or abort
     if (! SPIPORT[SPICONNECTION[id].status) 
     {
       // lock port for connection
       SPIPORT[id].status = id;
       __ARMLIB_enableIRQ();
       if (conn->selectactivelow) IOCLR = (1 << conn->selectport);
       else IOSET = (1 << conn->selectport);
       spi_delay(conn->selectdelay);
       conn->bufptr = 0;
       S0SPDR = conn->buffer[conn->bufptr];
       
     } else __ARMLIB_enableIRQ();
     
   } else __ARMLIB_enableIRQ();

   // disable isr, set connection mode
   // set selectline
   // wait for set delay time
   // write byte to port
}

void spiport_isr()
{
  // check error condition
  // determine active connection
  spiconnection *conn;
  if (SPIPORT[0].status) {
   conn = &SPICONNECTION[SPIPORT[0].status];
   SPIPORT[0].spistatus = S0SPSR;
   if ((SPIPORT[0].spistatus & BIT_07) == BIT_07) {
    // read returned value and write to buffer
    conn->buffer[conn->bufptr] = S0SPDR;
    if (conn->bufptr + 1 > conn->buflen) {   // if more to send
     conn->bufptr++;
     // write data to port
     S0SPDR = conn->buffer[conn->bufptr];
    } else { // if finished
     // unset selectline
     if (conn->selectactivelow) IOSET = (1 << conn->selectport);
     else IOCLR = (1 << conn->selectport);
     // free port
     SPIPORT[0].status = 0;
     // call transfer-completed routine of connection
     *(conn->completionhandler);
     for (int i = 0; i < SPIPORT[0].connectioncount; i++)
      if (SPICONNECTION[i].status) {
       spiconn_writeasync(i); //  if other connections have sends
       exit;
      } // if
    } // else
   } // if BIT_07
  } else { // ERROR: No pending transfer - why was ISR called??
#ifdef DEBUG_ALL
    debug_printf("ERROR: No pending transfer \n");
#endif
  }
}




BYTE spiconn_open(BYTE port, BYTE activelow, BYTE synconend, BYTE selectport, void *isr)
{
   // get next free id
   
   // initialize structure
}

BYTE spiconn_writesync(BYTE connection, BYTE data, BYTE wait)
{
   // check id valid
   // check if port free, wait or abort
   // lock port for connection
   // disable isr, set connection mode
   // set selectline
   // wait for set delay time
   // write byte to port
   // wait for sent flag
   // read returned value
   // unset selectline
   // unlock port
   // return data
}






void spi_keyboard_isr()
{

}

void spi_rtc_isr()
{

}


BYTE pkeyboard;
BYTE prtc;

spiport_open(0);
pkeyboard = spiconn_open(0,0,1,22,&spi_keyboard_isr());
prtc = spiconn_open(0,0,1,24,&spi_rtc_isr());


spiconn_writesync(pkeyboard,0x1);
spiconn_writesync(pkeyboard,0x2);




