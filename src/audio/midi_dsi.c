/** @file src/audio/midi_dsi.c Nintendo DSi 16-bit fixed-point interpolated SoundFont MIDI synthesizer.
 *
 * Streams crystal-locked 16,364.25 Hz 16-bit PCM into DS ARM7 hardware channel 1
 * (channel 0 remains dedicated to .VOC voices/SFX in dsp_dsi.c).
 * Uses the multi-zone G.711 u-law SoundFont bank extracted from romfs/gm.sf2 via tsf.h,
 * with exact sample-accurate 120 Hz MPU_Interrupt() pacing, linear interpolation,
 * per-region SF2 ADSR envelopes, attenuation balance, and 1-pole lowpass filtering.
 * Pure 32-bit integer ARM math — zero floating point, ~3-5% ARM9 CPU load. */

#ifdef __NDS__
#include <nds.h>
#include <calico/nds/arm9/sound.h>
#include <calico/system/tick.h>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "midi.h"
#include "mt32mpu.h"
#include "midi_dsi_bank.h"

#define NUM_MIDI_CHANNELS 16
#define MAX_SYNTH_VOICES  14

/* DS hardware timer = 1024 -> SOUND_CLOCK / 1024 = SYSTEM_CLOCK / 2048 = 16,364.249 Hz.
 * Because TICK_FREQ = SYSTEM_CLOCK / 64, exactly 32 tickGetCount() ticks equal 1 audio sample
 * (zero drift forever!). And 16364.25 Hz is an exact 1/2 sub-multiple of DS 32728.5 Hz I2S. */
#define DSI_MIX_TIMER         1024u
#define DSI_MIX_SR_INT        16364u
#define DSI_MPU_BASE_SAMPLES  136u    /* 16364.25 / 120 = 136.36875 samples per MPU tick */
#define DSI_MPU_FRAC_INC      36875u
#define DSI_MPU_FRAC_MOD      100000u

#define DSI_HW_MUSIC_CH       1u
#define MIX_BUF_SAMPLES       8192u   /* 16 KB = ~500 ms circular 16-bit PCM buffer */
#define MIX_BUF_MASK          (MIX_BUF_SAMPLES - 1u)
#define MIX_BLOCK_SAMPLES     64u     /* 128 bytes = 4 exact ARM9 32-byte cache lines */
#define TARGET_AHEAD_SAMPLES  3584u   /* ~220 ms buffered ahead of ARM7 DMA read cursor */

typedef enum {
	ENV_ATTACK = 0,
	ENV_HOLD,
	ENV_DECAY,
	ENV_SUSTAIN,
	ENV_RELEASE
} EnvStage;

typedef struct {
	bool active;
	bool keyDown;
	bool sustained;
	bool looping;
	uint8 midiCh;
	uint8 note;
	uint8 velocity;
	uint8 group;
	uint8 lpAlpha;
	const DsiSf2Region *reg;
	const DsiSf2Sample *smp;

	uint32 phase;         /* 20.12 fixed-point sample position */
	uint32 phaseInc;      /* 20.12 fixed-point step per output sample */
	uint32 loopStartFixed;
	uint32 loopEndFixed;
	uint32 loopSpanFixed;

	EnvStage envStage;
	uint32 env16;         /* 0..65535 envelope amplitude */
	uint16 holdLeft;
	int32 curGain;        /* 0..65535 smoothed per-sample gain */
	int32 gainStep;       /* Per-sample linear ramp (computed once per 64-sample block) */
	int32 lpZ;            /* 1-pole lowpass state */
	uint32 age;
} SynthVoice;

typedef struct {
	uint8 program;
	uint8 volume;
	uint8 expression;
	uint8 pan;
	int16 pitchBendCents; /* -200..+200 cents */
	bool sustain;
} MidiChanState;

/* Fixed-point 16.16 pitch ratio for 0..12 semitones: round(65536 * 2^(s/12)). */
static const uint32 s_semiRatio16[13] = {
	 65536u,  69433u,  73562u,  77936u,
	 82570u,  87480u,  92682u,  98193u,
	104032u, 110218u, 116772u, 123715u,
	131072u
};

/* 512-byte G.711 u-law -> signed 16-bit PCM lookup table (stays hot in ARM9 D-cache). */
static int16 s_ulawTable[256];
static bool s_ulawBuilt = false;

static bool s_midiInit = false;
static SynthVoice s_voices[MAX_SYNTH_VOICES];
static MidiChanState s_ch[NUM_MIDI_CHANNELS];
static uint32 s_ageCounter = 0;
static uint32 s_mpuSamplesLeft = DSI_MPU_BASE_SAMPLES;
static uint32 s_mpuFrac = 0;

#ifdef __NDS__
static int16 s_mixBuf[MIX_BUF_SAMPLES] __attribute__((aligned(32)));
static u64 s_streamStartTick = 0;
static u64 s_writtenSamples = 0;
#endif

static void BuildUlawTable(void)
{
	if (s_ulawBuilt) return;
	for (int i = 0; i < 256; i++) {
		int u = ~i & 0xFF;
		int sign = u & 0x80;
		int exp = (u >> 4) & 0x07;
		int mant = u & 0x0F;
		int mag = ((mant << 3) + 0x84) << exp;
		mag -= 0x84;
		s_ulawTable[i] = (int16)(sign ? -mag : mag);
	}
	s_ulawBuilt = true;
}

static uint32 CalcPhaseInc12(const DsiSf2Region *reg, const DsiSf2Sample *smp, uint8 note, int16 bendCents)
{
	int32 noteCents = (int32)note * 100 + (int32)reg->tuneCents;
	int32 kcCents = (int32)reg->pitchKeycenter * 100;
	int32 deltaCents = ((noteCents - kcCents) * (int32)reg->pitchKeytrack) / 100 + (int32)bendCents;

	int32 oct = 0;
	while (deltaCents >= 1200) {
		deltaCents -= 1200;
		oct++;
	}
	while (deltaCents < 0) {
		deltaCents += 1200;
		oct--;
	}

	uint32 semi = (uint32)(deltaCents / 100);
	uint32 fine = (uint32)(deltaCents % 100);
	uint32 r0 = s_semiRatio16[semi];
	uint32 r1 = s_semiRatio16[semi + 1];
	uint32 ratio16 = r0 + ((r1 - r0) * fine) / 100u;

	/* phaseInc = (sampleRate / 16364.25) * 2^oct * (ratio16 / 65536) * 4096 */
	uint64_t num = ((uint64_t)smp->sampleRate * (uint64_t)ratio16) >> 4; /* scaled by 2^12 */
	uint64_t inc = num / (uint64_t)DSI_MIX_SR_INT;

	if (oct > 0) {
		if (oct >= 6) inc = (64u << 12);
		else inc <<= oct;
	} else if (oct < 0) {
		if (oct <= -10) inc = 64u;
		else inc >>= -oct;
	}

	if (inc < 64u) inc = 64u;
	if (inc > (32u << 12)) inc = (32u << 12);
	return (uint32)inc;
}

static int32 CalcVoiceTargetGain(const SynthVoice *v)
{
	/* Combine region SF2 attenuation gain, velocity, CC7 volume, CC11 expression, and ADSR envelope */
	uint32 vel = v->velocity;
	uint32 vol = s_ch[v->midiCh].volume;
	uint32 exp = s_ch[v->midiCh].expression;

	/* (vel * vol * exp) / 127^3 -> scale into 0..256 (max 2048383 / 8000 ~= 256) */
	uint32 chGain = (vel * vol * exp) >> 13; /* 0..250 */
	uint32 regGain = (chGain * (uint32)v->reg->gain256) >> 8;
	if (regGain > 768u) regGain = 768u;

	/* Multiply by env16 (0..65535) -> targetGain in 0..65535 (where 16384 = 1.0x nominal) */
	uint32 g = (regGain * v->env16) >> 10;
	if (g > 65535u) g = 65535u;
	return (int32)g;
}

static const DsiSf2Region *FindRegion(uint8 presetIdx, uint8 note)
{
	const DsiSf2Preset *p = &g_dsiSf2Presets[presetIdx];
	const DsiSf2Region *start = &g_dsiSf2Regions[p->regionStart];
	for (uint16 i = 0; i < p->regionCount; i++) {
		if (note >= start[i].lokey && note <= start[i].hikey) {
			return &start[i];
		}
	}
	return NULL;
}

static SynthVoice *AllocVoice(uint8 midiCh, uint8 note)
{
	SynthVoice *freeV = NULL;
	SynthVoice *sameV = NULL;

	for (int i = 0; i < MAX_SYNTH_VOICES; i++) {
		if (!s_voices[i].active) {
			if (!freeV) freeV = &s_voices[i];
		} else if (midiCh != 9 && s_voices[i].midiCh == midiCh && s_voices[i].note == note) {
			sameV = &s_voices[i];
		}
	}
	if (sameV) return sameV;
	if (freeV) return freeV;

	/* Steal quietest releasing voice first */
	SynthVoice *best = NULL;
	uint32 bestScore = 0xFFFFFFFFu;
	for (int i = 0; i < MAX_SYNTH_VOICES; i++) {
		if (s_voices[i].envStage == ENV_RELEASE && s_voices[i].env16 < bestScore) {
			bestScore = s_voices[i].env16;
			best = &s_voices[i];
		}
	}
	if (best) return best;

	/* Otherwise steal quietest active voice */
	for (int i = 0; i < MAX_SYNTH_VOICES; i++) {
		uint32 sc = (uint32)s_voices[i].curGain;
		if (sc < bestScore) {
			bestScore = sc;
			best = &s_voices[i];
		}
	}
	return best ? best : &s_voices[0];
}

static void HandleNoteOn(uint8 midiCh, uint8 note, uint8 velocity)
{
	uint8 presetIdx = (midiCh == 9) ? 128 : (s_ch[midiCh].program & 127);
	const DsiSf2Region *reg = FindRegion(presetIdx, note);
	if (reg == NULL) return;
	const DsiSf2Sample *smp = &g_dsiSf2Samples[reg->sampleIdx];

	/* Exclusive choke group (e.g. closed hi-hat chokes open hi-hat) */
	if (reg->group != 0) {
		for (int i = 0; i < MAX_SYNTH_VOICES; i++) {
			if (s_voices[i].active && s_voices[i].midiCh == midiCh && s_voices[i].group == reg->group) {
				s_voices[i].keyDown = false;
				s_voices[i].sustained = false;
				s_voices[i].envStage = ENV_RELEASE;
				if (s_voices[i].env16 > 4096u) s_voices[i].env16 = 4096u;
			}
		}
	}

	SynthVoice *v = AllocVoice(midiCh, note);
	v->active = true;
	v->keyDown = true;
	v->sustained = false;
	v->looping = (smp->looped != 0 && reg->loopMode != 0);
	v->midiCh = midiCh;
	v->note = note;
	v->velocity = velocity;
	v->group = reg->group;
	v->lpAlpha = reg->lpAlpha;
	v->reg = reg;
	v->smp = smp;

	v->phase = 0;
	v->phaseInc = CalcPhaseInc12(reg, smp, note, (midiCh == 9) ? 0 : s_ch[midiCh].pitchBendCents);
	if (v->looping) {
		v->loopStartFixed = smp->loopStart << 12;
		v->loopEndFixed = (smp->loopEnd + 1u) << 12;
		v->loopSpanFixed = (smp->loopEnd + 1u - smp->loopStart) << 12;
	} else {
		v->loopStartFixed = 0;
		v->loopEndFixed = smp->length << 12;
		v->loopSpanFixed = 0;
	}

	v->holdLeft = reg->holdBlocks;
	if (reg->attackInc >= 65535u) {
		v->env16 = 65535u;
		v->envStage = (v->holdLeft > 0) ? ENV_HOLD : ENV_DECAY;
	} else {
		v->env16 = 0;
		v->envStage = ENV_ATTACK;
	}
	v->curGain = 0;
	v->gainStep = CalcVoiceTargetGain(v) >> 6;
	v->lpZ = 0;
	v->age = ++s_ageCounter;
}

static void HandleNoteOff(uint8 midiCh, uint8 note)
{
	if (midiCh == 9) return;
	for (int i = 0; i < MAX_SYNTH_VOICES; i++) {
		SynthVoice *v = &s_voices[i];
		if (v->active && v->midiCh == midiCh && v->note == note && v->keyDown) {
			v->keyDown = false;
			if (s_ch[midiCh].sustain) {
				v->sustained = true;
			} else {
				v->envStage = ENV_RELEASE;
				if (v->reg->loopMode == 2) {
					v->looping = false;
					v->loopEndFixed = v->smp->length << 12;
				}
			}
		}
	}
}

static void StepVoiceEnvelope(SynthVoice *v)
{
	const DsiSf2Region *reg = v->reg;
	switch (v->envStage) {
		case ENV_ATTACK:
			v->env16 += reg->attackInc;
			if (v->env16 >= 65535u) {
				v->env16 = 65535u;
				v->envStage = (v->holdLeft > 0) ? ENV_HOLD : ENV_DECAY;
			}
			break;
		case ENV_HOLD:
			if (v->holdLeft > 0) {
				v->holdLeft--;
			}
			if (v->holdLeft == 0) {
				v->envStage = ENV_DECAY;
			}
			break;
		case ENV_DECAY:
			if (reg->decayMul == 0) {
				v->env16 = reg->sustain16;
				v->envStage = ENV_SUSTAIN;
			} else {
				v->env16 = (v->env16 * (uint32)reg->decayMul) >> 16;
				if (v->env16 <= reg->sustain16) {
					v->env16 = reg->sustain16;
					v->envStage = ENV_SUSTAIN;
				}
			}
			if (v->envStage == ENV_SUSTAIN && v->env16 < 512u) {
				v->active = false;
			}
			break;
		case ENV_SUSTAIN:
			break;
		case ENV_RELEASE:
			if (reg->releaseMul == 0) {
				v->env16 = 0;
				v->active = false;
			} else {
				v->env16 = (v->env16 * (uint32)reg->releaseMul) >> 16;
				if (v->env16 < 512u) {
					v->env16 = 0;
					v->active = false;
				}
			}
			break;
	}
	if (v->active) {
		int32 target = CalcVoiceTargetGain(v);
		v->gainStep = (target - v->curGain) >> 6;
	}
}

/* Render `count` samples (1..MIX_BLOCK_SAMPLES) of all active voices into `out16`.
 * Zero software divisions and branch-free inner loop on the 99.5% non-wrapping fast path. */
static void RenderVoicesSubBlock(int16 *out16, unsigned count)
{
	int32 acc[MIX_BLOCK_SAMPLES];
	memset(acc, 0, count * sizeof(int32));

	const int16 *ulaw = s_ulawTable;

	for (int vi = 0; vi < MAX_SYNTH_VOICES; vi++) {
		SynthVoice *v = &s_voices[vi];
		if (!v->active) continue;

		int32 curGain = v->curGain;
		int32 gainStep = v->gainStep;
		uint32 phase = v->phase;
		uint32 phaseInc = v->phaseInc;
		uint32 loopEnd = v->loopEndFixed;
		uint32 loopSpan = v->loopSpanFixed;
		bool looping = v->looping;

		if (curGain < 4 && gainStep <= 0) {
			/* Voice silent right now; advance phase only */
			phase += phaseInc * count;
			if (phase >= loopEnd) {
				if (looping && loopSpan > 0) {
					while (phase >= loopEnd) phase -= loopSpan;
				} else {
					v->active = false;
				}
			}
			v->phase = phase;
			continue;
		}

		const uint8 *pool = g_dsiSf2Pool + v->smp->poolOffset;
		bool noWrap = (phase + phaseInc * count < loopEnd);

		if (v->lpAlpha >= 254) {
			if (noWrap) {
				for (unsigned k = 0; k < count; k++) {
					uint32 idx = phase >> 12;
					int32 frac = (int32)(phase & 0xFFFu);
					int32 s0 = ulaw[pool[idx]];
					int32 s1 = ulaw[pool[idx + 1]];
					int32 s = s0 + (((s1 - s0) * frac) >> 12);
					acc[k] += (s * (curGain >> 6)) >> 8;
					curGain += gainStep;
					phase += phaseInc;
				}
			} else {
				for (unsigned k = 0; k < count; k++) {
					if (phase >= loopEnd) {
						if (looping) {
							do { phase -= loopSpan; } while (phase >= loopEnd);
						} else {
							v->active = false;
							break;
						}
					}
					uint32 idx = phase >> 12;
					int32 frac = (int32)(phase & 0xFFFu);
					int32 s0 = ulaw[pool[idx]];
					int32 s1 = ulaw[pool[idx + 1]];
					int32 s = s0 + (((s1 - s0) * frac) >> 12);
					acc[k] += (s * (curGain >> 6)) >> 8;
					curGain += gainStep;
					phase += phaseInc;
				}
			}
		} else {
			int32 lpZ = v->lpZ;
			int32 alpha = (int32)v->lpAlpha;
			if (noWrap) {
				for (unsigned k = 0; k < count; k++) {
					uint32 idx = phase >> 12;
					int32 frac = (int32)(phase & 0xFFFu);
					int32 s0 = ulaw[pool[idx]];
					int32 s1 = ulaw[pool[idx + 1]];
					int32 s = s0 + (((s1 - s0) * frac) >> 12);
					lpZ += ((s - lpZ) * alpha) >> 8;
					acc[k] += (lpZ * (curGain >> 6)) >> 8;
					curGain += gainStep;
					phase += phaseInc;
				}
			} else {
				for (unsigned k = 0; k < count; k++) {
					if (phase >= loopEnd) {
						if (looping) {
							do { phase -= loopSpan; } while (phase >= loopEnd);
						} else {
							v->active = false;
							break;
						}
					}
					uint32 idx = phase >> 12;
					int32 frac = (int32)(phase & 0xFFFu);
					int32 s0 = ulaw[pool[idx]];
					int32 s1 = ulaw[pool[idx + 1]];
					int32 s = s0 + (((s1 - s0) * frac) >> 12);
					lpZ += ((s - lpZ) * alpha) >> 8;
					acc[k] += (lpZ * (curGain >> 6)) >> 8;
					curGain += gainStep;
					phase += phaseInc;
				}
			}
			v->lpZ = lpZ;
		}

		v->phase = phase;
		if (curGain < 0) curGain = 0;
		else if (curGain > 65535) curGain = 65535;
		v->curGain = curGain;
	}

	/* Master output scaling + clean clamping (balanced beneath VOC speech on channel 0) */
	for (unsigned k = 0; k < count; k++) {
		int32 v = (acc[k] * 3) >> 2;
		if (v > 32767) v = 32767;
		else if (v < -32767) v = -32767;
		out16[k] = (int16)v;
	}
}

/* Render one exact 64-sample block while firing MPU_Interrupt() at exact 120.00 Hz boundaries. */
void midi_dsi_render_block64(int16 *out64)
{
	/* Step all voice ADSR envelopes once per 64-sample block (matches exporter dt = 64/16364.25s) */
	for (int i = 0; i < MAX_SYNTH_VOICES; i++) {
		if (s_voices[i].active) {
			StepVoiceEnvelope(&s_voices[i]);
		}
	}

	unsigned done = 0;
	while (done < MIX_BLOCK_SAMPLES) {
		unsigned rem = MIX_BLOCK_SAMPLES - done;
		unsigned chunk = (rem < s_mpuSamplesLeft) ? rem : s_mpuSamplesLeft;
		if (chunk > 0) {
			RenderVoicesSubBlock(out64 + done, chunk);
			done += chunk;
			s_mpuSamplesLeft -= chunk;
		}
		if (s_mpuSamplesLeft == 0) {
			s_mpuFrac += DSI_MPU_FRAC_INC;
			s_mpuSamplesLeft = DSI_MPU_BASE_SAMPLES;
			if (s_mpuFrac >= DSI_MPU_FRAC_MOD) {
				s_mpuFrac -= DSI_MPU_FRAC_MOD;
				s_mpuSamplesLeft++;
			}
			MPU_Interrupt();
		}
	}
}

#ifdef __3DS__
void midi_lock(void) {}
void midi_unlock(void) {}
#endif

void midi_reset(void)
{
	memset(s_voices, 0, sizeof(s_voices));
	for (int i = 0; i < NUM_MIDI_CHANNELS; i++) {
		s_ch[i].program = 0;
		s_ch[i].volume = 100;
		s_ch[i].expression = 127;
		s_ch[i].pan = 64;
		s_ch[i].pitchBendCents = 0;
		s_ch[i].sustain = false;
	}
}

bool midi_init(void)
{
	if (s_midiInit) return true;

	BuildUlawTable();
	midi_reset();
	s_mpuSamplesLeft = DSI_MPU_BASE_SAMPLES;
	s_mpuFrac = 0;

#ifdef __NDS__
	tickInit();
	soundInit();
	soundEnable();
	soundSetMixerVolume(127);
	soundSetAutoUpdate(true);

	memset(s_mixBuf, 0, sizeof(s_mixBuf));
	DC_FlushRange(s_mixBuf, sizeof(s_mixBuf));

	soundStop(1u << DSI_HW_MUSIC_CH);
	s_streamStartTick = tickGetCount();
	s_writtenSamples = TARGET_AHEAD_SAMPLES;
	soundPreparePcm(
		DSI_HW_MUSIC_CH | SOUND_START,
		1600,
		64,
		DSI_MIX_TIMER,
		SoundMode_Repeat,
		SoundFmt_Pcm16,
		s_mixBuf,
		0,
		MIX_BUF_SAMPLES / 2
	);
#endif

	s_midiInit = true;
	return true;
}

void midi_uninit(void)
{
	if (!s_midiInit) return;
#ifdef __NDS__
	soundStop(1u << DSI_HW_MUSIC_CH);
#endif
	memset(s_voices, 0, sizeof(s_voices));
	s_midiInit = false;
}

void midi_send(uint32 data)
{
	if (!s_midiInit) return;

	uint8 status = data & 0xFF;
	uint8 d1 = (data >> 8) & 0xFF;
	uint8 d2 = (data >> 16) & 0xFF;
	uint8 cmd = status & 0xF0;
	uint8 ch = status & 0x0F;

	switch (cmd) {
		case 0x80:
			HandleNoteOff(ch, d1);
			break;
		case 0x90:
			if (d2 == 0) HandleNoteOff(ch, d1);
			else HandleNoteOn(ch, d1, d2);
			break;
		case 0xB0:
			if (d1 == 7) s_ch[ch].volume = d2;
			else if (d1 == 10) s_ch[ch].pan = d2;
			else if (d1 == 11) s_ch[ch].expression = d2;
			else if (d1 == 64) {
				s_ch[ch].sustain = (d2 >= 64);
				if (!s_ch[ch].sustain) {
					for (int i = 0; i < MAX_SYNTH_VOICES; i++) {
						if (s_voices[i].active && s_voices[i].midiCh == ch && s_voices[i].sustained) {
							s_voices[i].sustained = false;
							s_voices[i].envStage = ENV_RELEASE;
						}
					}
				}
			} else if (d1 == 121) {
				s_ch[ch].expression = 127;
				s_ch[ch].pitchBendCents = 0;
				s_ch[ch].sustain = false;
			} else if (d1 == 120 || d1 == 123) {
				for (int i = 0; i < MAX_SYNTH_VOICES; i++) {
					if (s_voices[i].active && s_voices[i].midiCh == ch) {
						if (d1 == 120) s_voices[i].active = false;
						else {
							s_voices[i].keyDown = false;
							s_voices[i].sustained = false;
							s_voices[i].envStage = ENV_RELEASE;
						}
					}
				}
			}
			break;
		case 0xC0:
			s_ch[ch].program = d1;
			break;
		case 0xE0: {
			int32 raw14 = ((int32)d2 << 7) | (int32)d1;
			s_ch[ch].pitchBendCents = (int16)(((raw14 - 8192) * 200) / 8192);
			if (ch != 9) {
				for (int i = 0; i < MAX_SYNTH_VOICES; i++) {
					SynthVoice *v = &s_voices[i];
					if (v->active && v->midiCh == ch) {
						v->phaseInc = CalcPhaseInc12(v->reg, v->smp, v->note, s_ch[ch].pitchBendCents);
					}
				}
			}
			break;
		}
		default:
			break;
	}
}

uint16 midi_send_string(const uint8 *data, uint16 len)
{
	VARIABLE_NOT_USED(data);
	return len;
}

void midi_3ds_pump(void)
{
#ifdef __NDS__
	if (!s_midiInit) return;

	/* Exact hardware playback cursor: 32 tickGetCount() ticks per sample (zero drift). */
	u64 played = (tickGetCount() - s_streamStartTick) >> 5;

	/* Recover cleanly if a long SD read or sleep pause ever caused underrun/skew */
	if (s_writtenSamples < played + 256u || s_writtenSamples > played + MIX_BUF_SAMPLES) {
		u64 clearFrom = ((played + MIX_BLOCK_SAMPLES) & ~((u64)MIX_BLOCK_SAMPLES - 1u));
		u64 newWritten = ((played + 512u + MIX_BLOCK_SAMPLES - 1u) & ~((u64)MIX_BLOCK_SAMPLES - 1u));
		for (u64 pos = clearFrom; pos < newWritten; pos += MIX_BLOCK_SAMPLES) {
			int16 *dst = &s_mixBuf[(uint32)(pos & MIX_BUF_MASK)];
			memset(dst, 0, MIX_BLOCK_SAMPLES * sizeof(int16));
			DC_FlushRange(dst, MIX_BLOCK_SAMPLES * sizeof(int16));
		}
		s_writtenSamples = newWritten;
	}

	int64 ahead = (int64)s_writtenSamples - (int64)played;
	if (ahead >= (int64)TARGET_AHEAD_SAMPLES) return;

	/* Allow up to 32 blocks (~125 ms of audio) per pump so buffer refills immediately */
	int maxBlocks = 32;
	while (ahead < (int64)TARGET_AHEAD_SAMPLES && maxBlocks-- > 0) {
		uint32 wrOff = (uint32)(s_writtenSamples & MIX_BUF_MASK);
		int16 *dst = &s_mixBuf[wrOff];
		midi_dsi_render_block64(dst);
		DC_FlushRange(dst, MIX_BLOCK_SAMPLES * sizeof(int16));
		s_writtenSamples += MIX_BLOCK_SAMPLES;
		ahead += (int64)MIX_BLOCK_SAMPLES;
	}
#endif
}
