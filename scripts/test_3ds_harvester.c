/* Exercise the unmodified refinery/unload/position routines extracted by the runner.
 * World I/O is stubbed; harvesting actions come from the production unit table. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "script/script.h"
#include "unit.h"
#include "structure.h"
#include "house.h"
#include "map.h"
#include "scenario.h"
#include "opendune.h"
#include "pool/unit.h"
#include "pool/house.h"
#include "tile.h"
#include "tools.h"
#include "gui/gui.h"
#include "audio/sound.h"
#include "table/strings.h"
#include "../src/table/unitinfo.c"
#include "../src/table/actioninfo.c"
StructureInfo g_table_structureInfo[STRUCTURE_MAX];
Structure *g_scriptCurrentStructure;
HouseType g_playerHouseID;
Scenario g_scenario;
Tile g_map[4096];
static Unit harvester;
static House owner;
static uint16 freeTile;
static bool occupied;
static int actionCalls;
Unit *Unit_Get_ByIndex(uint16 index) {assert(index==7);return &harvester;}
House *House_Get_ByIndex(uint8 index) {(void)index;return &owner;}
bool House_AreAllied(uint8 a,uint8 b) {return a==b;}
uint8 Tools_Random_256(void) {return 2;}
void Structure_SetState(Structure *s,int16 state) {s->state=state;}
uint16 Structure_FindFreePosition(Structure *s,bool spice) {(void)s;assert(spice);return freeTile;}
uint16 Unit_FindClosestRefinery(Unit *u) {u->originEncoded=1;return 1;}
bool Unit_IsTileOccupied(Unit *u) {(void)u;return occupied;}
void Unit_HouseUnitCount_Add(Unit *u,uint8 h) {(void)u;(void)h;}
ScriptInfo *g_scriptUnit;
void Script_Reset(ScriptEngine *script,ScriptInfo *info) {(void)script;(void)info;actionCalls++;}
void Script_Load(ScriptEngine *script,uint8 type) {(void)script;(void)type;}
void Script_LoadAsSubroutine(ScriptEngine *script,uint8 type) {(void)script;(void)type;}
void Unit_UpdateMap(uint16 type,Unit *u) {(void)type;(void)u;}
void Unit_SetOrientation(Unit *u,int8 orientation,bool instant,uint16 level) {(void)instant;u->orientation[level].current=orientation;}
int8 Tile_GetDirection(tile32 a,tile32 b) {(void)a;(void)b;return 0;}
void Object_Script_Variable4_Clear(Object *o) {o->script.variables[4]=0;}
void Sound_Output_Feedback(uint16 index) {(void)index;}
uint16 GUI_DisplayHint(uint16 stringID,uint16 spriteID) {(void)stringID;(void)spriteID;return 0;}
#include "harvester_core.inc"
int main(void)
{
 for(int house=HOUSE_HARKONNEN;house<=HOUSE_ORDOS;house++) {
  Structure refinery={0};
  memset(&harvester,0,sizeof(harvester));memset(&owner,0,sizeof(owner));
  g_playerHouseID=house;g_scriptCurrentStructure=&refinery;
  refinery.o.houseID=house;refinery.o.type=STRUCTURE_REFINERY;refinery.o.linkedID=7;
  refinery.o.hitpoints=g_table_structureInfo[STRUCTURE_REFINERY].o.hitpoints=450;
  harvester.o.type=UNIT_HARVESTER;harvester.o.houseID=house;harvester.o.linkedID=0xFF;
  harvester.o.flags.s.isNotOnMap=true;harvester.o.flags.s.inTransport=true;
  harvester.amount=100;harvester.actionID=ACTION_RETURN;
  for(int tick=0;tick<100 && harvester.amount;tick++) assert(Script_Structure_RefineSpice(NULL)==1);
  assert(!harvester.amount && !harvester.o.flags.s.inTransport && owner.credits==700);
  /* A blocked exit retains the docked harvester and can retry safely. */
  freeTile=0;occupied=false;
  assert(!Script_Structure_Unknown0C5A(NULL) && refinery.o.linkedID==7);
  freeTile=Tile_PackXY(20,21);occupied=true;
  assert(!Script_Structure_Unknown0C5A(NULL) && refinery.o.linkedID==7 && harvester.o.flags.s.isNotOnMap);
  occupied=false;actionCalls=0;
  assert(Script_Structure_Unknown0C5A(NULL)==1);
  assert(refinery.o.linkedID==0xFF && !harvester.o.flags.s.isNotOnMap);
  assert(harvester.actionID==ACTION_HARVEST && harvester.o.script.variables[0]==ACTION_HARVEST && actionCalls==1);
  assert(harvester.targetMove==0 && harvester.targetAttack==0);
 }
 puts("3DS refinery unload, blocked-exit retry and automatic harvesting checks passed");
}
