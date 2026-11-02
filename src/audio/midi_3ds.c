/** @file src/audio/midi_3ds.c Nintendo 3DS hardware MIDI synthesizer driver. */

#include <3ds.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "midi.h"

#define MIDI_AUDIO_CHANNEL 1
#define SAMPLE_RATE 22050
#define BUFFER_SAMPLES 256
#define AUDIO_BUFFER_COUNT 4
#define MAX_VOICES 32
#define NUM_MIDI_CHANNELS 16

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum {
	ENV_OFF = 0,
	ENV_ATTACK,
	ENV_DECAY,
	ENV_SUSTAIN,
	ENV_RELEASE
} EnvelopeStage;

typedef enum {
	WAVE_SINE = 0,
	WAVE_SAW,
	WAVE_SQUARE,
	WAVE_TRIANGLE,
	WAVE_NOISE,
	WAVE_ORGAN,
	WAVE_STRINGS
} WaveformType;

typedef enum {
	DRUM_NONE = 0,
	DRUM_KICK,
	DRUM_SNARE,
	DRUM_HIHAT_CLOSED,
	DRUM_HIHAT_OPEN,
	DRUM_TOM_LOW,
	DRUM_TOM_MID,
	DRUM_TOM_HIGH,
	DRUM_CYMBAL,
	DRUM_RIDE,
	DRUM_CLAP
} DrumType;

typedef struct {
	bool active;
	bool keyHeld;
	uint8 channel;
	uint8 note;
	uint8 velocity;

	uint16 phase;
	uint16 phase_step;

	EnvelopeStage env_stage;
	float env_level;
	float env_attack_rate;
	float env_decay_rate;
	float env_sustain_level;
	float env_release_rate;

	WaveformType waveform;

	bool is_drum;
	DrumType drum_type;
	uint32 drum_sample_count;
	uint32 drum_sample_limit;
	float drum_freq;
	float drum_pitch_decay;
} Voice;

typedef struct {
	uint8 program;
	uint8 volume;
	uint8 expression;
	uint8 pan;
	int16 pitch_bend;
	bool sustain;
} ChannelState;

static bool s_midiInitialized = false;
static ndspWaveBuf s_waveBuf[AUDIO_BUFFER_COUNT];
static int16 *s_pcmBuffer[AUDIO_BUFFER_COUNT] = { NULL };

static Voice s_voices[MAX_VOICES];
static ChannelState s_channels[NUM_MIDI_CHANNELS];

static int16 s_sineTable[512];
static uint32 s_noiseSeed = 0x12345678;

static uint32 NextRandom(void)
{
	s_noiseSeed = s_noiseSeed * 1664525u + 1013904223u;
	return s_noiseSeed;
}

static void InitSineTable(void)
{
	for (int i = 0; i < 512; i++) {
		s_sineTable[i] = (int16)(sinf((float)i * 2.0f * (float)M_PI / 512.0f) * 32760.0f);
	}
}

static uint16 NoteToPhaseStep(uint8 note, int16 pitch_bend)
{
	float bend_semitones = ((float)pitch_bend * 2.0f) / 8192.0f;
	float freq = 440.0f * powf(2.0f, ((float)note - 69.0f + bend_semitones) / 12.0f);
	if (freq < 10.0f) freq = 10.0f;
	if (freq > 11000.0f) freq = 11000.0f;
	return (uint16)((freq * 65536.0f) / (float)SAMPLE_RATE);
}

static void Voice_TriggerDrum(Voice *v, uint8 note, uint8 velocity)
{
	v->active = true;
	v->keyHeld = true;
	v->channel = 9;
	v->note = note;
	v->velocity = velocity;
	v->phase = 0;
	v->env_stage = ENV_DECAY;
	v->env_level = 1.0f;
	v->env_sustain_level = 0.0f;
	v->env_attack_rate = 1.0f;
	v->env_release_rate = 1.0f / (SAMPLE_RATE * 0.05f);
	v->is_drum = true;
	v->drum_sample_count = 0;
	v->drum_pitch_decay = 1.0f;

	switch (note) {
		case 35: /* Acoustic Bass Drum */
		case 36: /* Bass Drum 1 */
			v->drum_type = DRUM_KICK;
			v->drum_freq = 150.0f;
			v->drum_pitch_decay = 0.9982f;
			v->drum_sample_limit = (SAMPLE_RATE * 3) / 10; /* 0.3s */
			v->env_decay_rate = 1.0f / (float)v->drum_sample_limit;
			v->waveform = WAVE_SINE;
			break;

		case 38: /* Acoustic Snare */
		case 40: /* Electric Snare */
			v->drum_type = DRUM_SNARE;
			v->drum_freq = 180.0f;
			v->drum_pitch_decay = 0.999f;
			v->drum_sample_limit = (SAMPLE_RATE * 2) / 10; /* 0.2s */
			v->env_decay_rate = 1.0f / (float)v->drum_sample_limit;
			v->waveform = WAVE_NOISE;
			break;

		case 42: /* Closed Hi-Hat */
		case 44: /* Pedal Hi-Hat */
			v->drum_type = DRUM_HIHAT_CLOSED;
			v->drum_freq = 3000.0f;
			v->drum_sample_limit = SAMPLE_RATE / 20; /* 0.05s */
			v->env_decay_rate = 1.0f / (float)v->drum_sample_limit;
			v->waveform = WAVE_NOISE;
			break;

		case 46: /* Open Hi-Hat */
			v->drum_type = DRUM_HIHAT_OPEN;
			v->drum_freq = 2500.0f;
			v->drum_sample_limit = (SAMPLE_RATE * 35) / 100; /* 0.35s */
			v->env_decay_rate = 1.0f / (float)v->drum_sample_limit;
			v->waveform = WAVE_NOISE;
			break;

		case 41: /* Low Floor Tom */
		case 43: /* High Floor Tom */
			v->drum_type = DRUM_TOM_LOW;
			v->drum_freq = 95.0f;
			v->drum_pitch_decay = 0.9992f;
			v->drum_sample_limit = (SAMPLE_RATE * 25) / 100;
			v->env_decay_rate = 1.0f / (float)v->drum_sample_limit;
			v->waveform = WAVE_SINE;
			break;

		case 45: /* Low Tom */
		case 47: /* Low-Mid Tom */
			v->drum_type = DRUM_TOM_MID;
			v->drum_freq = 135.0f;
			v->drum_pitch_decay = 0.9992f;
			v->drum_sample_limit = (SAMPLE_RATE * 25) / 100;
			v->env_decay_rate = 1.0f / (float)v->drum_sample_limit;
			v->waveform = WAVE_SINE;
			break;

		case 48: /* Hi-Mid Tom */
		case 50: /* High Tom */
			v->drum_type = DRUM_TOM_HIGH;
			v->drum_freq = 185.0f;
			v->drum_pitch_decay = 0.9992f;
			v->drum_sample_limit = (SAMPLE_RATE * 25) / 100;
			v->env_decay_rate = 1.0f / (float)v->drum_sample_limit;
			v->waveform = WAVE_SINE;
			break;

		case 49: /* Crash Cymbal 1 */
		case 57: /* Crash Cymbal 2 */
			v->drum_type = DRUM_CYMBAL;
			v->drum_freq = 2000.0f;
			v->drum_sample_limit = (SAMPLE_RATE * 11) / 10; /* 1.1s */
			v->env_decay_rate = 1.0f / (float)v->drum_sample_limit;
			v->waveform = WAVE_NOISE;
			break;

		case 51: /* Ride Cymbal 1 */
		case 59: /* Ride Cymbal 2 */
			v->drum_type = DRUM_RIDE;
			v->drum_freq = 2200.0f;
			v->drum_sample_limit = (SAMPLE_RATE * 6) / 10; /* 0.6s */
			v->env_decay_rate = 1.0f / (float)v->drum_sample_limit;
			v->waveform = WAVE_NOISE;
			break;

		case 37: /* Side Stick */
		case 39: /* Hand Clap */
		default:
			v->drum_type = DRUM_CLAP;
			v->drum_freq = 800.0f;
			v->drum_sample_limit = SAMPLE_RATE / 12; /* ~0.08s */
			v->env_decay_rate = 1.0f / (float)v->drum_sample_limit;
			v->waveform = WAVE_NOISE;
			break;
	}

	v->phase_step = (uint16)((v->drum_freq * 65536.0f) / (float)SAMPLE_RATE);
}

static void Voice_TriggerMelodic(Voice *v, uint8 chan, uint8 note, uint8 velocity)
{
	v->active = true;
	v->keyHeld = true;
	v->channel = chan;
	v->note = note;
	v->velocity = velocity;
	v->phase = 0;
	v->is_drum = false;
	v->drum_type = DRUM_NONE;

	v->phase_step = NoteToPhaseStep(note, s_channels[chan].pitch_bend);

	uint8 prog = s_channels[chan].program;

	/* Configure envelope and waveform based on General MIDI patch category */
	if (prog < 8) {
		/* Piano: fast attack, medium decay, modest sustain */
		v->waveform = WAVE_TRIANGLE;
		v->env_attack_rate = 1.0f / (SAMPLE_RATE * 0.003f);
		v->env_decay_rate = 0.70f / (SAMPLE_RATE * 0.25f);
		v->env_sustain_level = 0.30f;
		v->env_release_rate = 1.0f / (SAMPLE_RATE * 0.10f);
	} else if (prog < 16) {
		/* Chromatic Percussion (Vibes/Marimba): sharp attack, percussive decay to 0 */
		v->waveform = WAVE_SINE;
		v->env_attack_rate = 1.0f / (SAMPLE_RATE * 0.002f);
		v->env_decay_rate = 1.0f / (SAMPLE_RATE * 0.35f);
		v->env_sustain_level = 0.0f;
		v->env_release_rate = 1.0f / (SAMPLE_RATE * 0.08f);
	} else if (prog < 24) {
		/* Organ: instantaneous attack, clean sustain */
		v->waveform = WAVE_ORGAN;
		v->env_attack_rate = 1.0f / (SAMPLE_RATE * 0.005f);
		v->env_decay_rate = 0.30f / (SAMPLE_RATE * 0.10f);
		v->env_sustain_level = 0.70f;
		v->env_release_rate = 1.0f / (SAMPLE_RATE * 0.05f);
	} else if (prog < 32) {
		/* Guitar: fast pluck, decaying sustain */
		v->waveform = WAVE_SAW;
		v->env_attack_rate = 1.0f / (SAMPLE_RATE * 0.002f);
		v->env_decay_rate = 0.80f / (SAMPLE_RATE * 0.20f);
		v->env_sustain_level = 0.20f;
		v->env_release_rate = 1.0f / (SAMPLE_RATE * 0.08f);
	} else if (prog < 40) {
		/* Bass: punchy low end, steady sustain */
		v->waveform = WAVE_TRIANGLE;
		v->env_attack_rate = 1.0f / (SAMPLE_RATE * 0.004f);
		v->env_decay_rate = 0.50f / (SAMPLE_RATE * 0.15f);
		v->env_sustain_level = 0.50f;
		v->env_release_rate = 1.0f / (SAMPLE_RATE * 0.06f);
	} else if (prog < 56) {
		/* Strings / Ensemble: soft attack, natural sustain */
		v->waveform = WAVE_STRINGS;
		v->env_attack_rate = 1.0f / (SAMPLE_RATE * 0.025f);
		v->env_decay_rate = 0.25f / (SAMPLE_RATE * 0.15f);
		v->env_sustain_level = 0.65f;
		v->env_release_rate = 1.0f / (SAMPLE_RATE * 0.10f);
	} else if (prog < 64) {
		/* Brass: crisp attack, bright sustain */
		v->waveform = WAVE_SAW;
		v->env_attack_rate = 1.0f / (SAMPLE_RATE * 0.010f);
		v->env_decay_rate = 0.30f / (SAMPLE_RATE * 0.12f);
		v->env_sustain_level = 0.65f;
		v->env_release_rate = 1.0f / (SAMPLE_RATE * 0.08f);
	} else if (prog < 80) {
		/* Reed / Pipe (Flute/Sax): mellow square/sine tone */
		v->waveform = (prog < 72) ? WAVE_SQUARE : WAVE_SINE;
		v->env_attack_rate = 1.0f / (SAMPLE_RATE * 0.015f);
		v->env_decay_rate = 0.30f / (SAMPLE_RATE * 0.12f);
		v->env_sustain_level = 0.65f;
		v->env_release_rate = 1.0f / (SAMPLE_RATE * 0.08f);
	} else {
		/* Synth Lead / Pad: bright saw/square */
		v->waveform = WAVE_SAW;
		v->env_attack_rate = 1.0f / (SAMPLE_RATE * 0.008f);
		v->env_decay_rate = 0.35f / (SAMPLE_RATE * 0.15f);
		v->env_sustain_level = 0.60f;
		v->env_release_rate = 1.0f / (SAMPLE_RATE * 0.08f);
	}

	v->env_stage = ENV_ATTACK;
	v->env_level = 0.0f;
}

static Voice *AllocateVoice(void)
{
	/* 1. Look for inactive voice */
	for (int i = 0; i < MAX_VOICES; i++) {
		if (!s_voices[i].active) return &s_voices[i];
	}

	/* 2. Look for voice in release stage */
	for (int i = 0; i < MAX_VOICES; i++) {
		if (s_voices[i].env_stage == ENV_RELEASE) return &s_voices[i];
	}

	/* 3. Steal lowest volume voice */
	int stealIdx = 0;
	float lowest = 1000.0f;
	for (int i = 0; i < MAX_VOICES; i++) {
		if (s_voices[i].env_level < lowest) {
			lowest = s_voices[i].env_level;
			stealIdx = i;
		}
	}
	return &s_voices[stealIdx];
}

static void midi_3ds_render_buffer(int16 *buf, int num_samples)
{
	memset(buf, 0, num_samples * sizeof(int16) * 2);

	for (int vIdx = 0; vIdx < MAX_VOICES; vIdx++) {
		Voice *v = &s_voices[vIdx];
		if (!v->active) continue;

		uint8 chan = v->channel;
		float chanVol = (float)s_channels[chan].volume / 127.0f;
		float chanExp = (float)s_channels[chan].expression / 127.0f;
		float vel = (float)v->velocity / 127.0f;
		float pan = (float)s_channels[chan].pan / 127.0f;
		float leftGain  = (1.0f - pan) * 1.4f;
		float rightGain = pan * 1.4f;
		if (leftGain > 1.0f) leftGain = 1.0f;
		if (rightGain > 1.0f) rightGain = 1.0f;

		float masterGain = chanVol * chanExp * vel * 0.42f;

		for (int s = 0; s < num_samples; s++) {
			/* Envelope processing */
			if (v->env_stage == ENV_ATTACK) {
				v->env_level += v->env_attack_rate;
				if (v->env_level >= 1.0f) {
					v->env_level = 1.0f;
					v->env_stage = ENV_DECAY;
				}
			} else if (v->env_stage == ENV_DECAY) {
				v->env_level -= v->env_decay_rate;
				if (v->env_level <= v->env_sustain_level) {
					v->env_level = v->env_sustain_level;
					v->env_stage = (v->env_sustain_level > 0.001f) ? ENV_SUSTAIN : ENV_OFF;
					if (v->env_stage == ENV_OFF) {
						v->active = false;
						break;
					}
				}
			} else if (v->env_stage == ENV_RELEASE) {
				v->env_level -= v->env_release_rate;
				if (v->env_level <= 0.001f) {
					v->env_level = 0.0f;
					v->env_stage = ENV_OFF;
					v->active = false;
					break;
				}
			}

			/* Drum pitch slide */
			if (v->is_drum) {
				v->drum_sample_count++;
				if (v->drum_sample_count >= v->drum_sample_limit) {
					v->active = false;
					break;
				}
				if (v->drum_pitch_decay < 1.0f) {
					v->drum_freq *= v->drum_pitch_decay;
					v->phase_step = (uint16)((v->drum_freq * 65536.0f) / (float)SAMPLE_RATE);
				}
			}

			/* Waveform evaluation */
			int32 rawSample = 0;
			uint16 phase = v->phase;

			switch (v->waveform) {
				case WAVE_SINE:
					rawSample = s_sineTable[phase >> 7];
					break;

				case WAVE_SAW:
					rawSample = (int32)phase - 32768;
					break;

				case WAVE_SQUARE:
					rawSample = (phase < 32768) ? 24000 : -24000;
					break;

				case WAVE_TRIANGLE:
					if (phase < 32768) {
						rawSample = -32768 + ((int32)phase * 2);
					} else {
						rawSample = 32767 - (((int32)phase - 32768) * 2);
					}
					break;

				case WAVE_ORGAN: {
					/* Fundamental + 2nd harmonic */
					int32 s1 = s_sineTable[phase >> 7];
					int32 s2 = s_sineTable[((uint16)(phase * 2)) >> 7];
					rawSample = (s1 * 3 + s2 * 2) / 5;
				} break;

				case WAVE_STRINGS: {
					/* Sawtooth detuned chorus feel */
					int32 saw1 = (int32)phase - 32768;
					uint16 phase2 = phase + (phase >> 5);
					int32 saw2 = (int32)phase2 - 32768;
					rawSample = (saw1 + saw2) / 2;
				} break;

				case WAVE_NOISE:
				default: {
					uint32 rnd = NextRandom();
					rawSample = (int16)(rnd >> 16);
					/* For snare, blend a little body tone */
					if (v->drum_type == DRUM_SNARE) {
						int32 body = s_sineTable[phase >> 7];
						rawSample = (rawSample * 3 + body * 2) / 5;
					}
				} break;
			}

			v->phase += v->phase_step;

			float currentGain = masterGain * v->env_level;
			int32 sLeft  = (int32)((float)rawSample * currentGain * leftGain);
			int32 sRight = (int32)((float)rawSample * currentGain * rightGain);

			int outIdx = s * 2;
			int32 lVal = (int32)buf[outIdx] + sLeft;
			int32 rVal = (int32)buf[outIdx + 1] + sRight;
			if (lVal < -32767) lVal = -32767; else if (lVal > 32767) lVal = 32767;
			if (rVal < -32767) rVal = -32767; else if (rVal > 32767) rVal = 32767;
			buf[outIdx]     = (int16)lVal;
			buf[outIdx + 1] = (int16)rVal;
		}
	}
}

void midi_3ds_pump(void)
{
	if (!s_midiInitialized) return;

	for (int b = 0; b < AUDIO_BUFFER_COUNT; b++) {
		if (s_waveBuf[b].status == NDSP_WBUF_DONE || s_waveBuf[b].status == NDSP_WBUF_FREE) {
			midi_3ds_render_buffer(s_pcmBuffer[b], BUFFER_SAMPLES);
			DSP_FlushDataCache(s_pcmBuffer[b], BUFFER_SAMPLES * sizeof(int16) * 2);
			s_waveBuf[b].status = NDSP_WBUF_FREE;
			ndspChnWaveBufAdd(MIDI_AUDIO_CHANNEL, &s_waveBuf[b]);
		}
	}
}

bool midi_init(void)
{
	if (s_midiInitialized) return true;

	Result rc = ndspInit();
	if (R_FAILED(rc)) {
		return false;
	}

	InitSineTable();

	memset(s_voices, 0, sizeof(s_voices));
	for (int c = 0; c < NUM_MIDI_CHANNELS; c++) {
		s_channels[c].program = 0;
		s_channels[c].volume = 100;
		s_channels[c].expression = 127;
		s_channels[c].pan = 64;
		s_channels[c].pitch_bend = 0;
		s_channels[c].sustain = false;
	}

	for (int b = 0; b < AUDIO_BUFFER_COUNT; b++) {
		s_pcmBuffer[b] = (int16 *)linearAlloc(BUFFER_SAMPLES * sizeof(int16) * 2);
		if (s_pcmBuffer[b] == NULL) {
			for (int allocated = 0; allocated < b; allocated++) {
				linearFree(s_pcmBuffer[allocated]);
				s_pcmBuffer[allocated] = NULL;
			}
			ndspExit();
			return false;
		}
		memset(s_pcmBuffer[b], 0, BUFFER_SAMPLES * sizeof(int16) * 2);
	}

	ndspChnReset(MIDI_AUDIO_CHANNEL);
	ndspChnSetInterp(MIDI_AUDIO_CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetRate(MIDI_AUDIO_CHANNEL, (float)SAMPLE_RATE);
	ndspChnSetFormat(MIDI_AUDIO_CHANNEL, NDSP_FORMAT_STEREO_PCM16);

	float mix[12];
	memset(mix, 0, sizeof(mix));
	mix[0] = 0.90f; // Left to Left
	mix[1] = 0.90f; // Right to Right
	ndspChnSetMix(MIDI_AUDIO_CHANNEL, mix);

	for (int b = 0; b < AUDIO_BUFFER_COUNT; b++) {
		memset(&s_waveBuf[b], 0, sizeof(ndspWaveBuf));
		s_waveBuf[b].data_vaddr = s_pcmBuffer[b];
		s_waveBuf[b].nsamples = BUFFER_SAMPLES;
		s_waveBuf[b].status = NDSP_WBUF_FREE;
	}

	s_midiInitialized = true;

	midi_3ds_pump();

	return true;
}

void midi_uninit(void)
{
	if (!s_midiInitialized) return;
	s_midiInitialized = false;

	ndspChnReset(MIDI_AUDIO_CHANNEL);

	for (int b = 0; b < AUDIO_BUFFER_COUNT; b++) {
		if (s_pcmBuffer[b] != NULL) {
			linearFree(s_pcmBuffer[b]);
			s_pcmBuffer[b] = NULL;
		}
	}

	ndspExit();
}

void midi_send(uint32 data)
{
	if (!s_midiInitialized) return;

	uint8 status = (uint8)(data & 0xFF);
	uint8 chan   = status & 0x0F;
	uint8 cmd    = status & 0xF0;
	uint8 d1     = (uint8)((data >> 8) & 0xFF);
	uint8 d2     = (uint8)((data >> 16) & 0xFF);

	switch (cmd) {
		case 0x90: { /* Note On */
			if (d2 == 0) {
				/* Velocity 0 = Note Off */
				for (int i = 0; i < MAX_VOICES; i++) {
					if (s_voices[i].active && s_voices[i].channel == chan && s_voices[i].note == d1) {
						s_voices[i].keyHeld = false;
						if (!s_channels[chan].sustain) {
							s_voices[i].env_stage = ENV_RELEASE;
						}
					}
				}
			} else {
				for (int i = 0; i < MAX_VOICES; i++) {
					if (s_voices[i].active && s_voices[i].channel == chan && s_voices[i].note == d1) {
						s_voices[i].env_stage = ENV_RELEASE;
					}
				}
				Voice *v = AllocateVoice();
				if (chan == 9) {
					Voice_TriggerDrum(v, d1, d2);
				} else {
					Voice_TriggerMelodic(v, chan, d1, d2);
				}
			}
		} break;

		case 0x80: { /* Note Off */
			for (int i = 0; i < MAX_VOICES; i++) {
				if (s_voices[i].active && s_voices[i].channel == chan && s_voices[i].note == d1) {
					s_voices[i].keyHeld = false;
					if (!s_channels[chan].sustain) {
						s_voices[i].env_stage = ENV_RELEASE;
					}
				}
			}
		} break;

		case 0xC0: { /* Program Change */
			s_channels[chan].program = d1;
		} break;

		case 0xB0: { /* Control Change */
			switch (d1) {
				case 7: /* Main Volume */
					s_channels[chan].volume = d2;
					break;
				case 10: /* Pan */
					s_channels[chan].pan = d2;
					break;
				case 11: /* Expression */
					s_channels[chan].expression = d2;
					break;
				case 64: /* Sustain Pedal */
					s_channels[chan].sustain = (d2 >= 64);
					if (!s_channels[chan].sustain) {
						for (int i = 0; i < MAX_VOICES; i++) {
							if (s_voices[i].active && s_voices[i].channel == chan && !s_voices[i].keyHeld) {
								s_voices[i].env_stage = ENV_RELEASE;
							}
						}
					}
					break;
				case 120: /* All Sound Off */
				case 123: /* All Notes Off */
					for (int i = 0; i < MAX_VOICES; i++) {
						if (s_voices[i].channel == chan) {
							s_voices[i].active = false;
							s_voices[i].env_stage = ENV_OFF;
						}
					}
					break;
				case 121: /* Reset All Controllers */
					s_channels[chan].sustain = false;
					s_channels[chan].pitch_bend = 0;
					s_channels[chan].expression = 127;
					for (int i = 0; i < MAX_VOICES; i++) {
						if (s_voices[i].channel == chan && !s_voices[i].keyHeld) {
							s_voices[i].env_stage = ENV_RELEASE;
						}
					}
					break;
				default:
					break;
			}
		} break;

		case 0xE0: { /* Pitch Bend */
			int16 bend = (int16)(((uint16)d2 << 7) | (uint16)d1) - 8192;
			s_channels[chan].pitch_bend = bend;
			for (int i = 0; i < MAX_VOICES; i++) {
				if (s_voices[i].active && s_voices[i].channel == chan && !s_voices[i].is_drum) {
					s_voices[i].phase_step = NoteToPhaseStep(s_voices[i].note, bend);
				}
			}
		} break;

		default:
			break;
	}
}

uint16 midi_send_string(const uint8 *data, uint16 len)
{
	VARIABLE_NOT_USED(data);
	return len;
}

void midi_reset(void)
{
	for (int i = 0; i < MAX_VOICES; i++) {
		s_voices[i].active = false;
		s_voices[i].env_stage = ENV_OFF;
	}
	for (int c = 0; c < NUM_MIDI_CHANNELS; c++) {
		s_channels[c].volume = 100;
		s_channels[c].expression = 127;
		s_channels[c].pan = 64;
		s_channels[c].pitch_bend = 0;
		s_channels[c].sustain = false;
	}
}
