/* Minimal NDSP test double for the host-side synthesizer regression test. */
#ifndef DUNE_TEST_3DS_H
#define DUNE_TEST_3DS_H
#include <stdlib.h>
#include <stdint.h>
typedef int Result;
#define R_FAILED(rc) ((rc)<0)
#define NDSP_WBUF_FREE 0
#define NDSP_WBUF_DONE 1
#define NDSP_WBUF_QUEUED 2
#define NDSP_WBUF_PLAYING 3
#define NDSP_OUTPUT_STEREO 0
#define NDSP_FORMAT_MONO_PCM8 1
#define NDSP_INTERP_LINEAR 0
#define NDSP_FORMAT_STEREO_PCM16 0
 typedef struct { void *data_vaddr; uint32_t nsamples; int status; } ndspWaveBuf;
static int test_ndsp_refs, test_alloc_count, test_alloc_fail_after = -1;
static inline Result ndspInit(void) {test_ndsp_refs++; return 0;}
static inline void ndspExit(void) {test_ndsp_refs--;}
static inline void *linearAlloc(size_t n) {
 if (test_alloc_fail_after == 0) return NULL;
 if (test_alloc_fail_after > 0) test_alloc_fail_after--;
 void *p = malloc(n); if (p) test_alloc_count++; return p;
}
static inline void linearFree(void *p) {if (p) test_alloc_count--; free(p);}
static inline void DSP_FlushDataCache(void *p,size_t n) {(void)p;(void)n;}
static inline void ndspChnWaveBufAdd(int c,ndspWaveBuf *b) {(void)c;b->status=NDSP_WBUF_QUEUED;}
static inline void ndspSetOutputMode(int mode) {(void)mode;}
static inline void ndspChnReset(int c) {(void)c;}
static inline void ndspChnSetInterp(int c,int i) {(void)c;(void)i;}
static inline void ndspChnSetRate(int c,float r) {(void)c;(void)r;}
static inline void ndspChnSetFormat(int c,int f) {(void)c;(void)f;}
static inline void ndspChnSetMix(int c,float *m) {(void)c;(void)m;}

/* Native applet and HID test fixtures. */
#include <stdio.h>
#include <string.h>
typedef uint32_t u32;
typedef struct {int16_t dx,dy;} circlePosition;
typedef struct {uint16_t px,py;} touchPosition;
enum {KEY_A=1u<<0,KEY_B=1u<<1,KEY_SELECT=1u<<2,KEY_START=1u<<3,
 KEY_DRIGHT=1u<<4,KEY_DLEFT=1u<<5,KEY_DUP=1u<<6,KEY_DDOWN=1u<<7,
 KEY_R=1u<<8,KEY_L=1u<<9,KEY_X=1u<<10,KEY_Y=1u<<11,KEY_ZL=1u<<14,KEY_ZR=1u<<15,KEY_TOUCH=1u<<20};
static u32 test_keys_down,test_keys_held,test_keys_up;
static circlePosition test_circle;
static touchPosition test_touch;
static inline int hidInit(void) {return 0;}
static inline void hidExit(void) {}
static inline void hidScanInput(void) {}
static inline u32 hidKeysDown(void) {return test_keys_down;}
static inline u32 hidKeysHeld(void) {return test_keys_held;}
static inline u32 hidKeysUp(void) {return test_keys_up;}
static inline void hidCircleRead(circlePosition *p) {*p=test_circle;}
static inline void hidTouchRead(touchPosition *p) {*p=test_touch;}
typedef enum {SWKBD_BUTTON_LEFT,SWKBD_BUTTON_MIDDLE,SWKBD_BUTTON_RIGHT,SWKBD_BUTTON_NONE} SwkbdButton;
enum {SWKBD_TYPE_NORMAL,SWKBD_NOTEMPTY_NOTBLANK=2};
typedef struct {int limit,buttons; char initial[51];} SwkbdState;
static int test_keyboard_result=SWKBD_BUTTON_RIGHT,test_keyboard_limit,test_keyboard_calls;
static char test_keyboard_initial[51];
static const char *test_keyboard_text="Saved game";
static inline void swkbdInit(SwkbdState *s,int type,int buttons,int limit) {(void)type;s->buttons=buttons;s->limit=limit;test_keyboard_limit=limit;}
static inline void swkbdSetValidation(SwkbdState *s,int v,int f,int d) {(void)s;(void)v;(void)f;(void)d;}
static inline void swkbdSetHintText(SwkbdState *s,const char *p) {(void)s;(void)p;}
static inline void swkbdSetButton(SwkbdState *s,int b,const char *p,int submit) {(void)s;(void)b;(void)p;(void)submit;}
static inline void swkbdSetInitialText(SwkbdState *s,const char *p) {(void)s;snprintf(test_keyboard_initial,51,"%s",p);}
static inline SwkbdButton swkbdInputText(SwkbdState *s,char *buf,size_t size) {(void)s;test_keyboard_calls++;snprintf(buf,size,"%s",test_keyboard_text);return (SwkbdButton)test_keyboard_result;}
#endif
