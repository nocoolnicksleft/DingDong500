
#IFNDEF SAA1064
 #define SAA1064		0b01110000
#ENDIF

int1 initSAA1064( void)
{
   int1 nack = 0;
   i2c_start();
   nack = i2c_write(SAA1064);
  if (nack) {
   i2c_stop();
   i2c_start();
   nack = i2c_write(SAA1064);
  }
   if (!nack) {
    nack = i2c_write(0x00);
    if (!nack) {
     nack = i2c_write(0b00010111);
    }
   }
   i2c_stop();
#if defined DEBUG_SAA1064
   if (nack) printf("1064initNACK\r\n");
#endif
   return !nack;
}

int1 testSAA1064( void) 
{
   int1 nack = 0;
   i2c_start();
   nack = i2c_write(SAA1064);
  if (nack) {
   i2c_stop();
   i2c_start();
   nack = i2c_write(SAA1064);
  }
   if (!nack) {
    nack = i2c_write(0x00);
    if (!nack) {
     nack = i2c_write(0b00011111);
    }
   }
   i2c_stop();
#if defined DEBUG_SAA1064
   if (nack) printf("1064testNACK\r\n");
#endif
   return (!nack);
}

int1 printSAA1064 (unsigned int16 value, int8 decimal)
{
  unsigned int16 chBCD1;
  int8 chBCD2;
  int8 i;
  int1 nack;

  i2c_start();
  nack = i2c_write(SAA1064);
  if (nack) {
   i2c_stop();
   i2c_start();
   nack = i2c_write(SAA1064);
  }
#if defined DEBUG_SAA1064
  if (nack) printf("1064printNACK1\r\n");
#endif
  
  if (!nack) {
   nack = i2c_write(0x1);
#if defined DEBUG_SAA1064
   if (nack) printf("1064printNACK2\r\n");
#endif
   if (!nack) {

    for (i=1;i<=4;i++) {
    
      chBCD1 = value % (int16)10;

      if (chBCD1>9) { 
       chBCD1 = 0; 
      }  

      switch(chBCD1) { 
        case 0:  chBCD2=0x3F; break;
        case 1:  chBCD2=0x06; break;
        case 2:  chBCD2=0x5B; break; 
        case 3:  chBCD2=0x4F; break;
        case 4:  chBCD2=0x66; break;
        case 5:  chBCD2=0x6D; break;
        case 6:  chBCD2=0x7C; break;
        case 7:  chBCD2=0x07; break;
        case 8:  chBCD2=0x7F; break;
        case 9:  chBCD2=0x67; break;
      } // switch

      if (decimal==i) chBCD2 |= 0b10000000; 

      nack = i2c_write(chBCD2);
#if defined DEBUG_SAA1064
  if (nack) printf("1064printNACK3\r\n");
#endif    
      value = (value / (int16)10);   
    } // for
   } // if !nack
  } // if !nack

  i2c_stop();
  return (!nack);
}
