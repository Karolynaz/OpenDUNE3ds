/** @file src/audio/dsp_3ds.c Nintendo 3DS NDSP implementation of the DSP driver. */

#include <3ds.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "../os/endian.h"
#include "../os/error.h"
#include "dsp.h"

#define DUNE_AUDIO_CHANNEL 0

static bool s_ndspInitialized = false;
static ndspWaveBuf s_waveBuf[2];
static int8 *s_audioBuffer[2] = { NULL, NULL };
static size_t s_audioBufferSize[2] = { 0, 0 };
static int s_currentBuf = 0;
static uint8 s_status = 0;

bool DSP_Init(void)
{
	Result rc;

	if (s_ndspInitialized) return true;

	rc = ndspInit();
	if (R_FAILED(rc)) {
		Warning("ndspInit() failed: 0x%08lX\n", rc);
		return false;
	}

	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnReset(DUNE_AUDIO_CHANNEL);
	ndspChnSetInterp(DUNE_AUDIO_CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetRate(DUNE_AUDIO_CHANNEL, 22050.0f);
	ndspChnSetFormat(DUNE_AUDIO_CHANNEL, NDSP_FORMAT_MONO_PCM8);

	float mix[12];
	memset(mix, 0, sizeof(mix));
	mix[0] = 1.0f; // Left
	mix[1] = 1.0f; // Right
	ndspChnSetMix(DUNE_AUDIO_CHANNEL, mix);

	memset(s_waveBuf, 0, sizeof(s_waveBuf));
	s_status = 0;
	s_ndspInitialized = true;

	return true;
}

void DSP_Stop(void)
{
	if (!s_ndspInitialized) return;

	ndspChnReset(DUNE_AUDIO_CHANNEL);
	s_status = 0;
}

void DSP_Uninit(void)
{
	if (!s_ndspInitialized) return;

	DSP_Stop();
	ndspExit();

	for (int i = 0; i < 2; i++) {
		if (s_audioBuffer[i] != NULL) {
			linearFree(s_audioBuffer[i]);
			s_audioBuffer[i] = NULL;
			s_audioBufferSize[i] = 0;
		}
	}

	s_ndspInitialized = false;
}

void DSP_Play(const uint8 *data)
{
	uint32 sampleLen;
	uint32 sampleRate;
	const uint8 *src;
	int8 *dst;

	if (!s_ndspInitialized && !DSP_Init()) return;

	DSP_Stop();

	if (data == NULL) return;

	/* VOC Header parsing */
	data += READ_LE_UINT16(data + 20);

	if (*data != 1) return; /* Block 1 = Sound data */

	uint32 blockLen = READ_LE_UINT32(data) >> 8;
	if (blockLen <= 2 || data[5] != 0) return; /* Uncompressed PCM only. */
	sampleLen = blockLen - 2;
	if (sampleLen == 0) return;

	sampleRate = 1000000 / (256 - data[4]);
	if (sampleRate < 4000) sampleRate = 4000;
	if (sampleRate > 48000) sampleRate = 48000;

	src = data + 6;

	s_currentBuf = 1 - s_currentBuf;

	/* Ensure linear allocated buffer for 3DS DSP DMA */
	if (s_audioBufferSize[s_currentBuf] < sampleLen) {
		if (s_audioBuffer[s_currentBuf] != NULL) {
			linearFree(s_audioBuffer[s_currentBuf]);
		}
		s_audioBufferSize[s_currentBuf] = (sampleLen + 4095) & ~4095;
		s_audioBuffer[s_currentBuf] = (int8 *)linearAlloc(s_audioBufferSize[s_currentBuf]);
		if (s_audioBuffer[s_currentBuf] == NULL) {
			s_audioBufferSize[s_currentBuf] = 0;
			return;
		}
	}

	dst = s_audioBuffer[s_currentBuf];

	/* Convert 8-bit unsigned PCM to signed PCM for NDSP */
	for (uint32 i = 0; i < sampleLen; i++) {
		dst[i] = (int8)((int)src[i] - 128);
	}

	DSP_FlushDataCache(dst, sampleLen);

	ndspChnReset(DUNE_AUDIO_CHANNEL);
	ndspChnSetFormat(DUNE_AUDIO_CHANNEL, NDSP_FORMAT_MONO_PCM8);
	ndspChnSetRate(DUNE_AUDIO_CHANNEL, (float)sampleRate);

	memset(&s_waveBuf[s_currentBuf], 0, sizeof(ndspWaveBuf));
	s_waveBuf[s_currentBuf].data_vaddr = dst;
	s_waveBuf[s_currentBuf].nsamples = sampleLen;
	s_waveBuf[s_currentBuf].status = NDSP_WBUF_FREE;

	ndspChnWaveBufAdd(DUNE_AUDIO_CHANNEL, &s_waveBuf[s_currentBuf]);
	s_status = 2;
}

uint8 DSP_GetStatus(void)
{
	if (!s_ndspInitialized || s_status == 0) return 0;

	/* NDSP starts a newly queued buffer asynchronously. IsPlaying can still
	 * be false here, which made Sound_StartSpeech skip to the next word and
	 * DSP_Stop discard the queued word before the hardware ever played it.
	 * Keep the speech sequencer busy for the entire queue/play lifetime. */
	int status = s_waveBuf[s_currentBuf].status;
	if (status == NDSP_WBUF_QUEUED || status == NDSP_WBUF_PLAYING) return 2;

	s_status = 0;
	return 0;
}
