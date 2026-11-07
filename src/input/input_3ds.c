/** @file src/input/input_3ds.c Nintendo 3DS physical controls and touchscreen input driver. */

#include <3ds.h>
#include <stdlib.h>
#include "types.h"
#include "../os/math.h"
#include "input_3ds.h"
#include "input.h"
#include "mouse.h"
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
static uint32 s_lastSelectTick;
static bool s_selectRecallArmed;

/* Fractional pixels preserve fine aiming without adding inertia. */
static int CursorStep(int value, int *fraction)
{
 int magnitude=abs(value);
 if(magnitude<=20) { *fraction=0; return 0; }
 magnitude=min(magnitude-20,135);
 int velocity=24 + magnitude*magnitude*(3*256-24)/(135*135);
 if((*fraction<0 && value>0) || (*fraction>0 && value<0)) *fraction=0;
 *fraction += value<0 ? -velocity : velocity;
 int step=*fraction/256; *fraction-=step*256; return step;
}

static bool s_touchActive = false;
static uint16 s_touchX, s_touchY;
static uint32 s_dpadScrollTimer = 0;
static uint32 s_edgeScrollTimer = 0;
static int s_menuRepeat = 0;
static int s_lastDpadDirection, s_lastEdgeDirection, s_lastMenuDirection;

void Input_3DS_Init(void)
{
	hidInit();
 s_cursorFractionX=s_cursorFractionY=0;
 s_selectRecallArmed=false;
	s_touchActive = false;
	s_menuRepeat = 0;
	s_dpadScrollTimer = 0;
	s_edgeScrollTimer = 0;
	s_lastDpadDirection=s_lastEdgeDirection=s_lastMenuDirection=0;
}

void Input_3DS_Uninit(void)
{
	hidExit();
}

void Input_3DS_Poll(void)
{
	hidScanInput();
	u32 kDown = hidKeysDown();
	u32 kHeld = hidKeysHeld();
	u32 kUp   = hidKeysUp();
	bool isMenuOrDialog = GUI_3DS_IsMenuOrDialogActive();
	bool customMenu = GUI_3DS_CustomMenuActive();

	/* 1. Circle Pad mouse emulation */
	circlePosition circle;
	hidCircleRead(&circle);
	if (!customMenu && (abs(circle.dx) > 15 || abs(circle.dy) > 15)) {
		int speedX = CursorStep(circle.dx, &s_cursorFractionX);
		int speedY = CursorStep(circle.dy, &s_cursorFractionY);
		g_cursor3DS_X += speedX;
		g_cursor3DS_Y -= speedY; /* dy is positive upwards in libctru */

		if (g_cursor3DS_X < 0) g_cursor3DS_X = 0;
		if (g_cursor3DS_X >= TOP_WIDTH) g_cursor3DS_X = TOP_WIDTH - 1;
		if (g_cursor3DS_Y < 0) g_cursor3DS_Y = 0;
		if (g_cursor3DS_Y >= TOP_HEIGHT) g_cursor3DS_Y = TOP_HEIGHT - 1;

		if (isMenuOrDialog) {
			int dosX = g_cursor3DS_X - 40;
			int dosY = g_cursor3DS_Y - 20;
			if (dosX < 0) dosX = 0;
			if (dosX >= SCREEN_WIDTH) dosX = SCREEN_WIDTH - 1;
			if (dosY < 0) dosY = 0;
			if (dosY >= SCREEN_HEIGHT) dosY = SCREEN_HEIGHT - 1;
			Mouse_EventHandler((uint16)dosX, (uint16)dosY, false, false);
		} else if (g_selectionType == SELECTIONTYPE_PLACE) {
			uint16 tile = GUI_3DS_ScreenPosToTile(g_cursor3DS_X, g_cursor3DS_Y);
			Map_SetSelection(tile);
			g_selectionRectanglePosition = tile;
			Map_SetSelectionObjectPosition(tile);
			g_selectionRectangleNeedRepaint = true;
			g_viewport_forceRedraw = true;
		}
	}

 else s_cursorFractionX=s_cursorFractionY=0;

	/* 2. D-Pad & Screen-Edge Camera Scrolling vs Menu Scrolling */
	/* The engine's own viewport widget re-derives the placement tile from the DOS mouse
	 * position every frame, so keep it locked to the 3DS cursor while placing/targeting. */
	if (!isMenuOrDialog && (g_selectionType == SELECTIONTYPE_PLACE || g_selectionType == SELECTIONTYPE_TARGET)) {
		int vx = (g_cursor3DS_X * 3) / 5;
		int vy = (g_cursor3DS_Y * 2) / 3 + 40;
		/* Stay clear of the engine's own edge-scroll widgets (we scroll the map ourselves). */
		if (vx < 16) vx = 16;
		if (vx > 223) vx = 223;
		if (vy < 56) vy = 56;
		if (vy > 183) vy = 183;
		Mouse_EventHandler((uint16)vx, (uint16)vy, false, false);
	}
	if (!isMenuOrDialog) {
		int edgeDX = 0, edgeDY = 0;
		if (!g_boxSelectActive) {
			if (g_cursor3DS_X <= 12) edgeDX = -1;
			else if (g_cursor3DS_X >= TOP_WIDTH - 13) edgeDX = 1;
			if (g_cursor3DS_Y <= 12) edgeDY = -1;
			else if (g_cursor3DS_Y >= TOP_HEIGHT - 13) edgeDY = 1;
		}

        int dpadDX=((kHeld&KEY_DRIGHT)!=0)-((kHeld&KEY_DLEFT)!=0);
        int dpadDY=((kHeld&KEY_DDOWN)!=0)-((kHeld&KEY_DUP)!=0);
        bool dpadActive=dpadDX || dpadDY;
        /* Physical direction always wins over a cursor parked at the opposite edge. */
        if(dpadActive) edgeDX=edgeDY=0;
        int edgeDirection=(edgeDX+1)*3+edgeDY+1;
        int dpadDirection=(dpadDX+1)*3+dpadDY+1;
        if(edgeDirection!=s_lastEdgeDirection) s_edgeScrollTimer=0;
        if(dpadDirection!=s_lastDpadDirection) s_dpadScrollTimer=0;
        s_lastEdgeDirection=edgeDirection; s_lastDpadDirection=dpadDirection;
        int scrollDX=0,scrollDY=0;
        if(edgeDX || edgeDY) {
            s_edgeScrollTimer++;
            if(s_edgeScrollTimer==8 || (s_edgeScrollTimer>8 && (s_edgeScrollTimer-8)%7==0)) {scrollDX=edgeDX;scrollDY=edgeDY;}
        } else s_edgeScrollTimer=0;
        if(dpadActive) {
            s_dpadScrollTimer++;
            if(s_dpadScrollTimer==1 || (s_dpadScrollTimer>=14 && (s_dpadScrollTimer-14)%5==0)) {scrollDX=dpadDX;scrollDY=dpadDY;}
        } else s_dpadScrollTimer=0;

		if (scrollDX != 0 || scrollDY != 0) {
			GUI_3DS_ScrollCamera(scrollDX, scrollDY);
			if (g_selectionType == SELECTIONTYPE_PLACE) {
				uint16 tile = GUI_3DS_ScreenPosToTile(g_cursor3DS_X, g_cursor3DS_Y);
				Map_SetSelection(tile);
				g_selectionRectanglePosition = tile;
				Map_SetSelectionObjectPosition(tile);
				g_selectionRectangleNeedRepaint = true;
				g_viewport_forceRedraw = true;
			}
		}
	} else if (customMenu) {
		int dx=0, dy=0;
		if ((kHeld & KEY_DUP) || circle.dy>40) dy=-1;
		if ((kHeld & KEY_DDOWN) || circle.dy < -40) dy=1;
		if ((kHeld & KEY_DLEFT) || circle.dx < -40) dx=-1;
		if ((kHeld & KEY_DRIGHT) || circle.dx>40) dx=1;
		int direction=(dx+1)*3+dy+1;
		if(direction!=s_lastMenuDirection) s_menuRepeat=0;
		s_lastMenuDirection=direction;
		if (!dx && !dy) s_menuRepeat=0;
		else {
			if (s_menuRepeat==0 || (s_menuRepeat>=18 && s_menuRepeat%6==0)) GUI_3DS_MenuInput(dx,dy,false);
			s_menuRepeat++;
			if (s_menuRepeat >= 24) s_menuRepeat = 18;
		}
	} else {
		/* In menus / factory window: D-Pad scrolls */
		if (kDown & KEY_DUP) {
			Input_EventHandler(0x48); /* Up arrow */
			Input_EventHandler(0x48 | 0x80);
		}
		if (kDown & KEY_DDOWN) {
			Input_EventHandler(0x50); /* Down arrow */
			Input_EventHandler(0x50 | 0x80);
		}
	}

	/* 3. Action Buttons & Modifiers */
	if (!isMenuOrDialog && !customMenu && (kHeld & KEY_L)) {
		/* L + Y: Cancel Build on active structure / Area Guard for selected units */
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
		/* L + X: Upgrade selected structure / Deploy selected unit */
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
		/* L + R: Return (harvesters), L + A: Ambush */
		if (g_unitSelected != NULL) {
			if (kDown & KEY_R) Unit_3DS_ExecuteUnitAction(ACTION_RETURN);
			if (kDown & KEY_A) Unit_3DS_ExecuteUnitAction(ACTION_AMBUSH);
		}
	} else {
		/* Standard Face Buttons */

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

		/* A Button: Smart Action / Drag Box Selection (Battle), Place (Building), or Confirm/Click (Menu) */
		if (kDown & KEY_A) {
			if (GUI_3DS_IsMentatButtonsActive()) {
				GUI_3DS_MenuInput(0, 0, true);
			} else if (customMenu) {
				GUI_3DS_MenuInput(0, 0, true);
			} else if (isMenuOrDialog) {
				Input_EventHandler(0x1C); /* Enter */
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

		/* B Button: Cancel / Deselect (Battle) or ESC / No / Repeat (Menu) */
		if (kDown & KEY_B) {
			if (g_boxSelectActive) {
				g_boxSelectActive = false;
			} else if (g_3dsHousePickerActive) {
				g_3dsHouseBack = true;
			} else if (GUI_3DS_IsMentatButtonsActive()) {
				GUI_3DS_MentatChoose(0x8002);
			} else if (GUI_3DS_CancelMenu()) {
				/* Handled by 3DS dialog/Hall of Fame cancel */
			} else if (isMenuOrDialog) {
				if (g_3dsMentatHelpActive) g_3dsMentatExit = true;
				GUI_3DS_MentatChoose(0x8002);
				Input_EventHandler(0x01); /* Escape */
				Input_EventHandler(0x01 | 0x80);
				if (g_factoryWindowResult == FACTORY_CONTINUE) {
					GUI_Production_ResumeGame_Click(NULL);
				}
			} else if (g_selectionType == SELECTIONTYPE_PLACE) {
				Unit_3DS_CancelPlacement();
			} else if (g_selectionType == SELECTIONTYPE_TARGET) {
				GUI_ChangeSelectionType(SELECTIONTYPE_UNIT);
			} else {
				Unit_3DS_ClearSelection();
				GUI_ChangeSelectionType(SELECTIONTYPE_STRUCTURE);
				Sound_StartSound(3);
			}
		}

		/* Y Button: Structure Build / Factory / Special OR Unit Guard */
		if (!isMenuOrDialog && !customMenu && (kDown & KEY_Y)) {
			Structure *s = Unit_3DS_GetSelectedStructure();
			if (s != NULL && s->o.houseID == g_playerHouseID) {
				if (s->o.type == STRUCTURE_CONSTRUCTION_YARD && s->o.linkedID != STRUCTURE_INVALID && s->countDown == 0) {
					g_3dsRequestedStartPlacement = s;
				} else if (g_table_structureInfo[s->o.type].o.flags.factory) {
					Unit_3DS_OpenFactory(s);
				} else if (s->o.type == STRUCTURE_PALACE && s->countDown == 0) {
					Structure_ActivateSpecial(s);
					Sound_StartSound(3);
				}
			} else if (g_unitSelected != NULL && g_selectedUnitsCount > 0) {
				Unit_3DS_ExecuteUnitAction(ACTION_GUARD);
				Sound_StartSound(3);
			}
		}

		/* X Button / ZL / ZR: Select all friendly units intersecting the visible battlefield */
		if (!isMenuOrDialog && !customMenu) {
			if ((kDown & KEY_X) || (kDown & KEY_ZL) || (kDown & KEY_ZR)) {
				if (g_selectionType == SELECTIONTYPE_PLACE) Unit_3DS_CancelPlacement();
				if ((kDown & KEY_X) && s_selectRecallArmed && (uint32)(g_timerGUI-s_lastSelectTick)<=24 && Unit_3DS_CenterSelection()) {
 s_selectRecallArmed=false;
 } else {
 Unit_3DS_SelectAllVisibleUnits();
 s_lastSelectTick=g_timerGUI;
 s_selectRecallArmed=(kDown & KEY_X) && g_selectedUnitsCount>0;
 }
			}
		}
	}

	/* Release must also work if a shoulder modifier was pressed mid-drag. */
	if (g_boxSelectActive && isMenuOrDialog) g_boxSelectActive = false;
	else if (g_boxSelectActive && (kUp & KEY_A)) Unit_3DS_FinishBoxSelection();

	if (customMenu && (kDown & KEY_X)) GUI_3DS_FactoryUpgrade();
	if (customMenu && (kDown & KEY_Y)) GUI_3DS_FactoryMinus();

	/* 4. Touchscreen / Stylus. Keep the last coordinates for release:
	 * hidTouchRead does not provide a valid position once the pen is lifted. */
	if (kDown & KEY_TOUCH) {
		touchPosition touch;
		hidTouchRead(&touch);
		if (!isMenuOrDialog || customMenu || GUI_3DS_IsCutsceneOrBriefing() || g_3dsModalMessageActive) {
			GUI_3DS_HandleBottomTouch(touch.px, touch.py);
		} else if (touch.px < SCREEN_WIDTH && touch.py >= 20 && touch.py < SCREEN_HEIGHT + 20) {
			s_touchX = touch.px;
			s_touchY = touch.py - 20;
			Mouse_EventHandler(s_touchX, s_touchY, false, false);
			Mouse_EventHandler(s_touchX, s_touchY, true, false);
			s_touchActive = true;
		}
	} else if (kHeld & KEY_TOUCH) {
		touchPosition touch;
		hidTouchRead(&touch);
		if (!isMenuOrDialog && !customMenu && !g_3dsModalMessageActive) {
			GUI_3DS_HandleBottomTouchHold(touch.px, touch.py);
		} else if (s_touchActive) {
			s_touchX = min(touch.px, SCREEN_WIDTH - 1);
			s_touchY = touch.py >= 20 ? min(touch.py - 20, SCREEN_HEIGHT - 1) : 0;
			Mouse_EventHandler(s_touchX, s_touchY, true, false);
		}
	} else if ((kUp & KEY_TOUCH) && s_touchActive) {
		Mouse_EventHandler(s_touchX, s_touchY, false, false);
		s_touchActive = false;
	}

	/* SELECT jumps to and selects the Construction Yard (or opens the currently selected factory if already selected). */
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

	/* 5. START Button: Options / Pause */
	if (kDown & KEY_START) {
		if (isMenuOrDialog) {
			Input_EventHandler(0x01); /* Escape */
			Input_EventHandler(0x01 | 0x80);
			if (g_factoryWindowResult == FACTORY_CONTINUE) {
				GUI_Production_ResumeGame_Click(NULL);
			}
		} else {
			g_3dsRequestedOptions = true;
		}
	}
}
