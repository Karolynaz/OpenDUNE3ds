/** @file src/unit_3ds.c Implementation of modern RTS multi-selection, control groups, and smart actions for 3DS. */

#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "os/math.h"
#include "unit_3ds.h"
#include "audio/sound.h"
#include "gui/gui.h"
#include "gui/widget.h"
#include "gui/gui_3ds.h"
#include "house.h"
#include "map.h"
#include "opendune.h"
#include "pool/pool.h"
#include "pool/unit.h"
#include "pool/structure.h"
#include "pool/house.h"
#include "scenario.h"
#include "sprites.h"
#include "structure.h"
#include "tile.h"
#include "tools.h"
#include "unit.h"

Unit *g_selectedUnits[MAX_SELECTED_UNITS];
uint16 g_selectedUnitsCount = 0;

static Unit *s_controlGroups[MAX_CONTROL_GROUPS][MAX_SELECTED_UNITS];
uint16 g_controlGroupCounts[MAX_CONTROL_GROUPS] = {0, 0, 0, 0};

bool g_boxSelectActive = false;
int16 g_boxSelectStartX = 0;
int16 g_boxSelectStartY = 0;
int16 g_boxSelectCurrX = 0;
int16 g_boxSelectCurrY = 0;

Structure *g_3dsRequestedOpenFactory = NULL;
Structure *g_3dsRequestedStartPlacement = NULL;
bool g_3dsRequestedOptions = false;
bool g_3dsRequestedMentat = false;

void Unit_3DS_Init(void)
{
	for (uint16 i = 0; i < MAX_SELECTED_UNITS; i++) {
		g_selectedUnits[i] = NULL;
	}
	g_selectedUnitsCount = 0;
	g_unitSelected = NULL;
	memset(s_controlGroups, 0, sizeof(s_controlGroups));
	memset(g_controlGroupCounts, 0, sizeof(g_controlGroupCounts));
	g_boxSelectActive = false;
	g_3dsRequestedOpenFactory = NULL;
	g_3dsRequestedStartPlacement = NULL;
	g_3dsRequestedOptions = false;
	g_3dsRequestedMentat = false;
}

/* Remove pool references before a slot can be recycled for another unit. */
void Unit_3DS_ForgetUnit(Unit *u)
{
	for (uint16 group = 0; group < MAX_CONTROL_GROUPS; group++) {
		uint16 count = 0;
		for (uint16 i = 0; i < g_controlGroupCounts[group]; i++) {
			if (s_controlGroups[group][i] != u) s_controlGroups[group][count++] = s_controlGroups[group][i];
		}
		g_controlGroupCounts[group] = count;
	}
	uint16 count = 0;
	for (uint16 i = 0; i < g_selectedUnitsCount; i++) {
		if (g_selectedUnits[i] != u) g_selectedUnits[count++] = g_selectedUnits[i];
	}
	for (uint16 i = count; i < g_selectedUnitsCount; i++) g_selectedUnits[i] = NULL;
	g_selectedUnitsCount = count;
	if (g_unitSelected == u) {
		g_unitSelected = count ? g_selectedUnits[0] : NULL;
		if (!count && g_gameMode == GM_NORMAL && (g_selectionType == SELECTIONTYPE_UNIT || g_selectionType == SELECTIONTYPE_TARGET)) GUI_ChangeSelectionType(SELECTIONTYPE_STRUCTURE);
	}
	if (g_unitActive == u) { g_unitActive = NULL; g_activeAction = 0xFFFF; }
}

void Unit_3DS_DeselectUnits(void)
{
	for (uint16 i = 0; i < g_selectedUnitsCount; i++) {
		if (g_selectedUnits[i] != NULL && g_selectedUnits[i]->o.flags.s.allocated) {
			Unit_UpdateMap(2, g_selectedUnits[i]);
		}
		g_selectedUnits[i] = NULL;
	}
	g_selectedUnitsCount = 0;
	g_unitSelected = NULL;
}

void Unit_3DS_ClearSelection(void)
{
	Unit_3DS_DeselectUnits();
	if (g_gameMode == GM_NORMAL) {
		Map_SetSelection(0xFFFF);
		g_selectionPosition = 0xFFFF;
		g_selectionRectanglePosition = 0xFFFF;
		Map_SetSelectionObjectPosition(0xFFFF);
	}
}

Structure *Unit_3DS_GetSelectedStructure(void)
{
	if (g_unitSelected != NULL) return NULL;
	if (g_selectionPosition == 0xFFFF) return NULL;
	Structure *s = Structure_Get_ByPackedTile(g_selectionPosition);
	if (s != NULL && s->o.flags.s.allocated && !s->o.flags.s.isNotOnMap) return s;
	return NULL;
}

void Unit_3DS_SelectStructure(Structure *s)
{
	if (s == NULL || !s->o.flags.s.allocated) return;
	Unit_3DS_DeselectUnits();
	uint16 structTile = Tile_PackTile(s->o.position);
	Map_SetSelection(structTile);
	/* Ensure structure selection isn't hijacked by a unit standing on the top-left corner */
	Unit_3DS_DeselectUnits();
	g_selectionPosition = structTile;
	g_selectionRectanglePosition = structTile;
	Map_SetSelectionSize(g_table_structureInfo[s->o.type].layout);
	GUI_ChangeSelectionType(SELECTIONTYPE_STRUCTURE);
}

void Unit_3DS_CycleStructureType(int category)
{
	/* category:
	 * 0 = Construction Yard
	 * 1 = Infantry (Barracks / WOR)
	 * 2 = Light Factory
	 * 3 = Heavy / Hi-Tech / Repair
	 * 4 = Starport / Palace
	 * 5 = Any Vehicle Factory / Starport
	 */
	Structure *matches[32];
	int count = 0;
	PoolFindStruct find = {g_playerHouseID, 0xFFFF, 0xFFFF};
	while (count < 32) {
		Structure *s = Structure_Find(&find);
		if (s == NULL) break;
		if (!s->o.flags.s.allocated || s->o.flags.s.isNotOnMap) continue;
		bool match = false;
		switch (category) {
			case 0:
				match = (s->o.type == STRUCTURE_CONSTRUCTION_YARD);
				break;
			case 1:
				match = (s->o.type == STRUCTURE_BARRACKS || s->o.type == STRUCTURE_WOR_TROOPER);
				break;
			case 2:
				match = (s->o.type == STRUCTURE_LIGHT_VEHICLE);
				break;
			case 3:
				match = (s->o.type == STRUCTURE_HEAVY_VEHICLE || s->o.type == STRUCTURE_HIGH_TECH || s->o.type == STRUCTURE_REPAIR);
				break;
			case 4:
				match = (s->o.type == STRUCTURE_STARPORT || s->o.type == STRUCTURE_PALACE);
				break;
			case 5:
				match = (s->o.type == STRUCTURE_LIGHT_VEHICLE || s->o.type == STRUCTURE_HEAVY_VEHICLE ||
				         s->o.type == STRUCTURE_HIGH_TECH || s->o.type == STRUCTURE_STARPORT);
				break;
		}
		if (match) matches[count++] = s;
	}
	if (count == 0) {
		return;
	}

	Structure *current = Unit_3DS_GetSelectedStructure();
	int nextIdx = 0;
	bool wasCurrent = false;
	for (int i = 0; i < count; i++) {
		if (matches[i] == current) {
			wasCurrent = true;
			nextIdx = (i + 1) % count;
			break;
		}
	}

	Structure *chosen = matches[nextIdx];
	if (wasCurrent && count == 1) {
		/* Tapping the same building category when only 1 exists opens it directly! */
		if (chosen->o.type == STRUCTURE_CONSTRUCTION_YARD && chosen->o.linkedID != STRUCTURE_INVALID && chosen->countDown == 0) {
			g_3dsRequestedStartPlacement = chosen;
			return;
		}
		if (g_table_structureInfo[chosen->o.type].o.flags.factory) {
			Unit_3DS_OpenFactory(chosen);
			return;
		}
		if (chosen->o.type == STRUCTURE_PALACE && chosen->countDown == 0) {
			Structure_ActivateSpecial(chosen);
			return;
		}
	}

	Unit_3DS_SelectStructure(chosen);
	GUI_3DS_CenterCameraOnTile(Tile_GetPackedX(Tile_PackTile(chosen->o.position)), Tile_GetPackedY(Tile_PackTile(chosen->o.position)));
	Sound_StartSound(3);
}

void Unit_3DS_ExecuteUnitAction(ActionType action)
{
	if (g_selectedUnitsCount == 0 || g_unitSelected == NULL || action >= ACTION_MAX) return;

	if (action == ACTION_ATTACK || action == ACTION_MOVE || action == ACTION_HARVEST || action == ACTION_SABOTAGE) {
		g_unitActive = g_unitSelected;
		g_activeAction = action;
		GUI_ChangeSelectionType(SELECTIONTYPE_TARGET);
		Sound_StartSound(3);
		return;
	}

	for (uint16 i = 0; i < g_selectedUnitsCount; i++) {
		Unit *u = g_selectedUnits[i];
		if (u == NULL || !u->o.flags.s.allocated || u->o.flags.s.isNotOnMap || u->o.houseID != g_playerHouseID) continue;

		bool supported = false;
		for (uint16 slot = 0; slot < 4; slot++) {
			uint16 nativeAction = g_table_unitInfo[u->o.type].o.actionsPlayer[slot];
			if (nativeAction == action || ((action == ACTION_AREA_GUARD || action == ACTION_AMBUSH) && nativeAction == ACTION_GUARD)) supported = true;
		}
		if (!supported) continue;

		Object_Script_Variable4_Clear(&u->o);
		u->targetAttack = 0;
		u->targetMove   = 0;
		u->route[0]     = 0xFF;
		Unit_SetAction(u, action);
	}

	const UnitInfo *ui = &g_table_unitInfo[g_unitSelected->o.type];
	Sound_StartSound(ui->movementType == MOVEMENT_FOOT ? 18 : 19);

	if (action == ACTION_DESTRUCT || action == ACTION_DEPLOY) {
		Unit_3DS_ClearSelection();
	}
}

void Unit_3DS_AddUnitToSelection(Unit *u)
{
	if (u == NULL || !u->o.flags.s.allocated || u->o.flags.s.isNotOnMap) return;
	if (u->o.houseID != g_playerHouseID) return;

	for (uint16 i = 0; i < g_selectedUnitsCount; i++) {
		if (g_selectedUnits[i] == u) return; /* Already in list */
	}

	if (g_selectedUnitsCount < MAX_SELECTED_UNITS) {
		g_selectedUnits[g_selectedUnitsCount++] = u;
		Unit_UpdateMap(2, u);
		if (g_unitSelected == NULL) {
			g_unitSelected = u;
			Unit_DisplayStatusText(u);
			GUI_ChangeSelectionType(SELECTIONTYPE_UNIT);
		}
	}
}

void Unit_3DS_SelectSingle(Unit *u)
{
	Unit_3DS_ClearSelection();
	if (u != NULL) {
		Unit_3DS_AddUnitToSelection(u);
	} else {
		GUI_ChangeSelectionType(SELECTIONTYPE_STRUCTURE);
	}
}

bool Unit_3DS_IsUnitSelected(const Unit *u)
{
	if (u == NULL) return false;
	for (uint16 i = 0; i < g_selectedUnitsCount; i++) {
		if (g_selectedUnits[i] == u) return true;
	}
	return false;
}

void Unit_3DS_StartBoxSelection(int16 x, int16 y)
{
	g_boxSelectStartX = x;
	g_boxSelectStartY = y;
	g_boxSelectCurrX = x;
	g_boxSelectCurrY = y;
	g_boxSelectActive = true;
}

void Unit_3DS_UpdateBoxSelection(int16 x, int16 y)
{
	if (!g_boxSelectActive) return;
	g_boxSelectCurrX = x;
	g_boxSelectCurrY = y;
}

void Unit_3DS_FinishBoxSelection(void)
{
	if (!g_boxSelectActive) return;
	g_boxSelectActive = false;

	int16 minX = (g_boxSelectStartX < g_boxSelectCurrX) ? g_boxSelectStartX : g_boxSelectCurrX;
	int16 maxX = (g_boxSelectStartX > g_boxSelectCurrX) ? g_boxSelectStartX : g_boxSelectCurrX;
	int16 minY = (g_boxSelectStartY < g_boxSelectCurrY) ? g_boxSelectStartY : g_boxSelectCurrY;
	int16 maxY = (g_boxSelectStartY > g_boxSelectCurrY) ? g_boxSelectStartY : g_boxSelectCurrY;

	/* If tiny box, treat as single click on map */
	if ((maxX - minX) < 8 && (maxY - minY) < 8) {
		uint16 tile = GUI_3DS_ScreenPosToTile(g_boxSelectStartX, g_boxSelectStartY);
		Unit_3DS_SmartAction(tile);
		return;
	}

	Unit_3DS_ClearSelection();

	PoolFindStruct find;
	find.houseID = g_playerHouseID;
	find.index   = 0xFFFF;
	find.type    = 0xFFFF;

	while (true) {
		Unit *u = Unit_Find(&find);
		if (u == NULL) break;
		if (!u->o.flags.s.allocated || u->o.flags.s.isNotOnMap) continue;

		int16 sx, sy;
		if (GUI_3DS_MapPosToScreenPos(u->o.position, &sx, &sy)) {
			if (sx >= minX && sx <= maxX && sy >= minY && sy <= maxY) {
				Unit_3DS_AddUnitToSelection(u);
			}
		}
	}

	if (g_selectedUnitsCount > 0) {
		Sound_StartSound(18); /* "Reporting" */
		GUI_ChangeSelectionType(SELECTIONTYPE_UNIT);
	}
}

void Unit_3DS_SelectAllVisibleUnits(void)
{
	Unit_3DS_ClearSelection();

	PoolFindStruct find;
	find.houseID = g_playerHouseID;
	find.index   = 0xFFFF;
	find.type    = 0xFFFF;

	while (true) {
		Unit *u = Unit_Find(&find);
		if (u == NULL) break;
		if (!u->o.flags.s.allocated || u->o.flags.s.isNotOnMap) continue;

        if(u->o.houseID!=g_playerHouseID || u->o.type>=UNIT_MAX) continue;
        const UnitInfo *ui=&g_table_unitInfo[u->o.type];
        if(!ui->flags.isNormalUnit || ui->flags.isBullet) continue;
        if(!g_debugScenario && !g_map[Tile_PackTile(u->o.position)].isUnveiled) continue;

        /* The renderer centers sprites on their map positions. Include partial
         * sprites at every edge, but exclude the padded off-screen render area. */
        int x=(u->o.position.x>>4)-(Tile_GetPackedX(g_viewportPosition)*16);
        int y=(u->o.position.y>>4)-(Tile_GetPackedY(g_viewportPosition)*16);
        int radius=max(1,(ui->dimension+1)/2);
        if(x+radius<=0 || x-radius>=VIEWPORT_TILES_X*16 ||
           y+radius<=0 || y-radius>=VIEWPORT_TILES_Y*16) continue;

		Unit_3DS_AddUnitToSelection(u);
	}

	if (g_selectedUnitsCount > 0) {
		Sound_StartSound(18);
		GUI_ChangeSelectionType(SELECTIONTYPE_UNIT);
	}
}

void Unit_3DS_SaveGroup(uint8 groupIndex)
{
	if (groupIndex >= MAX_CONTROL_GROUPS) return;

	g_controlGroupCounts[groupIndex] = 0;
	for (uint16 i = 0; i < g_selectedUnitsCount; i++) {
		if (g_selectedUnits[i] != NULL && g_selectedUnits[i]->o.flags.s.allocated) {
			s_controlGroups[groupIndex][g_controlGroupCounts[groupIndex]++] = g_selectedUnits[i];
		}
	}

	if (g_controlGroupCounts[groupIndex] > 0) {
		Sound_StartSound(20); /* Confirmation beep / order sound */
	}
}

void Unit_3DS_RecallGroup(uint8 groupIndex)
{
	if (groupIndex >= MAX_CONTROL_GROUPS) return;
	if (g_controlGroupCounts[groupIndex] == 0) return;

	Unit_3DS_ClearSelection();

	int32 sumX = 0;
	int32 sumY = 0;
	uint16 validCount = 0;

	for (uint16 i = 0; i < g_controlGroupCounts[groupIndex]; i++) {
		Unit *u = s_controlGroups[groupIndex][i];
		if (u != NULL && u->o.flags.s.allocated && !u->o.flags.s.isNotOnMap && u->o.houseID == g_playerHouseID) {
			Unit_3DS_AddUnitToSelection(u);
			sumX += Tile_GetPackedX(Tile_PackTile(u->o.position));
			sumY += Tile_GetPackedY(Tile_PackTile(u->o.position));
			s_controlGroups[groupIndex][validCount++] = u;
		}
	}

	g_controlGroupCounts[groupIndex] = validCount;

	if (validCount > 0) {
		/* Center camera on squad centroid */
		uint16 avgX = (uint16)(sumX / validCount);
		uint16 avgY = (uint16)(sumY / validCount);
		GUI_3DS_CenterCameraOnTile(avgX, avgY);

		Sound_StartSound(18); /* "Reporting" */
		GUI_ChangeSelectionType(SELECTIONTYPE_UNIT);
	}
}

bool Unit_3DS_StartPlacement(Structure *s)
{
	if (s == NULL || s->o.type != STRUCTURE_CONSTRUCTION_YARD || !s->o.flags.s.allocated || s->o.houseID != g_playerHouseID || s->o.linkedID >= STRUCTURE_INDEX_MAX_HARD || s->countDown != 0) {
		return false;
	}

	Structure *ns = Structure_Get_ByIndex(s->o.linkedID);
	if (ns == NULL || !ns->o.flags.s.used || ns->o.type >= STRUCTURE_MAX) return false;

	Unit_3DS_ClearSelection();

	g_structureActive = ns;
	g_structureActiveType = s->objectType;
	g_structureActivePosition = Tile_PackTile(s->o.position);
	s->o.linkedID = STRUCTURE_INVALID;

	GUI_ChangeSelectionType(SELECTIONTYPE_PLACE);

	uint16 tile = GUI_3DS_ScreenPosToTile(g_cursor3DS_X, g_cursor3DS_Y);
	Map_SetSelection(tile);
	g_selectionRectanglePosition = tile;
	Map_SetSelectionObjectPosition(tile);
	g_selectionRectangleNeedRepaint = true;
	g_viewport_forceRedraw = true;

	Sound_StartSound(20);
	return true;
}

bool Unit_3DS_PlaceActiveStructure(void)
{
	if (g_structureActive == NULL || g_selectionType != SELECTIONTYPE_PLACE) return false;

	Structure *placed = g_structureActive;
	if (Structure_Place(placed, g_selectionPosition)) {
		Voice_Play(20);

		if (placed->o.type == STRUCTURE_PALACE) {
			House_Get_ByIndex(placed->o.houseID)->palacePosition = placed->o.position;
		}

		if (g_structureActiveType == STRUCTURE_REFINERY && g_validateStrictIfZero == 0) {
			Unit *u;

			g_validateStrictIfZero++;
			u = Unit_CreateWrapper(g_playerHouseID, UNIT_HARVESTER, Tools_Index_Encode(placed->o.index, IT_STRUCTURE));
			g_validateStrictIfZero--;

			if (u == NULL) {
				g_playerHouse->harvestersIncoming++;
			} else {
				u->originEncoded = Tools_Index_Encode(placed->o.index, IT_STRUCTURE);
			}
		}

		/* Initialize default buildable object & upgrade state for newly placed factories/barracks */
		placed->upgradeTimeLeft = Structure_IsUpgradable(placed) ? 100 : 0;
		if (g_table_structureInfo[placed->o.type].o.flags.factory && placed->o.type != STRUCTURE_CONSTRUCTION_YARD && placed->o.type != STRUCTURE_STARPORT) {
			Structure_BuildObject(placed, 0xFFFE);
		}

		Structure *yard = Structure_Get_ByPackedTile(g_structureActivePosition);
		if (yard != NULL && yard->objectType < STRUCTURE_MAX && (Structure_GetBuildable(yard) & (1u << yard->objectType)) == 0) Structure_BuildObject(yard, 0xFFFE);
		House_UpdateRadarState(g_playerHouse);
		g_selectionState = 0;
		g_structureActive = NULL;
		g_structureActiveType = 0xFFFF;
		GUI_ChangeSelectionType(SELECTIONTYPE_STRUCTURE);

		/* Select the newly placed factory/barracks/starport/palace immediately so the player can enter it, or re-select the Construction Yard */
		if (g_table_structureInfo[placed->o.type].o.flags.factory || placed->o.type == STRUCTURE_PALACE || placed->o.type == STRUCTURE_REPAIR) {
			Unit_3DS_SelectStructure(placed);
		} else if (yard != NULL) {
			Unit_3DS_SelectStructure(yard);
		}
		return true;
	}

	Sound_StartSound(24); /* Error buzzer */
	return false;
}

void Unit_3DS_CancelPlacement(void)
{
	if (g_selectionType != SELECTIONTYPE_PLACE) return;

	GUI_Widget_Cancel_Click(NULL);
}

bool Unit_3DS_OpenFactory(Structure *s)
{
	if (s == NULL || !s->o.flags.s.allocated || s->o.houseID != g_playerHouseID || s->o.type >= STRUCTURE_MAX || !g_table_structureInfo[s->o.type].o.flags.factory) return false;

	Sound_StartSound(20);
	g_3dsRequestedOpenFactory = s;
	return true;
}

bool Unit_3DS_BuildStructureObject(Structure *s)
{
	if (s == NULL || !s->o.flags.s.allocated || s->o.houseID != g_playerHouseID || s->o.type >= STRUCTURE_MAX || !g_table_structureInfo[s->o.type].o.flags.factory) return false;
	if (s->o.type == STRUCTURE_STARPORT) {
		return Unit_3DS_OpenFactory(s);
	}
	if (s->o.linkedID != STRUCTURE_INVALID) return false;

	uint32 buildable = Structure_GetBuildable(s);
	if (buildable == 0) {
		Sound_StartSound(31);
		return false;
	}

	uint16 targetType = s->objectType;
	if (s->o.type == STRUCTURE_CONSTRUCTION_YARD) {
		if (targetType >= STRUCTURE_MAX || (buildable & (1u << targetType)) == 0) {
			for (uint8 i = 0; i < STRUCTURE_MAX; i++) {
				if ((buildable & (1u << i)) != 0) { targetType = i; break; }
			}
		}
	} else {
		if (targetType >= UNIT_MAX || (buildable & (1u << targetType)) == 0) {
			for (uint8 i = 0; i < UNIT_MAX; i++) {
				if ((buildable & (1u << i)) != 0) { targetType = i; break; }
			}
		}
	}

	s->objectType = targetType;
	bool started = Structure_BuildObject(s, targetType);
	if (started) {
		Sound_StartSound(3);
	} else {
		Sound_StartSound(31);
	}
	return started;
}

void Unit_3DS_SmartAction(uint16 packedTile)
{
	Unit *targetUnit = Unit_Get_ByPackedTile(packedTile);
	Structure *targetStructure = Structure_Get_ByPackedTile(packedTile);
	bool hasUnitsSelected = (g_selectedUnitsCount > 0 && g_unitSelected != NULL);

	/* Scenario 1: No friendly units selected */
	if (!hasUnitsSelected) {
		if (g_selectedUnitsCount > 0) Unit_3DS_DeselectUnits();

		if (targetUnit != NULL && targetUnit->o.houseID == g_playerHouseID) {
			Unit_3DS_SelectSingle(targetUnit);
			Sound_StartSound(18);
			return;
		}
		if (targetStructure != NULL && targetStructure->o.houseID == g_playerHouseID) {
			bool alreadySelected = (Unit_3DS_GetSelectedStructure() == targetStructure);

			if (alreadySelected) {
				/* Double-click / second press on selected factory or yard: */
				if (targetStructure->o.type == STRUCTURE_CONSTRUCTION_YARD && targetStructure->o.linkedID != STRUCTURE_INVALID && targetStructure->countDown == 0 && Unit_3DS_StartPlacement(targetStructure)) {
					return;
				}
				if (g_table_structureInfo[targetStructure->o.type].o.flags.factory) {
					Unit_3DS_OpenFactory(targetStructure);
					return;
				}
				if (targetStructure->o.type == STRUCTURE_PALACE && targetStructure->countDown == 0) {
					Structure_ActivateSpecial(targetStructure);
					return;
				}
			}

			/* First click on structure: select it */
			Unit_3DS_SelectStructure(targetStructure);
			Sound_StartSound(20);
			return;
		}
		return;
	}

	/* Scenario 2: Units are selected -> Issue Smart Order */

	/* If clicking on a friendly unit on this exact tile, select that unit instead */
	if (targetUnit != NULL && targetUnit->o.houseID == g_playerHouseID) {
		Unit_3DS_SelectSingle(targetUnit);
		Sound_StartSound(18);
		return;
	}

	/* If clicking on a friendly non-depot/refinery structure, switch selection to it */
	if (targetStructure != NULL && targetStructure->o.houseID == g_playerHouseID) {
		if (targetStructure->o.type != STRUCTURE_REFINERY && targetStructure->o.type != STRUCTURE_REPAIR) {
			Unit_3DS_SelectStructure(targetStructure);
			Sound_StartSound(20);
			return;
		}
	}

	/* B. If clicking on an enemy unit */
	if (targetUnit != NULL && targetUnit->o.houseID != g_playerHouseID &&
	    (targetUnit->o.seenByHouses & (1 << g_playerHouseID))) {
		Unit_3DS_IssueCommand(ACTION_ATTACK, packedTile);
		return;
	}

	/* C. If clicking on an enemy building */
	if (targetStructure != NULL && targetStructure->o.houseID != g_playerHouseID &&
	    (targetStructure->o.seenByHouses & (1 << g_playerHouseID))) {
		Unit_3DS_IssueCommand(ACTION_ATTACK, packedTile);
		return;
	}

	/* D. If harvester is selected and clicked on spice */
	if (g_unitSelected != NULL && g_unitSelected->o.type == UNIT_HARVESTER) {
		uint16 landType = Map_GetLandscapeType(packedTile);
		if (landType == LST_SPICE || landType == LST_THICK_SPICE) {
			Unit_3DS_IssueCommand(ACTION_HARVEST, packedTile);
			return;
		}
		if (targetStructure != NULL && targetStructure->o.type == STRUCTURE_REFINERY &&
		    targetStructure->o.houseID == g_playerHouseID) {
			Unit_3DS_IssueCommand(ACTION_RETURN, packedTile);
			return;
		}
	}

	/* E. Friendly repair depot or refinery */
	if (targetStructure != NULL && (targetStructure->o.type == STRUCTURE_REPAIR || targetStructure->o.type == STRUCTURE_REFINERY) &&
	    targetStructure->o.houseID == g_playerHouseID) {
		if (g_selectedUnitsCount == 1 && (g_unitSelected->o.type == UNIT_INFANTRY || g_unitSelected->o.type == UNIT_TROOPERS || g_unitSelected->o.type == UNIT_SOLDIER || g_unitSelected->o.type == UNIT_TROOPER)) {
			/* Foot soldiers don't use repair depots/refineries: select the building */
			Unit_3DS_SelectStructure(targetStructure);
			Sound_StartSound(20);
			return;
		}
		Unit_3DS_IssueCommand(ACTION_MOVE, packedTile);
		return;
	}

	/* F. Default action on terrain: Move */
	Unit_3DS_IssueCommand(ACTION_MOVE, packedTile);
}

void Unit_3DS_IssueCommand(ActionType action, uint16 packedTile)
{
	if (g_unitHouseMissile != NULL) {
		Unit_LaunchHouseMissile(packedTile);
		return;
	}

	if (g_selectedUnitsCount == 0 || action >= ACTION_MAX) return;

	static const int8 formationOffsets[16][2] = {
		{ 0,  0}, { 1,  0}, {-1,  0}, { 0,  1},
		{ 0, -1}, { 1,  1}, {-1,  1}, { 1, -1},
		{-1, -1}, { 2,  0}, {-2,  0}, { 0,  2},
		{ 0, -2}, { 2,  1}, {-2,  1}, { 2, -1}
	};

	uint16 baseTileX = Tile_GetPackedX(packedTile);
	uint16 baseTileY = Tile_GetPackedY(packedTile);
	/* Building entry must keep the same target for every squad member. */
	Structure *destinationStructure = action == ACTION_MOVE ? Structure_Get_ByPackedTile(packedTile) : NULL;

	for (uint16 i = 0; i < g_selectedUnitsCount; i++) {
		Unit *u = g_selectedUnits[i];
		if (u == NULL || !u->o.flags.s.allocated || u->o.flags.s.isNotOnMap || u->o.houseID != g_playerHouseID) continue;

		bool supported = false;
		for (uint16 slot = 0; slot < 4; slot++) {
			if (g_table_unitInfo[u->o.type].o.actionsPlayer[slot] == action) supported = true;
		}
		if (!supported) continue;

		Object_Script_Variable4_Clear(&u->o);
		u->targetAttack = 0;
		u->targetMove   = 0;
		u->route[0]     = 0xFF;

		Unit_SetAction(u, action);

		if (action == ACTION_MOVE) {
			int8 ox = destinationStructure ? 0 : formationOffsets[i % 16][0];
			int8 oy = destinationStructure ? 0 : formationOffsets[i % 16][1];
			int16 tx = (int16)baseTileX + ox;
			int16 ty = (int16)baseTileY + oy;

			const MapInfo *bounds = &g_mapInfos[g_scenario.mapScale];
			tx = clamp(tx, bounds->minX, bounds->minX + bounds->sizeX - 1);
			ty = clamp(ty, bounds->minY, bounds->minY + bounds->sizeY - 1);

			uint16 destTile = Tile_PackXY((uint16)tx, (uint16)ty);
			Unit_SetDestination(u, Tools_Index_Encode(destTile, IT_TILE));
		} else if (action == ACTION_HARVEST || action == ACTION_RETURN || action == ACTION_SABOTAGE) {
			u->targetMove = Tools_Index_Encode(packedTile, IT_TILE);
		} else if (action == ACTION_ATTACK) {
			uint16 targetPos = Unit_FindTargetAround(packedTile);
			Unit_SetTarget(u, Tools_Index_Encode(targetPos, IT_TILE));
			Unit *target = Tools_Index_GetUnit(u->targetAttack);
			if (target != NULL) target->blinkCounter = 8;
		}
	}

	if (g_selectionType == SELECTIONTYPE_TARGET) {
		g_unitActive = NULL;
		g_activeAction = 0xFFFF;
		GUI_ChangeSelectionType(SELECTIONTYPE_UNIT);
	}

	if (action == ACTION_ATTACK) {
		Sound_StartSound(17); /* Attack order confirmation */
	} else {
		Sound_StartSound(20); /* Movement order confirmation */
	}
}

/* Keep the current squad when recalling it after moving the camera. */
bool Unit_3DS_CenterSelection(void)
{
 uint32 x=0,y=0,count=0;
 for(uint16 i=0;i<g_selectedUnitsCount;i++) {
  const Unit *u=g_selectedUnits[i];
  if(!u || !u->o.flags.s.allocated || u->o.flags.s.isNotOnMap || u->o.houseID!=g_playerHouseID) continue;
  x+=u->o.position.x; y+=u->o.position.y; count++;
 }
 if(!count) return false;
 GUI_3DS_CenterCameraOnTile((x/count)>>8,(y/count)>>8);
 return true;
}
