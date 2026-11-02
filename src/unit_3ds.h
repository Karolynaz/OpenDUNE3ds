/** @file src/unit_3ds.h Modern RTS unit controls for Nintendo 3DS (multi-selection, groups, smart actions). */

#ifndef UNIT_3DS_H
#define UNIT_3DS_H

#include <stdio.h>
#include "types.h"
#include "unit.h"
#include "structure.h"
#include "pool/unit.h"

#define MAX_SELECTED_UNITS UNIT_INDEX_MAX
#define MAX_CONTROL_GROUPS 4

extern Unit *g_selectedUnits[MAX_SELECTED_UNITS];
extern uint16 g_selectedUnitsCount;

/* Box selection state */
extern bool g_boxSelectActive;
extern int16 g_boxSelectStartX;
extern int16 g_boxSelectStartY;
extern int16 g_boxSelectCurrX;
extern int16 g_boxSelectCurrY;

/* Control group counts */
extern uint16 g_controlGroupCounts[MAX_CONTROL_GROUPS];

/* Deferred modal actions to prevent recursive timer locking */
extern Structure *g_3dsRequestedOpenFactory;
extern Structure *g_3dsRequestedStartPlacement;
extern bool g_3dsRequestedOptions;
extern bool g_3dsRequestedMentat;

void Unit_3DS_Init(void);
void Unit_3DS_ForgetUnit(Unit *u);
void Unit_3DS_DeselectUnits(void);
void Unit_3DS_ClearSelection(void);
void Unit_3DS_AddUnitToSelection(Unit *u);
void Unit_3DS_SelectSingle(Unit *u);
bool Unit_3DS_IsUnitSelected(const Unit *u);
void Unit_3DS_SelectStructure(Structure *s);
Structure *Unit_3DS_GetSelectedStructure(void);
void Unit_3DS_CycleStructureType(int category);
void Unit_3DS_ExecuteUnitAction(ActionType action);

void Unit_3DS_StartBoxSelection(int16 x, int16 y);
void Unit_3DS_UpdateBoxSelection(int16 x, int16 y);
void Unit_3DS_FinishBoxSelection(void);

void Unit_3DS_SelectAllVisibleUnits(void);
bool Unit_3DS_CenterSelection(void);

void Unit_3DS_SaveGroup(uint8 groupIndex);
void Unit_3DS_RecallGroup(uint8 groupIndex);

void Unit_3DS_SmartAction(uint16 packedTile);
void Unit_3DS_IssueCommand(ActionType action, uint16 packedTile);

bool Unit_3DS_StartPlacement(Structure *s);
bool Unit_3DS_PlaceActiveStructure(void);
void Unit_3DS_CancelPlacement(void);
bool Unit_3DS_OpenFactory(Structure *s);

#endif /* UNIT_3DS_H */
