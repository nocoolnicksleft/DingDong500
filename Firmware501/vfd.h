


extern int vfd_modebits;
extern char vfd_turm[8];

extern int  vfd_putc(char data);
extern void vfd_setgraphics_macro(char id, char length, char data[]);
extern void vfd_setbrightness(char value);
extern void vfd_setcursor(char x, char y);
extern void vfd_gotoxy(char x, char y);
extern void vfd_docmd4(char command, char x1, char y1, char x2, char y2);
extern void vfd_init();
extern int  vfd_printf(const char *format, ...);
extern void vfd_keyboardscan(void);

static void vfd_delay(int d);

#define VFD_CMD_BACKSPACE 0x08
#define VFD_CMD_HORIZONTAL_TAB 0x09
#define VFD_CMD_LF 0x0A
#define VFD_CMD_HOME 0x0B
#define VFD_CMD_VERTICAL_TAB 0x0C
#define VFD_CMD_CR 0x0D
#define VFD_CMD_CLEAR_EOL 0x0E
#define VFD_CMD_TEST 0x0F
#define VFD_CMD_PIXEL_SET 0x16
#define VFD_CMD_PIXEL_CLEAR 0x17
#define VFD_CMD_RESET 0x19
#define VFD_CMD_AREA_SET 0x11
#define VFD_CMD_AREA_CLEAR 0x12
#define VFD_CMD_AREA_INVERT 0x13
#define VFD_CMD_OUTLINE_SET 0x14
#define VFD_CMD_OUTLINE_CLEAR 0x15
#define VFD_CMD_MODE_HORIZ_VERT 0x40
#define VFD_CMD_GRAPHIC_WRITE 0x18
#define VFD_CMD_SET_MODE 0x1A
#define VFD_CMD_FONT_MINI 0x1C
#define VFD_CMD_FONT_5X7 0x1D
#define VFD_CMD_FONT_10X14 0x1E

