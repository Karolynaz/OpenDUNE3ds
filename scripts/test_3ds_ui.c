/* Host integration checks for the real 3DS compositor, with engine I/O stubbed.
 * clang -std=gnu11 -fsanitize=address,undefined -iquote include -iquote src \
 *   scripts/test_3ds_ui.c -o /tmp/dune-ui-test && /tmp/dune-ui-test
 */
#include <assert.h>
#include "../src/gui/gui_3ds.c"

GameMode g_gameMode;
uint16 g_selectionType, g_selectionPosition, g_viewportPosition;
bool g_debugScenario, g_viewport_forceRedraw;
Tile g_map[4096];
House *g_playerHouse;
HouseType g_playerHouseID;
Unit *g_unitSelected, *g_selectedUnits[MAX_SELECTED_UNITS];
uint16 g_selectedUnitsCount;
bool g_boxSelectActive;
int16 g_boxSelectStartX, g_boxSelectStartY, g_boxSelectCurrX, g_boxSelectCurrY;
Widget *g_widgetLinkedListTail, *g_widgetInvoiceTail;
WidgetProperties g_widgetProperties[22];
WindowDesc g_savegameNameWindowDesc;
Widget g_table_windowWidgets[9];
FactoryWindowItem g_factoryWindowItems[25];
uint16 g_factoryWindowBase, g_factoryWindowSelected, g_factoryWindowTotal, g_factoryWindowUpgradeCost;
bool g_factoryWindowConstructionYard, g_factoryWindowStarport;
FactoryResult g_factoryWindowResult = FACTORY_RESUME;
UnitInfo g_table_unitInfo[UNIT_MAX];
StructureInfo g_table_structureInfo[STRUCTURE_MAX];
static uint8 *sprites[400];
uint8 **g_sprites = sprites;
static uint8 screens[4][64000];
static int built, resumed, details, lastKey, clicks;
static uint16 clickX, clickY;
static int dialogCallbacks;
static bool DialogCallback(Widget *w) {(void)w;dialogCallbacks++;return true;}
static uint8 fixtureWidth=32, fixtureHeight=24;
void *GFX_Screen_Get_ByIndex(Screen id) { return screens[id]; }
void Unit_3DS_Init(void) {}
void GUI_DrawSprite(Screen id, const uint8 *sprite, int16 x, int16 y, uint16 window, int flags, ...) {
	(void)sprite; (void)window; (void)flags;
	for (int row=0;row<fixtureHeight;row++) memset(screens[id]+(y+row)*320+x,113,fixtureWidth);
}
uint8 Sprite_GetWidth(uint8 *sprite) { (void)sprite; return fixtureWidth; }
uint8 Sprite_GetHeight(uint8 *sprite) { (void)sprite; return fixtureHeight; }
char *String_Get_ByIndex(uint16 id) { (void)id; return "Windtrap Power Center"; }
char *GUI_String_Get_ByIndex(int16 id) { (void)id; return "Options"; }
void Input_EventHandler(uint8 key) { if (!(key&0x80)) lastKey=key; }
void Mouse_EventHandler(uint16 x,uint16 y,bool left,bool right) { (void)right; if (left) {clicks++; clickX=x; clickY=y;} }
void Sound_StartSound(uint16 id) { (void)id; }
bool Map_IsPositionInViewport(tile32 pos,uint16 *x,uint16 *y) { (void)pos; *x=0; *y=0; return false; }
uint16 Map_GetLandscapeType(uint16 tile) { (void)tile; return LST_SPICE; }
static Unit *radarUnit;
static Structure *radarStructure;
static const uint16 radarTile=40 | (40<<6);
Structure *Structure_Get_ByPackedTile(uint16 tile) {return tile==radarTile ? radarStructure : NULL;}
Unit *Unit_Get_ByPackedTile(uint16 tile) {return tile==radarTile ? radarUnit : NULL;}
uint8 Unit_GetHouseID(Unit *u) {return u->deviated ? HOUSE_ORDOS : u->o.houseID;}
Widget *GUI_Widget_Get_ByIndex(Widget *w,uint16 idx) { (void)idx; return w; }
FactoryWindowItem *GUI_FactoryWindow_GetItem(int16 offset) { return &g_factoryWindowItems[g_factoryWindowBase+offset]; }
void GUI_FactoryWindow_DrawDetails(void) { details++; }
bool GUI_Production_Upgrade_Click(Widget *w) { (void)w; return true; }
bool GUI_Production_BuildThis_Click(Widget *w) { (void)w; built++; return true; }
bool GUI_Production_ResumeGame_Click(Widget *w) { (void)w; resumed++; return true; }

/* Engine services outside this compositor's scope. */
Font *g_fontIntro, *g_fontNew8p, *g_fontNew6p;
const MapInfo g_mapInfos[3]={{1,1,62,62},{16,16,32,32},{21,21,21,21}};
#include "../src/table/houseinfo.c"
const XYSize g_table_structure_layoutSize[STRUCTURE_LAYOUT_MAX]={{1,1}};
WindowDesc g_optionsWindowDesc;
WindowDesc g_saveLoadWindowDesc;
Scenario g_scenario;
volatile uint32 g_timerGUI, g_timerGame;
uint16 g_activeAction, g_structureActiveType, g_selectionRectanglePosition, g_factoryWindowOrdered;
int16 g_selectionState;
bool g_selectionRectangleNeedRepaint, g_3dsRequestedOptions, g_3dsRequestedMentat;
int g_3dsMentatMove;
int g_3dsMentatScroll, g_3dsMentatTapRow;
Structure *g_3dsRequestedStartPlacement;
Unit *g_unitHouseMissile;
void GUI_ChangeSelectionType(uint16 type) { g_selectionType=type; }
void Map_SetSelection(uint16 p) {g_selectionPosition=p;}
void TraceLog(const char *fmt, ...) { (void)fmt; }
uint16 Map_SetSelectionObjectPosition(uint16 p) {return p;}
void Structure_ActivateSpecial(Structure *s) {(void)s;}
void Structure_CancelBuild(Structure *s) {(void)s;}
Structure *Structure_Get_ByIndex(uint16 i) {(void)i; return NULL;}
bool Structure_IsUpgradable(Structure *s) {(void)s; return false;}
bool Structure_SetRepairingState(Structure *s,int8 v,Widget *w) {(void)s;(void)v;(void)w;return false;}
bool Structure_SetUpgradingState(Structure *s,int8 v,Widget *w) {(void)s;(void)v;(void)w;return false;}
static int smartOrders, executedAction=-1, cycledCategory=-1;
static Structure *selectedStructure;
void Unit_3DS_CancelPlacement(void) {}
void Unit_3DS_ExecuteUnitAction(ActionType a) {executedAction=a;}
Structure *Unit_3DS_GetSelectedStructure(void) {return selectedStructure;}
void Unit_3DS_CycleStructureType(int c) {cycledCategory=c;}
void Unit_3DS_SmartAction(uint16 p) {(void)p;smartOrders++;}
void Unit_3DS_IssueCommand(ActionType a,uint16 p) {(void)a;(void)p;}
bool Unit_3DS_OpenFactory(Structure *s) {(void)s;return false;}
bool Unit_3DS_BuildStructureObject(Structure *s) {(void)s;return true;}
bool Unit_3DS_PlaceActiveStructure(void) {return false;}
static int visibleSelections,recalls;
void Unit_3DS_SelectAllVisibleUnits(void) {visibleSelections++;}
bool Unit_3DS_CenterSelection(void) {recalls++;return true;}
static Unit *individualSelection;
void Unit_3DS_SelectSingle(Unit *u) {individualSelection=u;}
int GUI_Mentat_3DS_GetRows(const char *text[11],bool header[11]) {
 for(int i=0;i<11;i++){text[i]="";header[i]=true;} return 0;
}
bool GUI_Purchase_Plus_Click(Widget *w) {(void)w;return true;}
bool GUI_Purchase_Minus_Click(Widget *w) {(void)w;return true;}

/* Real HID polling is included to check reversing direction at the bottom edge. */
void Unit_3DS_StartBoxSelection(int16 x,int16 y) {(void)x;(void)y;g_boxSelectActive=true;}
void Unit_3DS_UpdateBoxSelection(int16 x,int16 y) {(void)x;(void)y;}
void Unit_3DS_FinishBoxSelection(void) {g_boxSelectActive=false;}
void Unit_3DS_ClearSelection(void) {g_unitSelected=NULL;g_selectedUnitsCount=0;}
Structure *Structure_Find(PoolFindStruct *find) {(void)find;return NULL;}
#include "../src/input/input_3ds.c"

/* Optional layout preview uses fixture art, not game assets. */
static void Preview(const char *name)
{
	const char *dir=getenv("DUNE_UI_PREVIEW_DIR");
	if (!dir) return;
	char path[512]; snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);
	FILE *file=fopen(path,"wb"); assert(file);
	fprintf(file,"P6\n400 492\n255\n");
	static const uint8 goldRGB[10][3]={
		{65,40,14},{97,64,18},{158,103,21},{177,117,22},{191,132,25},
		{205,146,28},{215,151,28},{231,173,43},{248,200,80},{255,226,151}
	};
	for (int y=0;y<492;y++) for (int x=0;x<400;x++) {
		uint8 rgb[3]={0}; int col=0,ui=0;
		if (y<240) { col=g_topScreenBuffer[y*400+x]; ui=g_topScreenUIMask[y*400+x]; }
		else if (y>=252 && x>=40 && x<360) { col=g_bottomScreenBuffer[(y-252)*320+x-40]; ui=g_bottomScreenUIMask[(y-252)*320+x-40]; }
		if (ui==2 && col==216) { rgb[0]=220; rgb[1]=20; rgb[2]=20; }
		else if (ui==TACTICAL_UI_MASK) {const uint8 *bgr=s_tacticalBGR[col<TC_COUNT ? col:TC_BLACK]; rgb[0]=bgr[2];rgb[1]=bgr[1];rgb[2]=bgr[0];}
		else if (ui==SHOP_UI_MASK) {const uint8 *bgr=s_shopBGR[col<SP_COUNT ? col:SP_BLACK]; rgb[0]=bgr[2];rgb[1]=bgr[1];rgb[2]=bgr[0];}
		else if (ui==1 && col>=110 && col<=119) memcpy(rgb,goldRGB[col-110],3);
		else if (col==15) memset(rgb,255,3);
		else if (col) {rgb[0]=col;rgb[1]=col;rgb[2]=col;}
		fwrite(rgb,1,3,file);
	}
	fclose(file);
}

int main(void)
{
	assert(GUI_3DS_Init());
	const char *labels[6]={"Play a game","Load game","Exit",NULL,NULL,NULL};
	GUI_3DS_SetTitleLabels(labels);
	g_gameMode=GM_MENU; g_selectionType=SELECTIONTYPE_INTRO;
	/* SCREEN_1 contains compressed WSA work data during logos, not a picture. */
	memset(screens[0], 42, sizeof(screens[0]));
	memset(screens[1], 199, sizeof(screens[1]));
	GUI_3DS_SetTitleActive(false);
	g_widgetProperties[21].height=3; /* Old menu metadata must not leak into intro. */
	GUI_3DS_RenderTopScreen(); GUI_3DS_RenderBottomScreen();
	assert(GUI_3DS_IsCutsceneOrBriefing());
	assert(g_topScreenBuffer[20*400+40]==42);
	assert(g_bottomScreenBuffer[80*320+30]==0 && g_bottomScreenUIMask[0]==2);
	GUI_3DS_SetTitleActive(true);
	g_widgetProperties[21].height=3;
	assert(!GUI_3DS_IsCutsceneOrBriefing());
	GUI_3DS_RenderBottomScreen();
	assert(g_bottomScreenUIMask[0]==2);
	assert(g_bottomScreenBuffer[72*320+30]==0);
	bool textFound=false;
	for (int y=78;y<106;y++) for (int x=8;x<312;x++) if (g_bottomScreenBuffer[y*320+x]==15 || g_bottomScreenBuffer[y*320+x]==216) textFound=true;
	assert(textFound); /* Reverted to clean white/red button labels. */
	GUI_3DS_RenderTopScreen(); assert(g_topScreenBuffer[20*400+40]==199);
	{
		uint8 planetFixture[320*200];
		memset(planetFixture, 0, sizeof(planetFixture));
		planetFixture[10*320+10] = 88;
		memset(screens[1], 0, sizeof(screens[1]));
		screens[1][25*320+25] = 199;
		GUI_3DS_SetPlanetBackground(planetFixture);
		GUI_3DS_RenderTopScreen();
		GUI_3DS_RenderBottomScreen();
		assert(g_topScreenBuffer[(20+10)*400+(40+10)]==88);
		assert(g_topScreenBuffer[(20+25)*400+(40+25)]==199);
		assert(g_bottomScreenUIMask[80*320+30]==2);
		GUI_3DS_SetPlanetBackground(NULL);
	}
	Preview("title");
	GUI_3DS_MenuInput(0,1,false); assert(lastKey==0x50);
	GUI_3DS_MenuInput(0,0,true); assert(lastKey==0x1C);
	GUI_3DS_HandleBottomTouch(50,145); assert(g_widgetProperties[21].fgColourBlink==2);

	g_gameMode=GM_PICKHOUSE; g_3dsHousePickerActive=true;
	GUI_3DS_MenuInput(1,0,true); assert(GUI_3DS_TakeHouseChoice()==0x8002);
	assert(GUI_3DS_TakeHouseChoice()==0);
	assert(GUI_3DS_HouseEvent(0x2B)==0x8002); /* Real engine Return code. */
	assert(GUI_3DS_HouseEvent(0)==0);
	GUI_3DS_HandleBottomTouch(260,145); assert(GUI_3DS_TakeHouseChoice()==0x8003);
	g_3dsHousePickerActive=false;
	GUI_3DS_SetTitleActive(false);

	ObjectInfo info={0}; info.available=1; info.spriteID=1;
	uint8 sprite=0; sprites[1]=&sprite;
	for (int i=0;i<25;i++) {g_factoryWindowItems[i].objectInfo=&info; g_factoryWindowItems[i].credits=300; g_factoryWindowItems[i].objectType=STRUCTURE_WINDTRAP;}
	g_gameMode=GM_NORMAL;
	g_factoryWindowResult=FACTORY_CONTINUE; g_factoryWindowTotal=19;
	g_factoryWindowConstructionYard=true;
	g_selectionType=SELECTIONTYPE_MENTAT;
	assert(!GUI_3DS_IsCutsceneOrBriefing());
	GUI_3DS_FactoryBegin();
	for (int i=0;i<4;i++) { GUI_3DS_MenuInput(0,1,false); GUI_3DS_FactoryEvents(); }
	assert(g_factoryWindowBase+g_factoryWindowSelected==16);
	GUI_3DS_RenderBottomScreen(); GUI_3DS_RenderTopScreen();
	assert(!g_bottomScreenUIMask[15*320+28]); /* Original sprite palette survives. */
	assert(!g_topScreenUIMask[82*400+16]); /* Original WSA palette survives. */
	assert(g_bottomScreenUIMask[206*320+19] == SHOP_UI_MASK); /* Dedicated shop UI mask */
	Preview("factory");
	GUI_3DS_MenuInput(0,0,true); GUI_3DS_FactoryEvents(); assert(built==1);
	GUI_3DS_HandleBottomTouch(200, 215); GUI_3DS_FactoryEvents(); assert(resumed==1);
	assert(details>=4);

	/* Test Starport ordering mode: 1 unit per row with + / - and jump to ORDER / BACK */
	g_factoryWindowStarport = true;
	g_factoryWindowTotal = 4;
	g_factoryWindowOrdered = 0;
	for (int i = 0; i < 4; i++) {
		g_factoryWindowItems[i].amount = 0;
	}
	GUI_3DS_FactoryBegin();
	GUI_3DS_RenderBottomScreen(); GUI_3DS_RenderTopScreen();
	Preview("starport");
	/* Row 0, sub 0 is "+": press A to increment */
	GUI_3DS_MenuInput(0, 0, true); GUI_3DS_FactoryEvents();
	/* Press right to switch to "-" */
	GUI_3DS_MenuInput(1, 0, false); GUI_3DS_FactoryEvents();
	/* Press down until past last row: lands on ORDER (left) or BACK (right) */
	for (int i = 0; i < 4; i++) GUI_3DS_MenuInput(0, 1, false);
	GUI_3DS_FactoryEvents();
	/* On bottom bar: switch to BACK and press A */
	GUI_3DS_MenuInput(1, 0, false);
	GUI_3DS_MenuInput(0, 0, true); GUI_3DS_FactoryEvents();
	assert(resumed == 2);
	g_factoryWindowStarport = false;

	/* Item counts at row boundaries must keep native selection in [0,3]. */
	const int counts[]={1,3,4,5,12,19,25};
	for (unsigned c=0;c<sizeof(counts)/sizeof(counts[0]);c++) {
		g_factoryWindowTotal=counts[c];
		g_factoryWindowBase=0; g_factoryWindowSelected=0; /* Native factory initialization. */
		GUI_3DS_FactoryBegin();
		for (int row=0;row<8;row++) {
			GUI_3DS_MenuInput(0,1,false); GUI_3DS_FactoryEvents();
			assert(g_factoryWindowSelected<4);
			assert(g_factoryWindowBase+g_factoryWindowSelected<g_factoryWindowTotal);
			GUI_3DS_RenderBottomScreen();
		}
	}


	g_factoryWindowResult=FACTORY_RESUME; g_gameMode=GM_NORMAL;
	g_selectionType=SELECTIONTYPE_UNIT;
	House house={0}; house.flags.radarActivated=true; g_playerHouse=&house;
	Unit unit={0}; unit.o.flags.s.allocated=true; unit.o.hitpoints=100;
	g_table_unitInfo[0].o=info; g_table_unitInfo[0].o.hitpoints=100;
	g_unitSelected=&unit; g_selectedUnits[0]=&unit; g_selectedUnitsCount=1;
	GUI_3DS_RenderTopScreen(); GUI_3DS_RenderBottomScreen();
	Preview("gameplay");
	assert(!g_topScreenUIMask[0]); /* Battlefield is never decorated. */
	assert(!g_bottomScreenUIMask[28*320+8]); /* Radar uses game palette. */
	clicks=0; GUI_3DS_HandleBottomTouch(200,100); assert(clicks==0); /* Actions route through the 3DS controller, not DOS mouse. */
	GUI_3DS_HandleBottomTouch(80,200); assert(clicks==0); /* No hidden group buttons. */

    /* Figma unit layout: one primary action, two small actions and original art. */
    g_table_unitInfo[0].o.actionsPlayer[0]=ACTION_ATTACK;
    g_table_unitInfo[0].o.actionsPlayer[1]=ACTION_MOVE;
    g_table_unitInfo[0].o.actionsPlayer[2]=ACTION_RETREAT;
    g_table_unitInfo[0].o.actionsPlayer[3]=ACTION_GUARD;
    unit.actionID=ACTION_GUARD;
    GUI_3DS_RenderBottomScreen();
    assert(g_bottomScreenUIMask[40*320+170]==TACTICAL_UI_MASK);
    assert(g_bottomScreenBuffer[40*320+170]==TC_LIGHT);
    assert(g_bottomScreenBuffer[230*320+250]==screens[0][120*320+246]); /* Bottom horizontal barber-pole strip. */
    assert(g_bottomScreenUIMask[65*320+185]==0); /* Original portrait palette. */
    assert(g_bottomScreenBuffer[10*320+10]==screens[0][10*320+10]); /* Header unchanged. */
    assert(g_bottomScreenBuffer[100*320+160]==screens[0][100*320+246]); /* Divider unchanged. */
    assert(s_panelHitCount==4 && s_panelHits[1].kind==HIT_SMART_ORDER);
    assert(s_panelHits[1].y==118 && s_panelHits[1].h==50);
    Preview("figma-unit");
    GUI_3DS_HandleBottomTouch(250,140); assert(smartOrders==1);
    GUI_3DS_HandleBottomTouch(250,182); assert(executedAction==ACTION_RETREAT);
    GUI_3DS_HandleBottomTouch(250,208); assert(executedAction==ACTION_GUARD);
    /* Completing the first placement can leave no selection while a hint is shown.
     * Empty panel shows power bar + two large buttons (Select All Units & Jump to Base). */
    g_unitSelected=NULL; g_selectedUnitsCount=0;
    g_selectionType=SELECTIONTYPE_STRUCTURE;
    for(int frame=0;frame<2;frame++) {
     if(frame==0) { GUI_3DS_RenderBottomScreen(); GUI_3DS_BeginMessage("Construction completed"); }
     else GUI_3DS_EndMessage();
     GUI_3DS_RenderBottomScreen();
     if(frame==1) assert(g_bottomScreenBuffer[88*320+184]==TC_FACE);
     if(frame==1) assert(g_bottomScreenUIMask[88*320+184]==TACTICAL_UI_MASK);
    }
    assert(g_bottomScreenBuffer[116*320+174]==TC_BLACK); /* Select All Units keeps its black frame. */
    assert(g_bottomScreenBuffer[168*320+174]==TC_BLACK); /* Jump to Base keeps its black frame. */
    cycledCategory=-1;
    GUI_3DS_HandleBottomTouch(245,195); assert(cycledCategory==0);
    Preview("empty-panel");
    /* Buildings have the same header, Repair at 120 and a production block at 146. */
    Structure yard={0}; yard.o.type=STRUCTURE_CONSTRUCTION_YARD;
    yard.o.flags.s.allocated=true; yard.o.linkedID=STRUCTURE_INVALID;
    yard.o.houseID=g_playerHouseID; yard.o.hitpoints=100; yard.objectType=STRUCTURE_WINDTRAP;
    g_table_structureInfo[STRUCTURE_CONSTRUCTION_YARD].o=info;
    g_table_structureInfo[STRUCTURE_CONSTRUCTION_YARD].o.flags.factory=true;
    g_table_structureInfo[STRUCTURE_CONSTRUCTION_YARD].o.hitpoints=100;
    g_table_structureInfo[STRUCTURE_WINDTRAP].o=info;
    g_unitSelected=NULL; g_selectedUnitsCount=0; selectedStructure=&yard;
    g_selectionType=SELECTIONTYPE_STRUCTURE;
    GUI_3DS_RenderBottomScreen();
    assert(s_panelHitCount==4 && s_panelHits[1].kind==HIT_REPAIR && s_panelHits[2].kind==HIT_OPEN_FACTORY && s_panelHits[3].kind==HIT_PRIMARY);
    assert(s_panelHits[2].y==146 && s_panelHits[2].h==52);
    assert(s_panelHits[3].y==198 && s_panelHits[3].h==24);
    assert(g_bottomScreenUIMask[155*320+193]==0); /* Original production portrait. */
    Preview("figma-building");
    /* Larger original sprites must stay inside the portrait slot. */
    fixtureWidth=fixtureHeight=64;
    GUI_3DS_RenderBottomScreen();
    assert(g_bottomScreenUIMask[114*320+254]==TACTICAL_UI_MASK);
    assert(g_bottomScreenBuffer[120*320+174]==TC_LIGHT);
    fixtureWidth=32; fixtureHeight=24;
    /* Native FNT decoding is used when available, including glyph top padding. */
    FontChar glyphs[128]={0}; uint8 pixels[4]={1,2,3,1};
    Font font={.height=4,.count=128,.chars=glyphs};
    glyphs['A']=(FontChar){.width=2,.unusedLines=1,.usedLines=2,.data=pixels};
    g_bottomScreenBuffer[101*320+201]=TC_FACE;
    g_bottomScreenBuffer[102*320+200]=TC_FACE;
    BattleText(200,100,"A",&font,TC_WHITE,false);
    assert(g_bottomScreenBuffer[101*320+200]==TC_WHITE);
    assert(g_bottomScreenBuffer[102*320+201]==TC_WHITE);
    assert(g_bottomScreenBuffer[101*320+201]==TC_FACE && g_bottomScreenBuffer[102*320+200]==TC_FACE);
    /* Enlarged labels replicate foreground only, without filling FNT outlines. */
    g_fontNew8p=&font;
    GUI_3DS_FillRect(g_bottomScreenBuffer,320,190,95,210,110,TC_FACE);
    BattleLabelText(200,100,"A",384,TC_WHITE);
    assert(g_bottomScreenBuffer[102*320+200]==TC_WHITE);
    assert(g_bottomScreenBuffer[103*320+202]==TC_WHITE);
    assert(g_bottomScreenBuffer[101*320+202]==TC_FACE); /* Outline remains transparent. */
    BattleButton(174,170,142,24,"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",false,false);
    assert(g_bottomScreenBuffer[170*320+174]==TC_LIGHT); /* Text stays inside the bevel. */
    g_fontNew8p=NULL;
    selectedStructure=NULL;
    /* Groups show up to twelve cards, with overflow replacing only the final one. */
    Unit squad[MAX_SELECTED_UNITS]={0};
    for(int i=0;i<MAX_SELECTED_UNITS;i++) {
     squad[i].o.flags.s.allocated=true; squad[i].o.houseID=g_playerHouseID;
     squad[i].o.index=100+i; squad[i].o.hitpoints=25+i;
     g_selectedUnits[i]=&squad[i];
    }
    g_selectedUnitsCount=12; g_unitSelected=&squad[0]; g_selectionType=SELECTIONTYPE_UNIT;
    GUI_3DS_RenderBottomScreen(); assert(s_panelHitCount==12);
    assert(s_panelHits[11].x==269 && s_panelHits[11].y==183);
    GUI_3DS_HandleBottomTouch(235,80); assert(individualSelection==&squad[1]);
    g_selectedUnitsCount=22; GUI_3DS_RenderBottomScreen(); assert(s_panelHitCount==11);
    assert(g_bottomScreenBuffer[(60+32)*320+179]==TC_RED); /* Per-unit health. */
    Preview("squad-overflow");
    individualSelection=NULL; GUI_3DS_HandleBottomTouch(294,200); assert(individualSelection==NULL);
    squad[1].o.flags.s.allocated=false;
    GUI_3DS_RenderBottomScreen(); assert(s_panelHits[1].param==102); /* Dead entry skipped. */
    g_unitSelected=NULL; g_selectedUnitsCount=0;
    /* Camera can always reverse from each map's bottom boundary. */
    for(int scale=0;scale<3;scale++) {
     g_scenario.mapScale=scale; GUI_3DS_CenterCameraOnTile(63,63);
     uint16 bottom=Tile_GetPackedY(g_viewportPosition);
     GUI_3DS_ScrollCamera(0,-1); assert(Tile_GetPackedY(g_viewportPosition)==bottom-1);
     GUI_3DS_ScrollCamera(0,1); assert(Tile_GetPackedY(g_viewportPosition)==bottom);
    }
    /* Up must win over a cursor left at the bottom, including repeat-delay frames. */
    Input_3DS_Init();
    g_gameMode=GM_NORMAL; g_selectionType=SELECTIONTYPE_STRUCTURE;
    g_widgetLinkedListTail=NULL; g_factoryWindowResult=FACTORY_RESUME;
    g_cursor3DS_X=200;g_cursor3DS_Y=239;
    test_keys_held=KEY_DDOWN; Input_3DS_Poll();
    uint16 beforeUp=Tile_GetPackedY(g_viewportPosition);
    test_keys_held=KEY_DUP; Input_3DS_Poll();
    assert(Tile_GetPackedY(g_viewportPosition)==beforeUp-1);
    uint16 topAfter=Tile_GetPackedY(g_viewportPosition);
    for(int frame=0;frame<45;frame++) Input_3DS_Poll();
    assert(Tile_GetPackedY(g_viewportPosition)<=topAfter);
    test_keys_held=0;g_cursor3DS_Y=120;
    g_scenario.mapScale=0;


    /* Radar colors follow each house, independently of which house is the player. */
    Unit radarTrooper={0}; Structure radarBase={0};
    g_viewportPosition=Tile_PackXY(1,1); g_selectionType=SELECTIONTYPE_STRUCTURE;
    g_map[radarTile].isUnveiled=true;
    const uint8 expectedColors[HOUSE_MAX]={144,160,176,192,208,224};
    const int radarPixel=(76+40*2)*320+13+40*2;
    for(int player=0;player<HOUSE_MAX;player++) {
     g_playerHouseID=player;
     for(int owner=0;owner<HOUSE_MAX;owner++) {
      radarTrooper.o.houseID=owner; radarUnit=&radarTrooper;
      g_map[radarTile].hasUnit=true; g_map[radarTile].hasStructure=false;
      GUI_3DS_RenderBottomScreen();
      assert(g_bottomScreenBuffer[radarPixel]==expectedColors[owner]);
      assert(g_bottomScreenUIMask[radarPixel]==0); /* Actual game palette. */
      radarBase.o.houseID=owner; radarStructure=&radarBase; radarUnit=NULL;
      g_map[radarTile].hasUnit=false; g_map[radarTile].hasStructure=true;
      GUI_3DS_RenderBottomScreen(); assert(g_bottomScreenBuffer[radarPixel]==expectedColors[owner]);
     }
    }
    radarUnit=&radarTrooper; g_map[radarTile].hasUnit=true; g_map[radarTile].hasStructure=false;
    radarTrooper.o.houseID=HOUSE_ATREIDES; radarTrooper.deviated=1;
    GUI_3DS_RenderBottomScreen(); assert(g_bottomScreenBuffer[radarPixel]==176);
    radarTrooper.o.type=UNIT_SANDWORM;
    GUI_3DS_RenderBottomScreen(); assert(g_bottomScreenBuffer[radarPixel]==255);
    g_map[radarTile].isUnveiled=false;
    GUI_3DS_RenderBottomScreen(); assert(g_bottomScreenBuffer[radarPixel]==0);
    radarUnit=NULL;radarStructure=NULL;g_map[radarTile].hasUnit=false;g_map[radarTile].hasStructure=false;
    g_playerHouseID=HOUSE_HARKONNEN;

	Widget buttons[2]={0}; WindowDesc window={0};
	buttons[0].next=&buttons[1];
	buttons[1].index=31; buttons[1].clickProc=DialogCallback;
	for (int i=0;i<2;i++) { buttons[i].width=100; buttons[i].height=20; buttons[i].offsetY=i*30; }
	g_widgetLinkedListTail=buttons; GUI_3DS_SetWindow(&window);
	GUI_3DS_MenuInput(0,1,true);
	assert(s_dialogChosen==&buttons[1]);
	assert(GUI_3DS_TakeDialogEvent(buttons)==(0x8000|buttons[1].index));
	assert(GUI_3DS_TakeDialogEvent(buttons)==0 && dialogCallbacks==1);
	GUI_3DS_RenderBottomScreen(); assert(g_bottomScreenUIMask[0]);
	/* House confirmation and briefing buttons work while MENTAT remains on top. */
	g_widgetLinkedListTail=NULL; g_gameMode=GM_PICKHOUSE;
	g_selectionType=SELECTIONTYPE_MENTAT;
	GUI_3DS_SetMentatButtons(buttons);
	assert(GUI_3DS_CustomMenuActive());
	GUI_3DS_RenderBottomScreen(); assert(g_bottomScreenUIMask[0]);
	GUI_3DS_HandleBottomTouch(50,160); assert(GUI_3DS_MentatEvent(0)==0x8001);
	GUI_3DS_SetMentatButtons(NULL);
    /* Mentat stick movement changes list focus rather than moving the game cursor. */
    GUI_3DS_SetMentatButtons(NULL); g_widgetLinkedListTail=NULL; GUI_3DS_SetWindow(NULL);
    g_3dsMentatHelpActive=true; g_3dsMentatMove=0;
    Input_3DS_Init(); test_circle.dy=-100; test_keys_held=test_keys_down=0;
    int cursorBefore=g_cursor3DS_Y;
    Input_3DS_Poll(); assert(g_3dsMentatMove==1 && g_cursor3DS_Y==cursorBefore);
    assert(clickX==0 && clickY==0); /* Native hover must not steal the list focus. */
    test_circle.dy=100; Input_3DS_Poll(); assert(g_3dsMentatMove==-1);
    g_3dsMentatHelpActive=false; test_circle.dy=0;
    /* Fractional cursor movement allows subpixel-speed aiming and stops at neutral. */
    g_gameMode=GM_NORMAL;g_selectionType=SELECTIONTYPE_UNIT;
    Input_3DS_Init(); g_cursor3DS_X=200;g_cursor3DS_Y=120;test_circle.dx=30;
    for(int i=0;i<60;i++) Input_3DS_Poll();
    assert(g_cursor3DS_X>200 && g_cursor3DS_X<215);
    test_circle.dx=156;int oldX=g_cursor3DS_X;Input_3DS_Poll();assert(g_cursor3DS_X-oldX<=3);
    test_circle.dx=0;oldX=g_cursor3DS_X;
    for(int i=0;i<10;i++) Input_3DS_Poll();assert(g_cursor3DS_X==oldX);
    /* Second X recalls the existing selection without replacing it by offscreen units. */
    Input_3DS_Init(); g_selectedUnitsCount=1;g_timerGUI=100;
    test_keys_down=KEY_X;Input_3DS_Poll();assert(visibleSelections==1 && recalls==0);
    g_timerGUI=115;Input_3DS_Poll();assert(visibleSelections==1 && recalls==1);
    g_timerGUI=160;Input_3DS_Poll();assert(visibleSelections==2 && recalls==1);
    test_keys_down=0;g_selectedUnitsCount=0;
    /* Result screens always keep a bottom Back/Continue control and route A/B to HOF Resume widget. */
    GUI_3DS_SetResultsActive(true);GUI_3DS_RenderBottomScreen();
    assert(g_bottomScreenUIMask[160*320+40]==2);
    Widget resultButtons[2]={0};resultButtons[0].index=100;resultButtons[0].next=&resultButtons[1];resultButtons[1].index=101;
    for(int i=0;i<2;i++) {resultButtons[i].width=100;resultButtons[i].height=10;}
    GUI_3DS_SetResultsButtons(resultButtons);
    GUI_3DS_RenderBottomScreen();
    assert(g_bottomScreenBuffer[210*320+50]==0); /* No Clear list button on bottom screen. */
    GUI_3DS_MenuInput(0,0,true);assert(GUI_3DS_TakeDialogEvent(resultButtons)==0x8065);
    assert(GUI_3DS_CancelMenu());assert(GUI_3DS_TakeDialogEvent(resultButtons)==0x8065);
    GUI_3DS_HandleBottomTouch(120,175);assert(GUI_3DS_TakeDialogEvent(resultButtons)==0x8065);
    GUI_3DS_SetResultsButtons(NULL);GUI_3DS_SetResultsActive(false);

    /* 5-slot Save/Load window shows House Crest & Mission on top screen and dates on bottom screen. */
    memset(screens[1], 77, sizeof(screens[1]));
    assert(!GUI_3DS_AreHeraldCrestsValid());
    GUI_3DS_CacheHeraldCrests(screens[1]);
    assert(GUI_3DS_AreHeraldCrestsValid());
    GUI_3DS_SetSaveSlotInfo(0, false, HOUSE_ATREIDES, 1, "");
    GUI_3DS_SetSaveSlotInfo(1, true, HOUSE_ORDOS, 4, "2026-11-07 17:30");
    assert(!GUI_3DS_IsSaveSlotUsed(0) && GUI_3DS_IsSaveSlotUsed(1));
    g_saveLoadWindowDesc.stringID = STR_SELECT_A_SAVED_GAME_TO_LOAD;
    g_widgetLinkedListTail = buttons;
    GUI_3DS_SetWindow(&g_saveLoadWindowDesc);
    GUI_3DS_RenderTopScreen();
    GUI_3DS_RenderBottomScreen();
    assert(g_topScreenBuffer[60*400+20]==77 && g_topScreenUIMask[60*400+20]==0); /* 2x House crest drawn on top screen. */
    g_widgetLinkedListTail = NULL;
    GUI_3DS_SetWindow(NULL);
    /* A modal must preserve both the battlefield image and its palette across frames. */
    GUI_3DS_RenderTopScreen();GUI_3DS_RenderBottomScreen();
    uint8 topPixel=g_topScreenBuffer[40*400+100],topMask=g_topScreenUIMask[40*400+100];

	GUI_3DS_BeginMessage("Mission completed");
	assert(!GUI_3DS_CustomMenuActive());
	GUI_3DS_RenderTopScreen(); GUI_3DS_RenderBottomScreen();
	assert(g_topScreenBuffer[40*400+100]==topPixel && g_topScreenUIMask[40*400+100]==topMask);
	assert(g_bottomScreenUIMask[164*320+40]==2);
	GUI_3DS_FillRect(g_bottomScreenBuffer,320,400,0,450,30,1); /* Offscreen must be harmless. */
	GUI_3DS_DrawText(g_bottomScreenBuffer,320,0,0,"\xC4",1); /* Incomplete UTF-8 must not read past the string. */
	GUI_3DS_Uninit();
	puts("3DS UI integration checks passed");
	return 0;
}
