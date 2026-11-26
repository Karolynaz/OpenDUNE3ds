/** @file src/video/video_dsi.c Nintendo DSi (TWL) hardware video driver. */

#include <nds.h>
#include <calico/nds/pm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "video.h"
#include "../gfx.h"
#include "../gui/gui_3ds.h"
#include "../gui/tactical_3ds.h"
#include "../gui/shop_3ds.h"
#include "../gui/popup_3ds.h"
#include "../input/input_3ds.h"
#include "../audio/midi.h"
#include "../opendune.h"
#include "../timer.h"
#include "../os/error.h"

#define DSI_PHYS_W 256
#define DSI_PHYS_H 192

static bool s_video_initialized = false;
static uint8 *s_framebuffer = NULL;
static uint8 s_paletteBGR[256 * 3];

/* Separate UI palette prevents title/briefing fades from recoloring the shell. */
static const uint8 s_goldBGR[10][3] = {
	{14,40,65}, {18,64,97}, {21,103,158}, {22,117,177}, {25,132,191},
	{28,146,205}, {28,151,215}, {43,173,231}, {80,200,248}, {151,226,255}
};

/* DS 16-bit BGR555 LUTs with bit 15 set (opaque alpha bit for BMP16 mode). */
static uint16 s_maskLUT16[8][256];
static uint16 s_maskDimLUT16[8][256];

static int s_topBg = -1;
static int s_botBg = -1;
static int s_topBack = 1; /* 0 -> BG_BMP_RAM(0), 1 -> BG_BMP_RAM(8) */

/* 96 KB staging buffer for tear-free bottom screen DMA */
static uint16 s_botStaging[DSI_PHYS_W * DSI_PHYS_H] __attribute__((aligned(32)));

/* Dirty tracking & throttling for bottom screen scaling (saves ~65% ARM9 CPU during battle) */
static uint32 s_paletteVersion = 1;
static uint32 s_lastBotHash = 0;
static bool s_botHashValid = false;
static bool s_botForceRedraw = true;
static uint32 s_botFrameCounter = 0;

void Video_DSI_MarkBottomDirty(void)
{
	s_botForceRedraw = true;
}

static inline uint16 PackRGB15(uint8 r, uint8 g, uint8 b)
{
	return (uint16)(0x8000u | ((uint16)(r >> 3)) | ((uint16)(g >> 3) << 5) | ((uint16)(b >> 3) << 10));
}

/* Branchless 50/50 average of two RGB15 pixels (preserves bit 15 = 1). */
static inline uint16 AvgRGB15(uint16 a, uint16 b)
{
	return (uint16)(0x8000u | ((a & b & 0x7FFFu) + (((a ^ b) & 0x7BDEu) >> 1)));
}

/* 75% a + 25% b blend of two RGB15 pixels. */
static inline uint16 Mix31RGB15(uint16 a, uint16 b)
{
	return AvgRGB15(a, AvgRGB15(a, b));
}

/* SWAR 2-pixels-at-a-time branchless 50/50 average of packed RGB15 pairs. */
static inline u32 AvgRGB15x2(u32 a, u32 b)
{
	return 0x80008000u | ((a & b & 0x7FFF7FFFu) + (((a ^ b) & 0x7BDE7BDEu) >> 1));
}

/* SWAR 2-pixels-at-a-time 75% a + 25% b blend of packed RGB15 pairs. */
static inline u32 Mix31RGB15x2(u32 a, u32 b)
{
	return AvgRGB15x2(a, AvgRGB15x2(a, b));
}

static inline const uint8 *DisplayColor(uint8 col, uint8 ui)
{
	static const uint8 white[3]  = {255, 255, 255};
	if (col == 254) return white;
	if (ui == TACTICAL_UI_MASK) return s_tacticalBGR[col < TC_COUNT ? col : TC_BLACK];
	if (ui == SHOP_UI_MASK) return s_shopBGR[col < SP_COUNT ? col : SP_BLACK];
	if (ui == POPUP_UI_MASK) return s_popupBGR[col < POPUP_COUNT ? col : POPUP_BLACK];
	if (ui == 1) {
		return (col >= 110 && col <= 119) ? s_goldBGR[col - 110] : &s_paletteBGR[col * 3];
	}
	if (ui == 2) {
		static const uint8 gold[3]   = {104, 208, 240};
		static const uint8 focus[3]  = {180, 242, 255};
		static const uint8 red[3]    = { 30,  30, 240};
		static const uint8 blue[3]   = {240, 140,  40};
		static const uint8 face[3]   = { 48,  40,  40};
		static const uint8 hilite[3] = {104,  88,  88};
		static const uint8 shadow[3] = { 24,  18,  18};
		static const uint8 border[3] = { 96, 184, 224};
		static const uint8 dim[3]    = {110, 104, 104};
		static const uint8 black[3]  = {  0,   0,   0};

		if (col == 15) return gold;
		if (col == 216) return focus;
		if (col == 8) return red;
		if (col == 11) return blue;
		if (col == 20) return face;
		if (col == 26) return hilite;
		if (col == 16) return shadow;
		if (col == 27) return border;
		if (col == 110) return dim;
		if (col == 0) return black;
		return &s_paletteBGR[col * 3];
	}
	return &s_paletteBGR[col * 3];
}

static void Video_RebuildMaskLUTsRange(int from, int to)
{
	if (from < 0) from = 0;
	if (to > 256) to = 256;
	for (int m = 0; m < 8; m++) {
		uint8 mask = (m < 6) ? (uint8)m : 0;
		for (int c = from; c < to; c++) {
			const uint8 *bgr = DisplayColor((uint8)c, mask);
			s_maskLUT16[m][c] = PackRGB15(bgr[2], bgr[1], bgr[0]);
			uint8 db = (uint8)((bgr[0] * 140) >> 8);
			uint8 dg = (uint8)((bgr[1] * 140) >> 8);
			uint8 dr = (uint8)((bgr[2] * 140) >> 8);
			s_maskDimLUT16[m][c] = PackRGB15(dr, dg, db);
		}
	}
}

static void Video_RebuildMaskLUTs(void)
{
	Video_RebuildMaskLUTsRange(0, 256);
}

bool Video_Init(int screen_magnification, VideoScaleFilter filter)
{
	VARIABLE_NOT_USED(screen_magnification);
	VARIABLE_NOT_USED(filter);

	if (s_video_initialized) return true;

	/* Configure dual 16-bit 256x192 bitmap displays:
	 * Main engine (Top screen): double-buffered across VRAM_A (base 0) & VRAM_B (base 8).
	 * Sub engine (Bottom screen): VRAM_C (base 0) fed via DMA from s_botStaging. */
	videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
	videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);

	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
	vramSetBankC(VRAM_C_SUB_BG_0x06200000);

	lcdMainOnTop();

	s_topBg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	s_botBg = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	bgUpdate();
	s_topBack = 1;

	/* Clear VRAM banks to opaque black */
	uint32 black2 = 0x80008000u;
	dmaFillWords(black2, BG_BMP_RAM(0), DSI_PHYS_W * DSI_PHYS_H * 2);
	dmaFillWords(black2, BG_BMP_RAM(8), DSI_PHYS_W * DSI_PHYS_H * 2);
	dmaFillWords(black2, BG_BMP_RAM_SUB(0), DSI_PHYS_W * DSI_PHYS_H * 2);

	if (!GUI_3DS_Init()) {
		GUI_3DS_Uninit();
		return false;
	}

	Input_3DS_Init();

	s_framebuffer = (uint8 *)calloc(1, SCREEN_WIDTH * (SCREEN_HEIGHT + 4));
	if (s_framebuffer == NULL) {
		Input_3DS_Uninit();
		GUI_3DS_Uninit();
		return false;
	}

	memset(s_paletteBGR, 0, sizeof(s_paletteBGR));
	s_paletteBGR[15 * 3 + 0] = 255;
	s_paletteBGR[15 * 3 + 1] = 255;
	s_paletteBGR[15 * 3 + 2] = 255;
	Video_RebuildMaskLUTs();

	s_video_initialized = true;
	return true;
}

void Video_Uninit(void)
{
	if (!s_video_initialized) return;
	s_video_initialized = false;

	Input_3DS_Uninit();
	GUI_3DS_Uninit();

	if (s_framebuffer != NULL) {
		free(s_framebuffer);
		s_framebuffer = NULL;
	}
}

void Video_SetPalette(void *palette, int from, int length)
{
	const uint8 *p = (const uint8 *)palette;
	int to = from + length;
	if (to > 256) to = 256;
	for (int i = from; i < to; i++) {
		uint8 r = (((*p++) & 0x3F) * 0x41) >> 4;
		uint8 g = (((*p++) & 0x3F) * 0x41) >> 4;
		uint8 b = (((*p++) & 0x3F) * 0x41) >> 4;

		s_paletteBGR[i * 3 + 0] = b;
		s_paletteBGR[i * 3 + 1] = g;
		s_paletteBGR[i * 3 + 2] = r;
	}
	Video_RebuildMaskLUTsRange(from, to);
	s_paletteVersion++;
	s_botForceRedraw = true;
}

void Video_Mouse_SetPosition(uint16 x, uint16 y)
{
	VARIABLE_NOT_USED(x);
	VARIABLE_NOT_USED(y);
}

void Video_Mouse_SetRegion(uint16 minX, uint16 maxX, uint16 minY, uint16 maxY)
{
	VARIABLE_NOT_USED(minX);
	VARIABLE_NOT_USED(maxX);
	VARIABLE_NOT_USED(minY);
	VARIABLE_NOT_USED(maxY);
}

void Video_SetOffset(uint16 offset)
{
	VARIABLE_NOT_USED(offset);
}

void *Video_GetFrameBuffer(uint16 size)
{
	VARIABLE_NOT_USED(size);
	return s_framebuffer;
}

/* Direct 1:1 blit for Top Screen (256x192 -> 256x192 RGB15, 4 pixels / 2 words per step). */
static void Video_BlitTopDirect(u16 *vramDst, const uint8 *src, const uint8 *maskBuf, bool modalActive)
{
	if (!modalActive) {
		u32 *dst32 = (u32 *)vramDst;
		const uint16 *lut0 = s_maskLUT16[0];
		const u32 *m32 = (const u32 *)maskBuf;
		const int totalQuads = (DSI_PHYS_W * DSI_PHYS_H) / 4;
		for (int i = 0; i < totalQuads; i++) {
			u32 m4 = m32[i];
			if (m4 == 0) {
				u32 p0 = lut0[src[0]];
				u32 p1 = lut0[src[1]];
				u32 p2 = lut0[src[2]];
				u32 p3 = lut0[src[3]];
				dst32[0] = p0 | (p1 << 16);
				dst32[1] = p2 | (p3 << 16);
			} else {
				u32 p0 = s_maskLUT16[(m4) & 7][src[0]];
				u32 p1 = s_maskLUT16[(m4 >> 8) & 7][src[1]];
				u32 p2 = s_maskLUT16[(m4 >> 16) & 7][src[2]];
				u32 p3 = s_maskLUT16[(m4 >> 24) & 7][src[3]];
				dst32[0] = p0 | (p1 << 16);
				dst32[1] = p2 | (p3 << 16);
			}
			src += 4;
			dst32 += 2;
		}
		return;
	}

	const int px0 = (TOP_WIDTH - POPUP_WIDTH) / 2;
	const int py0 = (TOP_HEIGHT - POPUP_HEIGHT) / 2;
	const int px1 = px0 + POPUP_WIDTH;
	const int py1 = py0 + POPUP_HEIGHT;

	for (int y = 0; y < DSI_PHYS_H; y++) {
		u32 *dst32 = (u32 *)(vramDst + y * DSI_PHYS_W);
		const uint8 *s = src + y * TOP_WIDTH;
		const uint8 *m = maskBuf + y * TOP_WIDTH;
		bool inY = (y >= py0 && y < py1);
		if (!inY) {
			for (int x = 0; x < DSI_PHYS_W; x += 2) {
				uint16 p0 = s_maskDimLUT16[m[x] & 7][s[x]];
				uint16 p1 = s_maskDimLUT16[m[x + 1] & 7][s[x + 1]];
				*dst32++ = (u32)p0 | ((u32)p1 << 16);
			}
		} else {
			for (int x = 0; x < DSI_PHYS_W; x += 2) {
				const uint16 (*lut0)[256] = (x >= px0 && x < px1) ? s_maskLUT16 : s_maskDimLUT16;
				const uint16 (*lut1)[256] = ((x + 1) >= px0 && (x + 1) < px1) ? s_maskLUT16 : s_maskDimLUT16;
				uint16 p0 = lut0[m[x] & 7][s[x]];
				uint16 p1 = lut1[m[x + 1] & 7][s[x + 1]];
				*dst32++ = (u32)p0 | ((u32)p1 << 16);
			}
		}
	}
}

/* Smooth 5->4 horizontal + vertical weighted RGB15 downscale: 320x240 -> 256x192.
 * Uses SWAR 2-pixels-per-u32 vertical blending for ~2x speedup on ARM946E-S. */
static void Video_BlitBottomScaled54(const uint8 *src, const uint8 *maskBuf, const uint16 lut[8][256])
{
	static uint16 rowBuf[5][DSI_PHYS_W] __attribute__((aligned(32)));

	for (int gy = 0; gy < 48; gy++) {
		int sy0 = gy * 5;
		/* Horizontal 5->4 pass for 5 consecutive source rows */
		for (int r = 0; r < 5; r++) {
			const uint8 *sr = src + (sy0 + r) * BOTTOM_WIDTH;
			const uint8 *mr = maskBuf + (sy0 + r) * BOTTOM_WIDTH;
			u32 *dr32 = (u32 *)rowBuf[r];
			for (int gx = 0; gx < 64; gx++) {
				uint16 c0 = lut[mr[0] & 7][sr[0]];
				uint16 c1 = lut[mr[1] & 7][sr[1]];
				uint16 c2 = lut[mr[2] & 7][sr[2]];
				uint16 c3 = lut[mr[3] & 7][sr[3]];
				uint16 c4 = lut[mr[4] & 7][sr[4]];
				u32 p0 = Mix31RGB15(c0, c1);
				u32 p1 = AvgRGB15(c1, c2);
				u32 p2 = AvgRGB15(c2, c3);
				u32 p3 = Mix31RGB15(c4, c3);
				dr32[0] = p0 | (p1 << 16);
				dr32[1] = p2 | (p3 << 16);
				sr += 5;
				mr += 5;
				dr32 += 2;
			}
		}

		/* Vertical 5->4 pass: SWAR 2 pixels per u32 into s_botStaging */
		u32 *o0 = (u32 *)(s_botStaging + (gy * 4 + 0) * DSI_PHYS_W);
		u32 *o1 = (u32 *)(s_botStaging + (gy * 4 + 1) * DSI_PHYS_W);
		u32 *o2 = (u32 *)(s_botStaging + (gy * 4 + 2) * DSI_PHYS_W);
		u32 *o3 = (u32 *)(s_botStaging + (gy * 4 + 3) * DSI_PHYS_W);
		const u32 *r0 = (const u32 *)rowBuf[0];
		const u32 *r1 = (const u32 *)rowBuf[1];
		const u32 *r2 = (const u32 *)rowBuf[2];
		const u32 *r3 = (const u32 *)rowBuf[3];
		const u32 *r4 = (const u32 *)rowBuf[4];

		for (int x = 0; x < DSI_PHYS_W / 2; x++) {
			u32 a = r0[x], b = r1[x], c = r2[x], d = r3[x], e = r4[x];
			o0[x] = Mix31RGB15x2(a, b);
			o1[x] = AvgRGB15x2(b, c);
			o2[x] = AvgRGB15x2(c, d);
			o3[x] = Mix31RGB15x2(e, d);
		}
	}

	DC_FlushRange(s_botStaging, sizeof(s_botStaging));
	dmaCopyWords(3, s_botStaging, BG_BMP_RAM_SUB(0), sizeof(s_botStaging));
}

static uint32 HashBottomScreen(const uint8 *buf, const uint8 *mask, bool modalActive)
{
	uint32 h = 0x811C9DC5u ^ s_paletteVersion ^ (modalActive ? 0x5A5A5A5Au : 0u);
	const u32 *b32 = (const u32 *)buf;
	const u32 *m32 = (const u32 *)mask;
	const int words = (BOTTOM_WIDTH * BOTTOM_HEIGHT) / 4;
	for (int i = 0; i < words; i += 4) {
		u32 v0 = b32[i]     ^ (m32[i] << 1);
		u32 v1 = b32[i + 1] ^ (m32[i + 1] << 2);
		u32 v2 = b32[i + 2] ^ (m32[i + 2] << 3);
		u32 v3 = b32[i + 3] ^ (m32[i + 3] << 4);
		h = ((h << 5) | (h >> 27)) ^ v0;
		h = ((h << 7) | (h >> 25)) ^ v1;
		h = ((h << 11) | (h >> 21)) ^ v2;
		h = ((h << 13) | (h >> 19)) ^ v3;
	}
	return h;
}

void Video_DSI_PresentBottomOnly(void)
{
	if (!s_video_initialized || g_bottomScreenBuffer == NULL) return;
	Video_BlitBottomScaled54(g_bottomScreenBuffer, g_bottomScreenUIMask, s_maskLUT16);
	s_botHashValid = false;
}

void Video_Tick(void)
{
	if (!s_video_initialized) return;

	if (!pmMainLoop()) {
		g_running = false;
		return;
	}

	Input_3DS_Poll();

	const bool modalActive = g_3dsModalMessageActive;

	/* Top screen always updates at full rate for crisp cursor & battlefield animation */
	GUI_3DS_RenderTopScreen();
	if (g_topScreenBuffer != NULL) {
		int mapBase = s_topBack ? 8 : 0;
		u16 *fbTop = (u16 *)BG_BMP_RAM(mapBase);
		Video_BlitTopDirect(fbTop, g_topScreenBuffer, g_topScreenUIMask, modalActive);
		bgSetMapBase(s_topBg, mapBase);
		bgUpdate();
		s_topBack ^= 1;
	}

	/* Bottom screen: during battle, throttle passive HUD redraws to ~10 Hz unless input/touch/palette
	 * dirtied it (0 ms touch latency). Always skip 5:4 downscale + DMA if pixels are unchanged. */
	s_botFrameCounter++;
	bool inQuietBattle = (g_gameMode == GM_NORMAL && !modalActive
		&& !GUI_3DS_IsMenuOrDialogActive() && !GUI_3DS_CustomMenuActive());
	bool shouldRenderBot = s_botForceRedraw || !s_botHashValid || !inQuietBattle || ((s_botFrameCounter % 6u) == 0);

	if (shouldRenderBot) {
		s_botForceRedraw = false;
		GUI_3DS_RenderBottomScreen();
		if (g_bottomScreenBuffer != NULL) {
			uint32 h = HashBottomScreen(g_bottomScreenBuffer, g_bottomScreenUIMask, modalActive);
			if (!s_botHashValid || h != s_lastBotHash) {
				s_lastBotHash = h;
				s_botHashValid = true;
				Video_BlitBottomScaled54(g_bottomScreenBuffer, g_bottomScreenUIMask,
					modalActive ? s_maskDimLUT16 : s_maskLUT16);
			}
		}
	}

	midi_3ds_pump();

	GFX_Screen_SetClean(SCREEN_0);
}
