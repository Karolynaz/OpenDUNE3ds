/* Host regression: queued speech is busy even before the DSP starts playback.
 * clang -std=gnu11 -fsanitize=address,undefined -I scripts/test_stubs \
 *   -iquote include -iquote src scripts/test_3ds_speech.c -o /tmp/dune-speech-test
 */
#include <assert.h>
#include <stdio.h>
#include "../src/audio/dsp_3ds.c"
void Warning(const char *format, ...) {(void)format;}
int main(void)
{
	/* A short VOC sound-data block: four unsigned 8-bit samples at 10 kHz. */
	uint8 voc[37]={0};
	voc[20]=26;
	voc[26]=1; voc[27]=6; voc[30]=156;
	voc[32]=0; voc[33]=128; voc[34]=255; voc[35]=64;
	assert(DSP_Init());
	DSP_Play(voc);
	assert(s_waveBuf[s_currentBuf].nsamples==4);
	assert(s_audioBuffer[s_currentBuf][0]==-128);
	assert(s_audioBuffer[s_currentBuf][1]==0);
	assert(s_audioBuffer[s_currentBuf][2]==127);
	/* No hardware playback yet: repeated speech ticks must retain this word. */
	assert(s_waveBuf[s_currentBuf].status==NDSP_WBUF_QUEUED);
	for (int tick=0;tick<20;tick++) assert(DSP_GetStatus()==2);
	s_waveBuf[s_currentBuf].status=NDSP_WBUF_PLAYING;
	assert(DSP_GetStatus()==2);
	s_waveBuf[s_currentBuf].status=NDSP_WBUF_DONE;
	assert(DSP_GetStatus()==0);
	DSP_Play(voc); assert(DSP_GetStatus()==2);
	DSP_Stop(); assert(DSP_GetStatus()==0);
	/* Short and compressed blocks cannot be interpreted as PCM payloads. */
	voc[27]=1; DSP_Play(voc); assert(DSP_GetStatus()==0);
	voc[27]=6; voc[31]=1; DSP_Play(voc); assert(DSP_GetStatus()==0);
	DSP_Uninit(); assert(DSP_GetStatus()==0);
	puts("3DS queued speech checks passed");
}
