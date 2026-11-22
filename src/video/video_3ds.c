/** @file src/video/video_3ds.c Nintendo 3DS hardware video driver. */

#include <3ds.h>
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

static bool s_video_initialized = false;
static uint8 *s_framebuffer = NULL;
static uint8 s_paletteBGR[256 * 3];
/* A separate UI palette prevents title/briefing fades from recoloring the shell.
 * Original sprites and radar pixels always use the current game palette. */
static const uint8 s_goldBGR[10][3] = {
	{14,40,65}, {18,64,97}, {21,103,158}, {22,117,177}, {25,132,191},
	{28,146,205}, {28,151,215}, {43,173,231}, {80,200,248}, {151,226,255}
};
static uint32 s_maskLUT32[8][256];
static uint32 s_maskDimLUT32[8][256];

static inline const uint8 *DisplayColor(uint8 col, uint8 ui)
{
	static const uint8 white[3]  = {255, 255, 255}; /* Pure white (cursor arrow, highlights) */
	if (col == 254) return white;
	if (ui == TACTICAL_UI_MASK) return s_tacticalBGR[col < TC_COUNT ? col : TC_BLACK];
	if (ui == SHOP_UI_MASK) return s_shopBGR[col < SP_COUNT ? col : SP_BLACK];
	if (ui == POPUP_UI_MASK) return s_popupBGR[col < POPUP_COUNT ? col : POPUP_BLACK];
	if (ui == 1) {
		return (col >= 110 && col <= 119) ? s_goldBGR[col - 110] : &s_paletteBGR[col * 3];
	}
	if (ui == 2) {
		static const uint8 gold[3]   = {104, 208, 240}; /* Dune II warm gold button text (BGR: #F0D068) */
		static const uint8 focus[3]  = {180, 242, 255}; /* Focused bright gold button text (BGR: #FFF2B4) */
		static const uint8 red[3]    = { 30,  30, 240}; /* Vibrant bright red (BGR) */
		static const uint8 blue[3]   = {240, 140,  40}; /* Vibrant cyan/blue header (BGR) */
		static const uint8 face[3]   = { 48,  40,  40}; /* Dark slate-black button face (BGR: #282830) */
		static const uint8 hilite[3] = {104,  88,  88}; /* Dark button top/left bevel (BGR: #585868) */
		static const uint8 shadow[3] = { 24,  18,  18}; /* Dark button bottom/right bevel (BGR: #121218) */
		static const uint8 border[3] = { 96, 184, 224}; /* Focused button inner gold border (BGR: #E0B860) */
		static const uint8 dim[3]    = {110, 104, 104}; /* Dim/disabled button text */
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
			s_maskLUT32[m][c] = (uint32)bgr[0] | ((uint32)bgr[1] << 8) | ((uint32)bgr[2] << 16);
			uint32 db = (uint32)((bgr[0] * 140) >> 8);
			uint32 dg = (uint32)((bgr[1] * 140) >> 8);
			uint32 dr = (uint32)((bgr[2] * 140) >> 8);
			s_maskDimLUT32[m][c] = db | (dg << 8) | (dr << 16);
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

	osSetSpeedupEnable(true);
	gfxInitDefault();
	gfxSetDoubleBuffering(GFX_TOP, true);
	gfxSetDoubleBuffering(GFX_BOTTOM, true);

	if (!GUI_3DS_Init()) {
		GUI_3DS_Uninit();
		gfxExit();
		return false;
	}

	Input_3DS_Init();

	s_framebuffer = (uint8 *)calloc(1, SCREEN_WIDTH * (SCREEN_HEIGHT + 4));
	if (s_framebuffer == NULL) {
		Input_3DS_Uninit();
		GUI_3DS_Uninit();
		gfxExit();
		return false;
	}

	memset(s_paletteBGR, 0, sizeof(s_paletteBGR));
	/* Set color 15 to White default */
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

	gfxExit();
	osSetSpeedupEnable(false);
}

void Video_SetPalette(void *palette, int from, int length)
{
	const uint8 *p = (const uint8 *)palette;
	int to = from + length;
	if (to > 256) to = 256;
	for (int i = from; i < to; i++) {
		/* Convert 6-bit DOS VGA color component to 8-bit */
		uint8 r = (((*p++) & 0x3F) * 0x41) >> 4;
		uint8 g = (((*p++) & 0x3F) * 0x41) >> 4;
		uint8 b = (((*p++) & 0x3F) * 0x41) >> 4;

		s_paletteBGR[i * 3 + 0] = b;
		s_paletteBGR[i * 3 + 1] = g;
		s_paletteBGR[i * 3 + 2] = r;
	}
	Video_RebuildMaskLUTsRange(from, to);
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

static void Video_BlitRotatedBGR8(u8 *fb, const uint8 *src, const uint8 *maskBuf, int width, const uint32 lut[8][256])
{
	const int stride = width;
	const int stride2 = width * 2;
	const int stride3 = width * 3;
	const int stride4 = width * 4;
	for (int x0 = 0; x0 < width; x0 += 16) {
		int xEnd = x0 + 16;
		if (xEnd > width) xEnd = width;
		for (int x = x0; x < xEnd; x++) {
			const uint8 *s = src + 239 * stride + x;
			const uint8 *m = maskBuf + 239 * stride + x;
			uint32 *dst32 = (uint32 *)(fb + x * 720);
			for (int cnt = 60; cnt > 0; cnt--) {
				uint32 c0 = lut[m[0] & 7][s[0]];
				uint32 c1 = lut[m[-stride] & 7][s[-stride]];
				uint32 c2 = lut[m[-stride2] & 7][s[-stride2]];
				uint32 c3 = lut[m[-stride3] & 7][s[-stride3]];
				dst32[0] = c0 | (c1 << 24);
				dst32[1] = (c1 >> 8) | (c2 << 16);
				dst32[2] = (c2 >> 16) | (c3 << 8);
				dst32 += 3;
				s -= stride4;
				m -= stride4;
			}
		}
	}
}

void Video_Tick(void)
{
	if (!s_video_initialized) return;

	if (!aptMainLoop()) {
		g_running = false;
		return;
	}

	Input_3DS_Poll();

	GUI_3DS_RenderTopScreen();
	GUI_3DS_RenderBottomScreen();

	/* Blit to 3DS hardware framebuffers */
	u16 topW, topH;
	u8 *fbTop = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &topW, &topH);

	u16 botW, botH;
	u8 *fbBot = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &botW, &botH);

	const bool modalActive = g_3dsModalMessageActive;

	/* Top Screen: 400x240 to 240x400 rotated BGR8 */
	if (fbTop != NULL && g_topScreenBuffer != NULL) {
		if (!modalActive) {
			Video_BlitRotatedBGR8(fbTop, g_topScreenBuffer, g_topScreenUIMask, TOP_WIDTH, s_maskLUT32);
		} else {
			const int px0 = (TOP_WIDTH - POPUP_WIDTH) / 2;
			const int py0 = (TOP_HEIGHT - POPUP_HEIGHT) / 2;
			const int px1 = px0 + POPUP_WIDTH;
			const int py1 = py0 + POPUP_HEIGHT;
			const int stride = TOP_WIDTH;
			const int stride2 = TOP_WIDTH * 2;
			const int stride3 = TOP_WIDTH * 3;
			const int stride4 = TOP_WIDTH * 4;
			for (int x0 = 0; x0 < TOP_WIDTH; x0 += 16) {
				int xEnd = x0 + 16;
				if (xEnd > TOP_WIDTH) xEnd = TOP_WIDTH;
				for (int x = x0; x < xEnd; x++) {
					bool inX = (x >= px0 && x < px1);
					const uint8 *s = g_topScreenBuffer + 239 * stride + x;
					const uint8 *m = g_topScreenUIMask + 239 * stride + x;
					uint32 *dst32 = (uint32 *)(fbTop + x * 720);
					for (int yTop = 239; yTop >= 3; yTop -= 4) {
						const uint32 (*in0)[256] = (inX && yTop >= py0 && yTop < py1) ? s_maskLUT32 : s_maskDimLUT32;
						const uint32 (*in1)[256] = (inX && (yTop - 1) >= py0 && (yTop - 1) < py1) ? s_maskLUT32 : s_maskDimLUT32;
						const uint32 (*in2)[256] = (inX && (yTop - 2) >= py0 && (yTop - 2) < py1) ? s_maskLUT32 : s_maskDimLUT32;
						const uint32 (*in3)[256] = (inX && (yTop - 3) >= py0 && (yTop - 3) < py1) ? s_maskLUT32 : s_maskDimLUT32;
						uint32 c0 = in0[m[0] & 7][s[0]];
						uint32 c1 = in1[m[-stride] & 7][s[-stride]];
						uint32 c2 = in2[m[-stride2] & 7][s[-stride2]];
						uint32 c3 = in3[m[-stride3] & 7][s[-stride3]];
						dst32[0] = c0 | (c1 << 24);
						dst32[1] = (c1 >> 8) | (c2 << 16);
						dst32[2] = (c2 >> 16) | (c3 << 8);
						dst32 += 3;
						s -= stride4;
						m -= stride4;
					}
				}
			}
		}
	}

	/* Bottom Screen: 320x240 to 240x320 rotated BGR8 */
	if (fbBot != NULL && g_bottomScreenBuffer != NULL) {
		Video_BlitRotatedBGR8(fbBot, g_bottomScreenBuffer, g_bottomScreenUIMask, BOTTOM_WIDTH, modalActive ? s_maskDimLUT32 : s_maskLUT32);
	}

	gfxFlushBuffers();
	gfxSwapBuffers();
	/* Video_Tick is already paced by the engine timer. Blocking here delays
	 * its 120 Hz music sequencer and bunches MIDI events after every frame. */

	midi_3ds_pump();

	GFX_Screen_SetClean(SCREEN_0);
}
