/** @file src/gui/gui_3ds.c Dual-Screen GUI and Dashboard implementation for Nintendo 3DS. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "gui_3ds.h"
#include "font.h"
#include "tactical_3ds.h"
#include "shop_3ds.h"
#include "shop_art_3ds.h"
#include "font_tiny5.h"
#include "gui.h"
#include "widget.h"
#include "../audio/sound.h"
#include "../config.h"
#include "../explosion.h"
#include "../gfx.h"
#include "../house.h"
#include "../map.h"
#include "../opendune.h"
#include "../os/math.h"
#include "../pool/pool.h"
#include "../pool/unit.h"
#include "../pool/structure.h"
#include "../pool/house.h"
#include "../scenario.h"
#include "../sprites.h"
#include "../string.h"
#include "../structure.h"
#include "../table/strings.h"
#include "../tile.h"
#include "../tools.h"
#include "../unit.h"
#include "../unit_3ds.h"
#include "../input/mouse.h"
#include "../input/input.h"
#include "../timer.h"
#include "../os/error.h"

uint8 *g_topScreenBuffer = NULL;
uint8 *g_bottomScreenBuffer = NULL;
uint8 g_topScreenUIMask[TOP_WIDTH * TOP_HEIGHT];
uint8 g_bottomScreenUIMask[BOTTOM_WIDTH * BOTTOM_HEIGHT];

int16 g_cursor3DS_X = 200;
int16 g_cursor3DS_Y = 120;

bool g_3dsModalMessageActive = false;
static int s_dialogFocus = 0;
static bool s_resultsActive;
static Widget *s_resultsButtons;
static Widget *s_mentatButtons = NULL;
static WindowDesc *s_dialogWindow = NULL;
static Widget *s_dialogChosen = NULL;
static uint8 s_messageTop[TOP_WIDTH*TOP_HEIGHT], s_messageTopMask[TOP_WIDTH*TOP_HEIGHT];
static uint8 s_messageBottom[BOTTOM_WIDTH*BOTTOM_HEIGHT], s_messageBottomMask[BOTTOM_WIDTH*BOTTOM_HEIGHT];
static char s_messageText[768];
void GUI_3DS_BeginMessage(const char *text)
{
 if(g_topScreenBuffer) memcpy(s_messageTop,g_topScreenBuffer,sizeof(s_messageTop));
 if(g_bottomScreenBuffer) memcpy(s_messageBottom,g_bottomScreenBuffer,sizeof(s_messageBottom));
 memcpy(s_messageTopMask,g_topScreenUIMask,sizeof(s_messageTopMask));
 memcpy(s_messageBottomMask,g_bottomScreenUIMask,sizeof(s_messageBottomMask));
 snprintf(s_messageText,sizeof(s_messageText),"%s",text ? text : "");
 g_3dsModalMessageActive=true;
}
void GUI_3DS_EndMessage(void) { g_3dsModalMessageActive=false; }
bool GUI_3DS_SetResultsActive(bool active) { bool old=s_resultsActive; s_resultsActive=active; return old; }
void GUI_3DS_SetResultsButtons(Widget *buttons) { s_resultsButtons=buttons; s_dialogFocus=0; }
static bool s_titleActive = false;
static bool s_launchActive = false;
static bool s_planetBgValid = false;
static uint8 s_planetBg[320 * 200];
static uint16 s_houseChoice;
static uint16 s_mentatChoice = 0;
static uint8 s_houseConfirmID = 0xFF;
static int s_houseFocus = 0;
static char s_subtitleText[512] = "";

static bool GUI_3DS_IsLithuanian(void)
{
	const char *yes = String_Get_ByIndex(STR_YES);
	return (yes != NULL && (yes[0] == 'T' || yes[0] == 't'));
}

#define L10N(en, lt) (GUI_3DS_IsLithuanian() ? (lt) : (en))

void GUI_3DS_SetTitleActive(bool active) { s_titleActive = active; }
void GUI_3DS_SetLaunchActive(bool active) { s_launchActive = active; }
void GUI_3DS_SetPlanetBackground(const uint8 *pixels320x200)
{
	if (pixels320x200 != NULL) {
		memcpy(s_planetBg, pixels320x200, sizeof(s_planetBg));
		s_planetBgValid = true;
	} else {
		s_planetBgValid = false;
	}
}
const uint8 *GUI_3DS_GetPlanetBackground(void)
{
	return s_planetBgValid ? s_planetBg : NULL;
}
uint16 GUI_3DS_TakeHouseChoice(void)
{
	uint16 result = s_houseChoice;
	s_houseChoice = 0;
	return result;
}

uint16 GUI_3DS_HouseEvent(uint16 key)
{
	if (key==0x2B || key==0x6C || key==0x61) GUI_3DS_MenuInput(0,0,true);
	uint16 choice=GUI_3DS_TakeHouseChoice();
	return choice ? choice : key;
}

void GUI_3DS_SetSubtitle(const char *text)
{
	if (text == NULL || text[0] == '\0') {
		s_subtitleText[0] = '\0';
		return;
	}
	snprintf(s_subtitleText, sizeof(s_subtitleText), "%s", text);
}

void GUI_3DS_SetHouseConfirm(uint8 houseID)
{
	s_houseConfirmID = houseID;
	if (houseID != 0xFF) {
		s_subtitleText[0] = '\0';
		s_dialogFocus = 0;
		s_houseFocus = (houseID == HOUSE_ATREIDES) ? 0 : ((houseID == HOUSE_ORDOS) ? 1 : 2);
	}
}

void GUI_3DS_MentatChoose(uint16 result)
{
	s_mentatChoice = result;
	if (s_mentatButtons != NULL) {
		s_dialogFocus = (result == 0x8001) ? 0 : 1;
		s_dialogChosen = (result == 0x8001) ? s_mentatButtons : (s_mentatButtons->next ? s_mentatButtons->next : s_mentatButtons);
	}
}

uint16 GUI_3DS_MentatEvent(uint16 key)
{
	if (s_mentatChoice != 0) {
		uint16 res = s_mentatChoice;
		s_mentatChoice = 0;
		return res;
	}
	if (key == 0x1C || key == 0x2B || key == 0x3D || key == 0x6C || key == 0x61) {
		GUI_3DS_MenuInput(0, 0, true);
		if (s_mentatChoice != 0) {
			uint16 res = s_mentatChoice;
			s_mentatChoice = 0;
			return res;
		}
	}
	if (key == 0x01) {
		GUI_3DS_MentatChoose(0x8002);
		if (s_mentatChoice != 0) {
			uint16 res = s_mentatChoice;
			s_mentatChoice = 0;
			return res;
		}
	}
	return key;
}

/* Simple built-in 8x8 font bitmap table (ASCII 32..127 + Lithuanian characters) */
static const uint8 s_font8x8_basic[96][8] = {
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /*   */
	{0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* ! */
	{0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}, /* " */
	{0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, /* # */
	{0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, /* $ */
	{0x00,0x66,0xA6,0xD4,0x2B,0x65,0x66,0x00}, /* % */
	{0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00}, /* & */
	{0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, /* ' */
	{0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, /* ( */
	{0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, /* ) */
	{0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* * */
	{0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, /* + */
	{0x00,0x00,0x00,0x00,0x18,0x18,0x30,0x00}, /* , */
	{0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, /* - */
	{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, /* . */
	{0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00}, /* / */
	{0x7C,0xC6,0xCE,0xD6,0xE6,0xC6,0x7C,0x00}, /* 0 */
	{0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 1 */
	{0x7C,0xC6,0x06,0x1C,0x30,0x60,0xFE,0x00}, /* 2 */
	{0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00}, /* 3 */
	{0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00}, /* 4 */
	{0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00}, /* 5 */
	{0x7C,0xC6,0xC0,0xFC,0xC6,0xC6,0x7C,0x00}, /* 6 */
	{0xFE,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}, /* 7 */
	{0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00}, /* 8 */
	{0x7C,0xC6,0xC6,0x7E,0x06,0xC6,0x7C,0x00}, /* 9 */
	{0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, /* : */
	{0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}, /* ; */
	{0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, /* < */
	{0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, /* = */
	{0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, /* > */
	{0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00}, /* ? */
	{0x7C,0xC6,0xDE,0xDE,0xDC,0xC0,0x7C,0x00}, /* @ */
	{0x38,0x6C,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, /* A */
	{0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00}, /* B */
	{0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00}, /* C */
	{0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00}, /* D */
	{0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00}, /* E */
	{0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00}, /* F */
	{0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3A,0x00}, /* G */
	{0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00}, /* H */
	{0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00}, /* I */
	{0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00}, /* J */
	{0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00}, /* K */
	{0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00}, /* L */
	{0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0x00}, /* M */
	{0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00}, /* N */
	{0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, /* O */
	{0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00}, /* P */
	{0x7C,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x0E}, /* Q */
	{0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00}, /* R */
	{0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00}, /* S */
	{0x7E,0x5A,0x18,0x18,0x18,0x18,0x3C,0x00}, /* T */
	{0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00}, /* U */
	{0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00}, /* V */
	{0xC6,0xC6,0xD6,0xFE,0xFE,0x6C,0x6C,0x00}, /* W */
	{0xC6,0x6C,0x38,0x38,0x6C,0xC6,0xC6,0x00}, /* X */
	{0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00}, /* Y */
	{0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00}, /* Z */
	{0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, /* [ */
	{0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00}, /* \ */
	{0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, /* ] */
	{0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00}, /* ^ */
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* _ */
	{0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, /* ` */
	{0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00}, /* a */
	{0xE0,0x60,0x7C,0x66,0x66,0x66,0xDC,0x00}, /* b */
	{0x00,0x00,0x78,0xCC,0xC0,0xCC,0x78,0x00}, /* c */
	{0x1C,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00}, /* d */
	{0x00,0x00,0x78,0xCC,0xFC,0xC0,0x78,0x00}, /* e */
	{0x38,0x6C,0x60,0xF0,0x60,0x60,0xF0,0x00}, /* f */
	{0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8}, /* g */
	{0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00}, /* h */
	{0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, /* i */
	{0x06,0x00,0x0E,0x06,0x06,0x66,0x66,0x3C}, /* j */
	{0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00}, /* k */
	{0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, /* l */
	{0x00,0x00,0xE6,0xFF,0xDB,0xC9,0xC9,0x00}, /* m */
	{0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x00}, /* n */
	{0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00}, /* o */
	{0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0}, /* p */
	{0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E}, /* q */
	{0x00,0x00,0xDC,0x76,0x60,0x60,0xF0,0x00}, /* r */
	{0x00,0x00,0x7C,0xC0,0x78,0x0C,0xF8,0x00}, /* s */
	{0x10,0x30,0xFC,0x30,0x30,0x34,0x18,0x00}, /* t */
	{0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00}, /* u */
	{0x00,0x00,0xC6,0xC6,0x6C,0x38,0x10,0x00}, /* v */
	{0x00,0x00,0xC6,0xD6,0xFE,0x6C,0x6C,0x00}, /* w */
	{0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00}, /* x */
	{0x00,0x00,0xC6,0xC6,0xC6,0x7E,0x06,0xFC}, /* y */
	{0x00,0x00,0xFE,0x8C,0x18,0x32,0xFE,0x00}, /* z */
	{0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, /* { */
	{0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* | */
	{0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, /* } */
	{0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}, /* ~ */
	{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}  /* block */
};

bool GUI_3DS_Init(void)
{
	g_topScreenBuffer = (uint8 *)calloc(1, TOP_WIDTH * TOP_HEIGHT);
	g_bottomScreenBuffer = (uint8 *)calloc(1, BOTTOM_WIDTH * BOTTOM_HEIGHT);

	if (g_topScreenBuffer == NULL || g_bottomScreenBuffer == NULL) {
		return false;
	}

	g_cursor3DS_X = 200;
	g_cursor3DS_Y = 120;

	Unit_3DS_Init();

	return true;
}

void GUI_3DS_Uninit(void)
{
	if (g_topScreenBuffer != NULL) {
		free(g_topScreenBuffer);
		g_topScreenBuffer = NULL;
	}
	if (g_bottomScreenBuffer != NULL) {
		free(g_bottomScreenBuffer);
		g_bottomScreenBuffer = NULL;
	}
}

void GUI_3DS_PutPixel(uint8 *buf, int16 stride, int16 x, int16 y, uint8 color)
{
	if (x >= 0 && x < stride && y >= 0 && y < 240) {
		buf[y * stride + x] = color;
	}
}

void GUI_3DS_FillRect(uint8 *buf, int16 stride, int16 x1, int16 y1, int16 x2, int16 y2, uint8 color)
{
	if (x1 > x2) { int16 t = x1; x1 = x2; x2 = t; }
	if (y1 > y2) { int16 t = y1; y1 = y2; y2 = t; }
	if (x1 < 0) x1 = 0;
	if (x2 >= stride) x2 = stride - 1;
	if (y1 < 0) y1 = 0;
	if (y2 >= 240) y2 = 239;
	if (x1 > x2 || y1 > y2) return;

	for (int16 y = y1; y <= y2; y++) {
		memset(buf + (y * stride + x1), color, x2 - x1 + 1);
	}
}

void GUI_3DS_DrawRect(uint8 *buf, int16 stride, int16 x1, int16 y1, int16 x2, int16 y2, uint8 color)
{
	if (x1 > x2) { int16 t = x1; x1 = x2; x2 = t; }
	if (y1 > y2) { int16 t = y1; y1 = y2; y2 = t; }

	for (int16 x = x1; x <= x2; x++) {
		GUI_3DS_PutPixel(buf, stride, x, y1, color);
		GUI_3DS_PutPixel(buf, stride, x, y2, color);
	}
	for (int16 y = y1; y <= y2; y++) {
		GUI_3DS_PutPixel(buf, stride, x1, y, color);
		GUI_3DS_PutPixel(buf, stride, x2, y, color);
	}
}

void GUI_3DS_DrawBevel(uint8 *buf, int16 stride, int16 x1, int16 y1, int16 x2, int16 y2, uint8 colLight, uint8 colDark)
{
	for (int16 x = x1; x < x2; x++) {
		GUI_3DS_PutPixel(buf, stride, x, y1, colLight);
	}
	for (int16 y = y1; y < y2; y++) {
		GUI_3DS_PutPixel(buf, stride, x1, y, colLight);
	}
	for (int16 x = x1; x <= x2; x++) {
		GUI_3DS_PutPixel(buf, stride, x, y2, colDark);
	}
	for (int16 y = y1; y <= y2; y++) {
		GUI_3DS_PutPixel(buf, stride, x2, y, colDark);
	}
}

/* Transliterates UTF-8 Lithuanian letters if needed to ASCII / accented representations */
void GUI_3DS_DrawText(uint8 *buf, int16 stride, int16 x, int16 y, const char *text, uint8 color)
{
	if (text == NULL) return;
	int16 curX = x;
	const unsigned char *p = (const unsigned char *)text;

	while (*p != '\0') {
		unsigned char c = *p++;
		if (c == '\n') {
			y += 9;
			curX = x;
			continue;
		}

		/* Lithuanian UTF-8 decode & transliterate */
		if (c == 0xC4 || c == 0xC5) {
			unsigned char next = *p;
			if (next == 0) break;
			p++;
			if (c == 0xC4) {
				switch (next) {
					case 0x84: case 0x85: c = 'A'; break; /* Ą, ą */
					case 0x8C: case 0x8D: c = 'C'; break; /* Č, č */
					case 0x98: case 0x99: c = 'E'; break; /* Ę, ę */
					case 0x96: case 0x97: c = 'E'; break; /* Ė, ė */
					case 0xAE: case 0xAF: c = 'I'; break; /* Į, į */
					default: c = '?'; break;
				}
			} else if (c == 0xC5) {
				switch (next) {
					case 0xA0: case 0xA1: c = 'S'; break; /* Š, š */
					case 0xB2: case 0xB3: c = 'U'; break; /* Ų, ų */
					case 0xAA: case 0xAB: c = 'U'; break; /* Ū, ū */
					case 0xBD: case 0xBE: c = 'Z'; break; /* Ž, ž */
					default: c = '?'; break;
				}
			}
		}

		if (c < 32 || c > 127) c = 32;
		const uint8 *glyph = s_font8x8_basic[c - 32];

		for (int row = 0; row < 8; row++) {
			uint8 line = glyph[row];
			for (int col = 0; col < 8; col++) {
				if (line & (0x80 >> col)) {
					GUI_3DS_PutPixel(buf, stride, curX + col, y + row, color);
				}
			}
		}
		curX += 8;
	}
}

void GUI_3DS_CenterCameraOnTile(uint16 tileX, uint16 tileY)
{
	const MapInfo *mapInfo = &g_mapInfos[g_scenario.mapScale];
	int16 cx = (int16)tileX - (VIEWPORT_TILES_X / 2);
	int16 cy = (int16)tileY - (VIEWPORT_TILES_Y / 2);

	int16 maxX = (int16)(mapInfo->minX + mapInfo->sizeX - VIEWPORT_TILES_X);
	int16 maxY = (int16)(mapInfo->minY + mapInfo->sizeY - VIEWPORT_TILES_Y);
	if (maxX < (int16)mapInfo->minX) maxX = (int16)mapInfo->minX;
	if (maxY < (int16)mapInfo->minY) maxY = (int16)mapInfo->minY;

	cx = clamp(cx, (int16)mapInfo->minX, maxX);
	cy = clamp(cy, (int16)mapInfo->minY, maxY);

	g_viewportPosition = Tile_PackXY((uint16)cx, (uint16)cy);
	g_viewport_forceRedraw = true;
}

void GUI_3DS_ScrollCamera(int16 dx, int16 dy)
{
	const MapInfo *mapInfo = &g_mapInfos[g_scenario.mapScale];
	uint16 curX = Tile_GetPackedX(g_viewportPosition);
	uint16 curY = Tile_GetPackedY(g_viewportPosition);
	int16 cx = (int16)curX + dx;
	int16 cy = (int16)curY + dy;

	int16 maxX = (int16)(mapInfo->minX + mapInfo->sizeX - VIEWPORT_TILES_X);
	int16 maxY = (int16)(mapInfo->minY + mapInfo->sizeY - VIEWPORT_TILES_Y);
	if (maxX < (int16)mapInfo->minX) maxX = (int16)mapInfo->minX;
	if (maxY < (int16)mapInfo->minY) maxY = (int16)mapInfo->minY;

	cx = clamp(cx, (int16)mapInfo->minX, maxX);
	cy = clamp(cy, (int16)mapInfo->minY, maxY);

	if (cx != curX || cy != curY) {
		g_viewportPosition = Tile_PackXY((uint16)cx, (uint16)cy);
		g_viewport_forceRedraw = true;
	}
}

uint16 GUI_3DS_ScreenPosToTile(int16 screenX, int16 screenY)
{
	if (screenX < 0) screenX = 0;
	if (screenX >= TOP_WIDTH) screenX = TOP_WIDTH - 1;
	if (screenY < 0) screenY = 0;
	if (screenY >= TOP_HEIGHT) screenY = TOP_HEIGHT - 1;

	/* Convert 400x240 screen coords to 240x160 viewport coords */
	int16 vx = (screenX * 3) / 5;
	int16 vy = (screenY * 2) / 3;

	uint16 camX = Tile_GetPackedX(g_viewportPosition);
	uint16 camY = Tile_GetPackedY(g_viewportPosition);

	uint16 tx = camX + (vx / 16);
	uint16 ty = camY + (vy / 16);

	if (tx >= 64) tx = 63;
	if (ty >= 64) ty = 63;

	return Tile_PackXY(tx, ty);
}

bool GUI_3DS_MapPosToScreenPos(tile32 mapPos, int16 *outX, int16 *outY)
{
	uint16 vx = 0, vy = 0;
	bool inViewport = Map_IsPositionInViewport(mapPos, &vx, &vy);

	int16 sx = ((int16)vx * 5) / 3;
	int16 sy = ((int16)vy * 3) / 2;

	if (outX != NULL) *outX = sx;
	if (outY != NULL) *outY = sy;

	return inViewport;
}

/* Classic Dune II mouse cursor arrow (11x15 pixels: 0 = black outline, 254 = white, 255 = transparent) */
static const uint8 s_cursorArrow[15][11] = {
	{0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
	{0,   0, 255, 255, 255, 255, 255, 255, 255, 255, 255},
	{0, 254,   0, 255, 255, 255, 255, 255, 255, 255, 255},
	{0, 254, 254,   0, 255, 255, 255, 255, 255, 255, 255},
	{0, 254, 254, 254,   0, 255, 255, 255, 255, 255, 255},
	{0, 254, 254, 254, 254,   0, 255, 255, 255, 255, 255},
	{0, 254, 254, 254, 254, 254,   0, 255, 255, 255, 255},
	{0, 254, 254, 254, 254, 254, 254,   0, 255, 255, 255},
	{0, 254, 254, 254, 254, 254, 254, 254,   0, 255, 255},
	{0, 254, 254, 254, 254, 254,   0,   0,   0,   0, 255},
	{0, 254, 254,   0, 254, 254,   0, 255, 255, 255, 255},
	{0, 254,   0, 255,   0, 254, 254,   0, 255, 255, 255},
	{0,   0, 255, 255, 255,   0, 254, 254,   0, 255, 255},
	{255, 255, 255, 255, 255, 255,   0, 254, 254,   0, 255},
	{255, 255, 255, 255, 255, 255, 255,   0,   0,   0, 255},
};

static void GUI_3DS_DrawCursor(uint8 *buf, int16 stride, int16 x, int16 y)
{
	uint8 *mask = (stride == TOP_WIDTH) ? g_topScreenUIMask : g_bottomScreenUIMask;
	for (int r = 0; r < 15; r++) {
		int py = y + r;
		if (py < 0 || py >= 240) continue;
		for (int c = 0; c < 11; c++) {
			uint8 pixel = s_cursorArrow[r][c];
			if (pixel != 255) {
				int px = x + c;
				if (px >= 0 && px < stride) {
					buf[py * stride + px] = pixel;
					mask[py * stride + px] = 2;
				}
			}
		}
	}
}

/* Classic Dune II directional scroll arrow (11x12 pixels pointing UP: 0 = black outline, 254 = white, 255 = transparent) */
static const uint8 s_cursorScrollUp[12][11] = {
	{255, 255, 255, 255, 255,   0, 255, 255, 255, 255, 255},
	{255, 255, 255, 255,   0, 254,   0, 255, 255, 255, 255},
	{255, 255, 255,   0, 254, 254, 254,   0, 255, 255, 255},
	{255, 255,   0, 254, 254, 254, 254, 254,   0, 255, 255},
	{255,   0, 254, 254, 254, 254, 254, 254, 254,   0, 255},
	{  0, 254, 254, 254, 254, 254, 254, 254, 254, 254,   0},
	{  0,   0,   0,   0, 254, 254, 254,   0,   0,   0,   0},
	{255, 255, 255,   0, 254, 254, 254,   0, 255, 255, 255},
	{255, 255, 255,   0, 254, 254, 254,   0, 255, 255, 255},
	{255, 255, 255,   0, 254, 254, 254,   0, 255, 255, 255},
	{255, 255, 255,   0, 254, 254, 254,   0, 255, 255, 255},
	{255, 255, 255,   0,   0,   0,   0,   0, 255, 255, 255},
};

/* Diagonal scroll arrow (11x11 pixels pointing UP-LEFT: 0 = black outline, 254 = white, 255 = transparent) */
static const uint8 s_cursorScrollDiag[11][11] = {
	{  0,   0,   0,   0,   0,   0,   0,   0, 255, 255, 255},
	{  0, 254, 254, 254, 254, 254, 254,   0, 255, 255, 255},
	{  0, 254, 254, 254, 254, 254,   0, 255, 255, 255, 255},
	{  0, 254, 254, 254, 254, 254,   0, 255, 255, 255, 255},
	{  0, 254, 254, 254, 254, 254, 254,   0, 255, 255, 255},
	{  0, 254, 254, 254, 254, 254, 254, 254,   0, 255, 255},
	{  0, 254,   0,   0, 254, 254, 254, 254, 254,   0, 255},
	{  0,   0, 255, 255,   0, 254, 254, 254, 254, 254,   0},
	{255, 255, 255, 255, 255,   0, 254, 254, 254,   0, 255},
	{255, 255, 255, 255, 255, 255,   0, 254,   0, 255, 255},
	{255, 255, 255, 255, 255, 255, 255,   0, 255, 255, 255},
};

static void GUI_3DS_DrawScrollCursor(uint8 *buf, int16 stride, int16 x, int16 y, int dx, int dy)
{
	uint8 *mask = (stride == TOP_WIDTH) ? g_topScreenUIMask : g_bottomScreenUIMask;
	if (dx != 0 && dy != 0) {
		int baseX = (dx < 0) ? max(1, (int)x) : min(stride - 12, (int)x - 10);
		int baseY = (dy < 0) ? max(1, (int)y) : min(240 - 12, (int)y - 10);
		for (int r = 0; r < 11; r++) {
			int sr = (dy < 0) ? r : (10 - r);
			int py = baseY + r;
			if (py < 0 || py >= 240) continue;
			for (int c = 0; c < 11; c++) {
				int sc = (dx < 0) ? c : (10 - c);
				uint8 px = s_cursorScrollDiag[sr][sc];
				if (px != 255) {
					int pxX = baseX + c;
					if (pxX >= 0 && pxX < stride) {
						buf[py * stride + pxX] = px;
						mask[py * stride + pxX] = 2;
					}
				}
			}
		}
		return;
	}

	if (dy != 0) {
		int baseX = clamp((int)x - 5, 1, stride - 12);
		int baseY = (dy < 0) ? max(1, (int)y) : min(240 - 13, (int)y - 11);
		for (int r = 0; r < 12; r++) {
			int sr = (dy < 0) ? r : (11 - r);
			int py = baseY + r;
			if (py < 0 || py >= 240) continue;
			for (int c = 0; c < 11; c++) {
				uint8 px = s_cursorScrollUp[sr][c];
				if (px != 255) {
					int pxX = baseX + c;
					if (pxX >= 0 && pxX < stride) {
						buf[py * stride + pxX] = px;
						mask[py * stride + pxX] = 2;
					}
				}
			}
		}
	} else if (dx != 0) {
		int baseX = (dx < 0) ? max(1, (int)x) : min(stride - 13, (int)x - 11);
		int baseY = clamp((int)y - 5, 1, 240 - 12);
		for (int r = 0; r < 11; r++) {
			int py = baseY + r;
			if (py < 0 || py >= 240) continue;
			for (int c = 0; c < 12; c++) {
				int sc = (dx < 0) ? c : (11 - c);
				uint8 px = s_cursorScrollUp[sc][r];
				if (px != 255) {
					int pxX = baseX + c;
					if (pxX >= 0 && pxX < stride) {
						buf[py * stride + pxX] = px;
						mask[py * stride + pxX] = 2;
					}
				}
			}
		}
	}
}

bool g_3dsMentatHelpActive = false;
bool g_3dsMentatExit = false;

bool GUI_3DS_IsCutsceneOrBriefing(void)
{
	if (g_3dsMentatHelpActive) return true;
	/* Production uses MENTAT selection internally, but has its own two-screen UI. */
	if (GUI_3DS_IsFactoryWindow() || g_3dsHousePickerActive || GUI_3DS_IsTitleMenu() || (g_widgetLinkedListTail != NULL)) return false;
	if (s_houseConfirmID != 0xFF) return true;
	if (g_gameMode == GM_MENU && !s_titleActive) return true;
	if (g_selectionType == SELECTIONTYPE_MENTAT ||
	    g_selectionType == SELECTIONTYPE_INTRO ||
	    g_selectionType == SELECTIONTYPE_UNKNOWN6) {
		return true;
	}
	if (g_gameMode != GM_NORMAL && g_gameMode != GM_MENU && g_gameMode != GM_PICKHOUSE) {
		return true;
	}
	return false;
}

bool GUI_3DS_IsTitleMenu(void)
{
	return (s_titleActive && g_gameMode == GM_MENU && g_widgetLinkedListTail == NULL && !g_3dsModalMessageActive);
}

bool GUI_3DS_IsFactoryWindow(void)
{
	return (g_factoryWindowResult == FACTORY_CONTINUE);
}

bool GUI_3DS_IsDialog(void)
{
	if (g_3dsHousePickerActive || s_houseConfirmID != 0xFF) return false;
	return (g_widgetLinkedListTail != NULL || g_gameMode == GM_PICKHOUSE);
}

bool GUI_3DS_IsMenuOrDialogActive(void)
{
	return s_resultsActive || (g_gameMode != GM_NORMAL) ||
	       g_3dsMentatHelpActive ||
	       (g_selectionType == SELECTIONTYPE_MENTAT) ||
	       (g_factoryWindowResult == FACTORY_CONTINUE) ||
	       (g_widgetLinkedListTail != NULL) ||
	       g_3dsModalMessageActive;
}


/* The native engine still owns menu actions and production rules. */

void GUI_3DS_SetMentatButtons(struct Widget *buttons)
{
	s_mentatButtons = buttons;
	s_dialogFocus = 0;
	s_mentatChoice = 0;
	s_dialogChosen = NULL;
	if (buttons != NULL) {
		s_subtitleText[0] = '\0';
	} else {
		s_houseConfirmID = 0xFF;
	}
}

bool GUI_3DS_IsMentatButtonsActive(void)
{
	return s_mentatButtons != NULL;
}

bool GUI_3DS_IsMentatButton(const struct Widget *w)
{
	if (w == NULL) return false;
	if (s_mentatButtons != NULL) {
		for (const struct Widget *cur = s_mentatButtons; cur != NULL; cur = cur->next) {
			if (cur == w) return true;
		}
	}
	if (w->drawParameterNormal.spriteID == 373 ||  /* Yes */
	    w->drawParameterNormal.spriteID == 374 ||  /* Yes pressed */
	    w->drawParameterNormal.spriteID == 375 ||  /* No */
	    w->drawParameterNormal.spriteID == 376 ||  /* No pressed */
	    w->drawParameterNormal.spriteID == 379 ||  /* Proceed */
	    w->drawParameterNormal.spriteID == 380 ||  /* Proceed pressed */
	    w->drawParameterNormal.spriteID == 381 ||  /* Repeat */
	    w->drawParameterNormal.spriteID == 382) {  /* Repeat pressed */
		return true;
	}
	return false;
}

static uint8 s_pauseSnapshot[TOP_WIDTH * TOP_HEIGHT];
static bool s_pauseValid = false;

typedef struct SaveSlotMeta3DS {
	bool   used;
	uint8  houseID;
	uint16 mission;
	char   dateStr[51];
} SaveSlotMeta3DS;

static SaveSlotMeta3DS s_saveSlots[5];
static uint8 s_heraldCrests[3][84 * 64];
static bool s_heraldCrestsValid = false;

void GUI_3DS_SetSaveSlotInfo(int slot, bool used, uint8 houseID, uint16 mission, const char *dateStr)
{
	if (slot < 0 || slot >= 5) return;
	s_saveSlots[slot].used = used;
	s_saveSlots[slot].houseID = (houseID < HOUSE_MAX) ? houseID : HOUSE_ATREIDES;
	s_saveSlots[slot].mission = clamp(mission, 1, 9);
	snprintf(s_saveSlots[slot].dateStr, sizeof(s_saveSlots[slot].dateStr), "%s", dateStr ? dateStr : "");
}

bool GUI_3DS_IsSaveSlotUsed(int slot)
{
	return (slot >= 0 && slot < 5) && s_saveSlots[slot].used;
}

void GUI_3DS_CacheHeraldCrests(const uint8 *herald320x200)
{
	if (herald320x200 == NULL) return;
	for (int i = 0; i < 3; i++) {
		int srcBaseX = 18 + i * 96;
		int srcBaseY = 54;
		for (int cy = 0; cy < 64; cy++) {
			memcpy(s_heraldCrests[i] + cy * 84, herald320x200 + (srcBaseY + cy) * SCREEN_WIDTH + srcBaseX, 84);
		}
	}
	s_heraldCrestsValid = true;
}

bool GUI_3DS_AreHeraldCrestsValid(void)
{
	return s_heraldCrestsValid;
}

void GUI_3DS_SetWindow(struct WindowDesc *window)
{
	/* Keep the last battlefield frame so the top screen can show a paused view behind menus. */
	if (!s_pauseValid && g_gameMode == GM_NORMAL && g_topScreenBuffer != NULL && window != NULL) {
		memcpy(s_pauseSnapshot, g_topScreenBuffer, sizeof(s_pauseSnapshot));
		s_pauseValid = true;
	}
	s_dialogWindow = window;
	s_dialogChosen = NULL;
	s_dialogFocus = 0;
	if (window == &g_saveLoadWindowDesc && window->stringID == STR_SELECT_A_SAVED_GAME_TO_LOAD) {
		for (int i = 0; i < 5; i++) {
			if (s_saveSlots[i].used) {
				s_dialogFocus = i;
				break;
			}
		}
	}
}

uint16 GUI_3DS_TakeDialogEvent(Widget *head)
{
	Widget *chosen = s_dialogChosen;
	s_dialogChosen = NULL;
	if (!chosen) return 0;
	for (Widget *w = head; w; w = w->next) {
		if (w != chosen || w->flags.invisible) continue;
		uint16 result = 0x8000 | w->index;
		w->state.buttonState = 0x11;
		if (w->clickProc) w->clickProc(w);
		return result;
	}
	return 0;
}

static bool CustomDialog(void)
{
	return g_widgetLinkedListTail != NULL && s_dialogWindow != NULL &&
	       s_dialogWindow != &g_savegameNameWindowDesc;
}

static int DialogItems(Widget **items)
{
	int count=0;
	for (Widget *w=s_mentatButtons ? s_mentatButtons : (s_resultsButtons && !CustomDialog()) ? s_resultsButtons : g_widgetLinkedListTail; w && count<9; w=w->next) {
		if (!w->flags.invisible && w->width && w->height) items[count++]=w;
	}
	/* Options menu: "Continue game" belongs below "Quit playing". */
	if (s_dialogWindow == &g_optionsWindowDesc) {
		int ci = -1, qi = -1;
		for (int i = 0; i < count; i++) {
			if (items[i]->stringID == STR_CONTINUE_GAME) ci = i;
			if (items[i]->stringID == STR_QUIT_PLAYING) qi = i;
		}
		if (ci >= 0 && qi >= 0 && ci < qi) {
			Widget *t = items[ci]; items[ci] = items[qi]; items[qi] = t;
		}
	}
	return count;
}


static void MenuButton(int x,int y,int w,int h,const char *label,bool highlight,bool dim);

/* Vertical layout of the dialog list on the bottom screen. */
static void DialogLayout(int *y0, int *step, int *rows)
{
	if (s_dialogWindow == &g_optionsWindowDesc) { *y0 = 25; *step = 27; *rows = 7; }
	else { *y0 = 36; *step = 27; *rows = 6; }
}

static const char *s_titleLabels[6];
static int s_factoryFocus;
static int s_factoryPending = -1;
/* Purchase window state. Focus index: [0,total) = list entry, total = left button (BUY / ORDER),
 * total+1 = BACK (Starport only; the Factory / Construction Yard list has no focusable buttons). */
static int s_factorySub;    /* Starport row sub-focus: 0 = "+", 1 = "-" */
static int s_factoryScroll; /* First visible grid row (Factory) or list row (Starport) */
#define SHOP_GRID_COLS 4
#define SHOP_VISIBLE_ROWS 3
bool g_3dsHousePickerActive = false;
bool g_3dsHouseBack = false;

void GUI_3DS_SetTitleLabels(const char **labels)
{
	for (int i = 0; i < 6; i++) s_titleLabels[i] = labels[i];
}

/* Measure text width in the native Dune II INTRO.FNT (g_fontIntro) */
static int GUI_3DS_GetIntroFontWidth(const char *str)
{
	if (str == NULL) return 0;
	if (g_fontIntro == NULL || g_fontIntro->chars == NULL) return (int)strlen(str) * 8;
	int w = 0;
	for (const unsigned char *p = (const unsigned char *)str; *p != '\0'; p++) {
		unsigned char c = *p;
		if (c < g_fontIntro->count) w += g_fontIntro->chars[c].width + 1;
	}
	return (w > 0) ? (w - 1) : 0;
}

/* Draw ornate text using the native Dune II INTRO.FNT (g_fontIntro) */
static int GUI_3DS_DrawIntroFontText(uint8 *buf, int stride, int x, int y, const char *str, uint8 mainColor, uint8 shadowColor)
{
	if (str == NULL || g_fontIntro == NULL || g_fontIntro->chars == NULL) {
		GUI_3DS_DrawText(buf, stride, x, y, str ? str : "", mainColor);
		return x + (str ? (int)strlen(str) * 8 : 0);
	}
	uint8 *mask = (stride == TOP_WIDTH) ? g_topScreenUIMask : g_bottomScreenUIMask;
	int curX = x;
	for (const unsigned char *p = (const unsigned char *)str; *p != '\0'; p++) {
		unsigned char c = *p;
		if (c >= g_fontIntro->count) continue;
		FontChar *fc = &g_fontIntro->chars[c];
		if (fc->data != NULL && fc->width > 0) {
			for (uint8 row = 0; row < fc->usedLines; row++) {
				int py = y + (int)fc->unusedLines + (int)row;
				if (py < 0 || py >= 240) continue;
				const uint8 *srcLine = fc->data + row * fc->width;
				for (uint8 col = 0; col < fc->width; col++) {
					uint8 px = srcLine[col];
					if (px == 0) continue;
					int pxX = curX + col;
					if (pxX < 0 || pxX >= stride) continue;
					uint8 drawCol = (px == 1) ? mainColor : shadowColor;
					buf[py * stride + pxX] = drawCol;
					mask[py * stride + pxX] = (drawCol >= 110 && drawCol <= 119) ? 1 : 2;
				}
			}
		}
		curX += fc->width + 1;
	}
	return curX;
}

void GUI_3DS_DrawTitleTextTopCentered(int y, const char *str, uint8 mainColor, uint8 shadowColor)
{
	if (g_topScreenBuffer == NULL || str == NULL) return;
	int w = GUI_3DS_GetIntroFontWidth(str);
	int x = max(0, (TOP_WIDTH - w) / 2);
	GUI_3DS_DrawIntroFontText(g_topScreenBuffer, TOP_WIDTH, x, y, str, mainColor, shadowColor);
}

/* Dark slate Dune II menu button for title, mentat, house confirmation, and modal dialogs */
static void MenuButton(int x, int y, int w, int h, const char *label, bool highlight, bool dim)
{
	uint8 *buf = g_bottomScreenBuffer;
	GUI_3DS_FillRect(buf, 320, x, y, x + w - 1, y + h - 1, 0);
	GUI_3DS_DrawBevel(buf, 320, x + 1, y + 1, x + w - 2, y + h - 2, 26, 16);
	GUI_3DS_FillRect(buf, 320, x + 2, y + 2, x + w - 3, y + h - 3, 20);
	if (highlight) {
		GUI_3DS_DrawRect(buf, 320, x + 2, y + 2, x + w - 3, y + h - 3, 27);
	}
	for (int py = max(0, y); py < min(240, y + h); py++) {
		memset(g_bottomScreenUIMask + py * 320 + max(0, x), 2, min(320, x + w) - max(0, x));
	}
	if (label != NULL && label[0] != '\0') {
		uint8 textColor = dim ? 110 : (highlight ? 216 : 15);
		int textLen = 0;
		for (const unsigned char *p = (const unsigned char *)label; *p != '\0'; ) {
			if (*p == 0xC4 || *p == 0xC5) { p += 2; }
			else { p++; }
			textLen++;
		}
		int tx = max(x + 4, x + (w - textLen * 8) / 2);
		int ty = y + (h - 8) / 2;
		GUI_3DS_DrawText(buf, 320, tx + 1, ty + 1, label, 0);
		GUI_3DS_DrawText(buf, 320, tx, ty, label, textColor);
	}
}

/* Draw word-wrapped, horizontally & vertically centered subtitle text on the 320x240 bottom screen */
static void GUI_3DS_DrawCenteredSubtitle(const char *text, int centerY)
{
	if (text == NULL || text[0] == '\0') return;
	char lines[10][42];
	int numLines = 0;
	const char *p = text;

	while (*p != '\0' && numLines < 10) {
		while (*p == ' ') p++;
		if (*p == '\0') break;
		if (*p == '\r' || *p == '\n') {
			p++;
			continue;
		}
		int maxChars = 34;
		int len = 0;
		int lastSpace = -1;
		while (p[len] != '\0' && p[len] != '\r' && p[len] != '\n' && len < maxChars) {
			if (p[len] == ' ') lastSpace = len;
			len++;
		}
		if (p[len] != '\0' && p[len] != '\r' && p[len] != '\n' && lastSpace > 0) {
			len = lastSpace;
		}
		memcpy(lines[numLines], p, len);
		lines[numLines][len] = '\0';
		numLines++;
		p += len;
		if (*p == '\r') p++;
		if (*p == '\n') p++;
	}

	int lineStep = 16;
	int startY = centerY - (numLines * lineStep) / 2 + 4;
	for (int i = 0; i < numLines; i++) {
		int lineW = (int)strlen(lines[i]) * 8;
		int tx = max(8, (BOTTOM_WIDTH - lineW) / 2);
		int ty = startY + i * lineStep;
		GUI_3DS_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, tx, ty, lines[i], 15);
	}
}


static void EmitKey(uint16 scan)
{
	Input_EventHandler(scan);
	Input_EventHandler(scan | 0x80);
}

bool GUI_3DS_CustomMenuActive(void)
{
	if (g_3dsModalMessageActive) return false;
	return s_resultsButtons != NULL || g_3dsMentatHelpActive || s_mentatButtons != NULL || CustomDialog() || GUI_3DS_IsTitleMenu() || g_3dsHousePickerActive || GUI_3DS_IsFactoryWindow();
}

bool GUI_3DS_CancelMenu(void)
{
	if (g_3dsModalMessageActive) return false;
	if (s_resultsButtons != NULL && !CustomDialog()) {
		for (Widget *w = s_resultsButtons; w != NULL; w = w->next) {
			if (w->index == 101) {
				s_dialogChosen = w;
				return true;
			}
		}
		s_dialogChosen = s_resultsButtons;
		return true;
	}
	if (CustomDialog()) {
		Widget *items[9];
		int count = DialogItems(items);
		if (count <= 0) return false;
		for (int i = count - 1; i >= 0; i--) {
			int16 sid = items[i]->stringID;
			if (sid == STR_CANCEL || sid == STR_CONTINUE_GAME || sid == STR_PREVIOUS || sid == STR_NO || items[i]->index == 0x23 || items[i]->index == 0x1F) {
				s_dialogChosen = items[i];
				return true;
			}
		}
		s_dialogChosen = items[count - 1];
		return true;
	}
	return false;
}

void GUI_3DS_MenuInput(int dx, int dy, bool confirm)
{
 if(g_3dsMentatHelpActive && !s_mentatButtons) {
  Mouse_EventHandler(0,0,false,false);
  if(dy || dx) g_3dsMentatMove=dy ? dy : dx;
  if(confirm) { const char *rows[11]; bool header[11]; int selected=GUI_Mentat_3DS_GetRows(rows,header); if(selected>=0 && selected<11 && !header[selected]) g_3dsMentatTapRow=selected; }
  return;
 }
 if(s_resultsButtons && !CustomDialog()) {
  s_dialogFocus = 0;
  if(confirm) {
   for(Widget *w=s_resultsButtons;w;w=w->next) if(w->index==101) s_dialogChosen=w;
   if(!s_dialogChosen) s_dialogChosen=s_resultsButtons;
  }
  return;
 }

	if (s_mentatButtons) {
		if (dx != 0 || dy != 0) {
			s_dialogFocus = (s_dialogFocus + dx + dy + 2) % 2;
		}
		if (confirm) {
			s_mentatChoice = (s_dialogFocus == 0) ? 0x8001 : 0x8002;
			s_dialogChosen = (s_dialogFocus == 0) ? s_mentatButtons : (s_mentatButtons->next ? s_mentatButtons->next : s_mentatButtons);
		}
	} else if (CustomDialog()) {
		Widget *items[9]; int count=DialogItems(items);
		if (!count) return;
		s_dialogFocus=(s_dialogFocus+dx+dy+count)%count;
        if(confirm) s_dialogChosen=items[s_dialogFocus];

	} else if (g_3dsHousePickerActive) {
		if (s_houseConfirmID != 0xFF) return;
		if (dx != 0 || dy != 0) {
			s_houseFocus = (s_houseFocus + dx + dy + 3) % 3;
		}
		if (confirm) {
			s_houseChoice = 0x8001 + s_houseFocus;
		}
	} else if (GUI_3DS_IsTitleMenu()) {
		if (dy || dx) {
			/* Keep mouse outside the native menu so it cannot undo keyboard focus. */
			Mouse_EventHandler(0, 0, false, false);
			EmitKey(dy < 0 || dx < 0 ? 0x48 : 0x50);
		}
		if (confirm) EmitKey(0x1C);
	} else if (GUI_3DS_IsFactoryWindow()) {
		Mouse_EventHandler(0,0,false,false);
		int total = g_factoryWindowTotal;
		if (total <= 0) return;
		if (!g_factoryWindowStarport) {
			/* Factory / Construction Yard: a plain grid. The BUY and BACK buttons are never
			 * focusable, so "A" always buys whatever is highlighted. */
			int f = clamp(s_factoryFocus, 0, total - 1);
			if (dx) f = clamp(f + dx, 0, total - 1);
			if (dy < 0 && f >= SHOP_GRID_COLS) f -= SHOP_GRID_COLS;
			if (dy > 0 && (f / SHOP_GRID_COLS + 1) * SHOP_GRID_COLS < total) f = min(f + SHOP_GRID_COLS, total - 1);
			s_factoryFocus = f;
			s_factoryPending = f;
			if (confirm) s_factoryPending = total; /* Buy / build the highlighted item */
		} else {
			/* Starport: one entry per row with "+" / "-"; walking past the last row lands on
			 * ORDER (under "+") or BACK (under "-"). */
			int f = s_factoryFocus;
			if (f < total) {
				if (dx < 0) s_factorySub = 0;
				else if (dx > 0) s_factorySub = 1;
				if (dy < 0) f = max(0, f - 1);
				else if (dy > 0) f = (f + 1 < total) ? f + 1 : total + s_factorySub;
			} else {
				if (dy < 0) { s_factorySub = (f == total + 1) ? 1 : 0; f = total - 1; }
				else if (dx) f = clamp(f + dx, total, total + 1);
			}
			s_factoryFocus = f;
			if (f < total) s_factoryPending = f;
			if (confirm) {
				if (f < total) s_factoryPending = s_factorySub ? total + 4 : total + 3; /* -1 / +1 */
				else if (f == total) { if (g_factoryWindowOrdered > 0) s_factoryPending = total; } /* ORDER */
				else s_factoryPending = total + 1;                                              /* BACK */
			}
		}
	}
}

void GUI_3DS_FactoryUpgrade(void)
{
	if (!GUI_3DS_IsFactoryWindow()) return;
	if (g_factoryWindowStarport) {
		/* In Starport, X sends the order immediately if items are ordered */
		if (g_factoryWindowOrdered > 0) s_factoryPending = g_factoryWindowTotal;
	} else if (g_factoryWindowUpgradeCost) {
		s_factoryPending = g_factoryWindowTotal + 2;
	}
}

void GUI_3DS_FactoryBegin(void)
{
	s_factoryFocus = 0;
	s_factoryPending = -1;
	s_factorySub = 0;
	s_factoryScroll = 0;

	Structure *st = Unit_3DS_GetSelectedStructure();
	if (st != NULL && st->o.houseID == g_playerHouseID && !g_factoryWindowStarport) {
		uint16 target = st->objectType;
		for (int i = 0; i < g_factoryWindowTotal; i++) {
			if (g_factoryWindowItems[i].objectType == target) {
				s_factoryFocus = i;
				s_factoryScroll = (i / SHOP_GRID_COLS) * SHOP_GRID_COLS;
				g_factoryWindowBase = i > 3 ? i - 3 : 0;
				g_factoryWindowSelected = i - g_factoryWindowBase;
				GUI_FactoryWindow_DrawDetails();
				break;
			}
		}
	}
}

int GUI_3DS_GetFactoryFocus(void)
{
	return (s_factoryFocus < g_factoryWindowTotal) ? s_factoryFocus : 0;
}

void GUI_3DS_FactoryEvents(void)
{
	int pending = s_factoryPending;
	s_factoryPending = -1;
	if (pending < 0) return;
	int total = g_factoryWindowTotal;
	if (pending < total || ((pending == total || pending == total + 3 || pending == total + 4) && s_factoryFocus < total)) {
		int selection = pending < total ? pending : s_factoryFocus;
		/* Preserve the DOS four-item window invariants for engine callbacks. */
		g_factoryWindowBase = selection > 3 ? selection - 3 : 0;
		g_factoryWindowSelected = selection - g_factoryWindowBase;
		GUI_FactoryWindow_DrawDetails();
	}
	if (pending == total) {
		/* Buy / build the highlighted item (Factory, Yard) or send the order (Starport). */
		if (g_factoryWindowStarport) {
			if (g_factoryWindowOrdered > 0) GUI_Production_BuildThis_Click(GUI_Widget_Get_ByIndex(g_widgetInvoiceTail, 56));
		} else {
			GUI_Production_BuildThis_Click(GUI_Widget_Get_ByIndex(g_widgetInvoiceTail, 57));
		}
	} else if (pending == total + 1) {
		GUI_Production_ResumeGame_Click(NULL);
	} else if (pending == total + 2 && g_factoryWindowUpgradeCost) {
		GUI_Production_Upgrade_Click(GUI_Widget_Get_ByIndex(g_widgetInvoiceTail, 53));
	} else if (pending == total + 3 && g_factoryWindowStarport) {
		GUI_Purchase_Plus_Click(GUI_Widget_Get_ByIndex(g_widgetInvoiceTail, 54));
	} else if (pending == total + 4 && g_factoryWindowStarport) {
		GUI_Purchase_Minus_Click(GUI_Widget_Get_ByIndex(g_widgetInvoiceTail, 55));
	}
}

static void DialogBottom(void)
{
	Widget *items[9]; int count = DialogItems(items);
	int y0, step, rows; DialogLayout(&y0, &step, &rows);
	if (count) s_dialogFocus = min(s_dialogFocus, count - 1);
	memset(g_bottomScreenBuffer, 0, BOTTOM_WIDTH * BOTTOM_HEIGHT);
	memset(g_bottomScreenUIMask, 2, sizeof(g_bottomScreenUIMask));

	if (s_dialogWindow != &g_optionsWindowDesc) {
		const char *heading = GUI_String_Get_ByIndex(s_dialogWindow->stringID);
		if (heading != NULL && heading[0] != '\0') {
			int headLen = 0;
			for (const unsigned char *p = (const unsigned char *)heading; *p != '\0';) {
				if (*p == 0xC4 || *p == 0xC5) { p += 2; }
				else { p++; }
				headLen++;
			}
			int headX = max(8, (BOTTOM_WIDTH - headLen * 8) / 2);
			GUI_3DS_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, headX, 14, heading, 15);
		}
	}

	int first = max(0, s_dialogFocus - (rows - 1));
	for (int i = first; i < count && i < first + rows; i++) {
		int y = y0 + (i - first) * step;
		const char *label = GUI_String_Get_ByIndex(items[i]->stringID);
		char combined[128];
		for (int j = 0; j < s_dialogWindow->widgetCount; j++) {
			if (items[i] != &g_table_windowWidgets[j] || s_dialogWindow->widgets[j].labelStringId == STR_NULL) continue;
			const char *prefix = GUI_String_Get_ByIndex(s_dialogWindow->widgets[j].labelStringId);
			snprintf(combined, sizeof(combined), "%.60s: %.60s", prefix ? prefix : "", label ? label : "");
			label = combined;
		}
		if (s_dialogWindow == &g_optionsWindowDesc && items[i]->stringID == STR_CONTINUE_GAME && label) {
			snprintf(combined, sizeof(combined), "%.28s (B)", label);
			label = combined;
		} else if (items[i]->stringID == STR_CANCEL || items[i]->index == 0x23) {
			snprintf(combined, sizeof(combined), "%s", L10N("Back (B)", "Atgal (B)"));
			label = combined;
		}
		if (label == NULL) continue;

		bool selected = (i == s_dialogFocus);
		bool dim = (s_dialogWindow != NULL && s_dialogWindow->stringID == STR_SELECT_A_SAVED_GAME_TO_LOAD && i < 5 && !s_saveSlots[i].used);
		MenuButton(12,y,296,step-3,label,selected,dim);
	}
}

void GUI_3DS_FactoryMinus(void)
{
	if (GUI_3DS_IsFactoryWindow() && g_factoryWindowStarport) {
		s_factoryPending = g_factoryWindowTotal + 4;
	}
}

static inline uint8 ShopHouseColor(void)
{
	if (g_playerHouseID == HOUSE_ATREIDES) return SP_SEL_BLUE;
	if (g_playerHouseID == HOUSE_ORDOS)    return SP_SEL_GREEN;
	return SP_SEL_RED; /* Harkonnen and fallback */
}

static inline uint8 ShopHouseTint(void)
{
	if (g_playerHouseID == HOUSE_ATREIDES) return SP_TINT_BLUE;
	if (g_playerHouseID == HOUSE_ORDOS)    return SP_TINT_GREEN;
	return SP_TINT_RED;
}

/* Dedicated shop button per Figma node 150:397.
 * 2px outer frame (or House selection color when active),
 * 2px inner top/left highlight #FFB200, 2px inner bottom/right shadow #A36600,
 * center fill #D68B00, and Tiny5 white text with 2px black drop shadow. */
static void ShopButton(int x, int y, int w, int h, const char *label, bool selected, bool dim)
{
	uint8 frameCol = selected ? ShopHouseColor() : SP_FRAME;
	for (int dy = 0; dy < h; dy++) {
		int py = y + dy;
		if (py < 0 || py >= BOTTOM_HEIGHT) continue;
		for (int dx = 0; dx < w; dx++) {
			int px = x + dx;
			if (px < 0 || px >= BOTTOM_WIDTH) continue;
			uint8 col;
			if (dx < 2 || dx >= w - 2 || dy < 2 || dy >= h - 2) {
				col = frameCol;
			} else if (dx < 4 || dy < 4) {
				col = SP_BTN_HL;
			} else if (dx >= w - 4 || dy >= h - 4) {
				col = SP_BTN_SH;
			} else {
				col = SP_BTN_FILL;
			}
			g_bottomScreenBuffer[py * BOTTOM_WIDTH + px] = col;
			g_bottomScreenUIMask[py * BOTTOM_WIDTH + px] = SHOP_UI_MASK;
		}
	}
	if (label && *label) {
		uint8 textColor = dim ? SP_TEXT_DIM : SP_WHITE;
		int baseY = y + h / 2 + 4;
		Tiny5_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, x + w / 2, baseY, label, textColor, true);
	}
}

static void ShopTileIcon(int x, int y, uint16 spriteID)
{
	uint8 *sprite = g_sprites[spriteID];
	if (sprite == NULL) return;
	int w = Sprite_GetWidth(sprite), h = Sprite_GetHeight(sprite);
	if (w > 64 || h > 64) return;
	uint8 saved[64 * 64];
	uint8 *scratch = GFX_Screen_Get_ByIndex(SCREEN_2);
	if (scratch == NULL) return;
	for (int row = 0; row < h; row++) {
		memcpy(saved + row * w, scratch + row * 320, w);
		memset(scratch + row * 320, 0, w);
	}
	GUI_DrawSprite(SCREEN_2, sprite, 0, 0, 0, 0);
	int offX = (w < 32) ? (32 - w) / 2 : 0;
	int offY = (h < 24) ? (24 - h) / 2 : 0;
	for (int row = 0; row < h; row++) {
		for (int col = 0; col < w; col++) {
			uint8 px = scratch[row * 320 + col];
			if (px != 0) {
				int dx = x + (offX + col) * 2;
				int dy = y + (offY + row) * 2;
				for (int sy = 0; sy < 2; sy++) {
					for (int sx = 0; sx < 2; sx++) {
						int fx = dx + sx, fy = dy + sy;
						if (fx >= x + 2 && fx < x + 62 && fy >= y + 2 && fy < y + 46) {
							g_bottomScreenBuffer[fy * BOTTOM_WIDTH + fx] = px;
							g_bottomScreenUIMask[fy * BOTTOM_WIDTH + fx] = 0; /* DOS palette for sprite */
						}
					}
				}
			}
		}
	}
	for (int row = 0; row < h; row++) {
		memcpy(scratch + row * 320, saved + row * w, w);
	}
}

static void FactoryBottom(void)
{
	memcpy(g_bottomScreenBuffer, s_shopBotBG, 320 * 240);
	memset(g_bottomScreenUIMask, 0, sizeof(g_bottomScreenUIMask));

	int total = g_factoryWindowTotal;
	if (total <= 0) return;

	if (!g_factoryWindowStarport) {
		/* --- FACTORY / CONSTRUCTION YARD: 4x3 Grid --- */
		int focusRow = s_factoryFocus / SHOP_GRID_COLS;
		if (focusRow < s_factoryScroll) s_factoryScroll = focusRow;
		if (focusRow >= s_factoryScroll + SHOP_VISIBLE_ROWS) {
			s_factoryScroll = focusRow - SHOP_VISIBLE_ROWS + 1;
		}
		int maxScroll = max(0, (total + SHOP_GRID_COLS - 1) / SHOP_GRID_COLS - SHOP_VISIBLE_ROWS);
		s_factoryScroll = clamp(s_factoryScroll, 0, maxScroll);

		for (int r = 0; r < SHOP_VISIBLE_ROWS; r++) {
			int gridY = 30 + r * 52;
			for (int c = 0; c < SHOP_GRID_COLS; c++) {
				int gridX = 21 + c * 71;
				int idx = (s_factoryScroll + r) * SHOP_GRID_COLS + c;
				if (idx >= total) continue;

				bool isSel = (s_factoryFocus == idx);
				uint8 borderCol = isSel ? ShopHouseColor() : SP_WHITE;

				/* 2px outer border + SP_TILE_FACE background */
				for (int dy = 0; dy < 48; dy++) {
					int py = gridY + dy;
					for (int dx = 0; dx < 64; dx++) {
						int px = gridX + dx;
						bool isBorder = (dx < 2 || dx >= 62 || dy < 2 || dy >= 46);
						g_bottomScreenBuffer[py * BOTTOM_WIDTH + px] = isBorder ? borderCol : SP_TILE_FACE;
						g_bottomScreenUIMask[py * BOTTOM_WIDTH + px] = SHOP_UI_MASK;
					}
				}

				/* 2x Centered icon */
				if (g_factoryWindowItems[idx].objectInfo) {
					ShopTileIcon(gridX, gridY, g_factoryWindowItems[idx].objectInfo->spriteID);
				}
			}
		}

		/* Bottom buttons: BUY (A) and BACK (B) - visual action hints, unfocusable */
		ShopButton(19, 206, 141, 27, L10N("BUY (A)", "PIRKTI (A)"), false, false);
		ShopButton(160, 206, 141, 27, L10N("BACK (B)", "ATGAL (B)"), false, false);

	} else {
		/* --- STARPORT: 1 unit per row with +, - buttons and 25% House tint on active row --- */
		int focusIdx = (s_factoryFocus < total) ? s_factoryFocus : (total - 1);
		if (focusIdx < s_factoryScroll) s_factoryScroll = focusIdx;
		if (focusIdx >= s_factoryScroll + SHOP_VISIBLE_ROWS) {
			s_factoryScroll = focusIdx - SHOP_VISIBLE_ROWS + 1;
		}
		int maxScroll = max(0, total - SHOP_VISIBLE_ROWS);
		s_factoryScroll = clamp(s_factoryScroll, 0, maxScroll);

		for (int r = 0; r < SHOP_VISIBLE_ROWS; r++) {
			int idx = s_factoryScroll + r;
			if (idx >= total) continue;

			int rowY = 20 + r * 54;
			bool isCurrentRow = (s_factoryFocus == idx);

			/* 25% House tint for active row */
			if (isCurrentRow) {
				uint8 tint = ShopHouseTint();
				for (int dy = 0; dy < 54; dy++) {
					int py = rowY + dy;
					if (py >= 20 && py <= 181) {
						for (int px = 16; px <= 303; px++) {
							g_bottomScreenBuffer[py * BOTTOM_WIDTH + px] = tint;
							g_bottomScreenUIMask[py * BOTTOM_WIDTH + px] = SHOP_UI_MASK;
						}
					}
				}
			}

			/* 64x48 Unit tile at X: 21, Y: rowY + 3 */
			int iconX = 21, iconY = rowY + 3;
			for (int dy = 0; dy < 48; dy++) {
				int py = iconY + dy;
				for (int dx = 0; dx < 64; dx++) {
					int px = iconX + dx;
					bool isBorder = (dx < 2 || dx >= 62 || dy < 2 || dy >= 46);
					g_bottomScreenBuffer[py * BOTTOM_WIDTH + px] = isBorder ? SP_WHITE : SP_TILE_FACE;
					g_bottomScreenUIMask[py * BOTTOM_WIDTH + px] = SHOP_UI_MASK;
				}
			}
			if (g_factoryWindowItems[idx].objectInfo) {
				ShopTileIcon(iconX, iconY, g_factoryWindowItems[idx].objectInfo->spriteID);
			}

			/* Quantity text: "X of Max" centered around X: 148, Y: rowY + 31 */
			char qtyStr[32];
			int avail = g_factoryWindowItems[idx].objectInfo ? g_factoryWindowItems[idx].objectInfo->available : 0;
			snprintf(qtyStr, sizeof(qtyStr), "%u %s %d",
				g_factoryWindowItems[idx].amount,
				L10N("of", "is"),
				avail > 0 ? avail : 0);
			for (int py = rowY + 16; py < rowY + 44; py++) {
				for (int px = 90; px < 210; px++) {
					if (!isCurrentRow) {
						g_bottomScreenBuffer[py * BOTTOM_WIDTH + px] = SP_SLOT;
						g_bottomScreenUIMask[py * BOTTOM_WIDTH + px] = SHOP_UI_MASK;
					}
				}
			}
			Tiny5_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, 148, rowY + 31, qtyStr, SP_WHITE, true);

			/* "+" button at X: 213, Y: rowY + 13, W: 40, H: 27 */
			bool plusSel = (isCurrentRow && s_factorySub == 0);
			bool plusDim = (avail <= 0 || (int)g_factoryWindowItems[idx].amount >= avail);
			ShopButton(213, rowY + 13, 40, 27, "+", plusSel, plusDim);

			/* "-" button at X: 259, Y: rowY + 13, W: 40, H: 27 */
			bool minusSel = (isCurrentRow && s_factorySub == 1);
			bool minusDim = (g_factoryWindowItems[idx].amount == 0);
			ShopButton(259, rowY + 13, 40, 27, "-", minusSel, minusDim);
		}

		/* Bottom buttons: ORDER and BACK (B) */
		bool orderSel = (s_factoryFocus == total);
		bool orderDim = (g_factoryWindowOrdered == 0);
		ShopButton(19, 206, 141, 27, L10N("ORDER", "UZSAKYTI"), orderSel, orderDim);

		bool backSel = (s_factoryFocus == total + 1);
		ShopButton(160, 206, 141, 27, L10N("BACK (B)", "ATGAL (B)"), backSel, false);
	}
}

static void FactoryTop(void)
{
	memcpy(g_topScreenBuffer, s_shopTopBG, 400 * 240);
	memset(g_topScreenUIMask, 0, sizeof(g_topScreenUIMask));

	int chosen = (s_factoryFocus < g_factoryWindowTotal) ? s_factoryFocus : (g_factoryWindowBase + g_factoryWindowSelected);
	FactoryWindowItem *item = (chosen >= 0 && chosen < g_factoryWindowTotal) ? &g_factoryWindowItems[chosen] : NULL;
	if (!item || !item->objectInfo) return;

	/* 1. Credits counter (top-right cells at X: 246..317, Y: 10..19) */
	uint32 credVal = g_playerHouse ? g_playerHouse->credits : 0;
	char credStr[16];
	snprintf(credStr, sizeof(credStr), "%u", (unsigned)credVal);
	int cw = (int)strlen(credStr) * 8;
	int credX = 315 - cw;
	if (credX < 246) credX = 246;
	GUI_3DS_DrawText(g_topScreenBuffer, 400, credX, 11, credStr, 15);

	/* 2. WSA animation preview centered in frame X: 89..309 (W=221), Y: 41..175 (H=135) */
	uint8 *src = GFX_Screen_Get_ByIndex(SCREEN_1);
	if (src != NULL) {
		int wsaX = 107; /* 89 + (221 - 184) / 2 */
		int wsaY = 52;  /* 41 + (135 - 112) / 2 */
		for (int y = 0; y < 112; y++) {
			memcpy(g_topScreenBuffer + (wsaY + y) * 400 + wsaX, src + (48 + y) * 320 + 128, 184);
			memset(g_topScreenUIMask + (wsaY + y) * 400 + wsaX, 0, 184);
		}
	}

	/* 3. Bottom plaque: X: 86..315 (W=230), Y: 194..229 (H=36) */
	/* Line 1: Item Name - Cost */
	const char *name = String_Get_ByIndex(item->objectInfo->stringID_full);
	char line1[64];
	if (item->objectInfo->available < 0) {
		snprintf(line1, sizeof(line1), "%s (%s)", name ? name : "", L10N("UPGRADE REQUIRED", "REIKIA TOBULINTI"));
	} else {
		snprintf(line1, sizeof(line1), "%s - %u", name ? name : "", item->credits);
	}
	int len1 = 0;
	for (const unsigned char *p = (const unsigned char *)line1; *p; ) {
		if (*p == 0xC4 || *p == 0xC5) p += 2; else p++;
		len1++;
	}
	int tx1 = max(90, 86 + (230 - len1 * 8) / 2);
	GUI_3DS_DrawText(g_topScreenBuffer, 400, tx1 + 1, 200, line1, 0);
	GUI_3DS_DrawText(g_topScreenBuffer, 400, tx1, 199, line1, 15);

	/* Line 2: Stats / Description (no repeated quantity in Starport) */
	char line2[64];
	line2[0] = '\0';
	if (g_factoryWindowConstructionYard) {
		if (item->objectType < STRUCTURE_MAX) {
			const StructureInfo *si = &g_table_structureInfo[item->objectType];
			snprintf(line2, sizeof(line2), L10N("Armor: %u  Power: %+d", "Sarvai: %u  Energija: %+d"),
				si->o.hitpoints, -si->powerUsage);
		}
	} else if (item->objectType < UNIT_MAX) {
		const UnitInfo *ui = &g_table_unitInfo[item->objectType];
		snprintf(line2, sizeof(line2), L10N("HP: %u  Pow: %u  Spd: %u", "HP: %u  Galia: %u  Grt: %u"),
			ui->o.hitpoints, ui->damage, ui->movingSpeedFactor);
	}
	if (line2[0] != '\0') {
		int len2 = 0;
		for (const unsigned char *p = (const unsigned char *)line2; *p; ) {
			if (*p == 0xC4 || *p == 0xC5) p += 2; else p++;
			len2++;
		}
		int tx2 = max(90, 86 + (230 - len2 * 8) / 2);
		GUI_3DS_DrawText(g_topScreenBuffer, 400, tx2 + 1, 215, line2, 0);
		GUI_3DS_DrawText(g_topScreenBuffer, 400, tx2, 214, line2, 110);
	}
}

void GUI_3DS_RenderTopScreen(void)
{
	if (g_topScreenBuffer == NULL || s_launchActive) return;
 if(g_3dsModalMessageActive) {
  memcpy(g_topScreenBuffer,s_messageTop,sizeof(s_messageTop));
  memcpy(g_topScreenUIMask,s_messageTopMask,sizeof(s_messageTopMask)); return;
 }
 if(s_resultsActive && !CustomDialog()) {
  memset(g_topScreenBuffer,0,400*240); memset(g_topScreenUIMask,0,sizeof(g_topScreenUIMask));
  const uint8 *screen=GFX_Screen_Get_ByIndex(SCREEN_0);
  if(screen) for(int y=0;y<200;y++) memcpy(g_topScreenBuffer+(y+20)*400+40,screen+y*320,320);
  return;
 }
	memset(g_topScreenUIMask, 0, sizeof(g_topScreenUIMask));

	uint8 *src = GFX_Screen_Get_ByIndex(SCREEN_0);

	/* 0. House Selection Screen: Large Crest on Left, Ornate House Name + Description on Right */
	if (g_3dsHousePickerActive) {
		memset(g_topScreenBuffer, 0, TOP_WIDTH * TOP_HEIGHT);
		memset(g_topScreenUIMask, 2, sizeof(g_topScreenUIMask));

		uint8 *herald = GFX_Screen_Get_ByIndex(SCREEN_1);
		if (herald == NULL) herald = src;
		if (herald != NULL) GUI_3DS_CacheHeraldCrests(herald);
		int focus = clamp(s_houseFocus, 0, 2);

		/* Extract focused House crest square (84x64 at srcX = 18 + focus*96, srcY = 54) and scale 2x (168x128) */
		if (herald != NULL) {
			int srcBaseX = 18 + focus * 96;
			int srcBaseY = 54;
			int crestW = 84;
			int crestH = 64; /* 64 rows excludes the hanging loops / feet */
			int dstBaseX = 14;
			int dstBaseY = (TOP_HEIGHT - crestH * 2) / 2; /* 56 */
			for (int cy = 0; cy < crestH; cy++) {
				const uint8 *srcRow = herald + (srcBaseY + cy) * SCREEN_WIDTH + srcBaseX;
				for (int cx = 0; cx < crestW; cx++) {
					uint8 px = srcRow[cx];
					int dx = dstBaseX + cx * 2;
					int dy = dstBaseY + cy * 2;
					g_topScreenBuffer[dy * TOP_WIDTH + dx] = px;
					g_topScreenBuffer[dy * TOP_WIDTH + dx + 1] = px;
					g_topScreenBuffer[(dy + 1) * TOP_WIDTH + dx] = px;
					g_topScreenBuffer[(dy + 1) * TOP_WIDTH + dx + 1] = px;
					g_topScreenUIMask[dy * TOP_WIDTH + dx] = 0;
					g_topScreenUIMask[dy * TOP_WIDTH + dx + 1] = 0;
					g_topScreenUIMask[(dy + 1) * TOP_WIDTH + dx] = 0;
					g_topScreenUIMask[(dy + 1) * TOP_WIDTH + dx + 1] = 0;
				}
			}
			/* Solid black rectangle beneath the crest to guarantee no feet or artifacts show */
			int bottomCoverY = dstBaseY + crestH * 2 - 2;
			if (bottomCoverY < TOP_HEIGHT) {
				memset(g_topScreenBuffer + bottomCoverY * TOP_WIDTH, 0, (TOP_HEIGHT - bottomCoverY) * TOP_WIDTH);
			}
		}

		/* Right Side: House Name in Ornate INTRO.FNT font + House Description */
		static const char *houseNames[3] = {"Atreides", "Ordos", "Harkonnen"};
		static const char *houseDescEN[3] = {
			"The noble House of\nCaladan.\n\nMasters of air power\nwith Ornithopters,\nSonic Tanks, and\nelite Fremen warriors.",
			"The wealthy cartel of\nSigma Draconis.\n\nFields fast Raider\ntrikes, Saboteurs, and\nDeviator gas tanks that\nmind-control enemies.",
			"The brutal House of\nGiedi Prime.\n\nCommands crushing\nheavy armor, Devastator\ntanks, and apocalyptic\nDeath Hand missiles."
		};
		static const char *houseDescLT[3] = {
			"Kilminga Kaladano\ngimine.\n\nStipri aviacija\n(Ornitopteriai), garso\ntankai ir elitiniai\nFremenu kariai.",
			"Turtingas Sigma\nDraconis kartelis.\n\nGreiti Raider triraciai,\nsabotazistai ir Deviator\ntankai, uzvaldantys\nprieso karius.",
			"Ziauri Giedi Prime\ngimine.\n\nGalinga sarvuota\ntechnika, Devastator\ntankai ir Mirties\nRankos raketos."
		};

		GUI_3DS_DrawIntroFontText(g_topScreenBuffer, TOP_WIDTH, 198, 74, houseNames[focus], 119, 112);
		GUI_3DS_DrawText(g_topScreenBuffer, TOP_WIDTH, 198, 100, L10N(houseDescEN[focus], houseDescLT[focus]), 15);
		return;
	}

	/* 1. Cutscenes, Intro Logos, Mentat Briefing & End Game: top screen only with pure black border */
	if (GUI_3DS_IsCutsceneOrBriefing() && !g_3dsHousePickerActive) {
		memset(g_topScreenBuffer, 0, TOP_WIDTH * TOP_HEIGHT);
		if (src != NULL) {
			int offsetX = (TOP_WIDTH - SCREEN_WIDTH) / 2;   /* 40 */
			int offsetY = (TOP_HEIGHT - SCREEN_HEIGHT) / 2; /* 20 */
			for (int y = 0; y < SCREEN_HEIGHT; y++) {
				memcpy(g_topScreenBuffer + ((offsetY + y) * TOP_WIDTH + offsetX), src + y * SCREEN_WIDTH, SCREEN_WIDTH);
			}
			if (g_3dsMentatHelpActive && s_mentatButtons == NULL) {
				/* Help subject list lives on the bottom screen: keep the top screen to the Mentat only. */
				GUI_3DS_FillRect(g_topScreenBuffer, TOP_WIDTH, offsetX + 115, 0, TOP_WIDTH - 1, TOP_HEIGHT - 1, 0);
			}
		}
		return;
	}

	/* 2. Title Screen: show planet background with Dune 2 title artwork overlaid on top screen */
	if (GUI_3DS_IsTitleMenu()) {
		memset(g_topScreenBuffer, 0, TOP_WIDTH * TOP_HEIGHT);
		int offsetX = (TOP_WIDTH - SCREEN_WIDTH) / 2;   /* 40 */
		int offsetY = (TOP_HEIGHT - SCREEN_HEIGHT) / 2; /* 20 */
		if (s_planetBgValid) {
			for (int y = 0; y < SCREEN_HEIGHT; y++) {
				const uint8 *pRow = s_planetBg + y * SCREEN_WIDTH;
				uint8 *dstRow = g_topScreenBuffer + (offsetY + y) * TOP_WIDTH + offsetX;
				for (int x = 0; x < SCREEN_WIDTH; x++) {
					if (pRow[x] != 0) dstRow[x] = pRow[x];
				}
			}
		}
		uint8 *art = GFX_Screen_Get_ByIndex(SCREEN_1);
		if (art == NULL) art = src;
		if (art != NULL) {
			for (int y = 0; y < SCREEN_HEIGHT; y++) {
				const uint8 *aRow = art + y * SCREEN_WIDTH;
				uint8 *dstRow = g_topScreenBuffer + (offsetY + y) * TOP_WIDTH + offsetX;
				for (int x = 0; x < SCREEN_WIDTH; x++) {
					if (aRow[x] != 0) dstRow[x] = aRow[x];
				}
			}
		}
		return;
	}

	if (GUI_3DS_IsFactoryWindow()) { FactoryTop(); return; }

	if (CustomDialog()) {
		if (s_dialogWindow != NULL && (s_dialogWindow->stringID == STR_SELECT_A_SAVED_GAME_TO_LOAD || s_dialogWindow->stringID == STR_SELECT_A_POSITION_TO_SAVE_TO)) {
			memset(g_topScreenBuffer, 0, TOP_WIDTH * TOP_HEIGHT);
			memset(g_topScreenUIMask, 2, sizeof(g_topScreenUIMask));
			int slot = s_dialogFocus;
			if (slot >= 0 && slot < 5 && s_saveSlots[slot].used) {
				uint8 hID = s_saveSlots[slot].houseID;
				int crestIdx = (hID == HOUSE_ATREIDES) ? 0 : ((hID == HOUSE_ORDOS) ? 1 : 2);
				static const char *hNames[3] = {"Atreides", "Ordos", "Harkonnen"};
				if (s_heraldCrestsValid) {
					int dstBaseX = 14;
					int dstBaseY = (TOP_HEIGHT - 64 * 2) / 2; /* 56 */
					for (int cy = 0; cy < 64; cy++) {
						const uint8 *srcRow = s_heraldCrests[crestIdx] + cy * 84;
						for (int cx = 0; cx < 84; cx++) {
							uint8 px = srcRow[cx];
							int dx = dstBaseX + cx * 2;
							int dy = dstBaseY + cy * 2;
							g_topScreenBuffer[dy * TOP_WIDTH + dx] = px;
							g_topScreenBuffer[dy * TOP_WIDTH + dx + 1] = px;
							g_topScreenBuffer[(dy + 1) * TOP_WIDTH + dx] = px;
							g_topScreenBuffer[(dy + 1) * TOP_WIDTH + dx + 1] = px;
							g_topScreenUIMask[dy * TOP_WIDTH + dx] = 0;
							g_topScreenUIMask[dy * TOP_WIDTH + dx + 1] = 0;
							g_topScreenUIMask[(dy + 1) * TOP_WIDTH + dx] = 0;
							g_topScreenUIMask[(dy + 1) * TOP_WIDTH + dx + 1] = 0;
						}
					}
					int bottomCoverY = dstBaseY + 64 * 2 - 2;
					if (bottomCoverY < TOP_HEIGHT) {
						memset(g_topScreenBuffer + bottomCoverY * TOP_WIDTH, 0, (TOP_HEIGHT - bottomCoverY) * TOP_WIDTH);
					}
				}
				GUI_3DS_DrawIntroFontText(g_topScreenBuffer, TOP_WIDTH, 198, 72, hNames[crestIdx], 119, 112);
				char missionLine[64];
				snprintf(missionLine, sizeof(missionLine), L10N("Mission %u", "Misija %u"), (unsigned)s_saveSlots[slot].mission);
				GUI_3DS_DrawText(g_topScreenBuffer, TOP_WIDTH, 198, 108, missionLine, 15);
				GUI_3DS_DrawText(g_topScreenBuffer, TOP_WIDTH, 198, 128, s_saveSlots[slot].dateStr, 110);
			} else if (slot >= 0 && slot < 5) {
				const char *emptyStr = L10N("Empty Slot", "Tuscia vieta");
				int ew = (int)strlen(emptyStr) * 8;
				GUI_3DS_DrawText(g_topScreenBuffer, TOP_WIDTH, (TOP_WIDTH - ew) / 2, 116, emptyStr, 110);
			}
			return;
		}
		/* Keep top screen clean during dialog: no yellow background or extra UI! */
		if (g_gameMode != GM_NORMAL) {
			memset(g_topScreenBuffer, 0, TOP_WIDTH * TOP_HEIGHT);
			return;
		}
		if (s_pauseValid) {
			/* Paused battle: dimmed frozen battlefield with a blinking bright red PAUSE indicator. */
			for (int y = 0; y < TOP_HEIGHT; y++) {
				const uint8 *sp = s_pauseSnapshot + y * TOP_WIDTH;
				uint8 *dp = g_topScreenBuffer + y * TOP_WIDTH;
				for (int x = 0; x < TOP_WIDTH; x++) dp[x] = (((x + y) & 1) != 0) ? 0 : sp[x];
			}
			if ((g_timerGUI % 60) < 40) {
				const char *pauseStr = L10N("PAUSED", "PAUZE");
				int pw = GUI_3DS_GetIntroFontWidth(pauseStr);
				int px = max(0, (TOP_WIDTH - pw) / 2);
				int py = 104;
				GUI_3DS_FillRect(g_topScreenBuffer, TOP_WIDTH, px - 12, py - 6, px + pw + 11, py + 22, 0);
				GUI_3DS_DrawRect(g_topScreenBuffer, TOP_WIDTH, px - 12, py - 6, px + pw + 11, py + 22, 8);
				for (int by = py - 6; by <= py + 22; by++) {
					if (by >= 0 && by < TOP_HEIGHT) {
						memset(g_topScreenUIMask + by * TOP_WIDTH + (px - 12), 2, pw + 24);
					}
				}
				GUI_3DS_DrawIntroFontText(g_topScreenBuffer, TOP_WIDTH, px, py, pauseStr, 8, 0);
			}
			return;
		}
	}

	/* 3. Modal Dialogs (Save/Load Name, Pick House): show centered with black border */
	if (GUI_3DS_IsDialog() || (g_3dsModalMessageActive && g_gameMode != GM_NORMAL)) {
		memset(g_topScreenBuffer, 0, TOP_WIDTH * TOP_HEIGHT);
		if (src != NULL) {
			int offsetX = (TOP_WIDTH - SCREEN_WIDTH) / 2;   /* 40 */
			int offsetY = (TOP_HEIGHT - SCREEN_HEIGHT) / 2; /* 20 */
			for (int y = 0; y < SCREEN_HEIGHT; y++) {
				memcpy(g_topScreenBuffer + ((offsetY + y) * TOP_WIDTH + offsetX), src + y * SCREEN_WIDTH, SCREEN_WIDTH);
			}
		}
		return;
	}

	/* 4. IN BATTLE MODE (GM_NORMAL):
	 * OpenDUNE renders the complete, animated battlefield into SCREEN_0 at X: 0..239, Y: 40..199.
	 * Fast unrolled 5:3 horizontal and 3:2 vertical scaler (zero divisions per pixel).
	 */
	s_pauseValid = false;
	if (src != NULL) {
		int lastSrcY = -1;
		for (int y = 0; y < TOP_HEIGHT; y++) {
			int srcY = 40 + (y * 2) / 3;
			uint8 *dstRow = g_topScreenBuffer + y * TOP_WIDTH;
			if (srcY == lastSrcY) {
				memcpy(dstRow, dstRow - TOP_WIDTH, TOP_WIDTH);
				continue;
			}
			lastSrcY = srcY;
			const uint8 *srcRow = src + srcY * SCREEN_WIDTH;
			uint8 *d = dstRow;
			const uint8 *s = srcRow;
			for (int block = 0; block < 80; block++) {
				uint8 s0 = s[0], s1 = s[1], s2 = s[2];
				d[0] = s0;
				d[1] = s0;
				d[2] = s1;
				d[3] = s1;
				d[4] = s2;
				s += 3;
				d += 5;
			}
		}
	} else {
		memset(g_topScreenBuffer, 0, TOP_WIDTH * TOP_HEIGHT);
	}

	/* Draw multi-selection brackets on selected units */
	if (g_selectedUnitsCount > 1) {
		for (uint16 i = 0; i < g_selectedUnitsCount; i++) {
			Unit *u = g_selectedUnits[i];
			if (u == NULL || !u->o.flags.s.allocated || u->o.flags.s.isNotOnMap) continue;

			int16 sx, sy;
			if (GUI_3DS_MapPosToScreenPos(u->o.position, &sx, &sy)) {
				GUI_3DS_DrawRect(g_topScreenBuffer, TOP_WIDTH, sx - 10, sy - 10, sx + 10, sy + 10, 15);
			}
		}
	}

	/* Draw Active Box Selection */
	if (g_boxSelectActive && (abs(g_boxSelectCurrX - g_boxSelectStartX) >= 4 || abs(g_boxSelectCurrY - g_boxSelectStartY) >= 4)) {
		int16 x1 = g_boxSelectStartX;
		int16 y1 = g_boxSelectStartY;
		int16 x2 = g_boxSelectCurrX;
		int16 y2 = g_boxSelectCurrY;

		GUI_3DS_DrawRect(g_topScreenBuffer, TOP_WIDTH, x1, y1, x2, y2, 147); /* Bright green box */
		GUI_3DS_DrawRect(g_topScreenBuffer, TOP_WIDTH, x1 + 1, y1 + 1, x2 - 1, y2 - 1, 254); /* White inner line */
	}

	/* Draw Cursor (hidden during modal dialog message) */
	if (!g_3dsModalMessageActive) {
		int16 cx = g_cursor3DS_X;
		int16 cy = g_cursor3DS_Y;

		int scrollX = (!g_boxSelectActive) ? ((cx <= 12) ? -1 : ((cx >= TOP_WIDTH - 13) ? 1 : 0)) : 0;
		int scrollY = (!g_boxSelectActive) ? ((cy <= 12) ? -1 : ((cy >= TOP_HEIGHT - 13) ? 1 : 0)) : 0;

		if (scrollX != 0 || scrollY != 0) {
			GUI_3DS_DrawScrollCursor(g_topScreenBuffer, TOP_WIDTH, cx, cy, scrollX, scrollY);
		} else if (g_selectionType == SELECTIONTYPE_TARGET) {
			for (int i = 2; i <= 7; i++) {
				GUI_3DS_PutPixel(g_topScreenBuffer, TOP_WIDTH, cx - i, cy, 254);
				GUI_3DS_PutPixel(g_topScreenBuffer, TOP_WIDTH, cx + i, cy, 254);
				GUI_3DS_PutPixel(g_topScreenBuffer, TOP_WIDTH, cx, cy - i, 254);
				GUI_3DS_PutPixel(g_topScreenBuffer, TOP_WIDTH, cx, cy + i, 254);
			}
			GUI_3DS_PutPixel(g_topScreenBuffer, TOP_WIDTH, cx, cy, 224);
		} else {
			GUI_3DS_DrawCursor(g_topScreenBuffer, TOP_WIDTH, cx, cy);
		}
	}
}

static const char *GUI_3DS_GetActionLabel(uint16 action)
{
	switch (action) {
		case ACTION_ATTACK:     return L10N("ATTACK", "PULTI");
		case ACTION_MOVE:       return L10N("MOVE", "JUDETI");
		case ACTION_RETREAT:    return L10N("RETREAT", "ATSIKR.");
		case ACTION_GUARD:      return L10N("GUARD", "SAUGOTI");
		case ACTION_AREA_GUARD: return L10N("AREA GRD", "GINYBA");
		case ACTION_HARVEST:    return L10N("HARVEST", "RINKTI");
		case ACTION_RETURN:     return L10N("RETURN", "GRIZTI");
		case ACTION_STOP:       return L10N("STOP", "STOP");
		case ACTION_AMBUSH:     return L10N("AMBUSH", "PASALA");
		case ACTION_SABOTAGE:   return L10N("SABOTAGE", "SABOTAZ.");
		case ACTION_DEPLOY:     return L10N("DEPLOY", "IKURTI");
		case ACTION_DESTRUCT:   return L10N("DESTRUCT", "SPROGTI");
		default:                return L10N("ORDERS", "VEIKSMAS");
	}
}

/* =========================================================================
 * Original-style Dune II action panel (bottom-right of the bottom screen)
 * ========================================================================= */

enum { HIT_PORTRAIT = 1, HIT_UNIT_ACTION, HIT_CANCEL, HIT_PLACE_HERE, HIT_CONFIRM, HIT_REPAIR, HIT_PRIMARY, HIT_UPGRADE, HIT_SMART_ORDER, HIT_SQUAD_UNIT, HIT_JUMP_BASE, HIT_OPEN_FACTORY };

typedef struct PanelHit {
	int16  x, y, w, h;
	uint8  kind;
	uint16 param;
} PanelHit;

static PanelHit s_panelHits[20];
static int s_panelHitCount;

static void PanelAddHit(int x, int y, int w, int h, uint8 kind, uint16 param)
{
	if (s_panelHitCount >= 20) return;
	PanelHit *hit = &s_panelHits[s_panelHitCount++];
	hit->x = (int16)x; hit->y = (int16)y; hit->w = (int16)w; hit->h = (int16)h;
	hit->kind = kind; hit->param = param;
}

static void BattleText(int cx, int y, const char *text, const Font *font, uint8 color, bool shadow)
{
	VARIABLE_NOT_USED(font);
	Tiny5_DrawText(g_bottomScreenBuffer, 320, cx, y + 8, text, color, shadow);
}

static int BattleTextWidth(const char *text, const Font *font)
{
	font = g_fontNew8p ? g_fontNew8p : font;
	int width = 0;
	for (const unsigned char *p = (const unsigned char *)text; *p; p++)
		width += font && *p < font->count ? max(1, font->chars[*p].width - 1) : 8;
	return width;
}

static __attribute__((unused)) void BattleLabelText(int cx, int y, const char *text, int scale, uint8 color)
{
	const Font *font = g_fontNew8p;
	if (!font) { BattleText(cx, y, text, NULL, color, true); return; }
	int left = cx - BattleTextWidth(text, font) * scale / 512;
	int advance = 0;
	for (int pass = 0; pass < 2; pass++) {
		advance = 0;
		for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
			if (*p >= font->count) { advance += 8; continue; }
			const FontChar *ch = &font->chars[*p];
			if (ch->data) for (int row = 0; row < ch->usedLines; row++) for (int col = 0; col < ch->width; col++) {
				if (ch->data[row * ch->width + col] != 1) continue;
				int x0 = left + (advance + col) * scale / 256 + (pass == 0);
				int x1 = left + (advance + col + 1) * scale / 256 - 1 + (pass == 0);
				int y0 = y + (ch->unusedLines + row) * scale / 256 + (pass == 0);
				int y1 = y + (ch->unusedLines + row + 1) * scale / 256 - 1 + (pass == 0);
				for (int py = max(41, y0); py <= min(237, y1); py++) for (int px = max(185, x0); px <= min(315, x1); px++)
					GUI_3DS_PutPixel(g_bottomScreenBuffer, 320, px, py, pass == 0 ? TC_BLACK : color);
			}
			advance += max(1, ch->width - 1);
		}
	}
}

static void BattleButtonLabel(int x,int y,int w,int h,const char *label,uint8 color)
{
 if(!label || !*label) return;
 char lines[2][48]; const char *split=strchr(label,'\n');
 int count=split ? 2 : 1;
 snprintf(lines[0],sizeof(lines[0]),"%.*s",split ? (int)(split-label) : (int)strlen(label),label);
 if(split) snprintf(lines[1],sizeof(lines[1]),"%s",split+1);

 if(count==1) {
  int top=y+(h-10)/2;
  int baseY=top+8;
  Tiny5_DrawText(g_bottomScreenBuffer,320,x+w/2,baseY,lines[0],color,true);
 } else {
  int top=y+(h-26)/2;
  int baseY0=top+8;
  int baseY1=top+16+8;
  Tiny5_DrawText(g_bottomScreenBuffer,320,x+w/2,baseY0,lines[0],color,true);
  Tiny5_DrawText(g_bottomScreenBuffer,320,x+w/2,baseY1,lines[1],color,true);
 }
}

static void BattleButton(int x,int y,int w,int h,const char *label,bool active,bool dim)
{
  uint8 *buf=g_bottomScreenBuffer;
  const int t=2; /* border thickness */
  GUI_3DS_FillRect(buf,320,x-2,y-2,x+w+1,y+h+1,TC_BLACK);
  GUI_3DS_FillRect(buf,320,x,y,x+w-1,y+h-1,TC_FACE);
  GUI_3DS_FillRect(buf,320,x,y,x+w-1-t,y+t-1,TC_LIGHT);             /* top #FAFFC5 */
  GUI_3DS_FillRect(buf,320,x,y,x+t-1,y+h-1-t,TC_LIGHT);             /* left #FAFFC5 */
  GUI_3DS_FillRect(buf,320,x+t,y+h-t,x+w-1,y+h-1,TC_SHADOW);        /* bottom #65654B */
  GUI_3DS_FillRect(buf,320,x+w-t,y+t,x+w-1,y+h-1,TC_SHADOW);        /* right #65654B */
  for (int py = max(0, y - 2); py <= min(239, y + h + 1); py++) {
    memset(g_bottomScreenUIMask + py * 320 + max(0, x - 2), TACTICAL_UI_MASK, min(320, x + w + 2) - max(0, x - 2));
  }
  const char *shown=label ? label : "";
  BattleButtonLabel(x,y,w,h,shown,dim ? TC_SHADOW : active ? TC_ACTIVE : TC_WHITE);
}

/* Fit original art into its Figma slot, using integer scaling whenever possible. */
static void BattlePortrait(int x,int y,int width,int height,uint16 spriteID)
{
	uint8 *sprite = g_sprites[spriteID]; if (!sprite) return;
	int sw = Sprite_GetWidth(sprite), sh = Sprite_GetHeight(sprite);
	if (sw <= 0 || sw > 64 || sh <= 0 || sh > 64) return;
	static uint8 saved[64*64]; uint8 *scratch = GFX_Screen_Get_ByIndex(SCREEN_2); if (!scratch) return;
	for (int row = 0; row < sh; row++) { memcpy(saved + row * sw, scratch + row * 320, sw); memset(scratch + row * 320, 0, sw); }
	GUI_DrawSprite(SCREEN_2, sprite, 0, 0, 0, 0);
	int scale = min(width / sw, height / sh), dw, dh;
	if (scale > 0) { dw = sw * scale; dh = sh * scale; }
	else if (width * sh <= height * sw) { dw = width; dh = max(1, sh * width / sw); }
	else { dh = height; dw = max(1, sw * height / sh); }
	int left = x + (width - dw) / 2, top = y + (height - dh) / 2;
	GUI_3DS_FillRect(g_bottomScreenBuffer, 320, x, y, x + width - 1, y + height - 1, TC_FACE);
	for (int row = 0; row < dh; row++) for (int col = 0; col < dw; col++) {
		int at = (top + row) * 320 + left + col;
		g_bottomScreenBuffer[at] = scratch[(row * sh / dh) * 320 + col * sw / dw]; g_bottomScreenUIMask[at] = 0;
	}
	for (int row = 0; row < sh; row++) memcpy(scratch + row * 320, saved + row * sw, sw);
}

static void PanelMeterBar(int x,int y,int w,int h,uint32 val,uint32 maxVal,const char *caption,int captionY,uint8 captionColor)
{
 uint8 *buf=g_bottomScreenBuffer;
 GUI_3DS_FillRect(buf,320,x,y,x+w-1,y+h-1,TC_SHADOW);
 GUI_3DS_FillRect(buf,320,x+2,y+2,x+w-3,y+h-3,TC_FACE);
 int fill=maxVal ? min(w-4,(int)((uint64_t)val*(w-4)/maxVal)) : 0;
 uint8 color=val<=maxVal/4 ? TC_RED : val<=maxVal/2 ? TC_YELLOW : TC_GREEN;
 if(fill) GUI_3DS_FillRect(buf,320,x+2,y+2,x+1+fill,y+h-3,color);
 GUI_3DS_FillRect(buf,320,x,y,x+w-1,y+1,TC_LIGHT);
 GUI_3DS_FillRect(buf,320,x,y+2,x+1,y+h-1,TC_LIGHT);
 GUI_3DS_FillRect(buf,320,x+w-2,y+2,x+w-1,y+h-1,TC_LIGHT);
 static const uint8 colors[]={TC_RED,TC_YELLOW,TC_GREEN};
 for(int i=0;i<3;i++) {
  int mx=x+i*(w-8)/2, my=y+h+2;
  GUI_3DS_FillRect(buf,320,mx+2,my,mx+3,my+3,colors[i]);
  GUI_3DS_FillRect(buf,320,mx,my+2,mx+5,my+3,colors[i]);
  GUI_3DS_FillRect(buf,320,mx,my+4,mx+7,my+5,TC_BLACK);
 }
 if(caption && *caption) Tiny5_DrawText(buf,320,x+w/2,captionY,caption,captionColor,false);
}

static void PanelDamageBar(int x,int y,int w,int h,uint32 hp,uint32 maxHP)
{
 PanelMeterBar(x,y,w,h,hp,maxHP,"DMG",104,TC_TEXT);
}

static void PanelHeader(const char *title,uint16 spriteID,bool isUnit)
{
 VARIABLE_NOT_USED(isUnit);
 char upper[48]; int i=0;
 for(;title && title[i] && i<(int)sizeof(upper)-1;i++) {
  unsigned char c=(unsigned char)title[i];
  upper[i]=(c>='a' && c<='z') ? (c-'a'+'A') : c;
 }
 upper[i]='\0';
 Tiny5_DrawText(g_bottomScreenBuffer,320,245,54,upper,TC_TEXT,false);
 BattlePortrait(178,61,69,53,spriteID);
 PanelAddHit(176,59,73,55,HIT_PORTRAIT,0);
}

static const char *PanelActionKey(uint16 action)
{
	switch (action) {
		case ACTION_ATTACK: case ACTION_MOVE: case ACTION_HARVEST: case ACTION_SABOTAGE: return "A";
		case ACTION_RETREAT:    return "R";
		case ACTION_GUARD:      return "Y";
		case ACTION_AREA_GUARD: return "L+Y";
		case ACTION_STOP:       return "L+Y";
		case ACTION_RETURN:     return "L+R";
		case ACTION_AMBUSH:     return "L+A";
		case ACTION_DEPLOY:     return "L+X";
		case ACTION_DESTRUCT:   return "";
		default:                return "";
	}
}

static void BattleProgress(int pct)
{
 int filled=clamp(pct*9/100,0,9);
 for(int row=0;row<3;row++) for(int col=0;col<3;col++) {
  int x=256+col*15,y=152+row*15;
  GUI_3DS_PutPixel(g_bottomScreenBuffer,320,x,y,TC_DOT);
  for(int d=5;d<=10;d+=5) { GUI_3DS_PutPixel(g_bottomScreenBuffer,320,x+d,y,TC_DOT); GUI_3DS_PutPixel(g_bottomScreenBuffer,320,x,y+d,TC_DOT); }
  if(row*3+col<filled) {
   GUI_3DS_FillRect(g_bottomScreenBuffer,320,x+2,y+2,x+12,y+12,TC_SHADOW);
   GUI_3DS_DrawBevel(g_bottomScreenBuffer,320,x+2,y+2,x+12,y+12,TC_DOT,TC_TEXT);
  }
 }
}

static void PanelStructureStatusBox(Structure *s,const StructureInfo *si)
{
 const ObjectInfo *info=&si->o;
 bool mine=s->o.houseID==g_playerHouseID;
 bool ready=s->o.type==STRUCTURE_CONSTRUCTION_YARD && s->o.linkedID!=STRUCTURE_INVALID && s->countDown==0;
 bool building=info->flags.factory && s->o.linkedID!=STRUCTURE_INVALID;
 char label[48],pctLabel[16];
 uint16 sprite=info->spriteID,buildTime=0;
 if(info->flags.factory) {
  if(s->o.type==STRUCTURE_CONSTRUCTION_YARD && s->objectType<STRUCTURE_MAX) {
   sprite=g_table_structureInfo[s->objectType].o.spriteID; buildTime=g_table_structureInfo[s->objectType].o.buildTime;
  } else if(s->o.type!=STRUCTURE_STARPORT && s->objectType<UNIT_MAX) {
   sprite=g_table_unitInfo[s->objectType].o.spriteID; buildTime=g_table_unitInfo[s->objectType].o.buildTime;
  }
 }
 int total=(int)buildTime<<8;
 int pct=ready ? 100 : building && total ? clamp((total-(int)s->countDown)*100/total,0,99) : 0;
 if(s->o.flags.s.upgrading) pct=clamp(100-(int)s->upgradeTimeLeft,0,100);
 const char *action = (s->o.type == STRUCTURE_CONSTRUCTION_YARD) ? L10N("BUILD IT (Y)", "STATYTI (Y)") : L10N("BUILD IT (Y)", "GAMINTI (Y)");
 if(!mine) action=L10N("ENEMY","PRIESAS");
 else if(ready) action=L10N("PLACE IT (Y)","STATYTI (Y)");
 else if(building) action=L10N("CANCEL (L+Y)","ATSAUKTI (L+Y)");
 else if(s->o.type==STRUCTURE_STARPORT) action=L10N("ORDER (Y)","UZSAKYTI (Y)");
 else if(s->o.type==STRUCTURE_PALACE) action=s->countDown ? L10N("CHARGING","KRAUNASI") : L10N("LAUNCH (Y)","PALEISTI (Y)");
 else if(!info->flags.factory) action=L10N("SELECT ALL (X)","VISI KARIAI (X)");
 snprintf(label,sizeof(label),"%s",action);
	BattleButton(174, 146, 142, 76, "", false, false);
	BattlePortrait(180, 150, 66, 51, sprite);
	BattleProgress(pct);
	if (building || s->o.flags.s.upgrading) {
		snprintf(pctLabel, sizeof(pctLabel), s->o.flags.s.onHold ? "HOLD" : "%d%%", pct);
		BattleText(278, 194, pctLabel, g_fontNew6p, TC_TEXT, false);
	} else if (mine && info->flags.factory && !ready && s->o.type != STRUCTURE_STARPORT) {
		BattleText(278, 194, L10N("MENU (X)", "MENIU (X)"), g_fontNew6p, TC_TEXT, false);
	}
	BattleButtonLabel(174, 200, 142, 22, label, (building && s->o.flags.s.onHold) ? TC_SHADOW : ready ? TC_ACTIVE : TC_WHITE);
	if (mine) {
		if (info->flags.factory && s->o.type != STRUCTURE_STARPORT) {
			PanelAddHit(174, 146, 142, 52, HIT_OPEN_FACTORY, 0);
			PanelAddHit(174, 198, 142, 24, HIT_PRIMARY, 0);
		} else {
			PanelAddHit(174, 146, 142, 76, HIT_PRIMARY, 0);
		}
	}
}

static bool DrawSquadPanel(void)
{
 Unit *units[MAX_SELECTED_UNITS]; int count=0;
 for(uint16 i=0;i<g_selectedUnitsCount && count<MAX_SELECTED_UNITS;i++) {
  Unit *u=g_selectedUnits[i];
  if(u && u->o.flags.s.allocated && !u->o.flags.s.isNotOnMap && u->o.houseID==g_playerHouseID) units[count++]=u;
 }
 if(count<=1) return false;
 char title[32]; snprintf(title,sizeof(title),L10N("UNITS: %d","KARIAI: %d"),count);
 BattleText(245,46,title,g_fontNew8p,TC_TEXT,false);
 int visible=count>12 ? 11 : count;
 for(int i=0;i<min(count,12);i++) {
  int x=177+(i%3)*46, y=60+(i/3)*41;
  GUI_3DS_FillRect(g_bottomScreenBuffer,320,x,y,x+43,y+38,TC_BLACK);
  if(i>=visible) {
   char more[16]; snprintf(more,sizeof(more),"+%d",count-visible);
   GUI_3DS_FillRect(g_bottomScreenBuffer,320,x+2,y+2,x+41,y+36,TC_FACE);
   BattleText(x+22,y+13,more,g_fontNew8p,TC_TEXT,false);
   continue;
  }
  Unit *u=units[i];
  BattlePortrait(x+2,y+2,40,28,g_table_unitInfo[u->o.type].o.spriteID);
  uint32 maxHP=g_table_unitInfo[u->o.type].o.hitpoints;
  uint8 color=u->o.hitpoints<=maxHP/4 ? TC_RED : u->o.hitpoints<=maxHP/2 ? TC_YELLOW : TC_GREEN;
  int fill=maxHP ? min(40,(int)((uint32)u->o.hitpoints*40/maxHP)) : 0;
  GUI_3DS_FillRect(g_bottomScreenBuffer,320,x+2,y+32,x+41,y+36,TC_SHADOW);
  if(fill) GUI_3DS_FillRect(g_bottomScreenBuffer,320,x+2,y+32,x+1+fill,y+36,color);
  PanelAddHit(x,y,44,39,HIT_SQUAD_UNIT,u->o.index);
 }
 return true;
}

static void DrawActionPanel(void)
{
	uint8 *buf = g_bottomScreenBuffer;
	s_panelHitCount = 0;

	/* Tactical panel spans X: 170..319, Y: 40..223 directly adjacent to the left and bottom barber-pole columns.
	 * Info area at the top has no black outer frame (drawn directly on the beveled cream background);
	 * only buttons have a black frame around them. */
	GUI_3DS_FillRect(buf, 320, 170, 40, 319, 223, TC_FACE);
	for (int y = 40; y < 224; y++) memset(g_bottomScreenUIMask + y * 320 + 170, TACTICAL_UI_MASK, 150);
	for (int inset = 0; inset < 2; inset++) GUI_3DS_DrawBevel(buf, 320, 170 + inset, 40 + inset, 319 - inset, 223 - inset, TC_LIGHT, TC_SHADOW);


	/* ---- Structure placement ------------------------------------------------ */
	if (g_selectionType == SELECTIONTYPE_PLACE && g_structureActiveType < STRUCTURE_MAX) {
		const StructureInfo *si = &g_table_structureInfo[g_structureActiveType];
		char title[32];
		snprintf(title, sizeof(title), "%.13s", String_Get_ByIndex(si->o.stringID_abbrev));
		PanelHeader(title, si->o.spriteID, true);

		const char *state = (g_selectionState > 0) ? L10N("OK", "TINKA") : ((g_selectionState < 0) ? L10N("NO SLABS", "BE PAMATO") : L10N("INVALID", "NETINKA"));
		uint8 stateCol = (g_selectionState > 0) ? TC_DARK_GREEN : ((g_selectionState < 0) ? TC_TEXT : TC_RED);
		BattleText(282, 76, state, g_fontNew6p, stateCol, false);

		BattleText(245, 118, L10N("SELECT LOCATION", "PASIRINKITE VIETA"), g_fontNew6p, TC_TEXT, false);
		BattleText(245, 132, L10N("TO BUILD.", "STATYBAI."), g_fontNew6p, TC_TEXT, false);
		BattleText(245, 150, L10N("D-PAD: SCROLL MAP", "D-PAD: ZEMELAPIS"), g_fontNew6p, TC_TEXT, false);

		BattleButton(174, 172, 142, 22, L10N("PLACE HERE (A)", "STATYTI (A)"), false, g_selectionState <= 0);
		PanelAddHit(174, 172, 142, 22, HIT_PLACE_HERE, 0);
		BattleButton(174, 198, 142, 22, L10N("CANCEL (B)", "ATSAUKTI (B)"), false, false);
		PanelAddHit(174, 198, 142, 22, HIT_CANCEL, 0);
		return;
	}

	/* ---- Target selection --------------------------------------------------- */
	if (g_selectionType == SELECTIONTYPE_TARGET) {
		const char *actName = (g_unitHouseMissile != NULL) ? L10N("DEATH HAND", "MIRTIES RANKA") : GUI_3DS_GetActionLabel(g_activeAction);
		uint16 sprite = (g_unitSelected != NULL) ? g_table_unitInfo[g_unitSelected->o.type].o.spriteID : 0;
		PanelHeader(actName, sprite, true);
		BattleText(245, 118, L10N("SELECT TARGET", "PASIRINKITE"), g_fontNew6p, TC_TEXT, false);
		BattleText(245, 132, L10N("ON THE MAP.", "TAIKINI ZEMELAPY."), g_fontNew6p, TC_TEXT, false);
		BattleButton(174, 172, 142, 22, L10N("CONFIRM (A)", "PATVIRTINTI (A)"), false, false);
		PanelAddHit(174, 172, 142, 22, HIT_CONFIRM, 0);
		BattleButton(174, 198, 142, 22, L10N("CANCEL (B)", "ATSAUKTI (B)"), false, false);
		PanelAddHit(174, 198, 142, 22, HIT_CANCEL, 0);
		return;
	}

    /* A group uses individual portraits and health instead of the single-unit orders. */
    if(g_selectedUnitsCount>1 && DrawSquadPanel()) return;

	/* ---- Unit(s) selected --------------------------------------------------- */
	if (g_unitSelected != NULL && g_unitSelected->o.flags.s.allocated) {
		const UnitInfo *ui = &g_table_unitInfo[g_unitSelected->o.type];
		uint32 hp = 0, maxHP = 0;
		char title[32];
		if (g_selectedUnitsCount > 1) {
			for (int i = 0; i < g_selectedUnitsCount; i++) {
				Unit *u = g_selectedUnits[i];
				if (!u || !u->o.flags.s.allocated || u->o.flags.s.isNotOnMap) continue;
				hp += u->o.hitpoints;
				maxHP += g_table_unitInfo[u->o.type].o.hitpoints;
			}
			snprintf(title, sizeof(title), L10N("SQUAD x%u", "BURYS x%u"), g_selectedUnitsCount);
		} else {
			hp = g_unitSelected->o.hitpoints;
			maxHP = ui->o.hitpoints;
			snprintf(title, sizeof(title), "%.13s", String_Get_ByIndex(ui->o.stringID_abbrev));
		}
		PanelHeader(title, ui->o.spriteID, true);
		PanelDamageBar(256, 61, 56, 20, hp, maxHP);

        bool combat=false;
        for(int slot=0;slot<4;slot++) if(ui->o.actionsPlayer[slot]==ACTION_ATTACK) combat=true;
        if(combat) {
         bool isAttackMove = (g_unitSelected->actionID==ACTION_ATTACK || g_unitSelected->actionID==ACTION_MOVE ||
                              g_unitSelected->nextActionID==ACTION_ATTACK || g_unitSelected->nextActionID==ACTION_MOVE);
         bool isRetreat = (g_unitSelected->actionID==ACTION_RETREAT || g_unitSelected->nextActionID==ACTION_RETREAT);
         bool isGuard = (g_unitSelected->actionID==ACTION_GUARD || g_unitSelected->nextActionID==ACTION_GUARD);
         BattleButton(174,118,142,50,L10N("ATTACK /\nMOVE (A)","PULTI /\nJUDETI (A)"),isAttackMove,false);
         PanelAddHit(174,118,142,50,HIT_SMART_ORDER,0);
         BattleButton(174,170,142,24,L10N("RETREAT (R)","ATSIKR. (R)"),isRetreat,false);
         PanelAddHit(174,170,142,24,HIT_UNIT_ACTION,ACTION_RETREAT);
         BattleButton(174,196,142,24,L10N("GUARD (Y)","SAUGOTI (Y)"),isGuard,false);
         PanelAddHit(174,196,142,24,HIT_UNIT_ACTION,ACTION_GUARD);
        } else {
         /* Harvesters, MCVs and saboteurs retain their original commands. */
         for(int slot=0;slot<4;slot++) {
          uint16 action=ui->o.actionsPlayer[slot]; if(action>=ACTION_MAX) continue;
          char label[40]; snprintf(label,sizeof(label),"%s (%s)",GUI_3DS_GetActionLabel(action),PanelActionKey(action));
          bool isActive = false;
          if(action==ACTION_HARVEST) {
           isActive = (g_unitSelected->actionID==ACTION_HARVEST ||
                       (g_unitSelected->actionID==ACTION_MOVE && g_unitSelected->nextActionID==ACTION_HARVEST));
          } else if(action==ACTION_RETURN) {
           isActive = (g_unitSelected->actionID==ACTION_RETURN ||
                       (g_unitSelected->actionID==ACTION_MOVE && g_unitSelected->nextActionID==ACTION_RETURN));
          } else if(action==ACTION_MOVE) {
           isActive = (g_unitSelected->actionID==ACTION_MOVE &&
                       g_unitSelected->nextActionID!=ACTION_HARVEST &&
                       g_unitSelected->nextActionID!=ACTION_RETURN);
          } else {
           isActive = (g_unitSelected->actionID==action || g_unitSelected->nextActionID==action);
          }
          BattleButton(174,118+slot*26,142,22,label,isActive,false);
          PanelAddHit(174,118+slot*26,142,22,HIT_UNIT_ACTION,action);
         }
        }
		return;
	}

	/* ---- Structure selected ------------------------------------------------- */
	Structure *s = Unit_3DS_GetSelectedStructure();
	if (s != NULL) {
		const StructureInfo *si = &g_table_structureInfo[s->o.type];
		char title[32];
		snprintf(title, sizeof(title), "%.13s", String_Get_ByIndex(si->o.stringID_abbrev));
		PanelHeader(title, si->o.spriteID, false);
		PanelDamageBar(256, 61, 56, 20, s->o.hitpoints, si->o.hitpoints);
		if (s->o.houseID == g_playerHouseID) {
			bool repairing = s->o.flags.s.repairing;
			BattleButton(174, 120, 142, 22, repairing ? L10N("REPAIRING (R)", "TAISOMA (R)") : L10N("REPAIR (R)", "TAISYTI (R)"), repairing, false);
			PanelAddHit(174, 120, 142, 22, HIT_REPAIR, 0);
		}
		PanelStructureStatusBox(s, si);
		return;
	}

	/* ---- Nothing selected: Power bar + "POWER OK"/"POWER LOW" + two large buttons ---- */
	{
		uint16 prod = (g_playerHouse != NULL) ? g_playerHouse->powerProduction : 0;
		uint16 req  = (g_playerHouse != NULL) ? g_playerHouse->powerUsage : 0;
		uint32 barVal = 100, barMax = 100;
		if (req > 0) {
			barVal = prod;
			barMax = max((uint16)prod, req);
		} else if (prod == 0) {
			barVal = 100;
			barMax = 100;
		}
		bool powerOk = (prod >= req);
		const char *powerStr = powerOk ? L10N("POWER OK", "GALIA OK") : L10N("POWER LOW", "MAZAI GALIOS");
		PanelMeterBar(217, 55, 56, 20, barVal, barMax, powerStr, 96, powerOk ? TC_TEXT : TC_RED);

		BattleButton(174, 118, 142, 50, L10N("SELECT ALL\nUNITS (X)", "VISI\nKARIAI (X)"), false, false);
		PanelAddHit(174, 118, 142, 50, HIT_PRIMARY, 0);
		BattleButton(174, 170, 142, 50, L10N("JUMP TO BASE\n(SELECT)", "I BAZE\n(SELECT)"), false, false);
		PanelAddHit(174, 170, 142, 50, HIT_JUMP_BASE, 0);
	}
}

void GUI_3DS_RenderBottomScreen(void)
{
	if (g_bottomScreenBuffer == NULL || s_launchActive) return;
 if(g_3dsModalMessageActive) {
  memcpy(g_bottomScreenBuffer,s_messageBottom,sizeof(s_messageBottom));
  memcpy(g_bottomScreenUIMask,s_messageBottomMask,sizeof(s_messageBottomMask));
  GUI_3DS_FillRect(g_bottomScreenBuffer,320,8,70,311,204,0);
  for(int y=70;y<=204;y++) memset(g_bottomScreenUIMask+y*320+8,2,304);
  GUI_3DS_DrawCenteredSubtitle(s_messageText,118);
  MenuButton(40,164,240,32,"Continue (A)",true,false); return;
 }
 if(s_resultsActive && !CustomDialog()) {
  memset(g_bottomScreenBuffer,0,320*240); memset(g_bottomScreenUIMask,2,sizeof(g_bottomScreenUIMask));
  MenuButton(40,160,240,36,s_resultsButtons ? L10N("Back (B)","Atgal (B)") : L10N("Continue (A)","Testi (A)"),true,false);
  return;
 }
	memset(g_bottomScreenUIMask, 0, sizeof(g_bottomScreenUIMask));

	/* 0. Bene Gesserit House Join Confirmation & Mentat Briefing Confirmation (pure black bottom screen) */
	if (s_mentatButtons) {
		memset(g_bottomScreenBuffer, 0, BOTTOM_WIDTH * BOTTOM_HEIGHT);
		memset(g_bottomScreenUIMask, 2, sizeof(g_bottomScreenUIMask));

		const bool isHousePick = (g_gameMode == GM_PICKHOUSE || g_playerHouseID == HOUSE_MERCENARY ||
		                          s_houseConfirmID < 3 || s_mentatButtons->drawParameterNormal.spriteID == 373);
		const char *optProceed = L10N("Proceed (A)", "Testi (A)");
		const char *optSecond  = isHousePick ? L10N("Back (B)", "Atgal (B)") : L10N("Repeat (B)", "Kartoti (B)");
		MenuButton(8,148,144,36,optProceed,s_dialogFocus==0,false);
		MenuButton(168,148,144,36,optSecond,s_dialogFocus==1,false);
		return;
	}

	/* 0b. Mentat help subject list: every choice lives on the bottom screen. */
	if (g_3dsMentatHelpActive) {
		memset(g_bottomScreenBuffer, 0, BOTTOM_WIDTH * BOTTOM_HEIGHT);
		memset(g_bottomScreenUIMask, 2, sizeof(g_bottomScreenUIMask));
		const char *rows[11]; bool header[11];
		int selected = GUI_Mentat_3DS_GetRows(rows, header);
		for (int i = 0; i < 11; i++) {
			int y = 6 + i * 17;
			if (header[i]) {
				GUI_3DS_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, 16, y + 2, rows[i], 11);
			} else {
				bool isSel = (i == selected);
				MenuButton(24,y,288,17,rows[i],isSel,false);
			}
		}
		MenuButton(16,198,149,36,L10N("EXIT (B)","ISEITI (B)"),false,false);
		MenuButton(168,198,68,36,L10N("UP","VIRS"),false,false);
		MenuButton(238,198,68,36,L10N("DOWN","ZEMYN"),false,false);
		return;
	}

	/* 1. Cutscenes, logos, and Mentat briefing: pure black bottom screen with centered subtitles */
	if (GUI_3DS_IsCutsceneOrBriefing() && !g_3dsHousePickerActive) {
		memset(g_bottomScreenBuffer, 0, BOTTOM_WIDTH * BOTTOM_HEIGHT);
		memset(g_bottomScreenUIMask, 2, sizeof(g_bottomScreenUIMask));
		if (s_subtitleText[0] != '\0') {
			GUI_3DS_DrawCenteredSubtitle(s_subtitleText, 120);
		}
		return;
	}

	/* 2. House Selection Screen: "Select House" header, 3 horizontal crest squares with house names below */
	if (g_3dsHousePickerActive) {
		memset(g_bottomScreenBuffer, 0, BOTTOM_WIDTH * BOTTOM_HEIGHT);
		memset(g_bottomScreenUIMask, 2, sizeof(g_bottomScreenUIMask));

		uint8 *herald = GFX_Screen_Get_ByIndex(SCREEN_1);
		if (herald == NULL) herald = GFX_Screen_Get_ByIndex(SCREEN_0);
		int focus = clamp(s_houseFocus, 0, 2);

		/* Centered "Select House" heading above the three crests */
		const char *titleStr = L10N("Select House", "Pasirinkite gimine");
		int titleLen = 0;
		for (const unsigned char *p = (const unsigned char *)titleStr; *p != '\0';) {
			if (*p == 0xC4 || *p == 0xC5) { p += 2; }
			else { p++; }
			titleLen++;
		}
		int titleX = max(8, (BOTTOM_WIDTH - titleLen * 8) / 2);
		GUI_3DS_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, titleX, 32, titleStr, 15);

		static const char *hNames[3] = {"Atreides", "Ordos", "Harkonnen"};
		int crestW = 84;
		int crestH = 64;
		int dstY = 74;

		for (int i = 0; i < 3; i++) {
			int dstX = 18 + i * 100; /* 18, 118, 218 -> total span 18..302 (centered on 320) */
			int srcX = 18 + i * 96;
			int srcY = 54;

			if (herald != NULL) {
				for (int cy = 0; cy < crestH; cy++) {
					memcpy(g_bottomScreenBuffer + (dstY + cy) * BOTTOM_WIDTH + dstX,
					       herald + (srcY + cy) * SCREEN_WIDTH + srcX,
					       crestW);
					memset(g_bottomScreenUIMask + (dstY + cy) * BOTTOM_WIDTH + dstX, 0, crestW);
				}
			}

			/* House choices use the same gold-on-navy controls as the native Exit button. */
			MenuButton(dstX-5,dstY+crestH+8,94,30,hNames[i],i==focus,false);
		}
		return;
	}

	/* 3. Title choices on the bottom screen, with a gold border around the focused control. */
	if (GUI_3DS_IsTitleMenu()) {
		memset(g_bottomScreenBuffer, 0, BOTTOM_WIDTH * BOTTOM_HEIGHT);
		memset(g_bottomScreenUIMask, 2, sizeof(g_bottomScreenUIMask));
		int count = min((int)g_widgetProperties[21].height, 6);
		int rowStep = 28;
		int totalH = count * rowStep;
		int firstY = (BOTTOM_HEIGHT - totalH) / 2;
		for (int i = 0; i < count; i++) {
			const char *label = s_titleLabels[i];
			if (label == NULL) continue;
			bool selected = (g_widgetProperties[21].fgColourBlink == i);
			MenuButton(20,firstY+i*rowStep,280,rowStep-3,label,selected,false);
		}
		return;
	}
	if (GUI_3DS_IsFactoryWindow()) { FactoryBottom(); return; }

	if (CustomDialog()) { DialogBottom(); return; }

	if (GUI_3DS_IsDialog()) {
		memset(g_bottomScreenBuffer, 0, BOTTOM_WIDTH * BOTTOM_HEIGHT);
		uint8 *src = GFX_Screen_Get_ByIndex(SCREEN_0);
		if (src != NULL) {
			int offsetY = (BOTTOM_HEIGHT - SCREEN_HEIGHT) / 2; /* 20 */
			for (int y = 0; y < SCREEN_HEIGHT; y++) {
				memcpy(g_bottomScreenBuffer + ((offsetY + y) * BOTTOM_WIDTH), src + y * SCREEN_WIDTH, SCREEN_WIDTH);
			}
		}
		return;
	}

	/* =========================================================================
	 * BATTLE DASHBOARD (GM_NORMAL)
	 * Authentic Dune II layout inspired by user mockup:
	 * - Header (Y: 0..39): Dune II DOS components (Mentat, Options, Credits, live ticker)
	 * - Barber-pole Column (X: 154..169, Y: 40..239): 16px vertical column from SCREEN_0
	 * - Bottom-Left (X: 0..153, Y: 40..239): Solid black, 128x128 radar centered, viewport box
	 * - Bottom-Right (X: 170..319, Y: 40..239): Tactical panel with portrait, DMG bar & actions
	 * ========================================================================= */

	uint8 *src = GFX_Screen_Get_ByIndex(SCREEN_0);

	/* 1. Header (Y: 0..39): Copy DOS screen header directly (includes live credits & ticker) */
	if (src != NULL) {
		memcpy(g_bottomScreenBuffer, src, BOTTOM_WIDTH * 40);
	}

	/* 2. Barber-pole Column (X: 154..169, Y: 40..239): 16px wide striped column */
	if (src != NULL) {
		for (int y = 40; y < 200; y++) {
			memcpy(g_bottomScreenBuffer + y * BOTTOM_WIDTH + 154, src + y * SCREEN_WIDTH + 240, 16);
		}
		for (int y = 200; y < 240; y++) {
			memcpy(g_bottomScreenBuffer + y * BOTTOM_WIDTH + 154, src + (y - 80) * SCREEN_WIDTH + 240, 16);
		}
	}

	/* 3. Bottom-Left Radar / Minimap (X: 0..153, Y: 40..239) */
	for (int y = 40; y < 240; y++) {
		memset(g_bottomScreenBuffer + y * BOTTOM_WIDTH, 0, 154);
	}

	/* 128x128 Radar centered at X: 13..140, Y: 76..203 */
	GUI_3DS_DrawBevel(g_bottomScreenBuffer, BOTTOM_WIDTH, 12, 75, 141, 204, 118, 110);
	for (int y = 76; y < 204; y++) {
		memset(g_bottomScreenUIMask + y * BOTTOM_WIDTH + 13, 0, 128);
	}

	for (int my = 0; my < 64; my++) {
		for (int mx = 0; mx < 64; mx++) {
			uint16 packed = Tile_PackXY(mx, my);
			Tile *t = &g_map[packed];
			uint8 col = 0; /* Fog of war */

			if (t->isUnveiled || g_debugScenario) {
				uint16 type = Map_GetLandscapeType(packed);
				switch (type) {
					case LST_SPICE: col = 178; break;
					case LST_THICK_SPICE: col = 186; break;
					case LST_ENTIRELY_ROCK: case LST_MOSTLY_ROCK: case LST_PARTIAL_ROCK:
					case LST_ENTIRELY_MOUNTAIN: case LST_PARTIAL_MOUNTAIN:
						col = 23; break;
					case LST_ENTIRELY_DUNE: case LST_PARTIAL_DUNE:
						col = 114; break;
					default: col = 112; break;
				}
				if (t->hasStructure) {
					Structure *st = Structure_Get_ByPackedTile(packed);
					if (st != NULL) {
						col = st->o.houseID < HOUSE_MAX ? g_table_houseInfo[st->o.houseID].minimapColor : 0;
					}
				}
				if (t->hasUnit) {
					Unit *un = Unit_Get_ByPackedTile(packed);
					if (un != NULL) {
						if (un->o.type == UNIT_SANDWORM) col = 255;
						else {
                            uint8 houseID=Unit_GetHouseID(un);
                            col=houseID < HOUSE_MAX ? g_table_houseInfo[houseID].minimapColor : 0;
                        }
					}
				}
			}

			int rx = 13 + mx * 2;
			int ry = 76 + my * 2;
			g_bottomScreenBuffer[ry * BOTTOM_WIDTH + rx] = col;
			g_bottomScreenBuffer[ry * BOTTOM_WIDTH + rx + 1] = col;
			g_bottomScreenBuffer[(ry + 1) * BOTTOM_WIDTH + rx] = col;
			g_bottomScreenBuffer[(ry + 1) * BOTTOM_WIDTH + rx + 1] = col;
		}
	}

	/* Viewport box */
	{
		uint16 camX = Tile_GetPackedX(g_viewportPosition);
		uint16 camY = Tile_GetPackedY(g_viewportPosition);
		int16 rCamX = 13 + camX * 2;
		int16 rCamY = 76 + camY * 2;
		int16 rCamW = VIEWPORT_TILES_X * 2;
		int16 rCamH = VIEWPORT_TILES_Y * 2;
		GUI_3DS_DrawRect(g_bottomScreenBuffer, BOTTOM_WIDTH, rCamX, rCamY, rCamX + rCamW, rCamY + rCamH, 15);
	}

	/* 4. Bottom-Right action panel, laid out like the original game */
	DrawActionPanel();

	/* 5. Horizontal Barber-pole Column under the action panel (X: 170..319, Y: 224..239) */
	if (src != NULL) {
		for (int y = 0; y < 16; y++) {
			uint8 *dstRow = g_bottomScreenBuffer + (224 + y) * BOTTOM_WIDTH;
			for (int x = 170; x < 320; x++) {
				int sy = 40 + (x - 170);
				if (sy >= 200) sy -= 80;
				dstRow[x] = src[sy * SCREEN_WIDTH + 240 + y];
				g_bottomScreenUIMask[(224 + y) * BOTTOM_WIDTH + x] = 0;
			}
		}
	}
}

void GUI_3DS_HandleBottomTouchHold(int16 touchX, int16 touchY)
{
	if (GUI_3DS_IsMenuOrDialogActive() || GUI_3DS_CustomMenuActive() || g_3dsModalMessageActive) return;

	/* Continuous Radar Drag (X: 0..153, Y: 40..239) */
	if (touchX >= 0 && touchX < 154 && touchY >= 40 && touchY < 240) {
		int mapX = clamp((touchX - 13) / 2, 0, 63);
		int mapY = clamp((touchY - 76) / 2, 0, 63);
		GUI_3DS_CenterCameraOnTile((uint16)mapX, (uint16)mapY);
		if (g_selectionType == SELECTIONTYPE_PLACE) {
			uint16 tile = GUI_3DS_ScreenPosToTile(g_cursor3DS_X, g_cursor3DS_Y);
			Map_SetSelection(tile);
			g_selectionRectanglePosition = tile;
			Map_SetSelectionObjectPosition(tile);
			g_selectionRectangleNeedRepaint = true;
			g_viewport_forceRedraw = true;
		}
	}
}

void GUI_3DS_HandleBottomTouch(int16 touchX, int16 touchY)
{
	if(s_resultsActive && !g_3dsModalMessageActive && !CustomDialog()) {
 if(touchX<40 || touchX>=280 || touchY<160 || touchY>=196) return;
 if(s_resultsButtons) {
  Widget *chosen=NULL;
  for(Widget *w=s_resultsButtons;w;w=w->next) if(w->index==101) chosen=w;
  s_dialogChosen=chosen ? chosen : s_resultsButtons;
 } else EmitKey(0x1C);
 return;
 }
 /* 0. If in cutscene, mentat briefing, or modal message dialog: tap advances/skips */
	if ((GUI_3DS_IsCutsceneOrBriefing() && !g_3dsHousePickerActive && !s_mentatButtons && !g_3dsMentatHelpActive) || g_3dsModalMessageActive) {
		Input_EventHandler(0x1C); /* Enter */
		Input_EventHandler(0x1C | 0x80);
		return;
	}

	/* Mentat help subject list (bottom screen): rows, Exit and scroll buttons */
	if (g_3dsMentatHelpActive && s_mentatButtons == NULL) {
		if (touchY >= 198 && touchY < 234) {
			if (touchX >= 16 && touchX < 165) g_3dsMentatExit = true;
			else if (touchX >= 168 && touchX < 236) g_3dsMentatScroll = -1;
			else if (touchX >= 238 && touchX < 306) g_3dsMentatScroll = 1;
		} else if (touchY >= 4 && touchY < 4 + 11 * 17 && touchX >= 16 && touchX < 304) {
			const char *rows[11]; bool header[11];
			GUI_Mentat_3DS_GetRows(rows, header);
			int row = (touchY - 4) / 17;
			if (row >= 0 && row < 11 && !header[row]) g_3dsMentatTapRow = row;
		}
		return;
	}

	if (GUI_3DS_CustomMenuActive()) {
		if (s_mentatButtons) {
			s_dialogFocus = (touchX < BOTTOM_WIDTH / 2) ? 0 : 1;
			GUI_3DS_MenuInput(0, 0, true);
			return;
		} else if (CustomDialog()) {
			Widget *items[9]; int count = DialogItems(items);
			int ly0, lstep, lrows; DialogLayout(&ly0, &lstep, &lrows);
			int first = max(0, s_dialogFocus - (lrows - 1));
			int visibleCount = min(count - first, lrows);
			if (touchY >= ly0 && touchY < ly0 + visibleCount * lstep) {
				int row = first + (touchY - ly0) / lstep;
				if (row >= 0 && row < count) { s_dialogFocus = row; GUI_3DS_MenuInput(0, 0, true); }
			}
		} else if (GUI_3DS_IsFactoryWindow()) {
			int total = g_factoryWindowTotal;
			if (touchY >= 200 && touchY <= 236) {
				if (touchX >= 16 && touchX < 158) {
					/* Left button: BUY (A) or ORDER */
					if (g_factoryWindowStarport) {
						if (g_factoryWindowOrdered > 0) {
							s_factoryFocus = total;
							s_factoryPending = total; /* ORDER */
						}
					} else {
						s_factoryPending = total; /* BUY highlighted item */
					}
				} else if (touchX >= 158 && touchX <= 304) {
					/* Right button: BACK (B) */
					s_factoryFocus = total + 1;
					s_factoryPending = total + 1; /* Resume / Back */
				}
			} else if (!g_factoryWindowStarport && touchX >= 16 && touchX <= 304 && touchY >= 26 && touchY <= 185) {
				/* Factory Grid tap */
				for (int r = 0; r < SHOP_VISIBLE_ROWS; r++) {
					int gy = 30 + r * 52;
					if (touchY >= gy && touchY < gy + 48) {
						for (int c = 0; c < SHOP_GRID_COLS; c++) {
							int gx = 21 + c * 71;
							if (touchX >= gx && touchX < gx + 64) {
								int idx = (s_factoryScroll + r) * SHOP_GRID_COLS + c;
								if (idx < total) {
									if (s_factoryFocus == idx) {
										s_factoryPending = total; /* Tap again builds immediately */
									} else {
										s_factoryFocus = idx;
										s_factoryPending = idx;
									}
								}
								break;
							}
						}
						break;
					}
				}
			} else if (g_factoryWindowStarport && touchX >= 16 && touchX <= 304 && touchY >= 20 && touchY <= 181) {
				/* Starport Row tap */
				for (int r = 0; r < SHOP_VISIBLE_ROWS; r++) {
					int ry = 20 + r * 54;
					if (touchY >= ry && touchY < ry + 54) {
						int idx = s_factoryScroll + r;
						if (idx < total) {
							if (touchX >= 210 && touchX < 255 && touchY >= ry + 10 && touchY <= ry + 42) {
								s_factoryFocus = idx;
								s_factorySub = 0;
								s_factoryPending = total + 3; /* + */
							} else if (touchX >= 256 && touchX <= 300 && touchY >= ry + 10 && touchY <= ry + 42) {
								s_factoryFocus = idx;
								s_factorySub = 1;
								s_factoryPending = total + 4; /* - */
							} else {
								s_factoryFocus = idx;
								s_factoryPending = idx; /* select row */
							}
						}
						break;
					}
				}
			}
		} else if (g_3dsHousePickerActive) {
			/* 3 horizontal crest squares & house names at X: 12..308, Y: 54..180 */
			if (touchX >= 12 && touchX <= 308 && touchY >= 54 && touchY <= 180) {
				int idx = clamp((touchX - 14) / 100, 0, 2);
				s_houseFocus = idx;
				GUI_3DS_MenuInput(0, 0, true);
			}
		} else if (GUI_3DS_IsTitleMenu()) {
			int count = min((int)g_widgetProperties[21].height, 6);
			int rowStep = 28;
			int totalH = count * rowStep;
			int firstY = (BOTTOM_HEIGHT - totalH) / 2;
			if (touchY >= firstY && touchY < firstY + count * rowStep) {
				int row = (touchY - firstY) / rowStep;
				if (row >= 0 && row < count) {
					g_widgetProperties[21].fgColourBlink = row;
					Mouse_EventHandler(0, 0, false, false);
					EmitKey(0x1C);
				}
			}
		}
		return;
	}

	/* 1. If in native dialog (e.g. savegame text box): pass touch to DOS mouse */
	if (GUI_3DS_IsMenuOrDialogActive()) {
		int dosX = touchX;
		int dosY = touchY - 20;
		if (dosX >= 0 && dosX < SCREEN_WIDTH && dosY >= 0 && dosY < SCREEN_HEIGHT) {
			Mouse_EventHandler((uint16)dosX, (uint16)dosY, false, false);
			Mouse_EventHandler((uint16)dosX, (uint16)dosY, true, false);
			Mouse_EventHandler((uint16)dosX, (uint16)dosY, false, false);
		}
		return;
	}

	/* =========================================================================
	 * BATTLE DASHBOARD TOUCH HANDLERS
	 * ========================================================================= */

	/* A. Header Bar (Y: 0..24): [MENTAT] & [OPTIONS] */
	if (touchY >= 0 && touchY <= 24) {
		if (touchX >= 12 && touchX <= 96) {
			Sound_StartSound(3);
			g_3dsRequestedMentat = true;
			return;
		}
		if (touchX >= 100 && touchX <= 184) {
			Sound_StartSound(3);
			g_3dsRequestedOptions = true;
			return;
		}
		return;
	}

	/* B. Radar Tap (X: 0..153, Y: 40..239) -> Jump Camera */
	if (touchX >= 0 && touchX < 154 && touchY >= 40 && touchY < 240) {
		int mapX = clamp((touchX - 13) / 2, 0, 63);
		int mapY = clamp((touchY - 76) / 2, 0, 63);
		GUI_3DS_CenterCameraOnTile((uint16)mapX, (uint16)mapY);
		if (g_selectionType == SELECTIONTYPE_PLACE) {
			uint16 tile = GUI_3DS_ScreenPosToTile(g_cursor3DS_X, g_cursor3DS_Y);
			Map_SetSelection(tile);
			g_selectionRectanglePosition = tile;
			Map_SetSelectionObjectPosition(tile);
			g_selectionRectangleNeedRepaint = true;
			g_viewport_forceRedraw = true;
		}
		Sound_StartSound(3);
		return;
	}

	/* C. Right action panel: dispatch to the hit zones registered while drawing it */
	if (touchX >= 170 && touchX <= 319 && touchY >= 40 && touchY <= 239) {
		for (int i = 0; i < s_panelHitCount; i++) {
			const PanelHit *hit = &s_panelHits[i];
			if (touchX < hit->x || touchX >= hit->x + hit->w || touchY < hit->y || touchY >= hit->y + hit->h) continue;

			switch (hit->kind) {
				case HIT_PORTRAIT: {
					if (g_unitSelected != NULL && g_unitSelected->o.flags.s.allocated) {
						uint16 p = Tile_PackTile(g_unitSelected->o.position);
						GUI_3DS_CenterCameraOnTile(Tile_GetPackedX(p), Tile_GetPackedY(p));
						Sound_StartSound(3);
					} else {
						Structure *st = Unit_3DS_GetSelectedStructure();
						if (st != NULL) {
							uint16 p = Tile_PackTile(st->o.position);
							GUI_3DS_CenterCameraOnTile(Tile_GetPackedX(p), Tile_GetPackedY(p));
							Sound_StartSound(3);
						}
					}
				} break;

				case HIT_SQUAD_UNIT: {
                    /* Store pool identity, not an array offset that can shift after a death. */
                    for(uint16 j=0;j<g_selectedUnitsCount;j++) {
                        Unit *u=g_selectedUnits[j];
                        if(u && u->o.index==hit->param && u->o.flags.s.allocated && !u->o.flags.s.isNotOnMap && u->o.houseID==g_playerHouseID) {
                            Unit_3DS_SelectSingle(u); break;
                        }
                    }
                } break;

                case HIT_SMART_ORDER:
                    Unit_3DS_SmartAction(GUI_3DS_ScreenPosToTile(g_cursor3DS_X,g_cursor3DS_Y));
                    break;

                case HIT_UNIT_ACTION:
					Unit_3DS_ExecuteUnitAction((ActionType)hit->param);
					break;

				case HIT_CANCEL:
					if (g_selectionType == SELECTIONTYPE_PLACE) Unit_3DS_CancelPlacement();
					else if (g_selectionType == SELECTIONTYPE_TARGET) { GUI_ChangeSelectionType(SELECTIONTYPE_UNIT); Sound_StartSound(3); }
					break;

				case HIT_PLACE_HERE:
					Unit_3DS_PlaceActiveStructure();
					break;

				case HIT_CONFIRM:
					Unit_3DS_IssueCommand(g_activeAction, GUI_3DS_ScreenPosToTile(g_cursor3DS_X, g_cursor3DS_Y));
					break;

				case HIT_REPAIR: {
					Structure *st = Unit_3DS_GetSelectedStructure();
					if (st != NULL && st->o.houseID == g_playerHouseID && Structure_SetRepairingState(st, -1, NULL)) Sound_StartSound(3);
				} break;

				case HIT_UPGRADE: {
					Structure *st = Unit_3DS_GetSelectedStructure();
					if (st != NULL && st->o.houseID == g_playerHouseID && Structure_IsUpgradable(st) && Structure_SetUpgradingState(st, -1, NULL)) Sound_StartSound(3);
				} break;

				case HIT_OPEN_FACTORY: {
					Structure *st = Unit_3DS_GetSelectedStructure();
					if (st != NULL && st->o.houseID == g_playerHouseID && g_table_structureInfo[st->o.type].o.flags.factory) {
						Unit_3DS_OpenFactory(st);
					}
				} break;

				case HIT_PRIMARY: {
					Structure *st = Unit_3DS_GetSelectedStructure();
					if (st != NULL && st->o.houseID == g_playerHouseID) {
						if (st->o.type == STRUCTURE_CONSTRUCTION_YARD && st->o.linkedID != STRUCTURE_INVALID && st->countDown == 0) {
							g_3dsRequestedStartPlacement = st;
						} else if (st->o.linkedID != STRUCTURE_INVALID) {
							Structure_CancelBuild(st);
							st->o.flags.s.onHold = false;
							Sound_StartSound(3);
						} else if (st->o.type == STRUCTURE_STARPORT) {
							Unit_3DS_OpenFactory(st);
						} else if (g_table_structureInfo[st->o.type].o.flags.factory) {
							Unit_3DS_BuildStructureObject(st);
						} else if (st->o.type == STRUCTURE_PALACE && st->countDown == 0) {
							Structure_ActivateSpecial(st);
							Sound_StartSound(3);
						} else {
							Unit_3DS_SelectAllVisibleUnits();
						}
					} else {
						Unit_3DS_SelectAllVisibleUnits();
					}
				} break;

				case HIT_JUMP_BASE:
					Unit_3DS_CycleStructureType(0);
					break;

				default: break;
			}
			return;
		}
	}
}
