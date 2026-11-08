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

static inline const uint8 *DisplayColor(uint8 col, uint8 ui)
{
	static const uint8 white[3]  = {255, 255, 255}; /* Pure white (cursor arrow, highlights) */
	if (col == 254) return white;
	if (ui == TACTICAL_UI_MASK) return s_tacticalBGR[col < TC_COUNT ? col : TC_BLACK];
	if (ui == SHOP_UI_MASK) return s_shopBGR[col < SP_COUNT ? col : SP_BLACK];
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

bool Video_Init(int screen_magnification, VideoScaleFilter filter)
{
	VARIABLE_NOT_USED(screen_magnification);
	VARIABLE_NOT_USED(filter);

	if (s_video_initialized) return true;

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
}

void Video_SetPalette(void *palette, int from, int length)
{
	const uint8 *p = (const uint8 *)palette;
	for (int i = from; i < from + length && i < 256; i++) {
		/* Convert 6-bit DOS VGA color component to 8-bit */
		uint8 r = (((*p++) & 0x3F) * 0x41) >> 4;
		uint8 g = (((*p++) & 0x3F) * 0x41) >> 4;
		uint8 b = (((*p++) & 0x3F) * 0x41) >> 4;

		s_paletteBGR[i * 3 + 0] = b;
		s_paletteBGR[i * 3 + 1] = g;
		s_paletteBGR[i * 3 + 2] = r;
	}
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

	/* Top Screen: 400x240 to 240x400 rotated BGR8 */
	if (fbTop != NULL && g_topScreenBuffer != NULL) {
		for (int x = 0; x < TOP_WIDTH; x++) {
			u8 *dst = fbTop + x * (240 * 3);
			for (int y = 239; y >= 0; y--) {
				u8 col = g_topScreenBuffer[y * TOP_WIDTH + x];
				const u8 *bgr = DisplayColor(col, g_topScreenUIMask[y * TOP_WIDTH + x]);
				dst[0] = bgr[0];
				dst[1] = bgr[1];
				dst[2] = bgr[2];
				dst += 3;
			}
		}
	}

	/* Bottom Screen: 320x240 to 240x320 rotated BGR8 */
	if (fbBot != NULL && g_bottomScreenBuffer != NULL) {
		for (int x = 0; x < BOTTOM_WIDTH; x++) {
			u8 *dst = fbBot + x * (240 * 3);
			for (int y = 239; y >= 0; y--) {
				u8 col = g_bottomScreenBuffer[y * BOTTOM_WIDTH + x];
				const u8 *bgr = DisplayColor(col, g_bottomScreenUIMask[y * BOTTOM_WIDTH + x]);
				dst[0] = bgr[0];
				dst[1] = bgr[1];
				dst[2] = bgr[2];
				dst += 3;
			}
		}
	}

	gfxFlushBuffers();
	gfxSwapBuffers();
	/* Video_Tick is already paced by the engine timer. Blocking here delays
	 * its 120 Hz music sequencer and bunches MIDI events after every frame. */

	midi_3ds_pump();

	GFX_Screen_SetClean(SCREEN_0);
}
