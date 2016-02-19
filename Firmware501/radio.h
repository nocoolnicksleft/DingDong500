
#ifdef __cplusplus
extern "C" {
#endif


extern void amp_standby();
extern void amp_on();
extern void amp_off();

extern static void serialISR(void) __attribute__ ((interrupt ("IRQ")));


#ifdef __cplusplus
}
#endif
