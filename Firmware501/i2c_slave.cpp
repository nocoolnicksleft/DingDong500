
#include "main.h"
#include "i2c_slave.h"


// #define I2CDEBUG 1

/***********************************************************/
/* CTOR                                                    */
/***********************************************************/
i2cSlave::i2cSlave(int portid, int address)
{
  _onclr = &I2C0CONCLR;
  _onset = &I2C0CONSET;
  _stat = &I2C0STAT;
  _dat = &I2C0DAT;
  _pinsel = &PINSEL0;
  _address = address;
  _status = 0;
  _timeout_ticks = 10000;
}

/***********************************************************/
/* init()                                                  */
/***********************************************************/
void i2cSlave::setAddress(uint32 address)
{
  _address = address;
}

/***********************************************************/
/* init()                                                  */
/***********************************************************/
void i2cSlave::init()
{
  /* Initialize I2C */
  PCONP |= PCONP_PCI2C0;

  PINSEL0 |= 0x50;

  /****************
   Bit Frequency = PCLK / (I2C0SCLH + I2C0SCLL)
   Divider Value = (PCLK / Bit Frequency) / 2

   @12 MHZ PCLK 
  
   @15 MHZ PCLK 
     0xC8 -> 37.5 kHz
     0x4B -> 100 kHz
     0x13 -> 400 kHz (394.736 kHz)
  ****************/
  I2C0SCLH = 0x13;
  I2C0SCLL = 0x13;

 // I2C0SCLH = 0xCC;
 // I2C0SCLL = 0xEE;
  
  (* _onclr) = I2C0CONCLR_AAC | I2C0CONCLR_SIC | I2C0CONCLR_STAC | I2C0CONCLR_I2ENC; /* clearing all flags */
  (* _onset) = I2C0CONSET_I2EN; /* enabling I2C */
}

/***********************************************************/
/* shutdown()                                              */
/***********************************************************/
void i2cSlave::shutdown()
{

  (* _onclr) = I2C0CONCLR_I2ENC; /* switch off */

  PCONP &= ~PCONP_PCI2C0; /* power down */

}


/***********************************************************/
/* setStart()                                              */
/***********************************************************/
bool i2cSlave::setStart()
{
  int timeout = _timeout_ticks;

  (* _onclr) = I2C0CONCLR_AAC ;
  (* _onset) = I2C0CONSET_STA | I2C0CONSET_SI;
  
  (* _onclr) = I2C0CONCLR_SIC;
  
  
  while (!((* _onset) & I2C0CONSET_SI)  && (--timeout)); 

  if (!timeout) {
#ifdef I2CDEBUG
    debug_printf("I2C START Timeout %X Address %d\n",_status,_address);
#endif
    return false;
  }
  
  _status = (* _stat);
  
#ifdef I2CDEBUG
  debug_printf("I2C START Status %X Address %d\n",_status,_address);
#endif

  if (_status != I2C_STATUS_START) 
  { 
#ifdef I2CDEBUG
    debug_printf("I2C Bus Busy, ABORTING Address %d\n",_address);
#endif
    return false;
  }

  return true;
}

/***********************************************************/
/* setRestart()                                            */
/***********************************************************/
bool i2cSlave::setRestart()
{
  int timeout = _timeout_ticks;

  // Re-Start
  (* _onset) = I2C0CONSET_STA;
  (* _onclr) = I2C0CONCLR_SIC;

  while (!((* _onset) & I2C0CONSET_SI)  && (--timeout)); 
  
  if (!timeout) {
#ifdef I2CDEBUG
    debug_printf("I2C START REPEAT Timeout %X\n",_status);
#endif
    return false;
  }

  _status = (* _stat);

#ifdef I2CDEBUG
  debug_printf("I2C START REPEAT %X\n",_status);
#endif

  if (_status != I2C_STATUS_REPEATSTART) return false;

  return true;
}




/***********************************************************/
/* setSLAW()                                               */
/***********************************************************/
bool i2cSlave::setSLAW()
{
  int timeout = _timeout_ticks;

  I2C0DAT = (_address) | 0;  // Slave Address | Write
  
  (* _onclr) = I2C0CONCLR_SIC;

  while (!((* _onset) & I2C0CONSET_SI)  && (--timeout)); 
  
  if (!timeout) {
#ifdef I2CDEBUG
    debug_printf("I2C SLAW Timeout %X Address %d\n",_status,_address);
#endif
    return false;
  }

  _status = (* _stat);

#ifdef I2CDEBUG
  debug_printf("I2C SLAW Status %X Address %d\n",_status,_address);
#endif

  if (_status != I2C_STATUS_SLAW_ACK) return false;

  return true;
}



/***********************************************************/
/* setData()                                               */
/***********************************************************/
bool i2cSlave::setData(int data)
{
  int timeout = _timeout_ticks;

  (* _onclr) = I2C0CONCLR_STAC;
  (* _dat) = data; // Pointer Byte 
  (* _onclr) = I2C0CONCLR_SIC;

  while (!((* _onset) & I2C0CONSET_SI)  && (--timeout)); 

  if (!timeout) {
#ifdef I2CDEBUG
    debug_printf("I2C TRANSMIT Timeout %X\n",_status);
#endif
    return false;
  }

  _status = (* _stat);

#ifdef I2CDEBUG
  debug_printf("I2C TRANSMIT Status %X\n",_status);
#endif

  if (_status != I2C_STATUS_TRANSMIT_ACK) return false;

  return true;
} 



/***********************************************************/
/* setSLAR()                                               */
/***********************************************************/
bool i2cSlave::setSLAR()
{
  int timeout = _timeout_ticks;

  (* _onclr) = I2C0CONCLR_STAC;
  (* _dat) = (_address ) | 1;  // Slave Address | Read
  (* _onclr) = I2C0CONCLR_SIC;

  while (!((* _onset) & I2C0CONSET_SI)  && (--timeout)); 
  
  if (!timeout) {
#ifdef I2CDEBUG
    debug_printf("I2C SLAR Timeout %X\n",_status);
#endif
    return false;
  }

  _status = (* _stat);

#ifdef I2CDEBUG
  debug_printf("I2C SLAR Status %X\n",_status);
#endif

  if (_status != I2C_STATUS_SLAR_ACK) return false;

  return true;
}
 


/***********************************************************/
/* readByte()                                              */
/***********************************************************/
unsigned char i2cSlave::readByte()
{
  int timeout = _timeout_ticks;
  
  (* _onclr) = I2C0CONCLR_STAC;
  (* _onset) = I2C0CONSET_AA;
  (* _onclr) = I2C0CONCLR_SIC;

  while (!((* _onset) & I2C0CONSET_SI)  && (--timeout)); 
  
  if (!timeout) {
#ifdef I2CDEBUG
    debug_printf("I2C READ Timeout %X\n",_status);
#endif
    return false;
  }

  _status = (* _stat);

#ifdef I2CDEBUG
  debug_printf("I2C READ Status %X\n",_status);
#endif

  if (_status != I2C_STATUS_RECEIVE_ACK) return 0;

  return (* _dat);
}



/***********************************************************/
/* readLastByte()                                          */
/***********************************************************/
unsigned char i2cSlave::readLastByte()
{
  int timeout = _timeout_ticks;

  (* _onclr) = I2C0CONCLR_STAC;
  (* _onclr) = I2C0CONCLR_AAC;
  (* _onclr) = I2C0CONCLR_SIC;

  while (!((* _onset) & I2C0CONSET_SI)  && (--timeout)); 

  if (!timeout) {
#ifdef I2CDEBUG
    debug_printf("I2C READLAST Timeout %X\n",_status);
#endif
    return false;
  }

  _status = (* _stat);

#ifdef I2CDEBUG
  debug_printf("I2C READLAST Status %X\n",_status);
#endif

  if (_status != I2C_STATUS_RECEIVE_NACK) return 0;

  return (* _dat);
}




/***********************************************************/
/* setStop()                                               */
/***********************************************************/
void i2cSlave::setStop()
{
  (* _onset) = I2C0CONSET_STO;
  (* _onclr) = I2C0CONCLR_SIC;

  _status = (* _stat);

#ifdef I2CDEBUG
  debug_printf("I2C STOP Status %X\n",_status);
#endif
}

int i2cSlave::status()
{
  return _status;
}



