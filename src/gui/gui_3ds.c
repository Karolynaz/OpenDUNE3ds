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
#include "cairo_text.h"
#include "popup_3ds.h"
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
static bool s_resultsContinueMission;
static Widget *s_mentatButtons = NULL;
static WindowDesc *s_dialogWindow = NULL;
static Widget *s_dialogChosen = NULL;
static uint8 s_messageTop[TOP_WIDTH*TOP_HEIGHT], s_messageTopMask[TOP_WIDTH*TOP_HEIGHT];
static uint8 s_messageBottom[BOTTOM_WIDTH*BOTTOM_HEIGHT], s_messageBottomMask[BOTTOM_WIDTH*BOTTOM_HEIGHT];
static char s_messageText[768];
static uint16 s_messageSpriteID = 0xFFFF;
static uint32 s_messageStartTick;
void GUI_3DS_BeginMessageEx(const char *text, uint16 spriteID)
{
 if(g_topScreenBuffer) memcpy(s_messageTop,g_topScreenBuffer,sizeof(s_messageTop));
 if(g_bottomScreenBuffer) memcpy(s_messageBottom,g_bottomScreenBuffer,sizeof(s_messageBottom));
 memcpy(s_messageTopMask,g_topScreenUIMask,sizeof(s_messageTopMask));
 memcpy(s_messageBottomMask,g_bottomScreenUIMask,sizeof(s_messageBottomMask));
 snprintf(s_messageText,sizeof(s_messageText),"%s",text ? text : "");
 s_messageSpriteID = spriteID;
 s_messageStartTick = g_timerGUI;
 g_3dsModalMessageActive=true;
}
void GUI_3DS_BeginMessage(const char *text) { GUI_3DS_BeginMessageEx(text, 0xFFFF); }
void GUI_3DS_EndMessage(void) { g_3dsModalMessageActive=false; }
bool GUI_3DS_SetResultsActive(bool active) { bool old=s_resultsActive; s_resultsActive=active; return old; }
void GUI_3DS_SetResultsButtons(Widget *buttons, bool continueMission)
{
	s_resultsButtons = buttons;
	s_resultsContinueMission = continueMission;
	s_dialogFocus = 0;
}
static bool s_titleActive = false;
static bool s_launchActive = false;
static bool s_planetBgValid = false;
static uint8 s_planetBg[320 * 200];
static uint16 s_houseChoice;
static uint16 s_mentatChoice = 0;
static bool s_mentatTextHasMore = false;
static uint8 s_houseConfirmID = 0xFF;
static int s_houseFocus = 0;
static char s_subtitleText[512] = "";
static char s_toastText[64] = "";
static uint32 s_toastUntilTick = 0;

void GUI_3DS_ShowTopToast(const char *text, uint32 durationTicks)
{
	if (text == NULL || text[0] == '\0' || durationTicks == 0) {
		s_toastText[0] = '\0';
		s_toastUntilTick = 0;
		return;
	}
	snprintf(s_toastText, sizeof(s_toastText), "%s", text);
	s_toastUntilTick = g_timerGUI + durationTicks;
}

bool GUI_3DS_IsLithuanian(void)
{
	if (g_config.language == LANGUAGE_LITHUANIAN) return true;
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

void GUI_3DS_SetMentatHasMore(bool hasMore)
{
	s_mentatTextHasMore = hasMore;
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
	s_toastText[0] = '\0';
	s_toastUntilTick = 0;

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

		/* Lithuanian UTF-8 decode + diacritic flag */
		int dia = 0;
		if (c == 0xC4 || c == 0xC5) {
			unsigned char next = *p;
			if (next == 0) break;
			p++;
			if (c == 0xC4) {
				switch (next) {
					case 0x84: c = 'A'; dia = 1; break; /* Ą */
					case 0x85: c = 'a'; dia = 1; break; /* ą */
					case 0x8C: c = 'C'; dia = 2; break; /* Č */
					case 0x8D: c = 'c'; dia = 2; break; /* č */
					case 0x98: c = 'E'; dia = 1; break; /* Ę */
					case 0x99: c = 'e'; dia = 1; break; /* ę */
					case 0x96: c = 'E'; dia = 3; break; /* Ė */
					case 0x97: c = 'e'; dia = 3; break; /* ė */
					case 0xAE: c = 'I'; dia = 1; break; /* Į */
					case 0xAF: c = 'i'; dia = 1; break; /* į */
					default: c = '?'; break;
				}
			} else {
				switch (next) {
					case 0xA0: c = 'S'; dia = 2; break; /* Š */
					case 0xA1: c = 's'; dia = 2; break; /* š */
					case 0xB2: c = 'U'; dia = 1; break; /* Ų */
					case 0xB3: c = 'u'; dia = 1; break; /* ų */
					case 0xAA: c = 'U'; dia = 4; break; /* Ū */
					case 0xAB: c = 'u'; dia = 4; break; /* ū */
					case 0xBD: c = 'Z'; dia = 2; break; /* Ž */
					case 0xBE: c = 'z'; dia = 2; break; /* ž */
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
		if (dia == 1) {
			GUI_3DS_PutPixel(buf, stride, curX + 5, y + 7, color);
			GUI_3DS_PutPixel(buf, stride, curX + 6, y + 8, color);
		} else if (dia == 2) {
			GUI_3DS_PutPixel(buf, stride, curX + 2, y - 2, color);
			GUI_3DS_PutPixel(buf, stride, curX + 3, y - 1, color);
			GUI_3DS_PutPixel(buf, stride, curX + 4, y - 2, color);
		} else if (dia == 3) {
			GUI_3DS_PutPixel(buf, stride, curX + 3, y - 2, color);
			GUI_3DS_PutPixel(buf, stride, curX + 4, y - 2, color);
		} else if (dia == 4) {
			GUI_3DS_PutPixel(buf, stride, curX + 2, y - 2, color);
			GUI_3DS_PutPixel(buf, stride, curX + 3, y - 2, color);
			GUI_3DS_PutPixel(buf, stride, curX + 4, y - 2, color);
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

/* Classic Dune II mouse cursor arrow (11x18: 0 = black outline, 254 = white, 216 = ivory bevel, 15 = gold bevel, 255 = transparent) */
static const uint8 s_cursorArrow[18][11] = {
	{  0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
	{  0,   0, 255, 255, 255, 255, 255, 255, 255, 255, 255},
	{  0, 254,   0, 255, 255, 255, 255, 255, 255, 255, 255},
	{  0, 254, 254,   0, 255, 255, 255, 255, 255, 255, 255},
	{  0, 254, 254, 216,   0, 255, 255, 255, 255, 255, 255},
	{  0, 254, 254, 254, 216,   0, 255, 255, 255, 255, 255},
	{  0, 254, 254, 254, 254, 216,   0, 255, 255, 255, 255},
	{  0, 254, 254, 254, 254, 254, 216,   0, 255, 255, 255},
	{  0, 254, 254, 254, 254, 254, 254, 216,   0, 255, 255},
	{  0, 254, 254, 254, 254, 254, 254, 254, 216,   0, 255},
	{  0, 254, 254, 254, 254, 254, 216,  15,  15,  15,   0},
	{  0, 254, 254, 254,   0, 254, 216,   0,   0,   0,   0},
	{  0, 254, 254,   0,   0, 254, 216,   0, 255, 255, 255},
	{  0, 254,   0, 255, 255,   0, 254, 216,   0, 255, 255},
	{  0,   0, 255, 255, 255,   0, 254, 216,   0, 255, 255},
	{255, 255, 255, 255, 255, 255,   0, 254, 216,   0, 255},
	{255, 255, 255, 255, 255, 255,   0, 254, 216,   0, 255},
	{255, 255, 255, 255, 255, 255, 255,   0,   0, 255, 255},
};

static void GUI_3DS_DrawCursor(uint8 *buf, int16 stride, int16 x, int16 y)
{
	uint8 *mask = (stride == TOP_WIDTH) ? g_topScreenUIMask : g_bottomScreenUIMask;
	for (int r = 0; r < 18; r++) {
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

/* Classic Dune II MOUSE.SHP #5 circular target reticle (15x15 mask, center dot at 7,7) */
static const uint8 s_cursorTarget[15][15] = {
	{0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
	{0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0},
	{0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0},
	{0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
	{0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
	{0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
	{0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0},
	{0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0},
	{0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0},
};

static bool CursorOverEnemy(int16 cx, int16 cy)
{
	uint16 packed = GUI_3DS_ScreenPosToTile(cx, cy);
	Unit *u = Unit_Get_ByPackedTile(packed);
	if (u != NULL && u->o.flags.s.allocated && !u->o.flags.s.isNotOnMap &&
	    u->o.houseID != g_playerHouseID &&
	    ((u->o.seenByHouses & (1 << g_playerHouseID)) != 0 || g_debugScenario)) {
		return true;
	}
	Structure *s = Structure_Get_ByPackedTile(packed);
	if (s != NULL && s->o.flags.s.allocated && !s->o.flags.s.isNotOnMap &&
	    s->o.houseID != g_playerHouseID &&
	    ((s->o.seenByHouses & (1 << g_playerHouseID)) != 0 || g_debugScenario)) {
		return true;
	}
	return false;
}

static void GUI_3DS_DrawTargetCursor(uint8 *buf, int16 stride, int16 x, int16 y, uint8 ringColor)
{
	uint8 *mask = (stride == TOP_WIDTH) ? g_topScreenUIMask : g_bottomScreenUIMask;
	int16 ox = x - 7;
	int16 oy = y - 7;
	for (int pass = 0; pass < 2; pass++) {
		int off = (pass == 0) ? 1 : 0;
		uint8 color = (pass == 0) ? 0 : ringColor;
		for (int r = 0; r < 15; r++) {
			int py = oy + r + off;
			if (py < 0 || py >= 240) continue;
			for (int c = 0; c < 15; c++) {
				if (!s_cursorTarget[r][c]) continue;
				int px = ox + c + off;
				if (px >= 0 && px < stride) {
					buf[py * stride + px] = color;
					mask[py * stride + px] = 2;
				}
			}
		}
	}
}

/* Classic Dune II directional scroll arrow (11x12 pixels pointing UP: 0 = black outline, 254 = white, 216/15 = bevel, 255 = transparent) */
static const uint8 s_cursorScrollUp[12][11] = {
	{255, 255, 255, 255, 255,   0, 255, 255, 255, 255, 255},
	{255, 255, 255, 255,   0, 254,   0, 255, 255, 255, 255},
	{255, 255, 255,   0, 254, 254, 216,   0, 255, 255, 255},
	{255, 255,   0, 254, 254, 254, 254, 216,   0, 255, 255},
	{255,   0, 254, 254, 254, 254, 254, 254, 216,   0, 255},
	{  0, 254, 254, 254, 254, 254, 254, 216,  15,  15,   0},
	{  0,   0,   0,   0, 254, 254, 216,   0,   0,   0,   0},
	{255, 255, 255,   0, 254, 254, 216,   0, 255, 255, 255},
	{255, 255, 255,   0, 254, 254, 216,   0, 255, 255, 255},
	{255, 255, 255,   0, 254, 254, 216,   0, 255, 255, 255},
	{255, 255, 255,   0, 254, 216,  15,   0, 255, 255, 255},
	{255, 255, 255,   0,   0,   0,   0,   0, 255, 255, 255},
};

/* Diagonal scroll arrow (11x11 pixels pointing UP-LEFT: 0 = black outline, 254 = white, 216/15 = bevel, 255 = transparent) */
static const uint8 s_cursorScrollDiag[11][11] = {
	{  0,   0,   0,   0,   0,   0,   0,   0, 255, 255, 255},
	{  0, 254, 254, 254, 254, 254, 254,   0, 255, 255, 255},
	{  0, 254, 254, 254, 254, 254,   0, 255, 255, 255, 255},
	{  0, 254, 254, 254, 254,   0, 255, 255, 255, 255, 255},
	{  0, 254, 254, 254, 254, 254,   0, 255, 255, 255, 255},
	{  0, 254, 254,   0, 254, 254, 216,   0, 255, 255, 255},
	{  0, 254,   0, 255,   0, 254, 254, 216,   0, 255, 255},
	{  0,   0, 255, 255, 255,   0, 216, 216,  15,   0, 255},
	{255, 255, 255, 255, 255, 255,   0, 216,  15,   0, 255},
	{255, 255, 255, 255, 255, 255, 255,   0,   0,   0, 255},
	{255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
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
	if (g_3dsMentatHelpActive || s_mentatButtons != NULL) return true;
	/* Production uses MENTAT selection internally, but has its own two-screen UI. */
	if (GUI_3DS_IsFactoryWindow() || g_3dsHousePickerActive || GUI_3DS_IsTitleMenu() || (g_widgetLinkedListTail != NULL)) return false;
	if (s_houseConfirmID != 0xFF) return true;
	if (g_gameMode == GM_MENU && !s_titleActive) return true;
	if (g_selectionType == SELECTIONTYPE_MENTAT ||
	    g_selectionType == SELECTIONTYPE_INTRO ||
	    g_selectionType == SELECTIONTYPE_UNKNOWN6) {
		return true;
	}
	if (g_gameMode != GM_NORMAL && g_gameMode != GM_MENU) {
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
	return (g_widgetLinkedListTail != NULL);
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
	s_mentatTextHasMore = false;
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
	    w->drawParameterNormal.spriteID == 377 ||  /* Exit */
	    w->drawParameterNormal.spriteID == 378 ||  /* Exit pressed */
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

static uint8 s_specialPortraits[3][69 * 42];
static uint8 s_specialPortraitMask = 0;

void GUI_3DS_CacheSpecialPortrait(uint8 weaponIndex, const uint8 *framePixels, uint16 width, uint16 height)
{
	if (weaponIndex >= 3 || framePixels == NULL || width == 0 || height == 0) return;
	for (int dy = 0; dy < 42; dy++) {
		int sy = dy * height / 42;
		const uint8 *srcRow = framePixels + sy * SCREEN_WIDTH;
		uint8 *dstRow = s_specialPortraits[weaponIndex] + dy * 69;
		for (int dx = 0; dx < 69; dx++) {
			dstRow[dx] = srcRow[dx * width / 69];
		}
	}
	s_specialPortraitMask |= (uint8)(1u << weaponIndex);
}

bool GUI_3DS_AreSpecialPortraitsValid(void)
{
	return s_specialPortraitMask != 0;
}

void GUI_3DS_SetWindow(struct WindowDesc *window)
{
	/* Keep the last battlefield frame so the top screen can show a paused view behind menus. */
	if (!s_pauseValid && g_gameMode == GM_NORMAL && g_topScreenBuffer != NULL && window != NULL) {
		memcpy(s_pauseSnapshot, g_topScreenBuffer, sizeof(s_pauseSnapshot));
		s_pauseValid = true;
	}
	bool sameWindow = (s_dialogWindow == window && window != NULL);
	s_dialogWindow = window;
	s_dialogChosen = NULL;
	if (!sameWindow) {
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
static void ButtonLabelText(int x, int y, int w, int h, const char *label, uint8 color, int shadowColor);

/* Shared vertical/horizontal layout for CustomDialog on the bottom screen. */
static void DialogMetrics(int count, int *outHeadingY, int *outHeadingCount, char headingLines[2][128],
                          int *outBtnX, int *outBtnW, int *outY0, int *outStep, int *outRows)
{
	int step = 27, rows = 6;
	if (s_dialogWindow == &g_gameControlWindowDesc) { step = 25; rows = 8; }
	else if (s_dialogWindow == &g_optionsWindowDesc) { step = 27; rows = 7; }
	else if (count <= 2) { step = 34; rows = 6; }

	int headingCount = 0;
	if (s_dialogWindow != NULL && s_dialogWindow != &g_optionsWindowDesc) {
		const char *heading = GUI_String_Get_ByIndex(s_dialogWindow->stringID);
		if (heading != NULL && heading[0] != '\0') {
			const char *remaining;
			headingCount = Cairo_WrapText(heading, BOTTOM_WIDTH - 24, headingLines, 2, &remaining);
		}
	}

	int headingH = (headingCount > 0) ? ((headingCount - 1) * CAIRO_LINE_H + CAIRO_GLYPH_H) : 0;
	int headingGap = (headingCount > 0) ? ((count <= 2) ? 14 : 8) : 0;
	int visible = min(count, rows);
	int listH = (visible > 0) ? ((visible - 1) * step + (step - 3)) : 0;
	int totalH = headingH + headingGap + listH;
	int topY = max(4, (BOTTOM_HEIGHT - totalH) / 2);

	if (outHeadingY) *outHeadingY = topY + CAIRO_BASE_ROW;
	if (outHeadingCount) *outHeadingCount = headingCount;
	*outY0 = topY + headingH + headingGap;
	*outStep = step;
	*outRows = rows;
	if (outBtnW) *outBtnW = (count <= 2) ? 220 : 296;
	if (outBtnX) *outBtnX = (BOTTOM_WIDTH - ((count <= 2) ? 220 : 296)) / 2;
}

static void DialogLayout(int count, int *y0, int *step, int *rows, int *btnX, int *btnW)
{
	char headingLines[2][128];
	DialogMetrics(count, NULL, NULL, headingLines, btnX, btnW, y0, step, rows);
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

/* Measure text width in the native Dune II INTRO.FNT (g_fontIntro), UTF-8 aware */
static int GUI_3DS_GetIntroFontWidth(const char *str)
{
	if (str == NULL) return 0;
	if (g_fontIntro == NULL || g_fontIntro->chars == NULL) return (int)strlen(str) * 8;
	int w = 0;
	const char *p = str;
	while (*p != '\0') {
		unsigned char c = Font_NextChar(&p, NULL);
		if (c < g_fontIntro->count) w += g_fontIntro->chars[c].width + 1;
	}
	return (w > 0) ? (w - 1) : 0;
}

/* Draw ornate text using the native Dune II INTRO.FNT (g_fontIntro) with Lithuanian diacritics */
static int GUI_3DS_DrawIntroFontText(uint8 *buf, int stride, int x, int y, const char *str, uint8 mainColor, uint8 shadowColor)
{
	if (str == NULL || g_fontIntro == NULL || g_fontIntro->chars == NULL) {
		GUI_3DS_DrawText(buf, stride, x, y, str ? str : "", mainColor);
		return x + (str ? (int)strlen(str) * 8 : 0);
	}
	uint8 *mask = (stride == TOP_WIDTH) ? g_topScreenUIMask : g_bottomScreenUIMask;
	int curX = x;
	const char *p = str;
	while (*p != '\0') {
		uint8 dia = FONT_DIA_NONE;
		unsigned char c = Font_NextChar(&p, &dia);
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
			if (dia != FONT_DIA_NONE) {
				int topY = y + (int)fc->unusedLines - 3;
				int midX = curX + fc->width / 2;
				uint8 maskVal = (mainColor >= 110 && mainColor <= 119) ? 1 : 2;
				if (dia == FONT_DIA_DOT && topY >= 0) {
					for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
						int pxX = midX + dx, py = topY + dy;
						if (pxX >= 0 && pxX < stride && py >= 0 && py < 240) { buf[py * stride + pxX] = mainColor; mask[py * stride + pxX] = maskVal; }
					}
				} else if (dia == FONT_DIA_CARON && topY >= 1) {
					int pts[3][2] = {{-2, -1}, {0, 0}, {2, -1}};
					for (int k = 0; k < 3; k++) {
						int pxX = midX + pts[k][0], py = topY + pts[k][1];
						if (pxX >= 0 && pxX < stride && py >= 0 && py < 240) { buf[py * stride + pxX] = mainColor; mask[py * stride + pxX] = maskVal; }
					}
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
		ButtonLabelText(x, y, w, h, label, textColor, dim ? CAIRO_NO_SHADOW : 0);
	}
}

/* Draw word-wrapped, horizontally & vertically centered subtitle text on the 320x240 bottom screen */
static void GUI_3DS_DrawCenteredSubtitle(const char *text, int centerY)
{
	if (text == NULL || text[0] == '\0') return;
	char lines[8][128];
	const char *next = NULL;
	int numLines = Cairo_WrapText(text, BOTTOM_WIDTH - 24, lines, 8, &next);
	int lineStep = CAIRO_LINE_H;
	int startY = centerY - ((numLines - 1) * lineStep) / 2 + 4;
	for (int i = 0; i < numLines; i++) {
		Cairo_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, BOTTOM_WIDTH / 2,
			startY + i * lineStep, lines[i], 15, CAIRO_NO_SHADOW);
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
				s_factoryScroll = i / SHOP_GRID_COLS;
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
	if (s_factoryFocus < g_factoryWindowTotal) return s_factoryFocus;
	/* Back and Buy move focus to a button, while the engine keeps the item. */
	return g_factoryWindowBase + g_factoryWindowSelected;
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
	char headingLines[2][128];
	int headingY, headingCount, btnX, btnW, y0, step, rows;
	DialogMetrics(count, &headingY, &headingCount, headingLines, &btnX, &btnW, &y0, &step, &rows);
	if (count) s_dialogFocus = min(s_dialogFocus, count - 1);
	memset(g_bottomScreenBuffer, 0, BOTTOM_WIDTH * BOTTOM_HEIGHT);
	memset(g_bottomScreenUIMask, 2, sizeof(g_bottomScreenUIMask));

	for (int i = 0; i < headingCount; i++) {
		Cairo_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, BOTTOM_WIDTH / 2,
			headingY + i * CAIRO_LINE_H, headingLines[i], 15, CAIRO_NO_SHADOW);
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
		} else if (items[i]->stringID == STR_CANCEL || items[i]->stringID == STR_PREVIOUS ||
		           (s_dialogWindow == &g_saveLoadWindowDesc && items[i]->index == 0x23)) {
			snprintf(combined, sizeof(combined), "%s", L10N("Back (B)", "Atgal (B)"));
			label = combined;
		}
		if (label == NULL) continue;

		bool selected = (i == s_dialogFocus);
		bool dim = (s_dialogWindow != NULL && s_dialogWindow->stringID == STR_SELECT_A_SAVED_GAME_TO_LOAD && i < 5 && !s_saveSlots[i].used);
		MenuButton(btnX, y, btnW, step - 3, NULL, selected, dim);
		ButtonLabelText(btnX, y, btnW, step - 3, label, dim ? 110 : selected ? 216 : 15, dim ? CAIRO_NO_SHADOW : 0);
	}
}

void GUI_3DS_FactoryMinus(void)
{
	if (GUI_3DS_IsFactoryWindow() && g_factoryWindowStarport && s_factoryFocus < g_factoryWindowTotal) {
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

/* Native bold bitmap button labels with a one-pixel shadow.
 * Lines are vertically centred as a block; overlong labels are clipped with an ellipsis rather than changing fonts. */
static void ButtonLabelText(int x, int y, int w, int h, const char *label, uint8 color, int shadowColor)
{
	if (!label || !*label) return;
	const char *lines[2] = { label, NULL };
	int len[2] = { (int)strlen(label), 0 }, count = 1;
	const char *nl = strchr(label, '\n');
	if (nl) { len[0] = (int)(nl - label); lines[1] = nl + 1; len[1] = (int)strlen(nl + 1); count = 2; }
	char buffers[2][4 * (BOTTOM_WIDTH / 8) + 1];
	for (int i = 0; i < count; i++) {
		const char *end = lines[i] + len[i];
		const char *p = lines[i];
		int capacity = clamp((w - 8) / 8, 0, BOTTOM_WIDTH / 8);
		int glyphs = Cairo_Width4(p, len[i]) / 32;
		bool shorten = glyphs > capacity;
		int keep = shorten ? max(0, capacity - 3) : glyphs;
		for (int j = 0; j < keep; j++) Cairo_NextChar(&p, end);
		int bytes = (int)(p - lines[i]);
		memcpy(buffers[i], lines[i], bytes);
		buffers[i][bytes] = '\0';
		if (shorten && capacity >= 3) memcpy(buffers[i] + bytes, "...", 4);
		lines[i] = buffers[i];
	}
	CairoBounds bounds = Cairo_BlockBounds(lines, count, CAIRO_LINE_H, shadowColor >= 0);
	int cx = x + (w - 1 - bounds.left - bounds.right) / 2;
	int baseline = y + (h - 1 - bounds.top - bounds.bottom) / 2;
	for (int i = 0; i < count; i++) {
		Cairo_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, cx, baseline + i * CAIRO_LINE_H,
			lines[i], color, shadowColor);
	}
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
		ButtonLabelText(x, y, w, h, label, textColor, dim ? CAIRO_NO_SHADOW : SP_BLACK);
	}
}

static void ShopTileIcon(int x, int y, uint16 spriteID)
{
	typedef struct ShopIconCacheEntry {
		const uint8 *sprite;
		uint16 spriteID;
		uint8 w, h;
		uint8 pixels[32 * 24];
	} ShopIconCacheEntry;
	static ShopIconCacheEntry s_shopIconCache[16];

	uint8 *sprite = g_sprites[spriteID];
	if (sprite == NULL) return;
	int w = Sprite_GetWidth(sprite), h = Sprite_GetHeight(sprite);
	if (w > 64 || h > 64) return;

	const uint8 *pix = NULL;
	int pixStride = w;
	uint8 saved[64 * 64];
	uint8 *scratch = NULL;
	ShopIconCacheEntry *entry = &s_shopIconCache[spriteID & 15];
	if (w <= 32 && h <= 24 && entry->sprite == sprite && entry->spriteID == spriteID && entry->w == w && entry->h == h) {
		pix = entry->pixels;
	} else {
		scratch = GFX_Screen_Get_ByIndex(SCREEN_2);
		if (scratch == NULL) return;
		for (int row = 0; row < h; row++) {
			memcpy(saved + row * w, scratch + row * 320, w);
			memset(scratch + row * 320, 0, w);
		}
		GUI_DrawSprite(SCREEN_2, sprite, 0, 0, 0, 0);
		if (w <= 32 && h <= 24) {
			for (int row = 0; row < h; row++) {
				memcpy(entry->pixels + row * w, scratch + row * 320, w);
			}
			entry->sprite = sprite;
			entry->spriteID = spriteID;
			entry->w = (uint8)w;
			entry->h = (uint8)h;
			pix = entry->pixels;
		} else {
			pix = scratch;
			pixStride = 320;
		}
	}

	int offX = (w < 32) ? (32 - w) / 2 : 0;
	int offY = (h < 24) ? (24 - h) / 2 : 0;
	for (int row = 0; row < h; row++) {
		for (int col = 0; col < w; col++) {
			uint8 px = pix[row * pixStride + col];
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
	if (scratch != NULL) {
		for (int row = 0; row < h; row++) {
			memcpy(scratch + row * 320, saved + row * w, w);
		}
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
				L10N("of", "iš"),
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
		ShopButton(19, 206, 141, 27, L10N("ORDER", "UŽSAKYTI"), orderSel, orderDim);

		bool backSel = (s_factoryFocus == total + 1);
		ShopButton(160, 206, 141, 27, L10N("BACK (B)", "ATGAL (B)"), backSel, false);
	}
}

static uint8 s_factoryPreview[184 * 112];
static bool s_factoryPreviewValid;

void GUI_3DS_CacheFactoryPreview(const uint8 *screen1)
{
	if (screen1 == NULL) {
		s_factoryPreviewValid = false;
		return;
	}
	for (int y = 0; y < 112; y++) {
		memcpy(s_factoryPreview + y * 184, screen1 + (48 + y) * 320 + 128, 184);
	}
	s_factoryPreviewValid = true;
}

void GUI_3DS_UpdateFactoryPreviewOverlay(const uint8 *screen1, int16 srcY)
{
	if (!s_factoryPreviewValid || screen1 == NULL || srcY < 0 || srcY > 191) return;
	for (int row = 0; row < 9; row++) {
		memcpy(s_factoryPreview + (51 + row) * 184, screen1 + (srcY + row) * 320 + 128, 184);
	}
}

static void FactoryPlaqueText(int baseline, const char *str, uint8 color)
{
	if (!str || !*str) return;
	char buf[128];
	const int capacity = 27; /* fits within 222px (27 * 8 = 216px) inside 230px plaque */
	int glyphs = Cairo_Width4(str, (int)strlen(str)) / 32;
	const char *p = str;
	const char *end = str + strlen(str);
	bool shorten = glyphs > capacity;
	int keep = shorten ? max(0, capacity - 3) : glyphs;
	for (int i = 0; i < keep; i++) Cairo_NextChar(&p, end);
	int bytes = (int)(p - str);
	if (bytes > (int)sizeof(buf) - 4) bytes = (int)sizeof(buf) - 4;
	memcpy(buf, str, bytes);
	buf[bytes] = '\0';
	if (shorten) memcpy(buf + bytes, "...", 4);
	Cairo_DrawText(g_topScreenBuffer, TOP_WIDTH, 201, baseline, buf, color, 0);
}

static void FactoryTop(void)
{
	memcpy(g_topScreenBuffer, s_shopTopBG, 400 * 240);
	memset(g_topScreenUIMask, 0, sizeof(g_topScreenUIMask));

	/* The artwork has six 12-pixel cells with separators at 245, 257, ... 317. */
	char credits[7];
	snprintf(credits, sizeof(credits), "%6u", g_playerHouse ? (unsigned)g_playerHouse->credits : 0u);
	for (int i = 0; i < 6; i++) {
		if (credits[i] == ' ') continue;
		const char digit[] = { credits[i], '\0' };
		GUI_3DS_DrawText(g_topScreenBuffer, TOP_WIDTH, 247 + 12 * i, 12, digit, 97);
	}

	int chosen = (s_factoryFocus < g_factoryWindowTotal) ? s_factoryFocus : (g_factoryWindowBase + g_factoryWindowSelected);
	FactoryWindowItem *item = (chosen >= 0 && chosen < g_factoryWindowTotal) ? &g_factoryWindowItems[chosen] : NULL;
	if (!item || !item->objectInfo) return;

	/* 2. WSA animation preview centered in frame X: 89..309 (W=221), Y: 41..175 (H=135) */
	int wsaX = 107; /* 89 + (221 - 184) / 2 */
	int wsaY = 52;  /* 41 + (135 - 112) / 2 */
	if (s_factoryPreviewValid) {
		for (int y = 0; y < 112; y++) {
			memcpy(g_topScreenBuffer + (wsaY + y) * 400 + wsaX, s_factoryPreview + y * 184, 184);
			memset(g_topScreenUIMask + (wsaY + y) * 400 + wsaX, 0, 184);
		}
	} else {
		uint8 *src = GFX_Screen_Get_ByIndex(SCREEN_1);
		if (src != NULL) {
			for (int y = 0; y < 112; y++) {
				memcpy(g_topScreenBuffer + (wsaY + y) * 400 + wsaX, src + (48 + y) * 320 + 128, 184);
				memset(g_topScreenUIMask + (wsaY + y) * 400 + wsaX, 0, 184);
			}
		}
	}

	/* 3. Bottom plaque: X: 86..315 (W=230), Y: 194..229 (H=36) */
	const char *name = String_Get_ByIndex(item->objectInfo->stringID_full);
	char line1[64];
	if (item->objectInfo->available < 0) {
		snprintf(line1, sizeof(line1), "%s (%s)", name ? name : "", L10N("UPGRADE", "TOBULINTI"));
	} else {
		snprintf(line1, sizeof(line1), "%s - %u", name ? name : "", item->credits);
	}
	FactoryPlaqueText(206, line1, 15);

	char line2[64];
	line2[0] = '\0';
	if (g_factoryWindowConstructionYard) {
		if (item->objectType < STRUCTURE_MAX) {
			const StructureInfo *si = &g_table_structureInfo[item->objectType];
			snprintf(line2, sizeof(line2), L10N("Armor: %u  Power: %+d", "Šarvai: %u  Energija: %+d"),
				si->o.hitpoints, -si->powerUsage);
		}
	} else if (item->objectType < UNIT_MAX) {
		const UnitInfo *ui = &g_table_unitInfo[item->objectType];
		snprintf(line2, sizeof(line2), L10N("HP: %u  Pow: %u  Spd: %u", "HP: %u  Galia: %u  Grt: %u"),
			ui->o.hitpoints, ui->damage, ui->movingSpeedFactor);
	}
	FactoryPlaqueText(223, line2, 117);
}

/* The original 304x120 region hit map starts at (8, 24) on SCREEN_0. */
enum { STRATEGIC_X = 8, STRATEGIC_Y = 24, STRATEGIC_W = 304, STRATEGIC_H = 120 };
static bool s_strategicActive;
static uint32 s_strategicRegions;
static int s_strategicX, s_strategicY;
static uint16 s_strategicChosenRegion;
static struct {
	uint16 region;
	int16 x, y;
} s_strategicArrows[20];
static unsigned s_strategicArrowCount;
static char s_strategicText[256], s_strategicPreviousText[256];
static int s_strategicTextStep;

void GUI_3DS_StrategicBegin(void)
{
	s_strategicActive = true;
	s_strategicRegions = 0;
	s_strategicChosenRegion = 0;
	s_strategicText[0] = s_strategicPreviousText[0] = '\0';
	s_strategicTextStep = 13;
}

void GUI_3DS_StrategicEnd(void)
{
	s_strategicActive = false;
	GUI_3DS_StrategicSelecting(0);
}

bool GUI_3DS_StrategicActive(void) { return s_strategicActive; }
bool GUI_3DS_StrategicIsSelecting(void) { return s_strategicRegions != 0; }

void GUI_3DS_StrategicSelecting(uint32 regions)
{
	s_strategicRegions = regions;
	s_strategicArrowCount = 0;
	s_strategicChosenRegion = 0;
	s_strategicX = TOP_WIDTH / 2;
	s_strategicY = 100;
}

void GUI_3DS_StrategicAddArrow(uint16 region, int16 x, int16 y)
{
	if (s_strategicArrowCount >= sizeof(s_strategicArrows) / sizeof(s_strategicArrows[0])) return;
	s_strategicArrows[s_strategicArrowCount].region = region;
	s_strategicArrows[s_strategicArrowCount].x = x;
	s_strategicArrows[s_strategicArrowCount].y = y;
	s_strategicArrowCount++;
}

void GUI_3DS_StrategicMoveCursor(int dx, int dy)
{
	/* Match the first and last pixels of the scaled playable map. */
	const int left = (STRATEGIC_X * TOP_WIDTH + SCREEN_WIDTH - 1) / SCREEN_WIDTH;
	const int right = ((STRATEGIC_X + STRATEGIC_W) * TOP_WIDTH - 1) / SCREEN_WIDTH;
	const int top = (STRATEGIC_Y * TOP_HEIGHT + SCREEN_HEIGHT - 1) / SCREEN_HEIGHT;
	const int bottom = ((STRATEGIC_Y + STRATEGIC_H) * TOP_HEIGHT - 1) / SCREEN_HEIGHT;
	s_strategicX = clamp(s_strategicX + dx, left, right);
	s_strategicY = clamp(s_strategicY + dy, top, bottom);
}

static uint16 StrategicHoveredRegion(void)
{
	if (!s_strategicRegions || !g_fileRgnclkCPS) return 0;
	int x = s_strategicX * SCREEN_WIDTH / TOP_WIDTH;
	int y = s_strategicY * SCREEN_HEIGHT / TOP_HEIGHT;
	uint16 arrowRegion = 0;
	int nearest = 16 * 16;
	for (unsigned i = 0; i < s_strategicArrowCount; i++) {
		int dx = x - s_strategicArrows[i].x;
		int dy = y - s_strategicArrows[i].y;
		if (dx < 0 || dx >= 16 || dy < 0 || dy >= 16) continue;
		int distance = (dx - 8) * (dx - 8) + (dy - 8) * (dy - 8);
		if (distance >= nearest) continue;
		nearest = distance;
		arrowRegion = s_strategicArrows[i].region;
	}
	if (arrowRegion != 0) return arrowRegion;

	uint8 region = g_fileRgnclkCPS[(y - STRATEGIC_Y) * STRATEGIC_W + x - STRATEGIC_X];
	return region > 0 && region < 32 && (s_strategicRegions & ((uint32)1 << region)) ? region : 0;
}

void GUI_3DS_StrategicConfirm(void)
{
	s_strategicChosenRegion = StrategicHoveredRegion();
}

uint16 GUI_3DS_StrategicTakeRegion(void)
{
	uint16 region = s_strategicChosenRegion;
	s_strategicChosenRegion = 0;
	return region;
}

void GUI_3DS_StrategicSetText(const char *text)
{
	memcpy(s_strategicPreviousText, s_strategicText, sizeof(s_strategicText));
	snprintf(s_strategicText, sizeof(s_strategicText), "%s", text != NULL ? text : "");
	s_strategicTextStep = 0;
}

void GUI_3DS_StrategicTextStep(int step)
{
	s_strategicTextStep = step;
}

static void StrategicTop(void)
{
	memset(g_topScreenBuffer, 0, TOP_WIDTH * TOP_HEIGHT);
	memset(g_topScreenUIMask, 0, sizeof(g_topScreenUIMask));

	/* Start below the title printed on the original map artwork. */
	const uint8 *src = GFX_Screen_Get_ByIndex(SCREEN_0);
	if (src != NULL) {
		for (int y = 20; y < TOP_HEIGHT; y++) {
			int sy = (y * 5) / 6;
			const uint8 *srcRow = src + sy * SCREEN_WIDTH;
			uint8 *dstRow = g_topScreenBuffer + y * TOP_WIDTH;
			for (int x = 0; x < TOP_WIDTH; x++) {
				int sx = (x * 4) / 5;
				dstRow[x] = srcRow[sx];
			}
		}
	}

	if (GUI_3DS_StrategicIsSelecting()) {
		GUI_3DS_DrawCursor(g_topScreenBuffer, TOP_WIDTH, s_strategicX, s_strategicY);
	}
}

static void StrategicBottom(void)
{
	memset(g_bottomScreenBuffer, 0, BOTTOM_WIDTH * BOTTOM_HEIGHT);
	memset(g_bottomScreenUIMask, 2, sizeof(g_bottomScreenUIMask));

	const int cx = BOTTOM_WIDTH / 2;
	if (GUI_3DS_StrategicIsSelecting()) {
		Cairo_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, cx, 124,
			L10N("SELECT YOUR NEXT MISSION", "PASIRINKITE KITĄ MISIJĄ"), 216, CAIRO_NO_SHADOW);
		return;
	}

	/* Scroll the old sentence out and the new one in through the same window. */
	enum { SUBTITLE_TOP = 88, SUBTITLE_BOTTOM = 152 };
	int offset = (SUBTITLE_BOTTOM - SUBTITLE_TOP) * s_strategicTextStep / 13;
	if (s_strategicTextStep < 13) GUI_3DS_DrawCenteredSubtitle(s_strategicPreviousText, 120 - offset);
	GUI_3DS_DrawCenteredSubtitle(s_strategicText, 120 + SUBTITLE_BOTTOM - SUBTITLE_TOP - offset);
	memset(g_bottomScreenBuffer, 0, SUBTITLE_TOP * BOTTOM_WIDTH);
	memset(g_bottomScreenBuffer + SUBTITLE_BOTTOM * BOTTOM_WIDTH, 0,
		(BOTTOM_HEIGHT - SUBTITLE_BOTTOM) * BOTTOM_WIDTH);
}

/* ---- In-game modal popup on top screen (Figma node 165:815) ---- */
static void PopupPortrait(int x, int y, int width, int height, uint16 spriteID)
{
	uint8 *sprite = Sprite_GetByIndex(spriteID);
	if (!sprite) return;
	int sw = Sprite_GetWidth(sprite), sh = Sprite_GetHeight(sprite);
	if (sw <= 0 || sw > 64 || sh <= 0 || sh > 64) return;
	static uint8 saved[64 * 64];
	uint8 *scratch = GFX_Screen_Get_ByIndex(SCREEN_2);
	if (!scratch) return;
	for (int row = 0; row < sh; row++) {
		memcpy(saved + row * sw, scratch + row * 320, sw);
		memset(scratch + row * 320, 0, sw);
	}
	GUI_DrawSprite(SCREEN_2, sprite, 0, 0, 0, 0);

	int scale = min(width / sw, height / sh), dw, dh;
	if (scale > 0) {
		dw = sw * scale;
		dh = sh * scale;
	} else if (width * sh <= height * sw) {
		dw = width;
		dh = max(1, sh * width / sw);
	} else {
		dh = height;
		dw = max(1, sw * height / sh);
	}
	int left = x + (width - dw) / 2, top = y + (height - dh) / 2;

	/* Clear portrait box area with palette index 0 (black background) and mask 0 */
	for (int r = 0; r < height; r++) {
		int py = y + r;
		if (py < 0 || py >= TOP_HEIGHT) continue;
		for (int c = 0; c < width; c++) {
			int px = x + c;
			if (px < 0 || px >= TOP_WIDTH) continue;
			int at = py * TOP_WIDTH + px;
			g_topScreenBuffer[at] = 0;
			g_topScreenUIMask[at] = 0;
		}
	}

	/* Copy scaled sprite pixels */
	for (int row = 0; row < dh; row++) {
		int py = top + row;
		if (py < 0 || py >= TOP_HEIGHT) continue;
		for (int col = 0; col < dw; col++) {
			int px = left + col;
			if (px < 0 || px >= TOP_WIDTH) continue;
			int at = py * TOP_WIDTH + px;
			uint8 c = scratch[(row * sh / dh) * 320 + col * sw / dw];
			g_topScreenBuffer[at] = c;
			g_topScreenUIMask[at] = 0;
		}
	}
	for (int row = 0; row < sh; row++) {
		memcpy(scratch + row * SCREEN_WIDTH, saved + row * sw, sw);
	}
}

static void GUI_3DS_DrawPopup(const char *text, uint16 spriteID)
{
	if (!g_topScreenBuffer) return;

	int x0 = (TOP_WIDTH - POPUP_WIDTH) / 2;   /* 58 */
	int y0 = (TOP_HEIGHT - POPUP_HEIGHT) / 2; /* 33 */
	int x1 = x0 + POPUP_WIDTH - 1;            /* 341 */
	int y1 = y0 + POPUP_HEIGHT - 1;           /* 205 */

	/* 1. Main background fill with POPUP_UI_MASK */
	for (int y = y0; y <= y1; y++) {
		int row = y * TOP_WIDTH;
		for (int x = x0; x <= x1; x++) {
			g_topScreenBuffer[row + x] = POPUP_BG;
			g_topScreenUIMask[row + x] = POPUP_UI_MASK;
		}
	}

	/* 2. Outer bevel: 2px raised (Top/Left = WHITE, Bottom/Right = BORDER_DARK) */
	for (int y = y0; y <= y0 + 1; y++) {
		for (int x = x0; x <= x1 - 2; x++) g_topScreenBuffer[y * TOP_WIDTH + x] = POPUP_WHITE;
	}
	for (int y = y0; y <= y1 - 2; y++) {
		for (int x = x0; x <= x0 + 1; x++) g_topScreenBuffer[y * TOP_WIDTH + x] = POPUP_WHITE;
	}
	for (int y = y1 - 1; y <= y1; y++) {
		for (int x = x0 + 2; x <= x1; x++) g_topScreenBuffer[y * TOP_WIDTH + x] = POPUP_BORDER_DARK;
	}
	for (int y = y0 + 2; y <= y1; y++) {
		for (int x = x1 - 1; x <= x1; x++) g_topScreenBuffer[y * TOP_WIDTH + x] = POPUP_BORDER_DARK;
	}

	/* 3. Inner bevel: 8px inset (2px outer + 6px gap), 2px recessed (Top/Left = BORDER_DARK, Bottom/Right = WHITE) */
	int ix0 = x0 + 8; /* 66 */
	int iy0 = y0 + 8; /* 41 */
	int ix1 = x1 - 8; /* 333 */
	int iy1 = y1 - 8; /* 197 */

	for (int y = iy0; y <= iy0 + 1; y++) {
		for (int x = ix0; x <= ix1 - 2; x++) g_topScreenBuffer[y * TOP_WIDTH + x] = POPUP_BORDER_DARK;
	}
	for (int y = iy0; y <= iy1 - 2; y++) {
		for (int x = ix0; x <= ix0 + 1; x++) g_topScreenBuffer[y * TOP_WIDTH + x] = POPUP_BORDER_DARK;
	}
	for (int y = iy1 - 1; y <= iy1; y++) {
		for (int x = ix0 + 2; x <= ix1; x++) g_topScreenBuffer[y * TOP_WIDTH + x] = POPUP_WHITE;
	}
	for (int y = iy0 + 2; y <= iy1; y++) {
		for (int x = ix1 - 1; x <= ix1; x++) g_topScreenBuffer[y * TOP_WIDTH + x] = POPUP_WHITE;
	}

	/* 4. Building / Object picture box */
	bool hasPortrait = Sprite_GetByIndex(spriteID) != NULL;
	if (hasPortrait) {
		int bw = 64, bh = 48;
		int bx = (TOP_WIDTH - bw) / 2; /* 168 */
		int by = 51;

		/* 1px white border around the picture box */
		for (int x = bx - 1; x <= bx + bw; x++) {
			g_topScreenBuffer[(by - 1) * TOP_WIDTH + x] = POPUP_WHITE;
			g_topScreenBuffer[(by + bh) * TOP_WIDTH + x] = POPUP_WHITE;
		}
		for (int y = by - 1; y <= by + bh; y++) {
			g_topScreenBuffer[y * TOP_WIDTH + bx - 1] = POPUP_WHITE;
			g_topScreenBuffer[y * TOP_WIDTH + bx + bw] = POPUP_WHITE;
		}

		PopupPortrait(bx, by, bw, bh, spriteID);
	}

	/* Reserve the bevel and shadow when measuring, and keep text below the portrait. */
	enum { PADDING = 5, SHADOW = 1 };
	const int textTop = hasPortrait ? 105 : iy0 + 2 + PADDING;
	const int textWidth = ix1 - ix0 + 1 - 2 * (2 + PADDING) - SHADOW;
	const int textBottom = iy1 - 1 - PADDING;
	const int textHeight = textBottom - textTop;
	char lines[8][128];
	const char *wrapped[8];
	const char *remaining;
	int maxLines = 1 + (textHeight - CAIRO_GLYPH_H - SHADOW) / CAIRO_LINE_H;
	int count = Cairo_WrapText(text, textWidth, lines, maxLines, &remaining);
	if (count == 0) return;
	if (*remaining) {
		/* Keep the portrait and bevel fixed while the text rises in a clipped window. */
		char line[1][128];
		const int scrollWidth = textWidth - 8;
		int totalLines = 0;
		for (const char *p = text; *p; p = remaining) {
			totalLines += Cairo_WrapText(p, scrollWidth, line, 1, &remaining);
		}
		int contentHeight = (totalLines - 1) * CAIRO_LINE_H + CAIRO_GLYPH_H + SHADOW;
		int maxScroll = contentHeight - textHeight;
		if (maxScroll > 0 && contentHeight > 0) {
			uint32 elapsed = g_timerGUI - s_messageStartTick;
			int scroll = elapsed > 120 ? min((elapsed - 120) / 4, (uint32)maxScroll) : 0;
			int baseline = textTop + CAIRO_BASE_ROW - scroll;
			for (const char *p = text; *p && baseline - CAIRO_BASE_ROW < textBottom; p = remaining) {
				if (Cairo_WrapText(p, scrollWidth, line, 1, &remaining) == 0) break;
				Cairo_DrawTextClipped(g_topScreenBuffer, TOP_WIDTH, TOP_WIDTH / 2 - 3, baseline,
					line[0], -1, POPUP_WHITE, POPUP_BLACK, textTop, textBottom);
				baseline += CAIRO_LINE_H;
			}
			int barX = ix1 - 7;
			int thumbHeight = max(7, textHeight * textHeight / contentHeight);
			int thumbY = textTop + scroll * (textHeight - thumbHeight) / maxScroll;
			GUI_3DS_FillRect(g_topScreenBuffer, TOP_WIDTH, barX, textTop, barX + 2, textBottom - 1, POPUP_BORDER_DARK);
			GUI_3DS_FillRect(g_topScreenBuffer, TOP_WIDTH, barX, thumbY, barX + 2, thumbY + thumbHeight - 1, POPUP_WHITE);
			return;
		}
	}
	for (int i = 0; i < count; i++) wrapped[i] = lines[i];
	CairoBounds bounds = Cairo_BlockBounds(wrapped, count, CAIRO_LINE_H, true);
	int baseline = textTop + (textHeight - 1 - bounds.top - bounds.bottom) / 2;
	for (int i = 0; i < count; i++) {
		Cairo_DrawText(g_topScreenBuffer, TOP_WIDTH, TOP_WIDTH / 2, baseline + i * CAIRO_LINE_H,
			lines[i], POPUP_WHITE, POPUP_BLACK);
	}
}

static void DrawMarkerRect(uint8 *buf, uint8 *mask, int stride, int x1, int y1, int x2, int y2, uint8 color)
{
	x1 = max(0, x1);
	x2 = min(stride - 1, x2);
	y1 = max(0, y1);
	y2 = min(TOP_HEIGHT - 1, y2);
	if (x1 > x2 || y1 > y2) return;
	for (int y = y1; y <= y2; y++) {
		memset(buf + y * stride + x1, color, x2 - x1 + 1);
		memset(mask + y * stride + x1, TACTICAL_UI_MASK, x2 - x1 + 1);
	}
}

static void DrawSelectionCorners(uint8 *buf, uint8 *mask, int stride,
		int left, int top, int right, int bottom, int arm)
{
	for (int side = 0; side < 2; side++) {
		int edgeY = side ? bottom : top;
		int insideY = edgeY + (side ? -arm : arm);
		DrawMarkerRect(buf, mask, stride, left, edgeY, left + arm, edgeY, TC_WHITE);
		DrawMarkerRect(buf, mask, stride, right - arm, edgeY, right, edgeY, TC_WHITE);
		DrawMarkerRect(buf, mask, stride, left, min(edgeY, insideY), left, max(edgeY, insideY), TC_WHITE);
		DrawMarkerRect(buf, mask, stride, right, min(edgeY, insideY), right, max(edgeY, insideY), TC_WHITE);
	}
}

static void DrawUnitMarker(const Unit *unit, int x, int y)
{
	const UnitInfo *info = &g_table_unitInfo[unit->o.type];
	int radius = info->movementType == MOVEMENT_FOOT ? 9 : 13;
	int left = x - radius, right = x + radius;
	int top = y - radius, bottom = y + radius;
	DrawSelectionCorners(g_topScreenBuffer, g_topScreenUIMask, TOP_WIDTH, left, top, right, bottom, 4);
	if (unit->o.houseID != g_playerHouseID) return;
	int maxHP = info->o.hitpoints;
	int hp = min(unit->o.hitpoints, maxHP);
	int width = right - left - 1;
	int fill = maxHP ? hp * width / maxHP : 0;
	uint8 color = hp <= maxHP / 4 ? TC_RED : hp <= maxHP / 2 ? TC_YELLOW : TC_GREEN;
	DrawMarkerRect(g_topScreenBuffer, g_topScreenUIMask, TOP_WIDTH, left, top - 6, right, top - 2, TC_BLACK);
	if (fill > 0) DrawMarkerRect(g_topScreenBuffer, g_topScreenUIMask, TOP_WIDTH, left + 1, top - 5, left + fill, top - 3, color);
}

void GUI_3DS_RenderTopScreen(void)
{
	if (g_topScreenBuffer == NULL || s_launchActive) return;
	if (s_strategicActive) { StrategicTop(); return; }
 if(g_3dsModalMessageActive) {
  memcpy(g_topScreenBuffer,s_messageTop,sizeof(s_messageTop));
  memcpy(g_topScreenUIMask,s_messageTopMask,sizeof(s_messageTopMask));
  GUI_3DS_DrawPopup(s_messageText, s_messageSpriteID);
  return;
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
			"Kilminga Kaladano\ngiminė.\n\nStipri aviacija\n(Ornitopteriai), garso\ntankai ir narsūs\nFremenų kariai.",
			"Turtingas Sigma\nDraconis kartelis.\n\nGreiti triračiai,\nsabotažistai ir\nNukreipėjai, perimantys\npriešo techniką.",
			"Žiauri Giedi Prime\ngiminė.\n\nSunkiai šarvuota\ntechnika, Niokotojai\nir atominės Mirties\nRankos raketos."
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
				const char *emptyStr = L10N("Empty Slot", "Tuščia vieta");
				Cairo_DrawText(g_topScreenBuffer, TOP_WIDTH, TOP_WIDTH / 2, 124, emptyStr, 110, 0);
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
				const char *pauseStr = L10N("PAUSED", "PAUZĖ");
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
			uint32 *d32 = (uint32 *)dstRow;
			const uint8 *s = srcRow;
			for (int block = 0; block < 20; block++) {
				uint32 s0 = s[0], s1 = s[1], s2 = s[2], s3 = s[3];
				uint32 s4 = s[4], s5 = s[5], s6 = s[6], s7 = s[7];
				uint32 s8 = s[8], s9 = s[9], s10 = s[10], s11 = s[11];
				d32[0] = s0 | (s0 << 8) | (s1 << 16) | (s1 << 24);
				d32[1] = s2 | (s3 << 8) | (s3 << 16) | (s4 << 24);
				d32[2] = s4 | (s5 << 8) | (s6 << 16) | (s6 << 24);
				d32[3] = s7 | (s7 << 8) | (s8 << 16) | (s9 << 24);
				d32[4] = s9 | (s10 << 8) | (s10 << 16) | (s11 << 24);
				s += 12;
				d32 += 5;
			}
		}
	} else {
		memset(g_topScreenBuffer, 0, TOP_WIDTH * TOP_HEIGHT);
	}

	int count = g_selectedUnitsCount ? g_selectedUnitsCount : 1;
	for (int i = 0; i < count; i++) {
		Unit *u = g_selectedUnitsCount ? g_selectedUnits[i] : g_unitSelected;
		if (!u || !u->o.flags.s.allocated || u->o.flags.s.isNotOnMap || u->o.type >= UNIT_MAX) continue;
		if (u->o.houseID != g_playerHouseID && (u->o.seenByHouses & (1 << g_playerHouseID)) == 0 && !g_debugScenario) continue;
		int16 sx, sy;
		if (GUI_3DS_MapPosToScreenPos(u->o.position, &sx, &sy)) DrawUnitMarker(u, sx, sy);
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

	if (s_toastText[0] != '\0') {
		if ((int32)(s_toastUntilTick - g_timerGUI) > 0) {
			if (!g_3dsModalMessageActive) {
				const int w = 216, h = 24;
				const int x0 = (TOP_WIDTH - w) / 2, y0 = 10;
				GUI_3DS_FillRect(g_topScreenBuffer, TOP_WIDTH, x0, y0, x0 + w - 1, y0 + h - 1, 0);
				GUI_3DS_DrawRect(g_topScreenBuffer, TOP_WIDTH, x0, y0, x0 + w - 1, y0 + h - 1, 27);
				for (int py = y0; py < y0 + h; py++) {
					memset(g_topScreenUIMask + py * TOP_WIDTH + x0, 2, w);
				}
				Cairo_DrawText(g_topScreenBuffer, TOP_WIDTH, TOP_WIDTH / 2, y0 + 16, s_toastText, 216, CAIRO_NO_SHADOW);
			}
		} else {
			s_toastText[0] = '\0';
		}
	}

	/* Draw Cursor (hidden during modal dialog message) */
	if (!g_3dsModalMessageActive) {
		int16 cx = g_cursor3DS_X;
		int16 cy = g_cursor3DS_Y;

		int scrollX = (!g_boxSelectActive) ? ((cx <= 12) ? -1 : ((cx >= TOP_WIDTH - 13) ? 1 : 0)) : 0;
		int scrollY = (!g_boxSelectActive) ? ((cy <= 12) ? -1 : ((cy >= TOP_HEIGHT - 13) ? 1 : 0)) : 0;

		if (scrollX != 0 || scrollY != 0) {
			GUI_3DS_DrawScrollCursor(g_topScreenBuffer, TOP_WIDTH, cx, cy, scrollX, scrollY);
		} else {
			bool isBoxDragging = g_boxSelectActive && (abs(g_boxSelectCurrX - g_boxSelectStartX) >= 4 || abs(g_boxSelectCurrY - g_boxSelectStartY) >= 4);
			bool hasFriendlyUnitSelected = (g_selectedUnitsCount > 0) ||
			    (g_unitSelected != NULL && g_unitSelected->o.flags.s.allocated && !g_unitSelected->o.flags.s.isNotOnMap && g_unitSelected->o.houseID == g_playerHouseID);
			bool showTargetCursor = !GUI_3DS_IsMenuOrDialogActive() && !GUI_3DS_CustomMenuActive() &&
			    g_selectionType != SELECTIONTYPE_PLACE && !isBoxDragging &&
			    (g_selectionType == SELECTIONTYPE_TARGET || hasFriendlyUnitSelected);
			if (showTargetCursor) {
				uint8 ringColor = (CursorOverEnemy(cx, cy) || (g_selectionType == SELECTIONTYPE_TARGET && g_unitHouseMissile != NULL)) ? 8 : 254;
				GUI_3DS_DrawTargetCursor(g_topScreenBuffer, TOP_WIDTH, cx, cy, ringColor);
			} else {
				GUI_3DS_DrawCursor(g_topScreenBuffer, TOP_WIDTH, cx, cy);
			}
		}
	}
}

static const char *GUI_3DS_GetActionLabel(uint16 action)
{
	switch (action) {
		case ACTION_ATTACK:     return L10N("ATTACK", "PULTI");
		case ACTION_MOVE:       return L10N("MOVE", "JUDĖTI");
		case ACTION_RETREAT:    return L10N("RETREAT", "ATSITR.");
		case ACTION_GUARD:      return L10N("GUARD", "SAUGOTI");
		case ACTION_AREA_GUARD: return L10N("AREA GRD", "GINYBA");
		case ACTION_HARVEST:    return L10N("HARVEST", "RINKTI");
		case ACTION_RETURN:     return L10N("RETURN", "GRĮŽTI");
		case ACTION_STOP:       return L10N("STOP", "STOP");
		case ACTION_AMBUSH:     return L10N("AMBUSH", "PASALA");
		case ACTION_SABOTAGE:   return L10N("SABOTAGE", "SABOTAŽ.");
		case ACTION_DEPLOY:     return L10N("DEPLOY", "ĮKURTI");
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

static void BattleButtonLabel(int x,int y,int w,int h,const char *label,uint8 color)
{
    ButtonLabelText(x, y, w, h, label, color, (color == TC_DISABLED || color == TC_SHADOW) ? CAIRO_NO_SHADOW : TC_BLACK);
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
  BattleButtonLabel(x,y,w,h,shown,dim ? TC_DISABLED : active ? TC_ACTIVE : TC_WHITE);
}

/* Fit original art into its Figma slot, using integer scaling whenever possible. */
static void BattlePortrait(int x,int y,int width,int height,uint16 spriteID, int trim)
{
	typedef struct PortraitCacheEntry {
		uint8 pixels[53 * 69];
		const uint8 *sprite;
		uint16 spriteID;
		int width;
		int height;
		int trim;
		int dw;
		int dh;
	} PortraitCacheEntry;
	static PortraitCacheEntry s_portraitCache[16];

	uint8 *sprite = g_sprites[spriteID]; if (!sprite) return;
	PortraitCacheEntry *entry = &s_portraitCache[((unsigned)spriteID ^ (unsigned)width ^ (unsigned)(trim << 4)) & 15];
	if (width <= 69 && height <= 53 && entry->sprite == sprite && entry->spriteID == spriteID &&
	    entry->width == width && entry->height == height && entry->trim == trim) {
		int left = x + (width - entry->dw) / 2, top = y + (height - entry->dh) / 2;
		GUI_3DS_FillRect(g_bottomScreenBuffer, 320, x, y, x + width - 1, y + height - 1, TC_FACE);
		for (int row = 0; row < entry->dh; row++) {
			memcpy(g_bottomScreenBuffer + (top + row) * 320 + left, entry->pixels + row * entry->dw, entry->dw);
			memset(g_bottomScreenUIMask + (top + row) * 320 + left, 0, entry->dw);
		}
		return;
	}
	int sw = Sprite_GetWidth(sprite), sh = Sprite_GetHeight(sprite);
	if (sw <= trim * 2 || sw > 64 || sh <= trim * 2 || sh > 64) return;
	static uint8 saved[64*64]; uint8 *scratch = GFX_Screen_Get_ByIndex(SCREEN_2); if (!scratch) return;
	for (int row = 0; row < sh; row++) { memcpy(saved + row * sw, scratch + row * 320, sw); memset(scratch + row * 320, 0, sw); }
	GUI_DrawSprite(SCREEN_2, sprite, 0, 0, 0, 0);
	int artWidth = sw - trim * 2, artHeight = sh - trim * 2;
	int scale = min(width / artWidth, height / artHeight), dw, dh;
	if (scale > 0) { dw = artWidth * scale; dh = artHeight * scale; }
	else if (width * artHeight <= height * artWidth) { dw = width; dh = max(1, artHeight * width / artWidth); }
	else { dh = height; dw = max(1, artWidth * height / artHeight); }
	if (dw > 320) return;
	int left = x + (width - dw) / 2, top = y + (height - dh) / 2;
	int srcCol[320];
	for (int col = 0; col < dw; col++) srcCol[col] = trim + col * artWidth / dw;
	GUI_3DS_FillRect(g_bottomScreenBuffer, 320, x, y, x + width - 1, y + height - 1, TC_FACE);
	bool cacheable = (width <= 69 && height <= 53 && dw <= 69 && dh <= 53);
	for (int row = 0; row < dh; row++) {
		const uint8 *srcRow = scratch + (trim + row * artHeight / dh) * 320;
		uint8 *dstRow = g_bottomScreenBuffer + (top + row) * 320 + left;
		for (int col = 0; col < dw; col++) dstRow[col] = srcRow[srcCol[col]];
		memset(g_bottomScreenUIMask + (top + row) * 320 + left, 0, dw);
		if (cacheable) memcpy(entry->pixels + row * dw, dstRow, dw);
	}
	if (cacheable) {
		entry->sprite = sprite;
		entry->spriteID = spriteID;
		entry->width = width;
		entry->height = height;
		entry->trim = trim;
		entry->dw = dw;
		entry->dh = dh;
	}
	for (int row = 0; row < sh; row++) memcpy(scratch + row * 320, saved + row * sw, sw);
}

static void BattleSpecialWeaponPortrait(int x, int y, int width, int height, uint8 specialWeapon)
{
	uint8 idx = 0xFF;
	uint16 fallbackSprite = g_table_structureInfo[STRUCTURE_PALACE].o.spriteID;
	switch (specialWeapon) {
		case HOUSE_WEAPON_MISSILE:  idx = 0; fallbackSprite = 0x1E; break;
		case HOUSE_WEAPON_FREMEN:   idx = 1; fallbackSprite = 0x5E; break;
		case HOUSE_WEAPON_SABOTEUR: idx = 2; fallbackSprite = 0x60; break;
		default: break;
	}
	if (idx < 3 && (s_specialPortraitMask & (1u << idx)) != 0) {
		int dw = width;
		int dh = max(1, min(height, width * 42 / 69));
		int left = x + (width - dw) / 2;
		int top = y + (height - dh) / 2;
		GUI_3DS_FillRect(g_bottomScreenBuffer, 320, x, y, x + width - 1, y + height - 1, TC_FACE);
		for (int row = 0; row < dh; row++) {
			const uint8 *srcRow = s_specialPortraits[idx] + (row * 42 / dh) * 69;
			uint8 *dstRow = g_bottomScreenBuffer + (top + row) * 320 + left;
			for (int col = 0; col < dw; col++) dstRow[col] = srcRow[col * 69 / dw];
			memset(g_bottomScreenUIMask + (top + row) * 320 + left, 0, dw);
		}
		return;
	}
	BattlePortrait(x, y, width, height, fallbackSprite, 0);
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
 PanelMeterBar(x,y,w,h,hp,maxHP,L10N("DMG","BŪKLĖ"),104,TC_TEXT);
}

static void PanelDrawTitle(const char *title)
{
	char upper[48]; size_t out = 0;
	for (size_t i = 0; title && title[i] && out + 2 < sizeof(upper); ) {
		unsigned char c = (unsigned char)title[i];
		if (c == 0xC4 || c == 0xC5) {
			unsigned char next = (unsigned char)title[i + 1];
			if (!next) break;
			/* Odd low-bit in Lithuanian UTF-8 0xC4/0xC5 block is lowercase -> clear bit 0 for uppercase, except ė/ų/ū. */
			if (c == 0xC4 && (next == 0x85 || next == 0x8D || next == 0x97 || next == 0x99 || next == 0xAF)) next--;
			else if (c == 0xC5 && (next == 0xA1 || next == 0xAB || next == 0xB3 || next == 0xBE)) next--;
			upper[out++] = (char)c;
			upper[out++] = (char)next;
			i += 2;
		} else {
			upper[out++] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : (char)c;
			i++;
		}
	}
	upper[out] = '\0';
	Tiny5_DrawText(g_bottomScreenBuffer, 320, 245, 54, upper, TC_TEXT, false);
}

static void PanelHeaderAt(const char *title, uint16 spriteID, int portraitX)
{
	PanelDrawTitle(title);
	BattlePortrait(portraitX, 61, 69, 53, spriteID, 0);
	PanelAddHit(portraitX - 2, 59, 73, 55, HIT_PORTRAIT, 0);
}

static void PanelHeader(const char *title, uint16 spriteID, bool isUnit)
{
	VARIABLE_NOT_USED(isUnit);
	PanelHeaderAt(title, spriteID, 178);
}

static void PanelHeaderCentered(const char *title, uint16 spriteID)
{
	PanelHeaderAt(title, spriteID, 210);
}

static void PanelHeaderSpecial(const char *title, uint8 specialWeapon)
{
	PanelDrawTitle(title);
	BattleSpecialWeaponPortrait(178, 61, 69, 53, specialWeapon);
	PanelAddHit(176, 59, 73, 55, HIT_PORTRAIT, 0);
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
	enum { X = 256, Y = 152, STEP = 15, CELLS = 3, DOT_STEP = 5 };
	int filled = clamp(pct * 9 / 100, 0, 9);
	for (int line = 0; line <= CELLS; line++) {
		for (int d = 0; d <= STEP * CELLS; d += DOT_STEP) {
			GUI_3DS_PutPixel(g_bottomScreenBuffer, BOTTOM_WIDTH, X + d, Y + line * STEP, TC_DOT);
			GUI_3DS_PutPixel(g_bottomScreenBuffer, BOTTOM_WIDTH, X + line * STEP, Y + d, TC_DOT);
		}
	}
	for (int cell = 0; cell < filled; cell++) {
		int x = X + (cell % CELLS) * STEP, y = Y + (cell / CELLS) * STEP;
		GUI_3DS_FillRect(g_bottomScreenBuffer, BOTTOM_WIDTH, x + 1, y + 1, x + STEP - 1, y + STEP - 1, TC_SHADOW);
		GUI_3DS_DrawBevel(g_bottomScreenBuffer, BOTTOM_WIDTH, x + 1, y + 1, x + STEP - 1, y + STEP - 1, TC_DOT, TC_TEXT);
	}
}

static void PanelStructureStatusBox(Structure *s,const StructureInfo *si)
{
 const ObjectInfo *info=&si->o;
 bool mine=s->o.houseID==g_playerHouseID;
 bool ready=s->o.type==STRUCTURE_CONSTRUCTION_YARD && s->o.linkedID!=STRUCTURE_INVALID && s->countDown==0;
 bool building=info->flags.factory && s->o.linkedID!=STRUCTURE_INVALID;
 char label[48];
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
 bool palaceReady = false;
 if(s->o.flags.s.upgrading) pct=clamp(100-(int)s->upgradeTimeLeft,0,100);
 else if(s->o.type==STRUCTURE_PALACE) {
  uint16 maxCD=s->o.houseID<HOUSE_MAX ? g_table_houseInfo[s->o.houseID].specialCountDown : 0;
  palaceReady=(mine && s->countDown==0 && g_unitHouseMissile==NULL);
  pct=(s->countDown==0 && !(mine && g_unitHouseMissile!=NULL)) ? 100 : (maxCD ? clamp(((int)maxCD-(int)s->countDown)*100/(int)maxCD,0,99) : 0);
 }
 const char *action = (s->o.type == STRUCTURE_CONSTRUCTION_YARD) ? L10N("BUILD IT (Y)", "STATYTI (Y)") : L10N("BUILD IT (Y)", "GAMINTI (Y)");
 if(!mine) action=L10N("ENEMY","PRIEŠAS");
 else if(ready) action=L10N("PLACE IT (Y)","STATYTI (Y)");
 else if(building) action=L10N("CANCEL (L+Y)","ATŠAUKTI (L+Y)");
 else if(s->o.type==STRUCTURE_STARPORT) action=L10N("ORDER (Y)","UŽSAKYTI (Y)");
 else if(s->o.type==STRUCTURE_PALACE) action=(g_unitHouseMissile!=NULL) ? L10N("AIMING","TAIKOMASI") : (s->countDown ? L10N("CHARGING","KRAUNASI") : L10N("LAUNCH (Y)","PALEISTI (Y)"));
 else if(!info->flags.factory) action=L10N("SELECT ALL (X)","VISI KARIAI (X)");
 snprintf(label,sizeof(label),"%s",action);
	BattleButton(174, 146, 142, 76, "", false, false);
	if (s->o.type == STRUCTURE_PALACE && s->o.houseID < HOUSE_MAX) {
		BattleSpecialWeaponPortrait(180, 150, 66, 51, g_table_houseInfo[s->o.houseID].specialWeapon);
	} else {
		BattlePortrait(180, 150, 66, 51, sprite, 0);
	}
	BattleProgress(pct);
	BattleButtonLabel(174, 198, 142, 24, label, (building && s->o.flags.s.onHold) ? TC_DISABLED : (ready || palaceReady) ? TC_ACTIVE : TC_WHITE);
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
  if(i>=visible) {
   char more[16]; snprintf(more,sizeof(more),"+%d",count-visible);
   BattleText(x+22,y+18,more,g_fontNew8p,TC_TEXT,false);
   continue;
  }
  Unit *u=units[i];
  uint32 maxHP=g_table_unitInfo[u->o.type].o.hitpoints;
  uint32 hp=min((uint32)u->o.hitpoints,maxHP);
  uint8 color=hp<=maxHP/4 ? TC_RED : hp<=maxHP/2 ? TC_YELLOW : TC_GREEN;
  int fill=maxHP ? min(40,(int)(hp*40/maxHP)) : 0;
  /* Health bar above unit portrait matches the top-screen unit marker style. */
  DrawMarkerRect(g_bottomScreenBuffer, g_bottomScreenUIMask, BOTTOM_WIDTH, x+1, y, x+42, y+4, TC_BLACK);
  if(fill>0) DrawMarkerRect(g_bottomScreenBuffer, g_bottomScreenUIMask, BOTTOM_WIDTH, x+2, y+1, x+1+fill, y+3, color);
  /* Skip the white frame baked into the original portrait. */
  BattlePortrait(x+2,y+7,40,28,g_table_unitInfo[u->o.type].o.spriteID,1);
  DrawSelectionCorners(g_bottomScreenBuffer, g_bottomScreenUIMask, BOTTOM_WIDTH,
    x+1, y+6, x+42, y+35, 6);
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
		PanelHeaderCentered(title, si->o.spriteID);

		BattleText(245, 118, L10N("SELECT LOCATION", "PASIRINKITE VIETĄ"), g_fontNew6p, TC_WHITE, true);
		BattleText(245, 132, L10N("TO BUILD", "STATYBAI"), g_fontNew6p, TC_WHITE, true);

		BattleButton(174, 172, 142, 22, L10N("PLACE (A)", "STATYTI (A)"), false, g_selectionState <= 0);
		PanelAddHit(174, 172, 142, 22, HIT_PLACE_HERE, 0);
		BattleButton(174, 198, 142, 22, L10N("CANCEL (B)", "ATŠAUKTI (B)"), false, false);
		PanelAddHit(174, 198, 142, 22, HIT_CANCEL, 0);
		return;
	}

	/* ---- Target selection --------------------------------------------------- */
	if (g_selectionType == SELECTIONTYPE_TARGET) {
		const char *actName = (g_unitHouseMissile != NULL) ? L10N("DEATH HAND", "MIRTIES RANKA") : GUI_3DS_GetActionLabel(g_activeAction);
		if (g_unitSelected == NULL && g_unitHouseMissile != NULL) {
			PanelHeaderSpecial(actName, HOUSE_WEAPON_MISSILE);
		} else {
			uint16 sprite = (g_unitSelected != NULL) ? g_table_unitInfo[g_unitSelected->o.type].o.spriteID : 0;
			PanelHeader(actName, sprite, true);
		}
		BattleText(245, 118, L10N("SELECT TARGET", "PASIRINKITE"), g_fontNew6p, TC_TEXT, false);
		BattleText(245, 132, L10N("ON THE MAP.", "TAIKINĮ ŽEMĖLAPY."), g_fontNew6p, TC_TEXT, false);
		BattleButton(174, 172, 142, 22, L10N("CONFIRM (A)", "PATVIRTINTI (A)"), false, false);
		PanelAddHit(174, 172, 142, 22, HIT_CONFIRM, 0);
		BattleButton(174, 198, 142, 22, L10N("CANCEL (B)", "ATŠAUKTI (B)"), false, false);
		PanelAddHit(174, 198, 142, 22, HIT_CANCEL, 0);
		return;
	}

    /* A group uses individual portraits and health instead of the single-unit orders. */
    if(g_selectedUnitsCount>1 && DrawSquadPanel()) return;

	/* ---- Unit(s) selected --------------------------------------------------- */
	if (g_unitSelected != NULL && g_unitSelected->o.flags.s.allocated &&
	    (g_unitSelected->o.houseID == g_playerHouseID || (g_unitSelected->o.seenByHouses & (1 << g_playerHouseID)) != 0 || g_debugScenario)) {
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
			snprintf(title, sizeof(title), L10N("SQUAD x%u", "BŪRYS x%u"), g_selectedUnitsCount);
		} else {
			hp = g_unitSelected->o.hitpoints;
			maxHP = ui->o.hitpoints;
			snprintf(title, sizeof(title), "%.18s", String_Get_ByIndex(ui->o.stringID_abbrev));
		}
		PanelHeader(title, ui->o.spriteID, true);
		PanelDamageBar(256, 61, 56, 20, hp, maxHP);
		if (g_unitSelected->o.houseID != g_playerHouseID) return;

        bool combat = false;
        ActionType secondary = ACTION_MAX;
        for (int slot = 0; slot < 4; slot++) {
            ActionType action = ui->o.actionsPlayer[slot];
            if (action == ACTION_ATTACK) combat = true;
            if (action == ACTION_RETREAT || action == ACTION_DESTRUCT) secondary = action;
        }
        if(combat) {
         bool isAttackMove = (g_unitSelected->actionID==ACTION_ATTACK || g_unitSelected->actionID==ACTION_MOVE ||
                              g_unitSelected->nextActionID==ACTION_ATTACK || g_unitSelected->nextActionID==ACTION_MOVE);
         bool isGuard = (g_unitSelected->actionID==ACTION_GUARD || g_unitSelected->nextActionID==ACTION_GUARD);
         BattleButton(174,118,142,50,L10N("ATTACK /\nMOVE (A)","PULTI /\nJUDĖTI (A)"),isAttackMove,false);
         PanelAddHit(174,118,142,50,HIT_SMART_ORDER,0);
         if (secondary != ACTION_MAX) {
             const char *label = secondary == ACTION_RETREAT ? L10N("RETREAT (R)", "ATSITR. (R)") : GUI_3DS_GetActionLabel(secondary);
             bool active = g_unitSelected->actionID == secondary || g_unitSelected->nextActionID == secondary;
             BattleButton(174, 170, 142, 24, label, active, false);
             PanelAddHit(174, 170, 142, 24, HIT_UNIT_ACTION, secondary);
         }
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
			bool damaged = (s->o.hitpoints < si->o.hitpoints);
			if (!repairing && !damaged && s->o.flags.s.upgrading) {
				BattleButton(174, 120, 142, 22, L10N("UPGRADING", "TOBULINAMA"), true, false);
				PanelAddHit(174, 120, 142, 22, HIT_UPGRADE, 0);
			} else if (!repairing && !damaged && Structure_IsUpgradable(s)) {
				BattleButton(174, 120, 142, 22, L10N("UPGRADE (L+X)", "TOBULINTI (L+X)"), false, false);
				PanelAddHit(174, 120, 142, 22, HIT_UPGRADE, 0);
			} else {
				BattleButton(174, 120, 142, 22, repairing ? L10N("REPAIRING (R)", "TAISOMA (R)") : L10N("REPAIR (R)", "TAISYTI (R)"), repairing, !repairing && !damaged);
				PanelAddHit(174, 120, 142, 22, HIT_REPAIR, 0);
			}
			if (si->o.flags.factory || s->o.type == STRUCTURE_PALACE) {
				PanelStructureStatusBox(s, si);
			}
		}
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
		const char *powerStr = powerOk ? L10N("POWER OK", "GALIA OK") : L10N("POWER LOW", "MAŽAI GALIOS");
		PanelMeterBar(217, 55, 56, 20, barVal, barMax, powerStr, 96, powerOk ? TC_TEXT : TC_RED);

		BattleButton(174, 118, 142, 50, L10N("SELECT ALL\nUNITS (X)", "VISI\nKARIAI (X)"), false, false);
		PanelAddHit(174, 118, 142, 50, HIT_PRIMARY, 0);
		BattleButton(174, 170, 142, 50, L10N("JUMP TO BASE\n(SELECT)", "Į BAZĘ\n(SELECT)"), false, false);
		PanelAddHit(174, 170, 142, 50, HIT_JUMP_BASE, 0);
	}
}

enum { BATTLE_TRIM_X = 150, BATTLE_PANEL_X = 170 };

static void DrawBattleTrim(const uint8 *src)
{
	if (src == NULL) return;

	static bool s_trimLUTInit = false;
	static uint16 s_trimSrcY[200];
	static uint8  s_trimSrcX[18];
	static uint16 s_trimBotSrcY[16];
	static uint16 s_trimBotSrcX[150];
	if (!s_trimLUTInit) {
		for (int y = 40; y < BOTTOM_HEIGHT; y++) {
			int sourceY;
			if (y < 58) sourceY = y < 55 ? y : y - 1;
			else if (y < 71) sourceY = 57 + y - 58;
			else if (y < 222) sourceY = 70 + (y - 71) % 13;
			else sourceY = 182 + y - 222;
			s_trimSrcY[y - 40] = (uint16)sourceY;
		}
		for (int x = 0; x < 18; x++) s_trimSrcX[x] = (uint8)(x * 14 / 18);
		for (int y = 224; y < BOTTOM_HEIGHT; y++) s_trimBotSrcY[y - 224] = (uint16)(125 + (y - 224) * 10 / 15);
		for (int x = BATTLE_PANEL_X; x < BOTTOM_WIDTH; x++) {
			s_trimBotSrcX[x - BATTLE_PANEL_X] = (uint16)(x < 179 ? 256 + x - BATTLE_PANEL_X : x >= 311 ? x : 265 + (x - 179) % 11);
		}
		s_trimLUTInit = true;
	}

	/* Keep the original end caps. Tile full stripe periods between the collars
	 * and widen the divider without changing its house palette. */
	for (int y = 40; y < BOTTOM_HEIGHT; y++) {
		const uint8 *stripe = src + s_trimSrcY[y - 40] * SCREEN_WIDTH + 240;
		uint8 *dst = g_bottomScreenBuffer + y * BOTTOM_WIDTH + BATTLE_TRIM_X;
		for (int x = 0; x < 18; x++) dst[x] = stripe[s_trimSrcX[x]];
		dst[18] = stripe[14];
		dst[19] = stripe[15];
	}

	/* Preserve the thin gold edges and stretch the striped center to the
	 * screen edge. The middle is twelve complete repeats of the original. */
	for (int y = 224; y < BOTTOM_HEIGHT; y++) {
		const uint8 *srcRow = src + s_trimBotSrcY[y - 224] * SCREEN_WIDTH;
		uint8 *dst = g_bottomScreenBuffer + y * BOTTOM_WIDTH + BATTLE_PANEL_X;
		for (int x = 0; x < BOTTOM_WIDTH - BATTLE_PANEL_X; x++) {
			dst[x] = srcRow[s_trimBotSrcX[x]];
		}
	}
}

void GUI_3DS_RenderBottomScreen(void)
{
	if (g_bottomScreenBuffer == NULL || s_launchActive) return;
	if (s_strategicActive) { StrategicBottom(); return; }
 if(g_3dsModalMessageActive) {
  memcpy(g_bottomScreenBuffer,s_messageBottom,sizeof(s_messageBottom));
  memcpy(g_bottomScreenUIMask,s_messageBottomMask,sizeof(s_messageBottomMask));
  return;
 }
 if(s_resultsActive && !CustomDialog()) {
  memset(g_bottomScreenBuffer,0,320*240); memset(g_bottomScreenUIMask,2,sizeof(g_bottomScreenUIMask));
  MenuButton(40,102,240,36,s_resultsButtons && !s_resultsContinueMission ? L10N("Back (B)","Atgal (B)") : L10N("Continue (A)","Tęsti (A)"),true,false);
  return;
 }
	memset(g_bottomScreenUIMask, 0, sizeof(g_bottomScreenUIMask));

	/* 0. Bene Gesserit House Join Confirmation & Mentat Briefing Confirmation (pure black bottom screen) */
	if (s_mentatButtons) {
		memset(g_bottomScreenBuffer, 0, BOTTOM_WIDTH * BOTTOM_HEIGHT);
		memset(g_bottomScreenUIMask, 2, sizeof(g_bottomScreenUIMask));

		if (s_subtitleText[0] != '\0') {
			GUI_3DS_DrawCenteredSubtitle(s_subtitleText, 82);
		}
		if (s_mentatButtons->next == NULL) {
			if (s_mentatButtons->drawParameterNormal.spriteID == 377 && s_mentatTextHasMore) {
				MenuButton(8, 168, 144, 36, L10N("Next (A)", "Tęsti (A)"), true, false);
				MenuButton(168, 168, 144, 36, L10N("Back (B)", "Atgal (B)"), false, false);
			} else {
				const char *opt = (s_mentatButtons->drawParameterNormal.spriteID == 379)
					? L10N("Proceed (A)", "Tęsti (A)")
					: L10N("Back (B)", "Atgal (B)");
				MenuButton(60, 168, 200, 36, opt, true, false);
			}
		} else {
			const bool isHousePick = (g_gameMode == GM_PICKHOUSE || g_playerHouseID == HOUSE_MERCENARY ||
			                          s_houseConfirmID < 3 || s_mentatButtons->drawParameterNormal.spriteID == 373);
			const char *optProceed = L10N("Proceed (A)", "Tęsti (A)");
			const char *optSecond  = isHousePick ? L10N("Back (B)", "Atgal (B)") : L10N("Repeat (B)", "Kartoti (B)");
			MenuButton(8, 168, 144, 36, optProceed, s_dialogFocus == 0, false);
			MenuButton(168, 168, 144, 36, optSecond, s_dialogFocus == 1, false);
		}
		return;
	}

	/* 0b. Mentat help subject list: every choice lives on the bottom screen. */
	if (g_3dsMentatHelpActive) {
		memset(g_bottomScreenBuffer, 0, BOTTOM_WIDTH * BOTTOM_HEIGHT);
		memset(g_bottomScreenUIMask, 2, sizeof(g_bottomScreenUIMask));
		const char *rows[11]; bool header[11];
		int selected = GUI_Mentat_3DS_GetRows(rows, header);
		for (int i = 0; i < 11; i++) {
			int y = 6 + i * 21;
			if (header[i]) {
				GUI_3DS_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, 14, y + 3, rows[i], 11);
			} else {
				bool isSel = (i == selected);
				MenuButton(20, y, 280, 19, rows[i], isSel, false);
			}
		}
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
		const char *titleStr = L10N("Select House", "Pasirinkite giminę");
		Cairo_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, BOTTOM_WIDTH / 2, 56, titleStr, 15, CAIRO_NO_SHADOW);

		static const char *hNames[3] = {"Atreides", "Ordos", "Harkonnen"};
		int crestW = 84;
		int crestH = 64;
		int dstY = 88;

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
		int totalH = max(0, count * rowStep - 3);
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
	 * - Striped divider (X: 150..169, Y: 40..239) from SCREEN_0
	 * - Bottom-Left (X: 0..149, Y: 40..239): Solid black, 128x128 radar centered, viewport box
	 * - Bottom-Right (X: 170..319, Y: 40..239): Tactical panel with portrait, DMG bar & actions
	 * ========================================================================= */

	uint8 *src = GFX_Screen_Get_ByIndex(SCREEN_0);

	/* 1. Header (Y: 0..39): Copy DOS screen header directly (includes live credits & ticker) */
	if (src != NULL) {
		memcpy(g_bottomScreenBuffer, src, BOTTOM_WIDTH * 40);
	}

	/* Bottom-left radar background. */
	for (int y = 40; y < BOTTOM_HEIGHT; y++) {
		memset(g_bottomScreenBuffer + y * BOTTOM_WIDTH, 0, BATTLE_TRIM_X);
	}
	DrawBattleTrim(src);

	/* 128x128 Radar centered at X: 13..140, Y: 76..203 */
	GUI_3DS_DrawBevel(g_bottomScreenBuffer, BOTTOM_WIDTH, 12, 75, 141, 204, 118, 110);

	{
		static uint8 s_radarCache[128 * 128];
		static uint32 s_radarCacheTick = 0xFFFFFFFF;
		static uint8 s_radarCacheHouse = 0xFF;
		bool radarDirty = true;
#ifdef __3DS__
		radarDirty = ((g_timerGame - s_radarCacheTick) >= 3 || s_radarCacheTick == 0xFFFFFFFF || g_viewport_forceRedraw || g_playerHouseID != s_radarCacheHouse);
#endif
		if (radarDirty) {
			s_radarCacheTick = g_timerGame;
			s_radarCacheHouse = g_playerHouseID;
			for (int my = 0; my < 64; my++) {
				uint8 *row0 = s_radarCache + (my * 2) * 128;
				uint8 *row1 = row0 + 128;
				for (int mx = 0; mx < 64; mx++) {
					uint16 packed = Tile_PackXY(mx, my);
					Tile *t = &g_map[packed];
					uint8 col = 0; /* Fog of war */

					if (t->isUnveiled || g_debugScenario) {
						if (t->hasUnit) {
							Unit *un = Unit_Get_ByPackedTile(packed);
							if (un != NULL) {
								if (un->o.type == UNIT_SANDWORM) col = 255;
								else {
									uint8 houseID = Unit_GetHouseID(un);
									col = houseID < HOUSE_MAX ? g_table_houseInfo[houseID].minimapColor : 0;
								}
							}
						}
						if (col == 0 && t->hasStructure) {
							Structure *st = Structure_Get_ByPackedTile(packed);
							if (st != NULL) {
								col = st->o.houseID < HOUSE_MAX ? g_table_houseInfo[st->o.houseID].minimapColor : 0;
							}
						}
						if (col == 0) {
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
						}
					}

					int rx = mx * 2;
					row0[rx] = col;
					row0[rx + 1] = col;
					row1[rx] = col;
					row1[rx + 1] = col;
				}
			}
		}
		for (int y = 0; y < 128; y++) {
			memcpy(g_bottomScreenBuffer + (76 + y) * BOTTOM_WIDTH + 13, s_radarCache + y * 128, 128);
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

	/* The gold lip separates the cream controls from the original striped trim. */
	uint8 *buf = g_bottomScreenBuffer;
	GUI_3DS_FillRect(buf, BOTTOM_WIDTH, 169, 40, 319, 40, 0xE2);
	GUI_3DS_FillRect(buf, BOTTOM_WIDTH, 169, 41, 319, 41, 0xE5);
	GUI_3DS_FillRect(buf, BOTTOM_WIDTH, 169, 41, 169, 222, 0xE2);
	GUI_3DS_FillRect(buf, BOTTOM_WIDTH, 170, 42, 170, 222, 0xE5);
	GUI_3DS_FillRect(buf, BOTTOM_WIDTH, 169, 222, 319, 222, 0xE5);
	GUI_3DS_FillRect(buf, BOTTOM_WIDTH, 169, 223, 319, 223, 0xE2);
	for (int y = 40; y < 224; y++) g_bottomScreenUIMask[y * BOTTOM_WIDTH + 170] = 0;
	memset(g_bottomScreenUIMask + 40 * BOTTOM_WIDTH + 170, 0, 150);
	memset(g_bottomScreenUIMask + 41 * BOTTOM_WIDTH + 170, 0, 150);
	memset(g_bottomScreenUIMask + 222 * BOTTOM_WIDTH + 170, 0, 150);
	memset(g_bottomScreenUIMask + 223 * BOTTOM_WIDTH + 170, 0, 150);
}

void GUI_3DS_HandleBottomTouchHold(int16 touchX, int16 touchY)
{
	if (GUI_3DS_IsMenuOrDialogActive() || GUI_3DS_CustomMenuActive() || g_3dsModalMessageActive) return;

	/* Continuous Radar Drag (X: 0..149, Y: 40..239) */
	if (touchX >= 0 && touchX < BATTLE_TRIM_X && touchY >= 40 && touchY < 240) {
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
		if (touchX >= 40 && touchX < 280 && touchY >= 102 && touchY < 138) {
			if(s_resultsButtons) {
				Widget *chosen=NULL;
				for(Widget *w=s_resultsButtons;w;w=w->next) if(w->index==101) chosen=w;
				s_dialogChosen=chosen ? chosen : s_resultsButtons;
			} else EmitKey(0x1C);
		} else {
			EmitKey(0x1C);
		}
		return;
	}
 /* 0. If in cutscene, mentat briefing, or modal message dialog: tap advances/skips */
	if ((GUI_3DS_IsCutsceneOrBriefing() && !g_3dsHousePickerActive && !s_mentatButtons && !g_3dsMentatHelpActive) || g_3dsModalMessageActive) {
		Input_EventHandler(0x1C); /* Enter */
		Input_EventHandler(0x1C | 0x80);
		return;
	}

	/* Mentat help subject list (bottom screen): full-height topic rows */
	if (g_3dsMentatHelpActive && s_mentatButtons == NULL) {
		if (touchY >= 6 && touchY < 6 + 11 * 21 && touchX >= 20 && touchX < 300) {
			const char *rows[11]; bool header[11];
			GUI_Mentat_3DS_GetRows(rows, header);
			int row = (touchY - 6) / 21;
			if (row >= 0 && row < 11 && !header[row]) g_3dsMentatTapRow = row;
		}
		return;
	}

	if (GUI_3DS_CustomMenuActive()) {
		if (s_mentatButtons) {
			if (s_mentatButtons->next == NULL && s_mentatButtons->drawParameterNormal.spriteID == 377) {
				if (s_mentatTextHasMore) {
					GUI_3DS_MentatChoose((touchX < BOTTOM_WIDTH / 2) ? 0x8001 : 0x8002);
				} else {
					GUI_3DS_MentatChoose(0x8002);
				}
				return;
			}
			s_dialogFocus = (touchX < BOTTOM_WIDTH / 2) ? 0 : 1;
			GUI_3DS_MenuInput(0, 0, true);
			return;
		} else if (CustomDialog()) {
			Widget *items[9]; int count = DialogItems(items);
			int ly0, lstep, lrows, lbtnX, lbtnW;
			DialogLayout(count, &ly0, &lstep, &lrows, &lbtnX, &lbtnW);
			int first = max(0, s_dialogFocus - (lrows - 1));
			int visibleCount = min(count - first, lrows);
			if (touchX >= lbtnX && touchX < lbtnX + lbtnW && touchY >= ly0 && touchY < ly0 + visibleCount * lstep) {
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
			/* 3 horizontal crest squares & house names at X: 12..308, Y: 68..194 */
			if (touchX >= 12 && touchX <= 308 && touchY >= 68 && touchY <= 194) {
				int idx = clamp((touchX - 14) / 100, 0, 2);
				s_houseFocus = idx;
				GUI_3DS_MenuInput(0, 0, true);
			}
		} else if (GUI_3DS_IsTitleMenu()) {
			int count = min((int)g_widgetProperties[21].height, 6);
			int rowStep = 28;
			int totalH = max(0, count * rowStep - 3);
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

	/* B. Radar Tap (X: 0..149, Y: 40..239) -> Jump Camera */
	if (touchX >= 0 && touchX < BATTLE_TRIM_X && touchY >= 40 && touchY < 240) {
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
					else if (g_selectionType == SELECTIONTYPE_TARGET) { Unit_3DS_CancelTarget(); Sound_StartSound(3); }
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
						} else if (st->o.type == STRUCTURE_PALACE && st->countDown == 0 && g_unitHouseMissile == NULL) {
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
