/** @file scripts/export_dsi_sf2.c
 * Host build tool: uses src/audio/tsf.h directly to extract every exact gm.sf2
 * keyzone, sample, ADSR envelope, attenuation, and filter cutoff used by Dune II
 * into src/audio/midi_dsi_bank.{h,c} (G.711 u-law 14-bit dynamic range PCM).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define TSF_IMPLEMENTATION
#include "../src/audio/tsf.h"

#define OUT_MIX_SR 16364.25
#define BLOCK_SAMPLES 64.0
#define MAX_SAMPLE_SR 16364
#define MAX_ONESHOT_SAMPLES 32768

/* Standard ITU G.711 u-law encoder (14-bit dynamic range in 8 bits). */
static uint8_t linear16_to_ulaw(int16_t pcm)
{
	const int BIAS = 0x84;
	const int CLIP = 32635;
	int sign = (pcm < 0) ? 0x80 : 0x00;
	int mag = (pcm < 0) ? -(int)pcm : (int)pcm;
	if (mag > CLIP) mag = CLIP;
	mag += BIAS;

	int exp = 7;
	for (int mask = 0x4000; (mag & mask) == 0 && exp > 0; exp--, mask >>= 1) {}
	int mant = (mag >> (exp + 3)) & 0x0F;
	return (uint8_t)(~(sign | (exp << 4) | mant));
}

typedef struct {
	unsigned int origOffset;
	unsigned int origEnd;
	unsigned int origLoopStart;
	unsigned int origLoopEnd;
	int loopMode;
	unsigned int origSr;

	uint32_t poolOffset;
	uint32_t length;
	uint32_t loopStart;
	uint32_t loopEnd;
	uint32_t sampleRate;
	uint8_t looped;
} ExportSample;

typedef struct {
	uint8_t lokey;
	uint8_t hikey;
	uint16_t sampleIdx;
	int8_t pitchKeycenter;
	int8_t pitchKeytrack;
	int16_t tuneCents;
	uint16_t gain256;
	uint16_t attackInc;
	uint16_t holdBlocks;
	uint16_t decayMul;
	uint16_t sustain16;
	uint16_t releaseMul;
	uint8_t lpAlpha;
	uint8_t group;
	uint8_t loopMode;
	uint8_t pad;
} ExportRegion;

typedef struct {
	uint16_t regionStart;
	uint16_t regionCount;
} ExportPreset;

static ExportSample g_samples[512];
static int g_sampleCount = 0;

static ExportRegion g_regions[2048];
static int g_regionCount = 0;

static ExportPreset g_presets[129];

static uint8_t *g_pool = NULL;
static size_t g_poolLen = 0;
static size_t g_poolCap = 0;

static void pool_push(uint8_t b)
{
	if (g_poolLen >= g_poolCap) {
		g_poolCap = g_poolCap ? g_poolCap * 2 : (1024 * 1024);
		g_pool = (uint8_t *)realloc(g_pool, g_poolCap);
	}
	g_pool[g_poolLen++] = b;
}

static uint16_t get_or_add_sample(const tsf *f, const struct tsf_region *r)
{
	int looped = (r->loop_mode != TSF_LOOPMODE_NONE && r->loop_end > r->loop_start + 8) ? 1 : 0;
	for (int i = 0; i < g_sampleCount; i++) {
		ExportSample *e = &g_samples[i];
		if (e->origOffset == r->offset && e->origEnd == r->end &&
		    e->origLoopStart == r->loop_start && e->origLoopEnd == r->loop_end &&
		    e->loopMode == r->loop_mode && e->origSr == r->sample_rate) {
			return (uint16_t)i;
		}
	}

	const short *src = f->fontSamples + r->offset;
	unsigned int rawEnd = (r->end > r->offset) ? (r->end - r->offset) : 0;
	unsigned int rawLs = (r->loop_start >= r->offset) ? (r->loop_start - r->offset) : 0;
	unsigned int rawLe = (r->loop_end >= r->offset) ? (r->loop_end - r->offset) : 0;

	if (rawEnd < 4) rawEnd = 4;
	if (looped) {
		if (rawLe >= rawEnd) rawLe = rawEnd - 1;
		if (rawLs >= rawLe) looped = 0;
	}

	/* For continuous loops, audio never plays past loop_end -> trim unused post-loop tail */
	unsigned int useLen = rawEnd;
	if (looped && r->loop_mode == TSF_LOOPMODE_CONTINUOUS) {
		useLen = rawLe + 1;
	}

	unsigned int sr = r->sample_rate ? r->sample_rate : 22050;
	double outSr = (sr > MAX_SAMPLE_SR) ? (double)MAX_SAMPLE_SR : (double)sr;
	double ratio = (double)sr / outSr;

	/* Cap extremely long one-shot tails gracefully */
	if (!looped && (unsigned int)(useLen / ratio) > MAX_ONESHOT_SAMPLES) {
		useLen = (unsigned int)(MAX_ONESHOT_SAMPLES * ratio);
	}

	uint32_t outLen = (uint32_t)llround((double)useLen / ratio);
	if (outLen < 8) outLen = 8;

	uint32_t outLs = 0, outLe = 0;
	if (looped) {
		outLs = (uint32_t)llround((double)rawLs / ratio);
		outLe = (uint32_t)llround((double)rawLe / ratio);
		if (outLe >= outLen) outLe = outLen - 1;
		if (outLs >= outLe) {
			outLs = 0;
			outLe = outLen - 1;
		}
		/* Preserve exact loop period pitch by recomputing exact outSr from loop span */
		if (rawLe > rawLs && outLe > outLs) {
			outSr = (double)sr * (double)(outLe - outLs) / (double)(rawLe - rawLs);
			ratio = (double)sr / outSr;
		}
	}

	uint32_t poolOff = (uint32_t)g_poolLen;

	/* Anti-aliased resample (3-tap box/tent filter when downsampling) + u-law encode */
	int16_t loopStartVal = 0;
	for (uint32_t i = 0; i < outLen; i++) {
		double pos;
		if (looped && i >= outLs && i <= outLe) {
			double t = (double)(i - outLs) / (double)(outLe - outLs);
			pos = (double)rawLs + t * (double)(rawLe - rawLs);
		} else {
			pos = (double)i * ratio;
		}

		double v = 0.0;
		if (ratio > 1.35) {
			/* Simple 3-tap lowpass anti-aliasing filter around pos */
			double half = ratio * 0.5;
			double p[3] = { pos - half * 0.6, pos, pos + half * 0.6 };
			double w[3] = { 0.25, 0.50, 0.25 };
			for (int k = 0; k < 3; k++) {
				double pk = p[k];
				if (pk < 0.0) pk = 0.0;
				if (pk >= (double)(rawEnd - 1)) pk = (double)(rawEnd - 1);
				unsigned int idx = (unsigned int)pk;
				double frac = pk - (double)idx;
				double s0 = (double)src[idx];
				double s1 = (idx + 1 < rawEnd) ? (double)src[idx + 1] : s0;
				v += w[k] * (s0 + (s1 - s0) * frac);
			}
		} else {
			if (pos < 0.0) pos = 0.0;
			if (pos >= (double)(rawEnd - 1)) pos = (double)(rawEnd - 1);
			unsigned int idx = (unsigned int)pos;
			double frac = pos - (double)idx;
			double s0 = (double)src[idx];
			double s1 = (idx + 1 < rawEnd) ? (double)src[idx + 1] : s0;
			v = s0 + (s1 - s0) * frac;
		}

		/* Gentle fade-out on last 16 samples of one-shot to avoid end clicks */
		if (!looped && outLen > 32 && i >= outLen - 16) {
			v *= (double)(outLen - 1 - i) / 16.0;
		}

		int iv = (int)llround(v);
		if (iv < -32767) iv = -32767;
		if (iv > 32767) iv = 32767;
		if (looped && i == outLs) loopStartVal = (int16_t)iv;
		pool_push(linear16_to_ulaw((int16_t)iv));
	}

	/* Guard sample at index [outLen] for branchless linear interpolation wrap */
	pool_push(linear16_to_ulaw(looped ? loopStartVal : 0));

	int idx = g_sampleCount++;
	ExportSample *e = &g_samples[idx];
	e->origOffset = r->offset;
	e->origEnd = r->end;
	e->origLoopStart = r->loop_start;
	e->origLoopEnd = r->loop_end;
	e->loopMode = r->loop_mode;
	e->origSr = r->sample_rate;
	e->poolOffset = poolOff;
	e->length = outLen;
	e->loopStart = outLs;
	e->loopEnd = outLe;
	e->sampleRate = (uint32_t)llround(outSr);
	e->looped = (uint8_t)looped;
	return (uint16_t)idx;
}

static uint16_t sec_to_exp_mul(double sec)
{
	if (sec <= 0.002) return 0; /* instant */
	double dt = BLOCK_SAMPLES / OUT_MIX_SR;
	/* Exact tsf.h exponential slope (-9.226 over 'sec' seconds) */
	double mul = exp(-9.226 * dt / sec);
	int im = (int)llround(mul * 65536.0);
	if (im < 0) im = 0;
	if (im > 65535) im = 65535;
	return (uint16_t)im;
}

static uint8_t filter_fc_to_alpha(int initialFilterFc)
{
	if (initialFilterFc >= 13000) return 255; /* wide open / bypass */
	double hz = 8.176 * pow(2.0, (double)initialFilterFc / 1200.0);
	if (hz >= OUT_MIX_SR * 0.42) return 255;
	if (hz < 80.0) hz = 80.0;
	/* 1-pole lowpass: y += alpha * (x - y), alpha = 1 - exp(-2*pi*fc/fs) */
	double a = 1.0 - exp(-2.0 * 3.14159265358979 * hz / OUT_MIX_SR);
	int ia = (int)llround(a * 255.0);
	if (ia < 8) ia = 8;
	if (ia > 254) ia = 254;
	return (uint8_t)ia;
}

static const struct tsf_region *find_best_region(const tsf *f, int presetIdx, int key)
{
	const struct tsf_preset *p = &f->presets[presetIdx];
	const struct tsf_region *best = NULL;
	for (int i = 0; i < p->regionNum; i++) {
		const struct tsf_region *r = &p->regions[i];
		if (key < r->lokey || key > r->hikey) continue;
		if (100 < r->lovel || 100 > r->hivel) continue;
		if (!best || r->attenuation < best->attenuation) best = r;
	}
	return best;
}

static void add_region_entry(const tsf *f, const struct tsf_region *r, uint8_t lokey, uint8_t hikey)
{
	ExportRegion *er = &g_regions[g_regionCount++];
	er->lokey = lokey;
	er->hikey = hikey;
	er->sampleIdx = get_or_add_sample(f, r);
	er->pitchKeycenter = (int8_t)r->pitch_keycenter;
	er->pitchKeytrack = (int8_t)r->pitch_keytrack;
	er->tuneCents = (int16_t)(r->transpose * 100 + r->tune);

	double gain = pow(10.0, -(double)r->attenuation / 20.0);
	int g256 = (int)llround(gain * 256.0);
	if (g256 < 1) g256 = 1;
	if (g256 > 4096) g256 = 4096;
	er->gain256 = (uint16_t)g256;

	double dt = BLOCK_SAMPLES / OUT_MIX_SR;
	int midKey = ((int)lokey + (int)hikey) / 2;

	double holdSec = (double)r->ampenv.hold;
	if (r->ampenv.keynumToHold) {
		double tc = (double)r->ampenv.hold + (double)(60 - midKey) * (double)r->ampenv.keynumToHold;
		holdSec = (tc < -11950.0) ? 0.0 : pow(2.0, tc / 1200.0);
	}
	double decaySec = (double)r->ampenv.decay;
	if (r->ampenv.keynumToDecay) {
		double tc = (double)r->ampenv.decay + (double)(60 - midKey) * (double)r->ampenv.keynumToDecay;
		decaySec = (tc < -11950.0) ? 0.0 : pow(2.0, tc / 1200.0);
	}

	if (r->ampenv.attack <= dt) {
		er->attackInc = 65535;
	} else {
		int inc = (int)llround(65535.0 * dt / (double)r->ampenv.attack);
		if (inc < 1) inc = 1;
		if (inc > 65535) inc = 65535;
		er->attackInc = (uint16_t)inc;
	}

	int hb = (int)llround(holdSec / dt);
	if (hb < 0) hb = 0;
	if (hb > 65535) hb = 65535;
	er->holdBlocks = (uint16_t)hb;

	er->decayMul = sec_to_exp_mul(decaySec);
	int sus = (int)llround((double)r->ampenv.sustain * 65535.0);
	if (sus < 0) sus = 0;
	if (sus > 65535) sus = 65535;
	er->sustain16 = (uint16_t)sus;
	er->releaseMul = sec_to_exp_mul((double)r->ampenv.release);

	int effFc = r->initialFilterFc;
	if (r->modEnvToFilterFc > 0) effFc += r->modEnvToFilterFc;
	er->lpAlpha = filter_fc_to_alpha(effFc);
	er->group = (uint8_t)(r->group & 0xFF);
	er->loopMode = (uint8_t)r->loop_mode;
	er->pad = 0;
}

typedef struct {
	int prog;
	int minKey;
	int maxKey;
} ProgRange;

int main(void)
{
	tsf *f = tsf_load_filename("romfs/gm.sf2");
	if (!f) {
		fprintf(stderr, "Failed to load romfs/gm.sf2\n");
		return 1;
	}

	/* Exact 53 melodic GM programs used across all 21 Dune II .C55 tracks, with exact note ranges */
	static const ProgRange usedProgs[] = {
		{  0, 36, 84}, { 10, 48, 84}, { 11, 60,104}, { 12, 55, 76}, { 14, 19,102},
		{ 29, 24, 56}, { 30, 24, 72}, { 32, 24, 48}, { 33, 20, 54}, { 37, 20, 48},
		{ 38, 24, 52}, { 39, 16, 48}, { 42, 24, 48}, { 46, 48, 88}, { 47, 24, 72},
		{ 48, 30, 92}, { 49, 24,104}, { 50, 22,104}, { 51, 20, 60}, { 52, 36, 88},
		{ 54, 60, 88}, { 55, 36, 88}, { 56, 36, 84}, { 60, 48, 88}, { 61, 28, 92},
		{ 63, 60, 90}, { 72, 72, 96}, { 73, 52, 88}, { 81, 20, 84}, { 82, 44, 64},
		{ 86, 48, 84}, { 87, 36, 60}, { 88, 52,104}, { 89, 52,100}, { 90, 24, 84},
		{ 92, 24,104}, { 93, 40, 88}, { 94, 36, 56}, { 95, 24, 64}, { 99, 55, 84},
		{100, 36, 84}, {102, 36, 96}, {113, 36, 88}, {115, 36, 72}, {116, 28, 56},
		{117, 48, 60}, {118, 32, 84}, {119, 56,104}, {121, 52,104}, {122, 20, 72},
		{124,  0,120}, {125, 40, 68}, {127, 20, 88}
	};
	const int numUsedProgs = (int)(sizeof(usedProgs) / sizeof(usedProgs[0]));

	uint8_t hasProg[128];
	memset(hasProg, 0, sizeof(hasProg));

	for (int i = 0; i < numUsedProgs; i++) {
		int prog = usedProgs[i].prog;
		int pidx = tsf_get_presetindex(f, 0, prog);
		if (pidx < 0) continue;

		g_presets[prog].regionStart = (uint16_t)g_regionCount;
		const struct tsf_region *cur = NULL;
		int runStart = 0;
		for (int k = usedProgs[i].minKey; k <= usedProgs[i].maxKey; k++) {
			const struct tsf_region *r = find_best_region(f, pidx, k);
			if (r != cur) {
				if (cur != NULL) {
					add_region_entry(f, cur, (uint8_t)runStart, (uint8_t)(k - 1));
				}
				cur = r;
				runStart = k;
			}
		}
		if (cur != NULL) {
			add_region_entry(f, cur, (uint8_t)runStart, (uint8_t)usedProgs[i].maxKey);
		}
		/* Extend first region down to 0 and last region up to 127 so out-of-range notes never go silent */
		int cnt = g_regionCount - g_presets[prog].regionStart;
		if (cnt > 0) {
			g_regions[g_presets[prog].regionStart].lokey = 0;
			g_regions[g_regionCount - 1].hikey = 127;
		}
		g_presets[prog].regionCount = (uint16_t)cnt;
		hasProg[prog] = 1;
	}

	/* Map any unused GM program to the nearest included GM program */
	for (int prog = 0; prog < 128; prog++) {
		if (hasProg[prog]) continue;
		int best = 0, bestDist = 999;
		for (int j = 0; j < numUsedProgs; j++) {
			int d = abs(usedProgs[j].prog - prog);
			if (d < bestDist) { bestDist = d; best = usedProgs[j].prog; }
		}
		g_presets[prog] = g_presets[best];
	}

	/* Preset 128: GM Drum Kit (bank 128, preset 0) for every drum note used in Dune II + standard kit */
	static const int usedDrums[] = {
		35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,75
	};
	int drumPidx = tsf_get_presetindex(f, 128, 0);
	g_presets[128].regionStart = (uint16_t)g_regionCount;
	if (drumPidx >= 0) {
		for (size_t i = 0; i < sizeof(usedDrums) / sizeof(usedDrums[0]); i++) {
			int k = usedDrums[i];
			const struct tsf_region *r = find_best_region(f, drumPidx, k);
			if (r != NULL) {
				add_region_entry(f, r, (uint8_t)k, (uint8_t)k);
			}
		}
	}
	g_presets[128].regionCount = (uint16_t)(g_regionCount - g_presets[128].regionStart);

	printf("Exported %d unique samples, %d regions, pool size = %zu bytes (%.1f KB)\n",
		g_sampleCount, g_regionCount, g_poolLen, (double)g_poolLen / 1024.0);

	/* Write header */
	FILE *fh = fopen("src/audio/midi_dsi_bank.h", "w");
	fprintf(fh,
		"/** @file src/audio/midi_dsi_bank.h Multi-zone G.711 u-law SoundFont bank extracted via tsf.h from romfs/gm.sf2. */\n"
		"#ifndef MIDI_DSI_BANK_H\n"
		"#define MIDI_DSI_BANK_H\n\n"
		"#include \"types.h\"\n\n"
		"typedef struct DsiSf2Sample {\n"
		"\tuint32 poolOffset;\n"
		"\tuint32 length;\n"
		"\tuint32 loopStart;\n"
		"\tuint32 loopEnd;\n"
		"\tuint32 sampleRate;\n"
		"\tuint8  looped;\n"
		"} DsiSf2Sample;\n\n"
		"typedef struct DsiSf2Region {\n"
		"\tuint8  lokey;\n"
		"\tuint8  hikey;\n"
		"\tuint16 sampleIdx;\n"
		"\tint8   pitchKeycenter;\n"
		"\tint8   pitchKeytrack;\n"
		"\tint16  tuneCents;\n"
		"\tuint16 gain256;\n"
		"\tuint16 attackInc;\n"
		"\tuint16 holdBlocks;\n"
		"\tuint16 decayMul;\n"
		"\tuint16 sustain16;\n"
		"\tuint16 releaseMul;\n"
		"\tuint8  lpAlpha;\n"
		"\tuint8  group;\n"
		"\tuint8  loopMode;\n"
		"\tuint8  pad;\n"
		"} DsiSf2Region;\n\n"
		"typedef struct DsiSf2Preset {\n"
		"\tuint16 regionStart;\n"
		"\tuint16 regionCount;\n"
		"} DsiSf2Preset;\n\n"
		"#define DSI_SF2_SAMPLE_COUNT %d\n"
		"#define DSI_SF2_REGION_COUNT %d\n"
		"#define DSI_SF2_POOL_SIZE    %zu\n\n"
		"extern const uint8        g_dsiSf2Pool[DSI_SF2_POOL_SIZE];\n"
		"extern const DsiSf2Sample g_dsiSf2Samples[DSI_SF2_SAMPLE_COUNT];\n"
		"extern const DsiSf2Region g_dsiSf2Regions[DSI_SF2_REGION_COUNT];\n"
		"extern const DsiSf2Preset g_dsiSf2Presets[129];\n\n"
		"#endif /* MIDI_DSI_BANK_H */\n",
		g_sampleCount, g_regionCount, g_poolLen
	);
	fclose(fh);

	/* Write C data file */
	FILE *fc = fopen("src/audio/midi_dsi_bank.c", "w");
	fprintf(fc,
		"/** @file src/audio/midi_dsi_bank.c Multi-zone SoundFont bank for Nintendo DSi 16-bit software synth. */\n"
		"#include \"midi_dsi_bank.h\"\n\n"
		"const uint8 g_dsiSf2Pool[DSI_SF2_POOL_SIZE] = {\n"
	);
	for (size_t i = 0; i < g_poolLen; i++) {
		if ((i % 24) == 0) fprintf(fc, "\t");
		fprintf(fc, "%u,", g_pool[i]);
		if ((i % 24) == 23 || i + 1 == g_poolLen) fprintf(fc, "\n");
	}
	fprintf(fc, "};\n\nconst DsiSf2Sample g_dsiSf2Samples[DSI_SF2_SAMPLE_COUNT] = {\n");
	for (int i = 0; i < g_sampleCount; i++) {
		ExportSample *e = &g_samples[i];
		fprintf(fc, "\t{ %uu, %uu, %uu, %uu, %uu, %uu },\n",
			e->poolOffset, e->length, e->loopStart, e->loopEnd, e->sampleRate, e->looped);
	}
	fprintf(fc, "};\n\nconst DsiSf2Region g_dsiSf2Regions[DSI_SF2_REGION_COUNT] = {\n");
	for (int i = 0; i < g_regionCount; i++) {
		ExportRegion *r = &g_regions[i];
		fprintf(fc, "\t{ %u, %u, %u, %d, %d, %d, %u, %u, %u, %u, %u, %u, %u, %u, %u, 0 },\n",
			r->lokey, r->hikey, r->sampleIdx, r->pitchKeycenter, r->pitchKeytrack, r->tuneCents,
			r->gain256, r->attackInc, r->holdBlocks, r->decayMul, r->sustain16, r->releaseMul,
			r->lpAlpha, r->group, r->loopMode);
	}
	fprintf(fc, "};\n\nconst DsiSf2Preset g_dsiSf2Presets[129] = {\n");
	for (int i = 0; i < 129; i++) {
		fprintf(fc, "\t{ %u, %u },\n", g_presets[i].regionStart, g_presets[i].regionCount);
	}
	fprintf(fc, "};\n");
	fclose(fc);

	tsf_close(f);
	free(g_pool);
	return 0;
}
