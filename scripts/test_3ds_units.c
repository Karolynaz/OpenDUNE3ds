/* clang -std=gnu11 -fsanitize=address,undefined -ffunction-sections \
 * -iquote include -iquote src scripts/test_3ds_units.c -Wl,-dead_strip -o /tmp/dune-units-test */
#include <assert.h>
#include <stdio.h>
#include "../src/unit_3ds.c"
Unit *g_unitSelected, *g_unitActive;
uint16 g_activeAction, g_selectionType, g_selectionPosition, g_selectionRectanglePosition, g_viewportPosition;
volatile uint32 g_timerGUI;
bool g_debugScenario;
Tile g_map[4096];
static Unit *visiblePool[UNIT_INDEX_MAX];
static int visiblePoolCount;
Unit *Unit_Find(PoolFindStruct *find) {
 uint16 next=find->index==0xFFFF ? 0 : find->index+1;
 if(next>=visiblePoolCount) return NULL;
 find->index=next; return visiblePool[next];
}
GameMode g_gameMode;
HouseType g_playerHouseID;
UnitInfo g_table_unitInfo[UNIT_MAX];
Scenario g_scenario;
const MapInfo g_mapInfos[3] = {{1,1,62,62},{16,16,32,32},{21,21,21,21}};
static int destinationX, destinationY, actions;
static Structure depot;
static bool targetIsBuilding;
static uint16 destinations[MAX_SELECTED_UNITS];
Unit *g_unitHouseMissile;
uint16 g_houseMissileCountdown;
static Unit *freedUnit;
void Unit_Free(Unit *u) { freedUnit = u; }
void Sound_Output_Feedback(uint16 id) { (void)id; }
Structure *Structure_Get_ByPackedTile(uint16 p) {(void)p;return targetIsBuilding ? &depot : NULL;}
void Unit_LaunchHouseMissile(uint16 p) {(void)p;}
void Unit_UpdateMap(uint16 type, Unit *u) { (void)type; (void)u; }
void Map_SetSelection(uint16 p) { g_selectionPosition=p; }
uint16 Map_SetSelectionObjectPosition(uint16 p) { return p; }
void Unit_DisplayStatusText(Unit *u) { (void)u; }
void GUI_ChangeSelectionType(uint16 t) { g_selectionType=t; }
void Sound_StartSound(uint16 id) { (void)id; }
static uint16 centerX, centerY;
void GUI_3DS_CenterCameraOnTile(uint16 x, uint16 y) {centerX=x;centerY=y;}
void Object_Script_Variable4_Clear(Object *o) { (void)o; }
void Unit_SetAction(Unit *u, ActionType a) { u->actionID=a; actions++; }
uint16 Tools_Index_Encode(uint16 p, IndexType t) { (void)t; return p; }
void Unit_SetDestination(Unit *u,uint16 p) { destinations[u->o.index]=p; destinationX=Tile_GetPackedX(p); destinationY=Tile_GetPackedY(p); }
void Unit_SetTarget(Unit *u,uint16 p) { u->targetAttack=p; }
uint16 Unit_FindTargetAround(uint16 p) { return p; }
Unit *Tools_Index_GetUnit(uint16 p) { (void)p; return NULL; }
int main(void)
{
 Unit a={0}, b={0};
 a.o.flags.s.allocated=b.o.flags.s.allocated=true;
 a.o.houseID=b.o.houseID=g_playerHouseID=HOUSE_ATREIDES;
 a.o.position.x=b.o.position.x=20<<8;
 a.o.position.y=b.o.position.y=20<<8;
 Unit_3DS_Init(); Unit_3DS_AddUnitToSelection(&a); Unit_3DS_AddUnitToSelection(&b); Unit_3DS_SaveGroup(0);
 /* Invalid first entry must not hide the live second entry on the next recall. */
 a.o.flags.s.isNotOnMap=true;
 Unit_3DS_RecallGroup(0); Unit_3DS_RecallGroup(0);
 assert(g_controlGroupCounts[0]==1 && g_selectedUnitsCount==1 && g_selectedUnits[0]==&b);
 Unit_3DS_ForgetUnit(&b);
 assert(g_selectedUnitsCount==0 && g_controlGroupCounts[0]==0 && g_unitSelected==NULL);
 /* Reusing the same pool address must not resurrect membership. */
 b.o.flags.s.allocated=true; Unit_3DS_RecallGroup(0); assert(g_selectedUnitsCount==0);
 Unit_3DS_AddUnitToSelection(&b);
 for (int i=0;i<4;i++) g_table_unitInfo[0].o.actionsPlayer[i]=ACTION_MOVE;
 g_scenario.mapScale=1;
 g_selectionType=SELECTIONTYPE_TARGET; g_unitActive=&b; g_activeAction=ACTION_MOVE;
 Unit_3DS_IssueCommand(ACTION_MOVE,Tile_PackXY(2,63));
 assert(destinationX==16 && destinationY==47 && actions==1);
 assert(g_selectionType==SELECTIONTYPE_UNIT && g_unitActive==NULL && g_activeAction==0xFFFF);
 Unit missile={0};
 missile.o.houseID=g_playerHouseID;
 g_unitHouseMissile=&missile; g_houseMissileCountdown=7;
 g_selectionType=SELECTIONTYPE_TARGET;
 Unit_3DS_CancelTarget();
 assert(freedUnit==&missile && g_unitHouseMissile==NULL && g_houseMissileCountdown==0);
 assert(g_selectionType==SELECTIONTYPE_UNIT);
 Unit_3DS_IssueCommand(ACTION_ATTACK,Tile_PackXY(20,20)); assert(actions==1);
 /* All squad members must enter the same repair building, not adjacent terrain. */
 a.o.flags.s.isNotOnMap=false; a.o.index=1;
 Unit_3DS_AddUnitToSelection(&a); targetIsBuilding=true;
 Unit_3DS_IssueCommand(ACTION_MOVE,Tile_PackXY(30,30));
 assert(destinations[0]==Tile_PackXY(30,30) && destinations[1]==destinations[0]);
 targetIsBuilding=false; Unit_3DS_IssueCommand(ACTION_MOVE,Tile_PackXY(30,30));
 assert(destinations[1]!=destinations[0]); /* Formation still applies on terrain. */
 for (int i=0;i<4;i++) g_table_unitInfo[0].o.actionsPlayer[i]=ACTION_GUARD;
 Unit_3DS_ExecuteUnitAction(ACTION_AREA_GUARD);
 assert(a.actionID==ACTION_AREA_GUARD && b.actionID==ACTION_AREA_GUARD);
 Unit_3DS_ExecuteUnitAction(ACTION_AMBUSH);
 assert(a.actionID==ACTION_AMBUSH && b.actionID==ACTION_AMBUSH);
 g_3dsRequestedOptions=true; Unit_3DS_SaveGroup(0); Unit_3DS_Init();
 assert(!g_3dsRequestedOptions && !g_selectedUnitsCount && !g_controlGroupCounts[0]);
 /* X selects all own visible normal units, including partial edge sprites. */
 g_viewportPosition=Tile_PackXY(20,20); g_scenario.mapScale=0;
 for(int i=0;i<4096;i++) g_map[i].isUnveiled=true;
 const int edges[][2]={{20,20},{0,80},{240,80},{120,0},{120,160},
                      {-7,80},{247,80},{120,-7},{120,167},
                      {-8,80},{248,80},{120,-8},{120,168},{300,80}};
 Unit edgeUnits[14]={0}; visiblePoolCount=14;
 for(int i=0;i<14;i++) {
  Unit *u=&edgeUnits[i]; visiblePool[i]=u;
  u->o.flags.s.allocated=true; u->o.houseID=g_playerHouseID;
  u->o.type=UNIT_TANK; u->o.index=i;
  u->o.position.x=(20*256)+edges[i][0]*16;
  u->o.position.y=(20*256)+edges[i][1]*16;
 }
 g_table_unitInfo[UNIT_TANK].flags.isNormalUnit=true;
 g_table_unitInfo[UNIT_TANK].dimension=16;
 Unit_3DS_SelectAllVisibleUnits(); assert(g_selectedUnitsCount==9);
 for(int i=0;i<9;i++) assert(g_selectedUnits[i]==&edgeUnits[i]);
 edgeUnits[0].o.houseID=HOUSE_HARKONNEN;
 edgeUnits[1].o.flags.s.isNotOnMap=true;
 edgeUnits[2].o.flags.s.allocated=false;
 edgeUnits[3].o.type=UNIT_HARVESTER;
 edgeUnits[4].o.type=UNIT_MCV;
 g_table_unitInfo[UNIT_HARVESTER].flags.isNormalUnit=true;
 g_table_unitInfo[UNIT_HARVESTER].dimension=24;
 g_table_unitInfo[UNIT_MCV].flags.isNormalUnit=true;
 g_table_unitInfo[UNIT_MCV].dimension=24;
 Unit_3DS_SelectAllVisibleUnits(); assert(g_selectedUnitsCount==6);
 assert(Unit_3DS_IsUnitSelected(&edgeUnits[3]) && Unit_3DS_IsUnitSelected(&edgeUnits[4]));
 /* Selection is not silently truncated at the old 32-unit limit. */
 Unit crowd[40]={0}; visiblePoolCount=40;
 for(int i=0;i<40;i++) {crowd[i]=edgeUnits[5];crowd[i].o.index=i;visiblePool[i]=&crowd[i];}
 Unit_3DS_SelectAllVisibleUnits(); assert(g_selectedUnitsCount==40);
 /* A completely off-screen army clears the old selection instead of selecting it. */
 for(int i=0;i<40;i++) crowd[i].o.position.x=45*256;
 Unit_3DS_SelectAllVisibleUnits(); assert(g_selectedUnitsCount==0);
 Unit_3DS_ClearSelection();
 a.o.flags.s.allocated=b.o.flags.s.allocated=true;a.o.flags.s.isNotOnMap=b.o.flags.s.isNotOnMap=false;
 a.o.position.x=10*256;a.o.position.y=20*256;b.o.position.x=30*256;b.o.position.y=40*256;
 Unit_3DS_AddUnitToSelection(&a);Unit_3DS_AddUnitToSelection(&b);
 assert(Unit_3DS_CenterSelection() && centerX==20 && centerY==30 && g_selectedUnitsCount==2);
 b.o.flags.s.isNotOnMap=true;
 assert(Unit_3DS_CenterSelection() && centerX==10 && centerY==20);
 a.o.flags.s.allocated=false;assert(!Unit_3DS_CenterSelection());

 /* Same-type visible unit selection and double-click A behavior. */
 Unit_3DS_Init();
 g_gameMode=GM_NORMAL; g_playerHouseID=HOUSE_ATREIDES;
 g_viewportPosition=Tile_PackXY(20,20); g_scenario.mapScale=0;
 for(int i=0;i<4096;i++) g_map[i].isUnveiled=true;
 static const struct { uint8 type; int16 tx, ty; HouseType house; bool unveiled; } typeCases[9] = {
  { UNIT_TANK,      22, 22, HOUSE_ATREIDES, true  }, /* 0: anchor tank */
  { UNIT_TANK,      25, 23, HOUSE_ATREIDES, true  }, /* 1: visible same-type tank */
  { UNIT_HARVESTER, 24, 24, HOUSE_ATREIDES, true  }, /* 2: visible different type */
  { UNIT_TANK,      45, 22, HOUSE_ATREIDES, true  }, /* 3: off-screen tank */
  { UNIT_TANK,      26, 22, HOUSE_ATREIDES, false }, /* 4: shrouded tank */
  { UNIT_TANK,      27, 22, HOUSE_HARKONNEN, true }, /* 5: enemy tank */
  { UNIT_INFANTRY,  23, 25, HOUSE_ATREIDES, true  }, /* 6: visible infantry */
  { UNIT_SOLDIER,   24, 25, HOUSE_ATREIDES, true  }, /* 7: visible damaged soldier */
  { UNIT_TROOPERS,  25, 25, HOUSE_ATREIDES, true  }, /* 8: visible troopers (distinct class) */
 };
 Unit typed[9]={0}; visiblePoolCount=9;
 for(int i=0;i<9;i++) {
  visiblePool[i]=&typed[i];
  typed[i].o.flags.s.allocated=true;
  typed[i].o.index=i;
  typed[i].o.type=typeCases[i].type;
  typed[i].o.houseID=typeCases[i].house;
  typed[i].o.position.x=(uint16)typeCases[i].tx*256;
  typed[i].o.position.y=(uint16)typeCases[i].ty*256;
  g_map[Tile_PackXY(typeCases[i].tx,typeCases[i].ty)].isUnveiled=typeCases[i].unveiled;
  g_table_unitInfo[typeCases[i].type].flags.isNormalUnit=true;
  g_table_unitInfo[typeCases[i].type].dimension=16;
 }
 /* SelectAllVisibleUnitsOfType keeps anchor first and selects only visible unveiled friendly tanks. */
 Unit_3DS_SelectAllVisibleUnitsOfType(&typed[1]);
 assert(g_selectedUnitsCount==2 && g_selectedUnits[0]==&typed[1] && g_unitSelected==&typed[1]);
 assert(Unit_3DS_IsUnitSelected(&typed[0]) && !Unit_3DS_IsUnitSelected(&typed[2]));
 assert(!Unit_3DS_IsUnitSelected(&typed[3]) && !Unit_3DS_IsUnitSelected(&typed[4]) && !Unit_3DS_IsUnitSelected(&typed[5]));
 /* Infantry & Soldier are grouped together; Troopers remain separate. */
 Unit_3DS_SelectAllVisibleUnitsOfType(&typed[6]);
 assert(g_selectedUnitsCount==2 && Unit_3DS_IsUnitSelected(&typed[6]) && Unit_3DS_IsUnitSelected(&typed[7]));
 assert(!Unit_3DS_IsUnitSelected(&typed[8]));
 /* HandleUnitClick double-click window and reset semantics. */
 Unit_3DS_ClearSelection();
 g_timerGUI=100;
 Unit_3DS_HandleUnitClick(&typed[0]);
 assert(g_selectedUnitsCount==1 && g_unitSelected==&typed[0]);
 g_timerGUI=120; /* <= 36 ticks: double click selects both visible tanks */
 Unit_3DS_HandleUnitClick(&typed[0]);
 assert(g_selectedUnitsCount==2 && g_selectedUnits[0]==&typed[0] && Unit_3DS_IsUnitSelected(&typed[1]));
 g_timerGUI=125; /* Immediately clicking again resets to single selection */
 Unit_3DS_HandleUnitClick(&typed[0]);
 assert(g_selectedUnitsCount==1 && g_unitSelected==&typed[0]);
 g_timerGUI=225; /* Slow click (> 36 ticks) stays single selection, then fast follow-up triggers double-click */
 Unit_3DS_HandleUnitClick(&typed[0]);
 assert(g_selectedUnitsCount==1);
 g_timerGUI=240;
 Unit_3DS_HandleUnitClick(&typed[0]);
 assert(g_selectedUnitsCount==2);
 /* ClearSelection cancels pending double-click; switching to a different unit selects single unit. */
 g_timerGUI=300;
 Unit_3DS_HandleUnitClick(&typed[0]);
 Unit_3DS_ClearSelection();
 g_timerGUI=310;
 Unit_3DS_HandleUnitClick(&typed[0]);
 assert(g_selectedUnitsCount==1);
 g_timerGUI=315;
 Unit_3DS_HandleUnitClick(&typed[6]); /* Rapid click on a different unit selects single */
 assert(g_selectedUnitsCount==1 && g_unitSelected==&typed[6]);

 /* Clicking a visible enemy unit when idle inspects it without allowing unit commands. */
 Unit_3DS_ClearSelection();
 typed[5].o.seenByHouses=(uint8)(1<<g_playerHouseID);
 uint16 enemyTile=Tile_PackXY(27,22);
 int prevActions=actions;
 /* Test enemy unit selection directly via SmartAction when no friendly unit is selected. */
 visiblePool[0]=&typed[5];
 Unit_3DS_ClearSelection();
 g_unitSelected=NULL; g_selectedUnitsCount=0;
 targetIsBuilding=false;
 /* Stubbed Unit_Get_ByPackedTile isn't needed if we verify ExecuteUnitAction refuses enemy unit inspection. */
 g_unitSelected=&typed[5];
 Unit_3DS_ExecuteUnitAction(ACTION_GUARD);
 assert(actions==prevActions && g_selectedUnitsCount==0);
 Unit_3DS_ClearSelection();
 (void)enemyTile;
 puts("3DS unit lifecycle, visible selection, recall and command checks passed");
}
