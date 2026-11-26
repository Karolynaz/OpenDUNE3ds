/** @file src/audio/midi_dsi_bank.h Multi-zone G.711 u-law SoundFont bank extracted via tsf.h from romfs/gm.sf2. */
#ifndef MIDI_DSI_BANK_H
#define MIDI_DSI_BANK_H

#include "types.h"

typedef struct DsiSf2Sample {
	uint32 poolOffset;
	uint32 length;
	uint32 loopStart;
	uint32 loopEnd;
	uint32 sampleRate;
	uint8  looped;
} DsiSf2Sample;

typedef struct DsiSf2Region {
	uint8  lokey;
	uint8  hikey;
	uint16 sampleIdx;
	int8   pitchKeycenter;
	int8   pitchKeytrack;
	int16  tuneCents;
	uint16 gain256;
	uint16 attackInc;
	uint16 holdBlocks;
	uint16 decayMul;
	uint16 sustain16;
	uint16 releaseMul;
	uint8  lpAlpha;
	uint8  group;
	uint8  loopMode;
	uint8  pad;
} DsiSf2Region;

typedef struct DsiSf2Preset {
	uint16 regionStart;
	uint16 regionCount;
} DsiSf2Preset;

#define DSI_SF2_SAMPLE_COUNT 141
#define DSI_SF2_REGION_COUNT 238
#define DSI_SF2_POOL_SIZE    832428

extern const uint8        g_dsiSf2Pool[DSI_SF2_POOL_SIZE];
extern const DsiSf2Sample g_dsiSf2Samples[DSI_SF2_SAMPLE_COUNT];
extern const DsiSf2Region g_dsiSf2Regions[DSI_SF2_REGION_COUNT];
extern const DsiSf2Preset g_dsiSf2Presets[129];

#endif /* MIDI_DSI_BANK_H */
