/** @file src/audio/dsp_dsi.c Nintendo DSi hardware ARM7 PCM8 DSP driver for .VOC voices and SFX. */

#include <nds.h>
#include <calico/nds/arm9/sound.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "../os/endian.h"
#include "../os/error.h"
#include "dsp.h"

#define DSI_VOC_CHANNEL 0
#define DSI_VOC_MAX_SAMPLES (256u * 1024u)

static bool s_dspInitialized = false;
static int8 *s_audioBuffer[2] = { NULL, NULL };
static size_t s_audioBufferSize[2] = { 0, 0 };
static int s_currentBuf = 0;
static uint8 s_status = 0;

static uint32 DSP_ClampSampleRate(uint8 timeConstant)
{
	uint32 sr = 1000000u / (uint32)(256u - (uint32)timeConstant);
	if (sr < 4000u) sr = 4000u;
	if (sr > 32768u) sr = 32768u;
	return sr;
}

bool DSP_Init(void)
{
	if (s_dspInitialized) return true;

	soundInit();
	soundEnable();
	soundSetMixerVolume(127);
	soundSetAutoUpdate(true);

	s_status = 0;
	s_dspInitialized = true;
	return true;
}

void DSP_Stop(void)
{
	if (!s_dspInitialized) return;
	soundStop(1u << DSI_VOC_CHANNEL);
	s_status = 0;
}

void DSP_Uninit(void)
{
	if (!s_dspInitialized) return;

	DSP_Stop();
	for (int i = 0; i < 2; i++) {
		if (s_audioBuffer[i] != NULL) {
			free(s_audioBuffer[i]);
			s_audioBuffer[i] = NULL;
			s_audioBufferSize[i] = 0;
		}
	}

	s_dspInitialized = false;
}

void DSP_Play(const uint8 *data)
{
	extern void midi_3ds_pump(void);
	uint32 totalSamples = 0;
	uint32 sampleRate = 0;
	const uint8 *blocksStart;
	const uint8 *blk;
	int8 *dst;

	midi_3ds_pump();
	if (!s_dspInitialized && !DSP_Init()) return;

	DSP_Stop();
	if (data == NULL) return;

	blocksStart = data + READ_LE_UINT16(data + 20);

	/* Pass 1: total PCM + silence sample count across VOC blocks */
	blk = blocksStart;
	while (*blk != 0) {
		uint8 blockType = *blk;
		uint32 blockLen = READ_LE_UINT32(blk) >> 8;

		if (blockType == 1) {
			if (blockLen <= 2 || blk[5] != 0) return;
			if (sampleRate == 0) sampleRate = DSP_ClampSampleRate(blk[4]);
			totalSamples += blockLen - 2;
		} else if (blockType == 2) {
			totalSamples += blockLen;
		} else if (blockType == 3 && blockLen >= 3) {
			uint32 period = (uint32)READ_LE_UINT16(blk + 4) + 1u;
			uint32 silRate = DSP_ClampSampleRate(blk[6]);
			uint32 effRate = sampleRate ? sampleRate : silRate;
			uint32 silSamples = (uint32)(((uint64)period * effRate) / silRate);
			totalSamples += silSamples;
		}

		blk += 4 + blockLen;
		if (totalSamples > DSI_VOC_MAX_SAMPLES) {
			totalSamples = DSI_VOC_MAX_SAMPLES;
			break;
		}
	}

	if (totalSamples < 4) return;
	if (sampleRate == 0) sampleRate = 11025u;

	uint32 alignedBytes = (totalSamples + 3u) & ~3u;
	s_currentBuf ^= 1;
	if (s_audioBufferSize[s_currentBuf] < alignedBytes) {
		if (s_audioBuffer[s_currentBuf] != NULL) {
			free(s_audioBuffer[s_currentBuf]);
			s_audioBuffer[s_currentBuf] = NULL;
			s_audioBufferSize[s_currentBuf] = 0;
		}
		size_t allocSize = (alignedBytes + 31u) & ~31u;
		s_audioBuffer[s_currentBuf] = (int8 *)aligned_alloc(32, allocSize);
		if (s_audioBuffer[s_currentBuf] == NULL) return;
		s_audioBufferSize[s_currentBuf] = allocSize;
	}

	dst = s_audioBuffer[s_currentBuf];
	uint32 written = 0;
	blk = blocksStart;
	while (*blk != 0 && written < totalSamples) {
		uint8 blockType = *blk;
		uint32 blockLen = READ_LE_UINT32(blk) >> 8;

		if (blockType == 1 && blockLen > 2 && blk[5] == 0) {
			uint32 n = blockLen - 2;
			if (n > totalSamples - written) n = totalSamples - written;
			const uint8 *src = blk + 6;
			for (uint32 i = 0; i < n; i++) dst[written + i] = (int8)((int)src[i] - 128);
			written += n;
		} else if (blockType == 2) {
			uint32 n = blockLen;
			if (n > totalSamples - written) n = totalSamples - written;
			const uint8 *src = blk + 4;
			for (uint32 i = 0; i < n; i++) dst[written + i] = (int8)((int)src[i] - 128);
			written += n;
		} else if (blockType == 3 && blockLen >= 3) {
			uint32 period = (uint32)READ_LE_UINT16(blk + 4) + 1u;
			uint32 silRate = DSP_ClampSampleRate(blk[6]);
			uint32 n = (uint32)(((uint64)period * sampleRate) / silRate);
			if (n > totalSamples - written) n = totalSamples - written;
			memset(dst + written, 0, n);
			written += n;
		}

		blk += 4 + blockLen;
	}

	if (written < 4) return;
	uint32 padBytes = (written + 3u) & ~3u;
	for (uint32 i = written; i < padBytes; i++) dst[i] = 0;

	DC_FlushRange(dst, padBytes);
	soundPreparePcm(
		DSI_VOC_CHANNEL | SOUND_START,
		2047,
		64,
		soundTimerFromHz(sampleRate),
		SoundMode_OneShot,
		SoundFmt_Pcm8,
		dst,
		0,
		padBytes / 4
	);
	s_status = 2;
}

uint8 DSP_GetStatus(void)
{
	if (!s_dspInitialized || s_status == 0) return 0;
	if ((soundGetActiveChannels() & (1u << DSI_VOC_CHANNEL)) == 0) {
		s_status = 0;
		return 0;
	}
	return 2;
}

uint32 DSP_GetDataSize(const uint8 *data)
{
	if (data == NULL) return 0;
	return READ_LE_UINT32(data + 24) >> 8;
}
