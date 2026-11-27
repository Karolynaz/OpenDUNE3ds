/** @file src/input/input_dsi.c Nintendo DSi physical controls (D-Pad cursor) and touchscreen input driver. */

#include <nds.h>
#include <calico/system/tick.h>
#include <stdlib.h>
#include "types.h"
#include "../os/math.h"
#include "input_3ds.h"
#include "input.h"
#include "mouse.h"
#include "../config.h"
#include "../gui/gui_3ds.h"
#include "../gui/gui.h"
#include "../gui/widget.h"
#include "../map.h"
#include "../tools.h"
#include "../timer.h"
#include "../house.h"
#include "../pool/pool.h"
#include "../pool/structure.h"
#include "../opendune.h"
#include "../unit_3ds.h"
#include "../audio/sound.h"

static int s_cursorFractionX, s_cursorFractionY;
static int s_cursorHoldTicks;
static int s_lastCursorDir;
static uint32 s_lastSelectTick;
static bool s_selectRecallArmed;

static u64 s_lastPollTick = 0;
static int s_pollScale256 = 256;

static bool s_touchActive = false;
static uint16 s_touchX, s_touchY;
static uint32 s_dpadScrollTimer = 0;
static int s_dpadScrollAccum = 0;
static uint32 s_edgeScrollTimer = 0;
static int s_edgeScrollAccum = 0;
static int s_menuRepeat = 0;
static int s_lastDpadDirection, s_lastEdgeDirection, s_lastMenuDirection;

/* Map physical DSi touchscreen (0..255, 0..191) to logical bottom UI (0..319, 0..239). */
static inline void TouchToLogical(u16 px, u16 py, int16 *outX, int16 *outY)
{
	int lx = ((int)px * BOTTOM_WIDTH + 128) / 256;
	int ly = ((int)py * BOTTOM_HEIGHT + 96) / 192;
	if (lx < 0) lx = 0;
	if (lx >= BOTTOM_WIDTH) lx = BOTTOM_WIDTH - 1;
	if (ly < 0) ly = 0;
	if (ly >= BOTTOM_HEIGHT) ly = BOTTOM_HEIGHT - 1;
	*outX = (int16)lx;
	*outY = (int16)ly;
}

/* Smooth D-Pad mouse cursor movement:
 * - Immediate 1px nudge on first press for precise unit targeting.
 * - Smooth acceleration ramp up to the selected g_gameConfig.cursorSpeed preset (0..4).
 * - Diagonal normalization (* 181 / 256) keeps diagonal speed identical to cardinal speed. */
static void DPadCursorStep(int dirX, int dirY, int *outDx, int *outDy)
{
	/* Fixed-point 24.8 pixels per 60Hz frame for presets 0..4 (scaled for 256x192 top screen). */
	static const int s_startVel[5] = { 288, 352, 448, 576, 704 };
	static const int s_maxVel[5]   = { 512, 704, 960, 1280, 1600 };

	if (dirX == 0 && dirY == 0) {
		s_cursorFractionX = 0;
		s_cursorFractionY = 0;
		s_cursorHoldTicks = 0;
		s_lastCursorDir = 0;
		*outDx = 0;
		*outDy = 0;
		return;
	}

	int dirCode = (dirX + 1) * 3 + (dirY + 1);
	bool freshPress = (s_cursorHoldTicks == 0);
	if (dirCode != s_lastCursorDir) {
		if (s_lastCursorDir == 0) freshPress = true;
		s_lastCursorDir = dirCode;
	}

	int speedIdx = (g_gameConfig.cursorSpeed <= 4) ? (int)g_gameConfig.cursorSpeed : 2;
	int startV = s_startVel[speedIdx];
	int maxV   = s_maxVel[speedIdx];

	/* Ramp from startV to maxV over ~14 frames (~230ms) */
	int ramp = min(s_cursorHoldTicks, 14);
	int vel = startV + ((maxV - startV) * ramp) / 14;
	vel = (vel * s_pollScale256) / 256;

	s_cursorHoldTicks++;

	if (freshPress) {
		/* Immediate responsive 1px step on tap */
		s_cursorFractionX = 0;
		s_cursorFractionY = 0;
		*outDx = dirX;
		*outDy = dirY;
		return;
	}

	int velX = dirX * vel;
	int velY = dirY * vel;
	if (dirX != 0 && dirY != 0) {
		velX = (velX * 181) / 256;
		velY = (velY * 181) / 256;
	}

	if ((s_cursorFractionX < 0 && velX >= 0) || (s_cursorFractionX > 0 && velX <= 0)) s_cursorFractionX = 0;
	if ((s_cursorFractionY < 0 && velY >= 0) || (s_cursorFractionY > 0 && velY <= 0)) s_cursorFractionY = 0;

	s_cursorFractionX += velX;
	s_cursorFractionY += velY;

	int stepX = s_cursorFractionX / 256;
	int stepY = s_cursorFractionY / 256;
	s_cursorFractionX -= stepX * 256;
	s_cursorFractionY -= stepY * 256;

	*outDx = stepX;
	*outDy = stepY;
}

void Input_3DS_Init(void)
{
	tickInit();
	s_cursorFractionX = s_cursorFractionY = 0;
	s_cursorHoldTicks = 0;
	s_lastCursorDir = 0;
	s_lastPollTick = 0;
	s_pollScale256 = 256;
	s_selectRecallArmed = false;
	s_touchActive = false;
	s_menuRepeat = 0;
	s_dpadScrollTimer = 0;
	s_dpadScrollAccum = 0;
	s_edgeScrollTimer = 0;
	s_edgeScrollAccum = 0;
	s_lastDpadDirection = s_lastEdgeDirection = s_lastMenuDirection = 0;
}

void Input_3DS_Uninit(void)
{
}

void Input_3DS_Poll(void)
{
	u64 nowTick = tickGetCount();
	if (s_lastPollTick != 0) {
		u64 delta = nowTick - s_lastPollTick;
		int scale = (int)((delta * 60ULL * 256ULL) / (u64)TICK_FREQ);
		scale = clamp(scale, 128, 512);
		s_pollScale256 = (s_pollScale256 * 3 + scale) / 4;
	} else {
		s_pollScale256 = 256;
	}
	s_lastPollTick = nowTick;

	scanKeys();
	u32 kDown = keysDown();
	u32 kHeld = keysHeld();
	u32 kUp   = keysUp();
	if ((kDown | kUp) != 0 || (kHeld & KEY_TOUCH) != 0) {
		extern void Video_DSI_MarkBottomDirty(void);
		Video_DSI_MarkBottomDirty();
	}
	bool isMenuOrDialog = GUI_3DS_IsMenuOrDialogActive();
	bool customMenu = GUI_3DS_CustomMenuActive();

	int dpadX = ((kHeld & KEY_RIGHT) != 0) - ((kHeld & KEY_LEFT) != 0);
	int dpadY = ((kHeld & KEY_DOWN) != 0)  - ((kHeld & KEY_UP) != 0);

	/* Strategic map: D-Pad moves top-screen cursor, A confirms region. */
	if (GUI_3DS_StrategicActive()) {
		if (!GUI_3DS_StrategicIsSelecting()) {
			if (kDown & (KEY_A | KEY_START | KEY_TOUCH)) {
				Input_EventHandler(0x01);
				Input_EventHandler(0x01 | 0x80);
			}
		} else {
			int dx = 0, dy = 0;
			DPadCursorStep(dpadX, dpadY, &dx, &dy);
			GUI_3DS_StrategicMoveCursor(dx, dy);
			if (kDown & KEY_A) GUI_3DS_StrategicConfirm();
		}
		return;
	}

	/* 1. D-Pad controls mouse cursor on top screen during battle (unless L is held to pan camera). */
	bool panWithL = (!isMenuOrDialog && !customMenu && (kHeld & KEY_L));
	if (!customMenu && !isMenuOrDialog && !panWithL && (dpadX != 0 || dpadY != 0)) {
		int stepX = 0, stepY = 0;
		DPadCursorStep(dpadX, dpadY, &stepX, &stepY);
		g_cursor3DS_X += stepX;
		g_cursor3DS_Y += stepY;

		if (g_cursor3DS_X < 0) g_cursor3DS_X = 0;
		if (g_cursor3DS_X >= TOP_WIDTH) g_cursor3DS_X = TOP_WIDTH - 1;
		if (g_cursor3DS_Y < 0) g_cursor3DS_Y = 0;
		if (g_cursor3DS_Y >= TOP_HEIGHT) g_cursor3DS_Y = TOP_HEIGHT - 1;

		if (g_selectionType == SELECTIONTYPE_PLACE) {
			uint16 tile = GUI_3DS_ScreenPosToTile(g_cursor3DS_X, g_cursor3DS_Y);
			Map_SetSelection(tile);
			g_selectionRectanglePosition = tile;
			Map_SetSelectionObjectPosition(tile);
			g_selectionRectangleNeedRepaint = true;
		}
	} else {
		s_cursorFractionX = s_cursorFractionY = 0;
		s_cursorHoldTicks = 0;
		s_lastCursorDir = 0;
	}

	/* Keep DOS viewport mouse synced while placing structures or targeting. */
	if (!isMenuOrDialog && (g_selectionType == SELECTIONTYPE_PLACE || g_selectionType == SELECTIONTYPE_TARGET)) {
		int vx = (g_cursor3DS_X * 240) / TOP_WIDTH;
		int vy = (g_cursor3DS_Y * 160) / TOP_HEIGHT + 40;
		if (vx < 16) vx = 16;
		if (vx > 223) vx = 223;
		if (vy < 56) vy = 56;
		if (vy > 183) vy = 183;
		Mouse_EventHandler((uint16)vx, (uint16)vy, false, false);
	}

	/* 2. Camera Scrolling (Screen-Edge when D-Pad pushes cursor to edge, OR L + D-Pad fast pan) vs Menu Navigation */
	if (!isMenuOrDialog) {
		int edgeDX = 0, edgeDY = 0;
		if (!g_boxSelectActive && !panWithL) {
			if (g_cursor3DS_X <= 8) edgeDX = -1;
			else if (g_cursor3DS_X >= TOP_WIDTH - 9) edgeDX = 1;
			if (g_cursor3DS_Y <= 8) edgeDY = -1;
			else if (g_cursor3DS_Y >= TOP_HEIGHT - 9) edgeDY = 1;
		}

		int panDX = panWithL ? dpadX : 0;
		int panDY = panWithL ? dpadY : 0;
		bool panActive = (panDX != 0 || panDY != 0);
		if (panActive) edgeDX = edgeDY = 0;

		int edgeDirection = (edgeDX + 1) * 3 + edgeDY + 1;
		int dpadDirection = (panDX + 1) * 3 + panDY + 1;
		if (edgeDirection != s_lastEdgeDirection) { s_edgeScrollTimer = 0; s_edgeScrollAccum = 0; }
		if (dpadDirection != s_lastDpadDirection) { s_dpadScrollTimer = 0; s_dpadScrollAccum = 0; }
		s_lastEdgeDirection = edgeDirection;
		s_lastDpadDirection = dpadDirection;

		/* Time-compensated scroll intervals (in 1/256ths of a 60Hz frame) for scrollSpeed 0..4:
		 * Slowest / Slow / Normal / Fast / Fastest. */
		static const int s_edgeStepInterval[5] = { 1536, 1024, 768, 512, 341 };
		static const int s_panStepInterval[5]  = { 1024,  614, 384, 307, 256 };
		int scrollIdx = (g_gameConfig.scrollSpeed <= 4) ? (int)g_gameConfig.scrollSpeed : 2;

		int scrollDX = 0, scrollDY = 0;
		if (edgeDX || edgeDY) {
			int stepInterval = s_edgeStepInterval[scrollIdx];
			if (s_edgeScrollTimer == 0) {
				s_edgeScrollTimer = 1;
				/* Brief ~50ms edge-touch guard, then trigger first tile smoothly */
				s_edgeScrollAccum = stepInterval - (3 * 256);
			} else {
				s_edgeScrollAccum += s_pollScale256;
				if (s_edgeScrollAccum >= stepInterval) {
					s_edgeScrollAccum -= stepInterval;
					if (s_edgeScrollAccum >= stepInterval) s_edgeScrollAccum = stepInterval - 1;
					scrollDX = edgeDX;
					scrollDY = edgeDY;
				}
			}
		} else {
			s_edgeScrollTimer = 0;
			s_edgeScrollAccum = 0;
		}

		if (panActive) {
			int dt256 = s_pollScale256;
			if (s_dpadScrollTimer == 0) {
				scrollDX = panDX;
				scrollDY = panDY;
				s_dpadScrollTimer = 1;
				s_dpadScrollAccum = -(3 * 256);
			} else {
				int stepInterval = s_panStepInterval[scrollIdx];
				int steps = 0;
				s_dpadScrollAccum += dt256;
				while (s_dpadScrollAccum >= stepInterval && steps < 2) {
					s_dpadScrollAccum -= stepInterval;
					steps++;
				}
				if (s_dpadScrollAccum >= stepInterval) s_dpadScrollAccum = stepInterval - 1;
				if (steps > 0) {
					scrollDX = panDX * steps;
					scrollDY = panDY * steps;
				}
			}
		} else {
			s_dpadScrollTimer = 0;
			s_dpadScrollAccum = 0;
		}

		if (scrollDX != 0 || scrollDY != 0) {
			GUI_3DS_ScrollCamera(scrollDX, scrollDY);
			if (g_selectionType == SELECTIONTYPE_PLACE) {
				uint16 tile = GUI_3DS_ScreenPosToTile(g_cursor3DS_X, g_cursor3DS_Y);
				Map_SetSelection(tile);
				g_selectionRectanglePosition = tile;
				Map_SetSelectionObjectPosition(tile);
				g_selectionRectangleNeedRepaint = true;
			}
		}
	} else if (customMenu) {
		int dx = dpadX, dy = dpadY;
		int direction = (dx + 1) * 3 + dy + 1;
		if (direction != s_lastMenuDirection) s_menuRepeat = 0;
		s_lastMenuDirection = direction;
		if (!dx && !dy) s_menuRepeat = 0;
		else {
			if (s_menuRepeat == 0 || (s_menuRepeat >= 18 && s_menuRepeat % 6 == 0)) GUI_3DS_MenuInput(dx, dy, false);
			s_menuRepeat++;
			if (s_menuRepeat >= 24) s_menuRepeat = 18;
		}
	} else {
		if (kDown & KEY_UP) {
			Input_EventHandler(0x48);
			Input_EventHandler(0x48 | 0x80);
		}
		if (kDown & KEY_DOWN) {
			Input_EventHandler(0x50);
			Input_EventHandler(0x50 | 0x80);
		}
	}

	/* 3. Action Buttons & Modifiers */
	if (!isMenuOrDialog && !customMenu && (kHeld & KEY_L)) {
		if (kDown & KEY_Y) {
			Structure *s = Unit_3DS_GetSelectedStructure();
			if (s != NULL && s->o.houseID == g_playerHouseID && s->o.linkedID != STRUCTURE_INVALID) {
				Structure_CancelBuild(s);
				s->o.flags.s.onHold = false;
				Sound_StartSound(3);
			} else if (g_unitSelected != NULL) {
				Unit_3DS_ExecuteUnitAction(ACTION_AREA_GUARD);
			}
		}
		if (kDown & KEY_X) {
			Structure *s = Unit_3DS_GetSelectedStructure();
			if (s != NULL && s->o.houseID == g_playerHouseID) {
				if (Structure_SetUpgradingState(s, -1, NULL)) {
					Sound_StartSound(3);
				}
			} else if (g_unitSelected != NULL) {
				Unit_3DS_ExecuteUnitAction(ACTION_DEPLOY);
			}
		}
		if (g_unitSelected != NULL) {
			if (kDown & KEY_R) Unit_3DS_ExecuteUnitAction(ACTION_RETURN);
			if (kDown & KEY_A) Unit_3DS_ExecuteUnitAction(ACTION_AMBUSH);
		}
	} else {
		/* R Button: Structure Repair OR Unit Retreat */
		if (!isMenuOrDialog && !customMenu && (kDown & KEY_R)) {
			Structure *s = Unit_3DS_GetSelectedStructure();
			if (s != NULL && s->o.houseID == g_playerHouseID) {
				if (Structure_SetRepairingState(s, -1, NULL)) {
					Sound_StartSound(3);
				}
			} else if (g_unitSelected != NULL) {
				Unit_3DS_ExecuteUnitAction(ACTION_RETREAT);
			}
		}

		/* A Button: Smart Action / Drag Box Selection / Place / Confirm */
		if (kDown & KEY_A) {
			if (GUI_3DS_IsMentatButtonsActive()) {
				GUI_3DS_MenuInput(0, 0, true);
			} else if (customMenu) {
				GUI_3DS_MenuInput(0, 0, true);
			} else if (isMenuOrDialog) {
				Input_EventHandler(0x1C);
				Input_EventHandler(0x1C | 0x80);
			} else if (g_selectionType == SELECTIONTYPE_PLACE) {
				Unit_3DS_PlaceActiveStructure();
			} else if (g_selectionType == SELECTIONTYPE_TARGET) {
				Unit_3DS_IssueCommand(g_activeAction, GUI_3DS_ScreenPosToTile(g_cursor3DS_X, g_cursor3DS_Y));
			} else {
				Unit_3DS_StartBoxSelection(g_cursor3DS_X, g_cursor3DS_Y);
			}
		} else if (kHeld & KEY_A) {
			if (!isMenuOrDialog && !customMenu && g_boxSelectActive) {
				Unit_3DS_UpdateBoxSelection(g_cursor3DS_X, g_cursor3DS_Y);
			}
		}

		/* B Button: Cancel / Deselect / Back */
		if (kDown & KEY_B) {
			if (g_boxSelectActive) {
				g_boxSelectActive = false;
			} else if (g_3dsHousePickerActive) {
				g_3dsHouseBack = true;
			} else if (GUI_3DS_IsMentatButtonsActive()) {
				GUI_3DS_MentatChoose(0x8002);
			} else if (GUI_3DS_CancelMenu()) {
				/* Handled by dialog cancel */
			} else if (g_3dsModalMessageActive) {
				Input_EventHandler(0x01);
				Input_EventHandler(0x01 | 0x80);
			} else if (isMenuOrDialog) {
				if (g_3dsMentatHelpActive) g_3dsMentatExit = true;
				Input_EventHandler(0x01);
				Input_EventHandler(0x01 | 0x80);
				if (g_factoryWindowResult == FACTORY_CONTINUE) {
					GUI_Production_ResumeGame_Click(NULL);
				}
			} else if (g_selectionType == SELECTIONTYPE_PLACE) {
				Unit_3DS_CancelPlacement();
			} else if (g_selectionType == SELECTIONTYPE_TARGET) {
				Unit_3DS_CancelTarget();
			} else {
				Unit_3DS_ClearSelection();
				GUI_ChangeSelectionType(SELECTIONTYPE_STRUCTURE);
				Sound_StartSound(3);
			}
		}

		/* Y Button: Build / Factory / Palace Special / Unit Guard */
		if (!isMenuOrDialog && !customMenu && (kDown & KEY_Y)) {
			Structure *s = Unit_3DS_GetSelectedStructure();
			if (s != NULL && s->o.houseID == g_playerHouseID) {
				if (s->o.type == STRUCTURE_CONSTRUCTION_YARD && s->o.linkedID != STRUCTURE_INVALID && s->countDown == 0) {
					g_3dsRequestedStartPlacement = s;
				} else if (s->o.type == STRUCTURE_STARPORT) {
					Unit_3DS_OpenFactory(s);
				} else if (g_table_structureInfo[s->o.type].o.flags.factory) {
					if (s->o.linkedID == STRUCTURE_INVALID) {
						Unit_3DS_BuildStructureObject(s);
					}
				} else if (s->o.type == STRUCTURE_PALACE && s->countDown == 0 && g_unitHouseMissile == NULL) {
					Structure_ActivateSpecial(s);
					Sound_StartSound(3);
				}
			} else if (g_unitSelected != NULL && g_selectedUnitsCount > 0) {
				Unit_3DS_ExecuteUnitAction(ACTION_GUARD);
				Sound_StartSound(3);
			}
		}

		/* X Button: Select all visible friendly units / double-tap X to center camera on squad */
		if (!isMenuOrDialog && !customMenu && (kDown & KEY_X)) {
			if (g_selectionType == SELECTIONTYPE_PLACE) Unit_3DS_CancelPlacement();
			if (s_selectRecallArmed && (uint32)(g_timerGUI - s_lastSelectTick) <= 24 && Unit_3DS_CenterSelection()) {
				s_selectRecallArmed = false;
			} else {
				Unit_3DS_SelectAllVisibleUnits();
				s_lastSelectTick = g_timerGUI;
				s_selectRecallArmed = g_selectedUnitsCount > 0;
			}
		}
	}

	if (g_boxSelectActive && isMenuOrDialog) g_boxSelectActive = false;
	else if (g_boxSelectActive && (kUp & KEY_A)) Unit_3DS_FinishBoxSelection();

	if (customMenu && (kDown & KEY_X)) GUI_3DS_FactoryUpgrade();
	if (customMenu && (kDown & KEY_Y)) GUI_3DS_FactoryMinus();

	/* 4. Touchscreen / Stylus mapped from 256x192 physical to 320x240 logical UI */
	if (kDown & KEY_TOUCH) {
		touchPosition touch;
		touchRead(&touch);
		int16 lx, ly;
		TouchToLogical(touch.px, touch.py, &lx, &ly);
		if (!isMenuOrDialog || customMenu || GUI_3DS_IsCutsceneOrBriefing() || g_3dsModalMessageActive) {
			GUI_3DS_HandleBottomTouch(lx, ly);
		} else if (lx < SCREEN_WIDTH && ly >= 20 && ly < SCREEN_HEIGHT + 20) {
			s_touchX = (uint16)lx;
			s_touchY = (uint16)(ly - 20);
			Mouse_EventHandler(s_touchX, s_touchY, false, false);
			Mouse_EventHandler(s_touchX, s_touchY, true, false);
			s_touchActive = true;
		}
	} else if (kHeld & KEY_TOUCH) {
		touchPosition touch;
		touchRead(&touch);
		int16 lx, ly;
		TouchToLogical(touch.px, touch.py, &lx, &ly);
		if (!isMenuOrDialog && !customMenu && !g_3dsModalMessageActive) {
			GUI_3DS_HandleBottomTouchHold(lx, ly);
		} else if (s_touchActive) {
			s_touchX = (uint16)min((int)lx, SCREEN_WIDTH - 1);
			s_touchY = (uint16)(ly >= 20 ? min((int)(ly - 20), SCREEN_HEIGHT - 1) : 0);
			Mouse_EventHandler(s_touchX, s_touchY, true, false);
		}
	} else if ((kUp & KEY_TOUCH) && s_touchActive) {
		Mouse_EventHandler(s_touchX, s_touchY, false, false);
		s_touchActive = false;
	}

	/* SELECT: Jump to Construction Yard (2nd tap opens Construction Menu) */
	if (!isMenuOrDialog && (kDown & KEY_SELECT)) {
		Structure *sel = Unit_3DS_GetSelectedStructure();
		if (sel != NULL && sel->o.houseID == g_playerHouseID && g_table_structureInfo[sel->o.type].o.flags.factory) {
			if (sel->o.type == STRUCTURE_CONSTRUCTION_YARD && sel->o.linkedID != STRUCTURE_INVALID && sel->countDown == 0) {
				g_3dsRequestedStartPlacement = sel;
			} else {
				Unit_3DS_OpenFactory(sel);
			}
		} else {
			if (g_selectionType == SELECTIONTYPE_PLACE) Unit_3DS_CancelPlacement();
			Unit_3DS_CycleStructureType(0);
		}
	}

	/* START: Options / Pause */
	if (kDown & KEY_START) {
		if (g_3dsModalMessageActive) {
			Input_EventHandler(0x01);
			Input_EventHandler(0x01 | 0x80);
		} else if (isMenuOrDialog) {
			Input_EventHandler(0x01);
			Input_EventHandler(0x01 | 0x80);
			if (g_factoryWindowResult == FACTORY_CONTINUE) {
				GUI_Production_ResumeGame_Click(NULL);
			}
		} else {
			g_3dsRequestedOptions = true;
		}
	}
}
