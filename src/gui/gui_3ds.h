/** @file src/gui/gui_3ds.h Dual-Screen GUI and Dashboard for Nintendo 3DS. */

#ifndef GUI_3DS_H
#define GUI_3DS_H

#include <stdio.h>
#include "types.h"
#include "unit.h"
#include "structure.h"

#define TOP_WIDTH 400
#define TOP_HEIGHT 240
#define BOTTOM_WIDTH 320
#define BOTTOM_HEIGHT 240

#define VIEWPORT_TILES_X 15
#define VIEWPORT_TILES_Y 10

extern uint8 *g_topScreenBuffer;
extern uint8 *g_bottomScreenBuffer;
extern uint8 g_topScreenUIMask[TOP_WIDTH * TOP_HEIGHT];
extern uint8 g_bottomScreenUIMask[BOTTOM_WIDTH * BOTTOM_HEIGHT];
extern int16 g_cursor3DS_X;
extern int16 g_cursor3DS_Y;
extern bool g_3dsModalMessageActive;

struct Widget;
bool GUI_3DS_Init(void);
bool GUI_3DS_InputText(char *text, uint16 maxLength, const char *hint, const char *confirm);
void GUI_3DS_BeginMessage(const char *text);
void GUI_3DS_EndMessage(void);
bool GUI_3DS_SetResultsActive(bool active);
void GUI_3DS_SetResultsButtons(struct Widget *buttons);
bool GUI_3DS_InputSaveName(char *description);
void GUI_3DS_Uninit(void);

void GUI_3DS_CenterCameraOnTile(uint16 tileX, uint16 tileY);
void GUI_3DS_ScrollCamera(int16 dx, int16 dy);
uint16 GUI_3DS_ScreenPosToTile(int16 screenX, int16 screenY);
bool GUI_3DS_MapPosToScreenPos(tile32 mapPos, int16 *outX, int16 *outY);

void GUI_3DS_RenderTopScreen(void);
void GUI_3DS_RenderBottomScreen(void);

void GUI_3DS_HandleBottomTouch(int16 touchX, int16 touchY);
void GUI_3DS_HandleBottomTouchHold(int16 touchX, int16 touchY);
bool GUI_3DS_IsMenuOrDialogActive(void);
bool GUI_3DS_IsCutsceneOrBriefing(void);
bool GUI_3DS_IsTitleMenu(void);
bool GUI_3DS_IsFactoryWindow(void);
bool GUI_3DS_IsDialog(void);

/* 2D Drawing primitives on 8-bit paletted buffers */
void GUI_3DS_PutPixel(uint8 *buf, int16 stride, int16 x, int16 y, uint8 color);
void GUI_3DS_FillRect(uint8 *buf, int16 stride, int16 x1, int16 y1, int16 x2, int16 y2, uint8 color);
void GUI_3DS_DrawRect(uint8 *buf, int16 stride, int16 x1, int16 y1, int16 x2, int16 y2, uint8 color);
void GUI_3DS_DrawBevel(uint8 *buf, int16 stride, int16 x1, int16 y1, int16 x2, int16 y2, uint8 colLight, uint8 colDark);
void GUI_3DS_DrawText(uint8 *buf, int16 stride, int16 x, int16 y, const char *text, uint8 color);

struct Widget;
void GUI_3DS_SetMentatButtons(struct Widget *buttons);
bool GUI_3DS_IsMentatButtonsActive(void);
bool GUI_3DS_IsMentatButton(const struct Widget *w);
void GUI_3DS_SetSubtitle(const char *text);
void GUI_3DS_SetHouseConfirm(uint8 houseID);
uint16 GUI_3DS_MentatEvent(uint16 key);
void GUI_3DS_MentatChoose(uint16 result);
struct WindowDesc;
void GUI_3DS_SetWindow(struct WindowDesc *window);
void GUI_3DS_SetSaveSlotInfo(int slot, bool used, uint8 houseID, uint16 mission, const char *dateStr);
void GUI_3DS_CacheHeraldCrests(const uint8 *herald320x200);
bool GUI_3DS_AreHeraldCrestsValid(void);
bool GUI_3DS_IsSaveSlotUsed(int slot);
bool GUI_3DS_CancelMenu(void);
extern bool g_3dsHousePickerActive;
extern bool g_3dsHouseBack; /* B pressed on the house selection screen. */
extern bool g_3dsMentatHelpActive; /* True while the Mentat help list is open during a battle. */
extern bool g_3dsMentatExit;
extern int g_3dsMentatTapRow;
extern int g_3dsMentatScroll;
extern int g_3dsMentatMove;
int GUI_Mentat_3DS_GetRows(const char *text[11], bool header[11]);       /* Set by the B button to leave the Mentat help list. */
void GUI_3DS_SetTitleActive(bool active);
void GUI_3DS_SetLaunchActive(bool active);
void GUI_3DS_SetPlanetBackground(const uint8 *pixels320x200);
const uint8 *GUI_3DS_GetPlanetBackground(void);
void GUI_3DS_DrawTitleTextTopCentered(int y, const char *str, uint8 mainColor, uint8 shadowColor);
uint16 GUI_3DS_TakeHouseChoice(void);
uint16 GUI_3DS_HouseEvent(uint16 key);
void GUI_3DS_SetTitleLabels(const char **labels);
bool GUI_3DS_CustomMenuActive(void);
void GUI_3DS_MenuInput(int dx, int dy, bool confirm);
uint16 GUI_3DS_TakeDialogEvent(struct Widget *head);
void GUI_3DS_FactoryBegin(void);
int  GUI_3DS_GetFactoryFocus(void);
void GUI_3DS_FactoryUpgrade(void);
void GUI_3DS_FactoryMinus(void);
void GUI_3DS_FactoryEvents(void);

#endif /* GUI_3DS_H */
