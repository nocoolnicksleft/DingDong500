



/*********************************************************************/
/* CONFIGURATION                                                     */
/*********************************************************************/

#define MP3_COLUMN_LENGTH 21
#define MP3_BUFFERSIZE 60
#define MP3_QUEUE_LENGTH 30

#define MP3_STATUS_READY 0
#define MP3_CMD_QUERY_STATUS 1
#define MP3_CMD_SET_VOLUME 2
#define MP3_CMD_PLAY 3
#define MP3_CMD_STOP 4
#define MP3_CMD_OPEN_TITLEFILE 9
#define MP3_CMD_OPEN_COUNTERFILE 12
#define MP3_CMD_QUERY_COUNTERS 13
#define MP3_CMD_CLOSE_TITLEFILE 14
#define MP3_CMD_CLOSE_COUNTERFILE 15
#define MP3_CMD_SET_BASS_ENHANCE 17
#define MP3_CMD_SET_SERIAL_SPEED 18
#define MP3_CMD_LOAD_MACRO 19
#define MP3_CMD_QUERY_ALL 20
#define MP3_CMD_DEVICE_INFO 21

#define MP3_DEFAULT_TIMEOUT 20

#define MP3_VOLUME_MINIMUM 120
#define MP3_VOLUME_MAXIMUM 50
#define MP3_VOLUME_DEFAULT 90
#define MP3_BUFFER_SIZE 100


extern unsigned short int mp3message;
extern char mp3buffer[MP3_BUFFERSIZE];
extern unsigned short int mp3bufferptr;

extern unsigned short int mp3status;
extern unsigned short int mp3completed;

extern unsigned short int mp3lasttitle;
extern unsigned short int mp3playing;
extern unsigned short int mp3timeout;
extern unsigned short int mp3retries;
extern unsigned short int mp3fetch;
extern unsigned short int mp3currenttime;
extern unsigned short int mp3cardmissing;

struct mp3titledata {
 char artist[MP3_COLUMN_LENGTH + 1];
 char title[MP3_COLUMN_LENGTH + 1];
 unsigned short int length;
};



#ifdef __cplusplus
extern "C" {
#endif

extern void serialISR(void) __attribute__ ((interrupt ("IRQ")));

extern void mp3clearqueue();
extern void mp3commandpush(char c);
extern char mp3commandpop();
extern void mp3sendcommand();
extern void mp3execute();
extern void mp3setnextfile();
extern void mp3setprevfile();
extern void mp3clear();
extern void mp3displaytime();
extern void mp3displaydata(struct mp3titledata *data);
extern void mp3displaycurrentdata();
extern void mp3displayvolume();
extern void mp3start();
extern void domp3message();



extern void amp_standby();
extern void amp_off();
extern void amp_power();

#ifdef __cplusplus
}
#endif


