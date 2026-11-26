/** @file src/gui/keyboard_dsi.c On-screen touch/button QWERTY keyboard for Nintendo DSi. */

#include <nds.h>
#include <calico/nds/pm.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "gui_3ds.h"
#include "tactical_3ds.h"
#include "../input/input.h"
#include "../opendune.h"

extern void Video_DSI_PresentBottomOnly(void);

static const char *s_rowsLower[4] = {
	"1234567890",
	"qwertyuiop",
	"asdfghjkl-",
	"zxcvbnm.,_"
};

static const char *s_rowsUpper[4] = {
	"1234567890",
	"QWERTYUIOP",
	"ASDFGHJKL-",
	"ZXCVBNM.,_"
};

static void DrawKeyBox(int x, int y, int w, int h, const char *label, bool focused, bool accent)
{
	uint8 face = focused ? TC_YELLOW : (accent ? TC_LIGHT : TC_FACE);
	uint8 textCol = focused ? TC_BLACK : TC_WHITE;
	GUI_3DS_FillRect(g_bottomScreenBuffer, BOTTOM_WIDTH, x, y, x + w - 1, y + h - 1, face);
	GUI_3DS_DrawBevel(g_bottomScreenBuffer, BOTTOM_WIDTH, x, y, x + w - 1, y + h - 1, TC_WHITE, TC_SHADOW);
	for (int py = y; py < y + h; py++) {
		if (py >= 0 && py < BOTTOM_HEIGHT) {
			memset(g_bottomScreenUIMask + py * BOTTOM_WIDTH + x, TACTICAL_UI_MASK, w);
		}
	}
	int len = (int)strlen(label);
	int tx = x + (w - len * 8) / 2;
	int ty = y + (h - 8) / 2;
	GUI_3DS_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, tx, ty, label, textCol);
}

static void RenderKeyboard(const char *hint, const char *confirm, const char *text, bool shift, int selRow, int selCol)
{
	memset(g_bottomScreenBuffer, TC_NAVY, BOTTOM_WIDTH * BOTTOM_HEIGHT);
	memset(g_bottomScreenUIMask, TACTICAL_UI_MASK, BOTTOM_WIDTH * BOTTOM_HEIGHT);

	/* Hint title */
	GUI_3DS_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, 10, 8, hint ? hint : "Enter text", TC_YELLOW);

	/* Input box */
	GUI_3DS_FillRect(g_bottomScreenBuffer, BOTTOM_WIDTH, 10, 24, 309, 48, TC_BLACK);
	GUI_3DS_DrawRect(g_bottomScreenBuffer, BOTTOM_WIDTH, 10, 24, 309, 48, TC_WHITE);
	char disp[40];
	size_t tlen = strlen(text);
	const char *view = (tlen > 34) ? (text + tlen - 34) : text;
	snprintf(disp, sizeof(disp), "%s_", view);
	GUI_3DS_DrawText(g_bottomScreenBuffer, BOTTOM_WIDTH, 16, 32, disp, TC_WHITE);

	/* 4 rows of 10 keys (28x28 each, x=10..308, y=58..182) */
	const char **rows = shift ? s_rowsUpper : s_rowsLower;
	for (int r = 0; r < 4; r++) {
		for (int c = 0; c < 10; c++) {
			char chStr[2] = { rows[r][c], '\0' };
			int kx = 10 + c * 30;
			int ky = 58 + r * 32;
			DrawKeyBox(kx, ky, 28, 28, chStr, (selRow == r && selCol == c), false);
		}
	}

	/* Bottom action row (y = 190..226): SHIFT | SPACE | DEL | CANCEL | OK */
	DrawKeyBox(10,  190, 52, 34, shift ? "abc" : "ABC", (selRow == 4 && selCol == 0), true);
	DrawKeyBox(66,  190, 68, 34, "SPACE", (selRow == 4 && selCol == 1), false);
	DrawKeyBox(138, 190, 48, 34, "DEL",   (selRow == 4 && selCol == 2), true);
	DrawKeyBox(190, 190, 58, 34, "BACK",  (selRow == 4 && selCol == 3), true);
	DrawKeyBox(252, 190, 58, 34, confirm ? confirm : "OK", (selRow == 4 && selCol == 4), true);

	Video_DSI_PresentBottomOnly();
}

bool GUI_3DS_InputText(char *description, uint16 maxLength, const char *hint, const char *confirm)
{
	char text[256];
	if (maxLength == 0 || maxLength >= sizeof(text)) return false;

	snprintf(text, maxLength + 1, "%s", description ? description : "");
	bool shift = true;
	int selRow = 1, selCol = 0;
	bool accepted = false;
	bool done = false;

	RenderKeyboard(hint, confirm, text, shift, selRow, selCol);

	while (!done && pmMainLoop()) {
		swiWaitForVBlank();
		scanKeys();
		u32 kDown = keysDown();
		bool dirty = false;

		if (kDown & KEY_B) {
			done = true;
			accepted = false;
			break;
		}
		if (kDown & KEY_START) {
			done = true;
			accepted = true;
			break;
		}
		if (kDown & KEY_UP) {
			selRow = (selRow + 4) % 5;
			if (selRow == 4 && selCol > 4) selCol = 4;
			dirty = true;
		}
		if (kDown & KEY_DOWN) {
			selRow = (selRow + 1) % 5;
			if (selRow == 4 && selCol > 4) selCol = 4;
			dirty = true;
		}
		if (kDown & KEY_LEFT) {
			int cols = (selRow == 4) ? 5 : 10;
			selCol = (selCol + cols - 1) % cols;
			dirty = true;
		}
		if (kDown & KEY_RIGHT) {
			int cols = (selRow == 4) ? 5 : 10;
			selCol = (selCol + 1) % cols;
			dirty = true;
		}
		if (kDown & KEY_Y) {
			size_t len = strlen(text);
			if (len > 0) { text[len - 1] = '\0'; dirty = true; }
		}
		if (kDown & KEY_X) {
			shift = !shift;
			dirty = true;
		}

		bool activate = (kDown & KEY_A) != 0;
		if (kDown & KEY_TOUCH) {
			touchPosition tp;
			touchRead(&tp);
			int lx = ((int)tp.px * BOTTOM_WIDTH + 128) / 256;
			int ly = ((int)tp.py * BOTTOM_HEIGHT + 96) / 192;
			if (ly >= 58 && ly < 186 && lx >= 10 && lx < 310) {
				selRow = (ly - 58) / 32;
				selCol = (lx - 10) / 30;
				if (selRow > 3) selRow = 3;
				if (selCol > 9) selCol = 9;
				activate = true;
			} else if (ly >= 190 && ly <= 224) {
				selRow = 4;
				if (lx >= 10 && lx < 62) selCol = 0;
				else if (lx >= 66 && lx < 134) selCol = 1;
				else if (lx >= 138 && lx < 186) selCol = 2;
				else if (lx >= 190 && lx < 248) selCol = 3;
				else if (lx >= 252 && lx < 310) selCol = 4;
				else activate = false;
				if (lx >= 10 && lx < 310) activate = true;
			}
		}

		if (activate) {
			dirty = true;
			if (selRow < 4) {
				size_t len = strlen(text);
				if (len < maxLength) {
					const char **rows = shift ? s_rowsUpper : s_rowsLower;
					text[len] = rows[selRow][selCol];
					text[len + 1] = '\0';
					shift = false;
				}
			} else {
				if (selCol == 0) {
					shift = !shift;
				} else if (selCol == 1) {
					size_t len = strlen(text);
					if (len < maxLength) { text[len] = ' '; text[len + 1] = '\0'; }
				} else if (selCol == 2) {
					size_t len = strlen(text);
					if (len > 0) text[len - 1] = '\0';
				} else if (selCol == 3) {
					done = true;
					accepted = false;
				} else if (selCol == 4) {
					done = true;
					accepted = true;
				}
			}
		}

		if (dirty && !done) {
			RenderKeyboard(hint, confirm, text, shift, selRow, selCol);
		}
	}

	Input_History_Clear();
	scanKeys();
	g_viewport_forceRedraw = true;

	if (!accepted) return false;
	bool nonblank = false;
	for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
		if (!isspace(*p)) nonblank = true;
	}
	if (!nonblank) return false;
	memcpy(description, text, strlen(text) + 1);
	return true;
}

bool GUI_3DS_InputSaveName(char *description)
{
	return GUI_3DS_InputText(description, 50,
		GUI_3DS_IsLithuanian() ? "Isaugojimo aprasymas" : "Saved game description",
		GUI_3DS_IsLithuanian() ? "OK" : "Save");
}
