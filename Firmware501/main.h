
#include <targets/LPC210x.h>
#include <targets/LPC2141.h>

#include <cross_studio_io.h>

#include <libarm.h>
#include <ctl_api.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>


typedef signed char int8;
typedef unsigned char uint8;
typedef unsigned char u8;

typedef short signed int int16;
typedef short unsigned int uint16;
typedef short unsigned int u16;

typedef signed int int32;
typedef unsigned int uint32;
typedef unsigned int u32;


#define BIT_00 0x1
#define BIT_01 0x2
#define BIT_02 0x4
#define BIT_03 0x8
#define BIT_04 0x10
#define BIT_05 0x20
#define BIT_06 0x40
#define BIT_07 0x80
#define BIT_08 0x100
#define BIT_09 0x200
#define BIT_10 0x400
#define BIT_11 0x800
#define BIT_12 0x1000
#define BIT_13 0x2000
#define BIT_14 0x4000
#define BIT_15 0x8000
#define BIT_16 0x10000
#define BIT_17 0x20000
#define BIT_18 0x40000
#define BIT_19 0x80000
#define BIT_20 0x100000
#define BIT_21 0x200000
#define BIT_22 0x400000
#define BIT_23 0x800000
#define BIT_24 0x1000000
#define BIT_25 0x2000000
#define BIT_26 0x4000000
#define BIT_27 0x8000000
#define BIT_28 0x10000000
#define BIT_29 0x20000000
#define BIT_30 0x40000000
#define BIT_31 0x80000000

#define B8(y)	( 0##y       &   1 \
		| 0##y >>  2 &   2 \
		| 0##y >>  4 &   4 \
		| 0##y >>  6 &   8 \
		| 0##y >>  8 &  16 \
		| 0##y >> 10 &  32 \
		| 0##y >> 12 &  64 \
		| 0##y >> 14 & 128 )


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
#define PO_BIT_KBD_SEL (0x1 << 7)
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
/* RTC STATUS                                                        */
/*********************************************************************/

#define RTC_ALARMSTATE 0
#define RTC_ALARMMODE 1
#define RTC_ALARMMINUTE 2
#define RTC_ALARMHOUR 3
#define RTC_ALARMDAYS 4
#define RTC_MP3TITLE1 5
#define RTC_MP3TITLE2 6
#define RTC_MP3VOLUME 7
#define RTC_SHOWDEBUG 8
#define RTC_MAGICNUMBER 9
#define RTC_FREQUENCY 10

// CURRENT CONFIGURATION VALUES
extern unsigned short int cfg_alarmstate;
extern unsigned short int cfg_alarmmode;
extern unsigned short int cfg_alarmminute;
extern unsigned short int cfg_alarmhour;
extern unsigned short int cfg_alarmdays;
extern unsigned short int cfg_mp3state;
extern unsigned short int cfg_mp3title;
extern unsigned short int cfg_mp3volume;
extern unsigned short int cfg_mp3timeout;
extern unsigned short int cfg_displaytimeout;
extern unsigned short int cfg_showdebug;
extern unsigned short int cfg_entrymode;

// STRINGS
extern const char Exit[];
extern const char Save[];
extern const char TimeFormat[]; 
extern const char FrequencyFormat[]; 
extern const char Days[];

#ifdef __cplusplus
extern "C" {
#endif

char spi_lock(char);
void spi_unlock(char);

#ifdef __cplusplus
}
#endif



