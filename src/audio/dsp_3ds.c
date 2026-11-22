/** @file src/audio/dsp_3ds.c Nintendo 3DS NDSP implementation of the DSP driver. */

#include <3ds.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "../os/endian.h"
#include "../os/error.h"
#include "dsp.h"

#define DUNE_AUDIO_CHANNEL 0
#define DUNE_VOC_MAX_SAMPLES (256u * 1024u)

static bool s_ndspInitialized = false;
static ndspWaveBuf s_waveBuf[2];
static int8 *s_audioBuffer[2] = { NULL, NULL };
static size_t s_audioBufferSize[2] = { 0, 0 };
static int s_currentBuf = 0;
static uint8 s_status = 0;

static void DSP_ApplyChannelMix(void)
{
	float mix[12];
	memset(mix, 0, sizeof(mix));
	mix[0] = 1.0f; /* Left */
	mix[1] = 1.0f; /* Right */
	ndspChnSetMix(DUNE_AUDIO_CHANNEL, mix);
}

static uint32 DSP_ClampSampleRate(uint8 timeConstant)
{
	uint32 sr = 1000000u / (uint32)(256u - (uint32)timeConstant);
	if (sr < 4000u) sr = 4000u;
	if (sr > 48000u) sr = 48000u;
	return sr;
}

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
	DSP_ApplyChannelMix();

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
	uint32 totalSamples = 0;
	uint32 sampleRate = 0;
	const uint8 *blocksStart;
	const uint8 *blk;
	int8 *dst;

	if (!s_ndspInitialized && !DSP_Init()) return;

	DSP_Stop();

	if (data == NULL) return;

	/* VOC Header parsing */
	blocksStart = data + READ_LE_UINT16(data + 20);

	/* Pass 1: compute total PCM + silence sample count across all blocks */
	blk = blocksStart;
	while (*blk != 0) {
		uint8 blockType = *blk;
		uint32 blockLen = READ_LE_UINT32(blk) >> 8;

		if (blockType == 1) {
			if (blockLen <= 2 || blk[5] != 0) return; /* Uncompressed PCM only. */
			if (sampleRate == 0) sampleRate = DSP_ClampSampleRate(blk[4]);
			totalSamples += blockLen - 2;
		} else if (blockType == 2) {
			totalSamples += blockLen;
		} else if (blockType == 3 && blockLen >= 3) {
			uint32 period = (uint32)READ_LE_UINT16(blk + 4) + 1u;
			uint32 silRate = DSP_ClampSampleRate(blk[6]);
			if (sampleRate == 0) sampleRate = silRate;
			uint32 silSamples = (uint32)(((uint64_t)period * sampleRate) / silRate);
			totalSamples += silSamples;
		} else {
			break;
		}

		if (totalSamples > DUNE_VOC_MAX_SAMPLES) {
			totalSamples = DUNE_VOC_MAX_SAMPLES;
			break;
		}
		blk += 4 + blockLen;
	}

	if (totalSamples == 0 || sampleRate == 0) return;

	s_currentBuf = 1 - s_currentBuf;

	/* Ensure linear allocated buffer for 3DS DSP DMA */
	if (s_audioBufferSize[s_currentBuf] < totalSamples) {
		if (s_audioBuffer[s_currentBuf] != NULL) {
			linearFree(s_audioBuffer[s_currentBuf]);
		}
		s_audioBufferSize[s_currentBuf] = (totalSamples + 4095u) & ~4095u;
		s_audioBuffer[s_currentBuf] = (int8 *)linearAlloc(s_audioBufferSize[s_currentBuf]);
		if (s_audioBuffer[s_currentBuf] == NULL) {
			s_audioBufferSize[s_currentBuf] = 0;
			return;
		}
	}

	dst = s_audioBuffer[s_currentBuf];

	/* Pass 2: convert unsigned 8-bit PCM to signed PCM and expand silence blocks */
	uint32 outPos = 0;
	blk = blocksStart;
	while (*blk != 0 && outPos < totalSamples) {
		uint8 blockType = *blk;
		uint32 blockLen = READ_LE_UINT32(blk) >> 8;

		if (blockType == 1) {
			const uint8 *src = blk + 6;
			uint32 n = blockLen - 2;
			if (outPos + n > totalSamples) n = totalSamples - outPos;
			for (uint32 i = 0; i < n; i++) {
				dst[outPos++] = (int8)((int)src[i] - 128);
			}
		} else if (blockType == 2) {
			const uint8 *src = blk + 4;
			uint32 n = blockLen;
			if (outPos + n > totalSamples) n = totalSamples - outPos;
			for (uint32 i = 0; i < n; i++) {
				dst[outPos++] = (int8)((int)src[i] - 128);
			}
		} else if (blockType == 3 && blockLen >= 3) {
			uint32 period = (uint32)READ_LE_UINT16(blk + 4) + 1u;
			uint32 silRate = DSP_ClampSampleRate(blk[6]);
			uint32 n = (uint32)(((uint64_t)period * sampleRate) / silRate);
			if (outPos + n > totalSamples) n = totalSamples - outPos;
			memset(dst + outPos, 0, n);
			outPos += n;
		} else {
			break;
		}
		blk += 4 + blockLen;
	}

	/* Gentle 16-sample de-click ramp on real VOC payloads (>= 64 samples). */
	if (outPos >= 64) {
		for (uint32 i = 0; i < 16; i++) {
			dst[i] = (int8)(((int)dst[i] * (int)i) / 16);
			dst[outPos - 1u - i] = (int8)(((int)dst[outPos - 1u - i] * (int)i) / 16);
		}
	}

	DSP_FlushDataCache(dst, outPos);

	ndspChnReset(DUNE_AUDIO_CHANNEL);
	ndspChnSetInterp(DUNE_AUDIO_CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetFormat(DUNE_AUDIO_CHANNEL, NDSP_FORMAT_MONO_PCM8);
	ndspChnSetRate(DUNE_AUDIO_CHANNEL, (float)sampleRate);
	DSP_ApplyChannelMix();

	memset(&s_waveBuf[s_currentBuf], 0, sizeof(ndspWaveBuf));
	s_waveBuf[s_currentBuf].data_vaddr = dst;
	s_waveBuf[s_currentBuf].nsamples = outPos;
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
