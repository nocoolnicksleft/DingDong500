

#define RTC_SEL BIT_24
#define RTC_IRQ BIT_23

#define DS1305_CTRL_RD 0x0F
#define DS1305_CTRL_WR 0x8F

#define DS1305_CTRL_NEOSC BIT_07
#define DS1305_CTRL_WP BIT_06
#define DS1305_CTRL_INTCN BIT_02
#define DS1305_CTRL_AIE1 BIT_01
#define DS1305_CTRL_AIE0 BIT_00

#define DS1305_STA_RD 0x10
#define DS1305_STA_IRQF0 BIT_00
#define DS1305_STA_IRQF1 BIT_01

#define DS1305_TRC_RD 0x11
#define DS1305_TRC_WR 0x91

#define DS1305_TIME_RD 0x00
#define DS1305_TIME_WR 0x80

#define DS1305_USER_RD 0x20
#define DS1305_USER_WR 0xA0

#define DS1305_SEL_DELAY 10

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

