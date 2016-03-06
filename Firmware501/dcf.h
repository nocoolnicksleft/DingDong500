

extern volatile unsigned int dcfbuffer[8];
extern volatile unsigned int dcfdeltatimer;
extern volatile unsigned int dcfbitcount;
extern volatile unsigned int dcfsequence;
extern volatile unsigned int dcfdecodenow;
extern volatile unsigned int dcfsavetimenow;
extern volatile unsigned short int dcfsignal;
extern volatile unsigned short int dcfvalid;
extern volatile unsigned short int dcfreceived;
extern volatile unsigned short int dcfsync;

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


#ifdef __cplusplus
extern "C" {
#endif

extern void dcfdownISR(void) __attribute__ ((interrupt ("IRQ")));
extern void dcfupISR(void) __attribute__ ((interrupt ("IRQ")));
extern void dcfsavetime(void);
extern void dcfdecode(void);


#ifdef __cplusplus
}
#endif

