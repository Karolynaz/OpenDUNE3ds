/* Host integration checks for the real 3DS compositor, with engine I/O stubbed.
 * clang -std=gnu11 -fsanitize=address,undefined -iquote include -iquote src \
 *   scripts/test_3ds_ui.c -o /tmp/dune-ui-test && /tmp/dune-ui-test
 */
#include <assert.h>
#include "../src/gui/gui_3ds.c"

GameMode g_gameMode;
GameCfg g_gameConfig = { .cursorSpeed = 2, .scrollSpeed = 2 };
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
Widget g_table_windowWidgets[11];
FactoryWindowItem g_factoryWindowItems[25];
uint16 g_factoryWindowBase, g_factoryWindowSelected, g_factoryWindowTotal, g_factoryWindowUpgradeCost;
bool g_factoryWindowConstructionYard, g_factoryWindowStarport;
FactoryResult g_factoryWindowResult = FACTORY_RESUME;
UnitInfo g_table_unitInfo[UNIT_MAX];
StructureInfo g_table_structureInfo[STRUCTURE_MAX];
static uint8 *sprites[400];
uint8 **g_sprites = sprites;
uint8 *g_fileRgnclkCPS;
uint8 *Sprite_GetByIndex(uint16 index) { return index < sizeof(sprites) / sizeof(sprites[0]) ? sprites[index] : NULL; }
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
static bool unitOnScreen;
bool Map_IsPositionInViewport(tile32 pos,uint16 *x,uint16 *y) { (void)pos; *x=120; *y=80; return unitOnScreen; }
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
DuneCfg g_config = {0};
unsigned char Font_NextChar(const char **str, uint8 *diacritic) {
	if (diacritic != NULL) *diacritic = 0;
	unsigned char c = (unsigned char)**str;
	if (c != '\0') (*str)++;
	return c;
}
const MapInfo g_mapInfos[3]={{1,1,62,62},{16,16,32,32},{21,21,21,21}};
#include "../src/table/houseinfo.c"
const XYSize g_table_structure_layoutSize[STRUCTURE_LAYOUT_MAX]={{1,1}};
WindowDesc g_optionsWindowDesc;
WindowDesc g_gameControlWindowDesc;
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
void Unit_3DS_CancelTarget(void) {}
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
static bool mentatFixtureRows = false;
int GUI_Mentat_3DS_GetRows(const char *text[11],bool header[11]) {
 for(int i=0;i<11;i++){
  text[i]=mentatFixtureRows ? "Row" : "";
  header[i]=!mentatFixtureRows;
 }
 return 0;
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
	/* GM_PICKHOUSE transition from in-game (SELECTIONTYPE_STRUCTURE) must never duplicate SCREEN_0 onto the bottom screen. */
	g_selectionType=SELECTIONTYPE_STRUCTURE;
	memset(screens[0], 77, sizeof(screens[0]));
	assert(!GUI_3DS_IsDialog());
	assert(GUI_3DS_IsCutsceneOrBriefing());
	GUI_3DS_RenderTopScreen(); GUI_3DS_RenderBottomScreen();
	assert(g_topScreenBuffer[20*400+40]==77);
	assert(g_bottomScreenBuffer[20*320+40]==0 && g_bottomScreenBuffer[120*320+160]==0);

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
	g_table_unitInfo[0].movementType=MOVEMENT_FOOT;
	unitOnScreen=true;
	GUI_3DS_RenderTopScreen(); GUI_3DS_RenderBottomScreen();
	assert(g_topScreenBuffer[111*400+191]==TC_WHITE);
	assert(g_topScreenBuffer[107*400+192]==TC_GREEN);
	assert(g_topScreenUIMask[107*400+192]==TACTICAL_UI_MASK);
	unitOnScreen=false;
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
    assert(g_bottomScreenUIMask[40*320+170]==0);
    assert(g_bottomScreenBuffer[40*320+170]==0xE2);
    assert(g_bottomScreenBuffer[230*320+250]==screens[0][129*320+270]);
    assert(g_bottomScreenBuffer[239*320+250]==screens[0][135*320+270]); /* Trim reaches the bottom edge. */
    assert(g_bottomScreenUIMask[65*320+185]==0); /* Original portrait palette. */
    assert(g_bottomScreenBuffer[10*320+10]==screens[0][10*320+10]); /* Header unchanged. */
    assert(g_bottomScreenBuffer[100*320+160]==screens[0][73*320+247]); /* Full repeat in the widened divider. */
    assert(g_bottomScreenBuffer[239*320+160]==screens[0][199*320+247]);
    assert(s_panelHitCount==4 && s_panelHits[1].kind==HIT_SMART_ORDER);
    assert(s_panelHits[1].y==118 && s_panelHits[1].h==50);
    Preview("figma-unit");
    GUI_3DS_HandleBottomTouch(250,140); assert(smartOrders==1);
    GUI_3DS_HandleBottomTouch(250,182); assert(executedAction==ACTION_RETREAT);
    GUI_3DS_HandleBottomTouch(250,208); assert(executedAction==ACTION_GUARD);
    g_table_unitInfo[0].o.actionsPlayer[2]=ACTION_DESTRUCT;
    GUI_3DS_RenderBottomScreen();
    GUI_3DS_HandleBottomTouch(250,182);
    assert(executedAction==ACTION_DESTRUCT);
    g_table_unitInfo[0].o.actionsPlayer[2]=ACTION_RETREAT;
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
    /* Bitmap labels have identical pixels regardless of the loaded game font. */
    g_fontNew8p=&font;
    BattleButton(174,170,142,24,"REPAIR (R)",false,false);
    uint8 labelSnapshot[320*240];
    memcpy(labelSnapshot,g_bottomScreenBuffer,sizeof(labelSnapshot));
    g_fontNew8p=NULL;
    BattleButton(174,170,142,24,"REPAIR (R)",false,false);
    assert(memcmp(labelSnapshot,g_bottomScreenBuffer,sizeof(labelSnapshot))==0);
    int firstInk = 194, lastInk = 170;
    for (int y=172;y<192;y++) for (int x=180;x<310;x++) {
        uint8 pixel = g_bottomScreenBuffer[y*320+x];
        if (pixel == TC_WHITE || pixel == TC_BLACK) {
            firstInk = min(firstInk, y);
            lastInk = max(lastInk, y);
        }
    }
    assert(abs(firstInk + lastInk - (2 * 170 + 23)) <= 1);
    /* Every native glyph bit is preserved, including the two-pixel bold stems. */
    GUI_3DS_FillRect(g_bottomScreenBuffer,320,0,0,319,239,TC_FACE);
    Cairo_DrawText(g_bottomScreenBuffer,320,100,40,"A",TC_WHITE,CAIRO_NO_SHADOW);
    for (int row=0;row<14;row++) for (int col=0;col<8;col++)
        assert(g_bottomScreenBuffer[(30+row)*320+96+col] ==
               ((s_terminusRows['A'-32][row] & (0x80u >> col)) ? TC_WHITE : TC_FACE));
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
    assert(g_bottomScreenBuffer[66*320+178]==TC_WHITE);
    assert(g_bottomScreenBuffer[66*320+199]==TC_FACE); /* Open between the corners. */
    assert(g_bottomScreenBuffer[80*320+178]==TC_FACE);
    GUI_3DS_HandleBottomTouch(235,80); assert(individualSelection==&squad[1]);
    g_selectedUnitsCount=22; GUI_3DS_RenderBottomScreen(); assert(s_panelHitCount==11);
    assert(g_bottomScreenBuffer[60*320+178]==TC_BLACK && g_bottomScreenBuffer[64*320+178]==TC_BLACK); /* Bordered health bar above corners. */
    assert(g_bottomScreenBuffer[61*320+179]==TC_RED && g_bottomScreenBuffer[65*320+178]==TC_FACE); /* Per-unit health + 1px gap. */
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
    /* Radial fractional cursor movement preserves consistent speed across angles and speeds. */
    g_gameMode=GM_NORMAL;g_selectionType=SELECTIONTYPE_UNIT;
    Input_3DS_Init(); g_cursor3DS_X=200;g_cursor3DS_Y=120;test_circle.dx=30;test_circle.dy=0;
    for(int i=0;i<60;i++) Input_3DS_Poll();
    assert(g_cursor3DS_X>=230 && g_cursor3DS_X<=248 && g_cursor3DS_Y==120);
    test_circle.dx=156;int oldX=g_cursor3DS_X;Input_3DS_Poll();assert(g_cursor3DS_X-oldX>=5 && g_cursor3DS_X-oldX<=6);
    g_gameConfig.cursorSpeed=0;oldX=g_cursor3DS_X;Input_3DS_Poll();int slowDelta=g_cursor3DS_X-oldX;assert(slowDelta>=2 && slowDelta<=4);
    g_gameConfig.cursorSpeed=4;oldX=g_cursor3DS_X;Input_3DS_Poll();int fastDelta=g_cursor3DS_X-oldX;assert(fastDelta>=8 && fastDelta<=10);
    /* Diagonal speed matches cardinal Euclidean speed without sqrt(2) surge, and shallow angles keep Y motion. */
    Input_3DS_Init(); g_gameConfig.cursorSpeed=2; g_cursor3DS_X=200; g_cursor3DS_Y=120;
    test_circle.dx=110; test_circle.dy=110;
    for(int i=0;i<20;i++) Input_3DS_Poll();
    int diagDX=g_cursor3DS_X-200, diagDY=120-g_cursor3DS_Y;
    assert(diagDX>=74 && diagDX<=82 && diagDY>=74 && diagDY<=82);
    Input_3DS_Init(); g_cursor3DS_X=200; g_cursor3DS_Y=120;
    test_circle.dx=50; test_circle.dy=10;
    for(int i=0;i<20;i++) Input_3DS_Poll();
    assert(g_cursor3DS_X>220 && g_cursor3DS_Y<120);
    /* Radial deadzone blocks drift even diagonally, and neutral stops cleanly. */
    g_cursor3DS_X=200; g_cursor3DS_Y=120; test_circle.dx=10; test_circle.dy=10;
    for(int i=0;i<10;i++) Input_3DS_Poll();
    assert(g_cursor3DS_X==200 && g_cursor3DS_Y==120);
    test_circle.dx=0;test_circle.dy=0;oldX=g_cursor3DS_X;
    for(int i=0;i<10;i++) Input_3DS_Poll();assert(g_cursor3DS_X==oldX && g_cursor3DS_Y==120);
    /* Holding D-Pad scrolls battle map quickly (~40 tiles/sec at Normal) and honors scrollSpeed presets without forceRedraw. */
    g_viewport_forceRedraw=false;
    g_gameConfig.scrollSpeed=2;
    g_viewportPosition=Tile_PackXY(10,10); g_cursor3DS_X=200; g_cursor3DS_Y=120;
    test_keys_held=KEY_DRIGHT;
    for(int i=0;i<15;i++) Input_3DS_Poll();
    int normalPan = Tile_GetPackedX(g_viewportPosition) - 10;
    assert(normalPan>=7 && !g_viewport_forceRedraw);
    test_keys_held=0; Input_3DS_Poll();
    g_gameConfig.scrollSpeed=0; g_viewportPosition=Tile_PackXY(10,10); test_keys_held=KEY_DRIGHT;
    for(int i=0;i<15;i++) Input_3DS_Poll();
    int slowPan = Tile_GetPackedX(g_viewportPosition) - 10;
    test_keys_held=0; Input_3DS_Poll();
    g_gameConfig.scrollSpeed=4; g_viewportPosition=Tile_PackXY(10,10); test_keys_held=KEY_DRIGHT;
    for(int i=0;i<15;i++) Input_3DS_Poll();
    int fastPan = Tile_GetPackedX(g_viewportPosition) - 10;
    assert(slowPan < normalPan && normalPan < fastPan);
    test_keys_held=0; Input_3DS_Poll();
    /* Screen-edge scrolling also scales smoothly with scrollSpeed presets. */
    g_gameConfig.scrollSpeed=0; g_viewportPosition=Tile_PackXY(10,10); g_cursor3DS_X=TOP_WIDTH-1; g_cursor3DS_Y=120;
    for(int i=0;i<24;i++) Input_3DS_Poll();
    int slowEdge = Tile_GetPackedX(g_viewportPosition) - 10;
    g_cursor3DS_X=200; Input_3DS_Poll();
    g_gameConfig.scrollSpeed=4; g_viewportPosition=Tile_PackXY(10,10); g_cursor3DS_X=TOP_WIDTH-1;
    for(int i=0;i<24;i++) Input_3DS_Poll();
    int fastEdge = Tile_GetPackedX(g_viewportPosition) - 10;
    assert(slowEdge >= 1 && fastEdge > slowEdge * 2);
    g_gameConfig.scrollSpeed=2; g_cursor3DS_X=200; Input_3DS_Poll();
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
    GUI_3DS_SetResultsButtons(resultButtons, false);
    GUI_3DS_RenderBottomScreen();
    assert(g_bottomScreenBuffer[210*320+50]==0); /* No Clear list button on bottom screen. */
    GUI_3DS_MenuInput(0,0,true);assert(GUI_3DS_TakeDialogEvent(resultButtons)==0x8065);
    assert(GUI_3DS_CancelMenu());assert(GUI_3DS_TakeDialogEvent(resultButtons)==0x8065);
    GUI_3DS_HandleBottomTouch(120,120);assert(GUI_3DS_TakeDialogEvent(resultButtons)==0x8065);
    GUI_3DS_SetResultsButtons(NULL, false);GUI_3DS_SetResultsActive(false);

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
    uint8 topPixel=g_topScreenBuffer[20*400+100],topMask=g_topScreenUIMask[20*400+100];

	uint8 bottomPixel = g_bottomScreenBuffer[164*320+40];
	uint8 bottomMask = g_bottomScreenUIMask[164*320+40];
	for (int row = 0; row < 200; row++) memset(screens[2] + row * 320, row + 1, 320);
	GUI_3DS_BeginMessageEx("Mission completed", 1);
	assert(!GUI_3DS_CustomMenuActive());
	GUI_3DS_RenderTopScreen(); GUI_3DS_RenderBottomScreen();
	assert(g_topScreenBuffer[20*400+100]==topPixel && g_topScreenUIMask[20*400+100]==topMask);
	assert(g_topScreenUIMask[40*400+100] == POPUP_UI_MASK);
	assert(g_bottomScreenBuffer[164*320+40] == bottomPixel && g_bottomScreenUIMask[164*320+40] == bottomMask);
	for (int row = 0; row < 200; row++) {
		for (int col = 0; col < 320; col++) assert(screens[2][row * 320 + col] == row + 1);
	}
	char wrapped[8][128];
	const char *remaining;
	assert(Cairo_WrapText("\xC4", 244, wrapped, 8, &remaining) == 1 && !*remaining);
	assert(Cairo_WrapText("Ą–Ž", 244, wrapped, 8, &remaining) == 1 && strcmp(wrapped[0], "Ą–Ž") == 0);
	assert(Cairo_WrapText("abcd efgh", 32, wrapped, 8, &remaining) == 2);
	assert(strcmp(wrapped[0], "abcd") == 0 && strcmp(wrapped[1], "efgh") == 0);
	assert(Cairo_WrapText("first\r\nsecond", 244, wrapped, 1, &remaining) == 1 && strcmp(remaining, "second") == 0);

	/* Overflowing text moves upward and clips before reaching the portrait or bevel. */
	GUI_3DS_EndMessage();
	GUI_3DS_BeginMessageEx("Construction Facility: All structures are built by the construction facility. "
		"This longer message scrolls without covering the portrait or the frame. "
		"Check the final words.", 1);
	GUI_3DS_RenderTopScreen();
	static uint8 firstPopup[400 * 240];
	memcpy(firstPopup, g_topScreenBuffer, sizeof(firstPopup));
	assert(firstPopup[105*400+326] == POPUP_WHITE); /* Scroll thumb starts at the top. */
	g_timerGUI += 600;
	GUI_3DS_RenderTopScreen();
	assert(memcmp(firstPopup, g_topScreenBuffer, 105 * 400) == 0);
	assert(memcmp(firstPopup + 105*400, g_topScreenBuffer + 105*400, 85*400) != 0);
	assert(g_topScreenBuffer[105*400+326] == POPUP_BORDER_DARK);
	for (int y = 44; y < 196; y++) {
		assert(g_topScreenBuffer[y*400+66] == POPUP_BORDER_DARK);
		assert(g_topScreenBuffer[y*400+333] == POPUP_WHITE);
	}
	GUI_3DS_FillRect(g_bottomScreenBuffer,320,400,0,450,30,1); /* Offscreen must be harmless. */
	GUI_3DS_DrawText(g_bottomScreenBuffer,320,0,0,"\xC4",1); /* Incomplete UTF-8 must not read past the string. */

	/* Mouse cursor arrow must be a crisp 11x18 classic RTS pointer with bevel shading and target reticle. */
	GUI_3DS_EndMessage();
	g_selectionType = SELECTIONTYPE_UNIT;
	g_cursor3DS_X = 200;
	g_cursor3DS_Y = 120;
	GUI_3DS_RenderTopScreen();
	assert(g_topScreenBuffer[120 * 400 + 200] == 0 && g_topScreenUIMask[120 * 400 + 200] == 2); /* Tip outline */
	assert(g_topScreenBuffer[122 * 400 + 201] == 254 && g_topScreenUIMask[122 * 400 + 201] == 2); /* Lit white body */
	assert(g_topScreenBuffer[124 * 400 + 203] == 216 && g_topScreenUIMask[124 * 400 + 203] == 2); /* Ivory bevel */
	assert(g_topScreenBuffer[130 * 400 + 207] == 15 && g_topScreenUIMask[130 * 400 + 207] == 2); /* Gold underside bevel */
	/* 2:1 stem rows 11..16 */
	assert(g_topScreenBuffer[(120 + 11) * 400 + (200 + 5)] == 254);
	assert(g_topScreenBuffer[(120 + 11) * 400 + (200 + 6)] == 216);
	assert(g_topScreenBuffer[(120 + 13) * 400 + (200 + 6)] == 254);
	assert(g_topScreenBuffer[(120 + 13) * 400 + (200 + 7)] == 216);
	assert(g_topScreenBuffer[(120 + 15) * 400 + (200 + 7)] == 254);
	assert(g_topScreenBuffer[(120 + 15) * 400 + (200 + 8)] == 216);
	/* Rounded black cap at row 17 */
	assert(g_topScreenBuffer[(120 + 17) * 400 + (200 + 7)] == 0);
	assert(g_topScreenBuffer[(120 + 17) * 400 + (200 + 8)] == 0);
	/* Classic circular target reticle: white over ground/friendly, red over visible enemy unit or structure. */
	{
		uint16 oldVP = g_viewportPosition;
		HouseType oldHouse = g_playerHouseID;
		g_playerHouseID = HOUSE_ATREIDES;
		g_viewportPosition = Tile_PackXY(33, 35); /* (200,120) -> tile (40,40) == radarTile */
		radarUnit = NULL;
		radarStructure = NULL;
		g_unitHouseMissile = NULL;

		g_selectionType = SELECTIONTYPE_TARGET;
		GUI_3DS_RenderTopScreen();
		assert(g_topScreenBuffer[120 * 400 + 200] == 254 && g_topScreenUIMask[120 * 400 + 200] == 2); /* Center dot */
		assert(g_topScreenBuffer[121 * 400 + 201] == 0 && g_topScreenUIMask[121 * 400 + 201] == 2);   /* Drop shadow */
		assert(g_topScreenBuffer[113 * 400 + 200] == 254 && g_topScreenUIMask[113 * 400 + 200] == 2); /* Top ring */
		assert(g_topScreenBuffer[115 * 400 + 200] == 254 && g_topScreenUIMask[115 * 400 + 200] == 2); /* Top inward tick */
		assert(g_topScreenBuffer[120 * 400 + 195] == 254 && g_topScreenUIMask[120 * 400 + 195] == 2); /* Left inward tick */
		assert(g_topScreenBuffer[127 * 400 + 200] == 254 && g_topScreenUIMask[127 * 400 + 200] == 2); /* Bottom ring */

		/* Friendly unit selected shows white circular target reticle over ground. */
		Unit friendlySel = {0};
		friendlySel.o.flags.s.allocated = true;
		friendlySel.o.houseID = g_playerHouseID;
		g_selectedUnits[0] = &friendlySel;
		g_unitSelected = &friendlySel;
		g_selectedUnitsCount = 1;
		g_selectionType = SELECTIONTYPE_UNIT;
		GUI_3DS_RenderTopScreen();
		assert(g_topScreenBuffer[120 * 400 + 200] == 254);
		assert(g_topScreenBuffer[113 * 400 + 200] == 254);

		/* Hovering visible enemy unit turns target reticle red (8). */
		Unit enemyTarget = {0};
		enemyTarget.o.flags.s.allocated = true;
		enemyTarget.o.houseID = HOUSE_HARKONNEN;
		enemyTarget.o.seenByHouses = (uint8)(1 << g_playerHouseID);
		radarUnit = &enemyTarget;
		GUI_3DS_RenderTopScreen();
		assert(g_topScreenBuffer[120 * 400 + 200] == 8 && g_topScreenUIMask[120 * 400 + 200] == 2);
		assert(g_topScreenBuffer[113 * 400 + 200] == 8);
		assert(g_topScreenBuffer[121 * 400 + 201] == 0);

		/* Unseen enemy unit under shroud stays white (no cheat probing). */
		enemyTarget.o.seenByHouses = 0;
		GUI_3DS_RenderTopScreen();
		assert(g_topScreenBuffer[120 * 400 + 200] == 254);

		/* Hovering visible enemy structure turns target reticle red (8); friendly stays white. */
		radarUnit = NULL;
		Structure enemyBldg = {0};
		enemyBldg.o.flags.s.allocated = true;
		enemyBldg.o.houseID = HOUSE_HARKONNEN;
		enemyBldg.o.seenByHouses = (uint8)(1 << g_playerHouseID);
		radarStructure = &enemyBldg;
		GUI_3DS_RenderTopScreen();
		assert(g_topScreenBuffer[120 * 400 + 200] == 8);
		enemyBldg.o.houseID = g_playerHouseID;
		GUI_3DS_RenderTopScreen();
		assert(g_topScreenBuffer[120 * 400 + 200] == 254);
		radarStructure = NULL;

		/* Active box selection or inspecting an enemy unit falls back to regular pointer arrow. */
		g_boxSelectActive = true;
		g_boxSelectStartX = 180; g_boxSelectStartY = 100;
		g_boxSelectCurrX = 200;  g_boxSelectCurrY = 120;
		GUI_3DS_RenderTopScreen();
		assert(g_topScreenBuffer[120 * 400 + 200] == 0);
		assert(g_topScreenBuffer[122 * 400 + 201] == 254);
		g_boxSelectActive = false;

		enemyTarget.o.seenByHouses = (uint8)(1 << g_playerHouseID);
		g_unitSelected = &enemyTarget;
		g_selectedUnitsCount = 0;
		GUI_3DS_RenderTopScreen();
		assert(g_topScreenBuffer[120 * 400 + 200] == 0);
		assert(g_topScreenBuffer[122 * 400 + 201] == 254);

		g_unitSelected = NULL;
		g_selectedUnits[0] = NULL;
		g_selectedUnitsCount = 0;
		g_selectionType = SELECTIONTYPE_UNIT;
		g_viewportPosition = oldVP;
		g_playerHouseID = oldHouse;
	}


	/* Touching Back must retain the selected production item. */
	g_factoryWindowResult = FACTORY_CONTINUE;
	g_factoryWindowTotal = 4;
	s_factoryFocus = 2;
	g_factoryWindowBase = 0;
	g_factoryWindowSelected = 2;
	GUI_3DS_HandleBottomTouch(220, 215);
	assert(GUI_3DS_GetFactoryFocus() == 2);
	g_factoryWindowResult = FACTORY_RESUME;

	/* Strategic cursor uses the same transform as the rendered map. */
	{
		static uint8 regions[304 * 120];
		g_fileRgnclkCPS = regions;
		memset(screens[0], 77, sizeof(screens[0]));
		GUI_3DS_StrategicBegin();
		GUI_3DS_RenderTopScreen();
		assert(g_topScreenUIMask[100 * 400 + 200] == 0);
		GUI_3DS_StrategicSelecting((1u << 5) | (1u << 7));
		GUI_3DS_RenderTopScreen();
		assert(g_topScreenUIMask[100 * 400 + 200] == 2);
		int row = 100 * 5 / 6 - 24;
		regions[row * 304 + 152] = 6;
		regions[row * 304 + 153] = 7;
		test_keys_down = KEY_A;
		test_keys_held = 0;
		test_circle = (circlePosition){0, 0};
		Input_3DS_Poll();
		assert(GUI_3DS_StrategicTakeRegion() == 0); /* Unavailable territory. */
		test_keys_held = KEY_DRIGHT;
		Input_3DS_Poll();
		assert(GUI_3DS_StrategicTakeRegion() == 7);
		assert(GUI_3DS_StrategicTakeRegion() == 0);
		GUI_3DS_StrategicMoveCursor(-400, -240);
		regions[0] = 5;
		GUI_3DS_StrategicConfirm();
		assert(GUI_3DS_StrategicTakeRegion() == 5);
		GUI_3DS_StrategicSelecting((1u << 5) | (1u << 7));
		GUI_3DS_StrategicAddArrow(5, 152, 75);
		GUI_3DS_StrategicConfirm(); /* The arrow overlaps unavailable region 6. */
		assert(GUI_3DS_StrategicTakeRegion() == 5);
		GUI_3DS_StrategicEnd();
		assert(!GUI_3DS_StrategicActive());
	}

	/* Top-screen toast appears during durationTicks and clears afterward. */
	g_gameMode = GM_NORMAL;
	g_selectionType = SELECTIONTYPE_STRUCTURE;
	memset(screens[0], 42, sizeof(screens[0]));
	g_timerGUI = 500;
	GUI_3DS_ShowTopToast("Game Saved", 120);
	GUI_3DS_RenderTopScreen();
	assert(g_topScreenUIMask[10 * 400 + 200] == 2);
	assert(g_topScreenBuffer[10 * 400 + 200] == 27);
	g_timerGUI = 620;
	GUI_3DS_RenderTopScreen();
	assert(g_topScreenUIMask[10 * 400 + 200] == 0);

	/* Mentat detail hides Exit sprites (377/378), switches Next(A)/Back(B), and list fills screen without footer buttons. */
	{
		Widget exitBtn = {0};
		exitBtn.index = 1;
		exitBtn.drawParameterNormal.spriteID = 377;
		assert(GUI_3DS_IsMentatButton(&exitBtn));
		exitBtn.drawParameterNormal.spriteID = 378;
		assert(GUI_3DS_IsMentatButton(&exitBtn));
		exitBtn.drawParameterNormal.spriteID = 377;

		g_gameMode = GM_NORMAL;
		GUI_3DS_SetMentatButtons(&exitBtn);
		GUI_3DS_SetMentatHasMore(true);
		GUI_3DS_RenderBottomScreen();
		GUI_3DS_HandleBottomTouch(50, 180);
		assert(GUI_3DS_MentatEvent(0) == 0x8001);
		GUI_3DS_HandleBottomTouch(220, 180);
		assert(GUI_3DS_MentatEvent(0) == 0x8002);
		GUI_3DS_SetMentatHasMore(false);
		GUI_3DS_HandleBottomTouch(50, 180);
		assert(GUI_3DS_MentatEvent(0) == 0x8002);
		GUI_3DS_SetMentatButtons(NULL);

		mentatFixtureRows = true;
		g_3dsMentatHelpActive = true;
		g_3dsMentatExit = false;
		g_3dsMentatScroll = 0;
		g_3dsMentatTapRow = -1;
		GUI_3DS_RenderBottomScreen();
		GUI_3DS_HandleBottomTouch(50, 220); /* Bottom row 10 sits cleanly where EXIT/UP/DOWN used to be. */
		assert(!g_3dsMentatExit && g_3dsMentatScroll == 0 && g_3dsMentatTapRow == 10);
		g_3dsMentatHelpActive = false;
		g_3dsMentatTapRow = -1;
		mentatFixtureRows = false;
	}

	/* Palace lower panel and Death Hand target header show the house special weapon portrait. */
	{
		uint8 weaponFrame[SCREEN_WIDTH * 112];
		for (int i = 0; i < 3; i++) {
			memset(weaponFrame, 91 + i, sizeof(weaponFrame));
			GUI_3DS_CacheSpecialPortrait((uint8)i, weaponFrame, 184, 112);
		}
		assert(GUI_3DS_AreSpecialPortraitsValid());
		g_table_structureInfo[STRUCTURE_PALACE].o.spriteID = 12;
		sprites[12] = (uint8 *)weaponFrame; /* Non-NULL dummy sprite (stub renders 113). */
		sprites[30] = sprites[94] = sprites[96] = (uint8 *)weaponFrame;
		Structure palace = {0};
		palace.o.flags.s.allocated = true;
		palace.o.type = STRUCTURE_PALACE;
		palace.o.hitpoints = 100;
		g_table_structureInfo[STRUCTURE_PALACE].o.hitpoints = 100;
		selectedStructure = &palace;
		g_selectionType = SELECTIONTYPE_STRUCTURE;

		g_playerHouseID = HOUSE_HARKONNEN;
		palace.o.houseID = HOUSE_HARKONNEN;
		GUI_3DS_RenderBottomScreen();
		assert(g_bottomScreenBuffer[175 * BOTTOM_WIDTH + 213] == 91 && g_bottomScreenUIMask[175 * BOTTOM_WIDTH + 213] == 0);

		g_playerHouseID = HOUSE_ATREIDES;
		palace.o.houseID = HOUSE_ATREIDES;
		GUI_3DS_RenderBottomScreen();
		assert(g_bottomScreenBuffer[175 * BOTTOM_WIDTH + 213] == 92 && g_bottomScreenUIMask[175 * BOTTOM_WIDTH + 213] == 0);

		g_playerHouseID = HOUSE_ORDOS;
		palace.o.houseID = HOUSE_ORDOS;
		GUI_3DS_RenderBottomScreen();
		assert(g_bottomScreenBuffer[175 * BOTTOM_WIDTH + 213] == 93 && g_bottomScreenUIMask[175 * BOTTOM_WIDTH + 213] == 0);

		Unit missile = {0};
		g_unitHouseMissile = &missile;
		g_selectionType = SELECTIONTYPE_TARGET;
		GUI_3DS_RenderBottomScreen();
		assert(g_bottomScreenBuffer[87 * BOTTOM_WIDTH + 212] == 91 && g_bottomScreenUIMask[87 * BOTTOM_WIDTH + 212] == 0);
		g_unitHouseMissile = NULL;
		selectedStructure = NULL;
	}

	/* Confirmation dialogs (Quit, Restart) and Options/Settings center both heading and buttons on 320x240. */
	{
		Widget yn[2] = {0};
		yn[0].index = 30; yn[0].stringID = STR_YES; yn[0].width = 72; yn[0].height = 15; yn[0].next = &yn[1];
		yn[1].index = 31; yn[1].stringID = STR_NO;  yn[1].width = 72; yn[1].height = 15;
		WindowDesc yesNoDesc = {0};
		yesNoDesc.stringID = STR_ARE_YOU_SURE_YOU_WANT_TO_QUIT_PLAYING;
		g_widgetLinkedListTail = yn;
		GUI_3DS_SetWindow(&yesNoDesc);
		GUI_3DS_RenderBottomScreen();
		/* Centered heading (top ~73..87) and centered Yes/No buttons (x=50..270, y=101..166). */
		int minY = 240, maxY = -1, btnMinX = 320, btnMaxX = -1;
		for (int y = 0; y < 240; y++) {
			for (int x = 0; x < 320; x++) {
				if (g_bottomScreenBuffer[y * 320 + x] != 0) {
					if (y < minY) minY = y;
					if (y > maxY) maxY = y;
					if (y >= 101 && y <= 166) {
						if (x < btnMinX) btnMinX = x;
						if (x > btnMaxX) btnMaxX = x;
					}
				}
			}
		}
		assert(abs((minY) - (239 - maxY)) <= 2);
		assert(btnMinX == 51 && btnMaxX == 268);
		s_dialogFocus = 0;
		GUI_3DS_HandleBottomTouch(160, 145); /* Tapping the centered No button selects it cleanly. */
		assert(s_dialogChosen == &yn[1]);
		assert(GUI_3DS_TakeDialogEvent(yn) == 0x801F);
		g_widgetLinkedListTail = NULL;
		GUI_3DS_SetWindow(NULL);
	}

	/* Placement mode centers the building portrait without OK/INVALID text, and disabled buttons render shadowless grey text. */
	{
		static uint8 dummySprite[64] = {1};
		sprites[15] = dummySprite;
		g_table_structureInfo[STRUCTURE_SLAB_1x1].o.spriteID = 15;
		g_gameMode = GM_NORMAL;
		g_selectionType = SELECTIONTYPE_PLACE;
		g_structureActiveType = STRUCTURE_SLAB_1x1;
		g_selectionState = 0; /* Disabled PLACE (A) */
		GUI_3DS_RenderBottomScreen();
		/* Portrait at x=210..278 -> sprite rendered inside (x=212,y=87) with no OK/INVALID text at x=282..310. */
		assert(g_bottomScreenBuffer[87 * BOTTOM_WIDTH + 212] == 113 && g_bottomScreenUIMask[87 * BOTTOM_WIDTH + 212] == 0);
		for (int py = 65; py <= 105; py++) {
			for (int px = 282; px <= 310; px++) assert(g_bottomScreenBuffer[py * BOTTOM_WIDTH + px] == TC_FACE);
		}
		/* Inside disabled PLACE (A) inner face (x=178..310, y=175..190): TC_DISABLED text, no TC_BLACK shadow. */
		int disabledPixels = 0;
		for (int py = 175; py <= 190; py++) {
			for (int px = 178; px <= 310; px++) {
				uint8 col = g_bottomScreenBuffer[py * BOTTOM_WIDTH + px];
				assert(col != TC_BLACK);
				if (col == TC_DISABLED) disabledPixels++;
			}
		}
		assert(disabledPixels > 0);
		g_selectionType = SELECTIONTYPE_STRUCTURE;
	}

	/* Non-producing structures (e.g., Windtrap/Silo/Turret) omit the lower SELECT ALL (X) box; enemy units/structures show DMG only and enemy unit marker omits top HP bar. */
	{
		static uint8 dummyArt[64] = {1};
		sprites[16] = dummyArt;
		g_table_structureInfo[STRUCTURE_WINDTRAP].o.spriteID = 16;
		g_table_structureInfo[STRUCTURE_WINDTRAP].o.hitpoints = 100;
		g_table_structureInfo[STRUCTURE_WINDTRAP].o.flags.factory = false;
		Structure wt = {0};
		wt.o.flags.s.allocated = true;
		wt.o.type = STRUCTURE_WINDTRAP;
		wt.o.houseID = g_playerHouseID = HOUSE_ATREIDES;
		wt.o.hitpoints = 100;
		selectedStructure = &wt;
		g_unitSelected = NULL;
		g_selectedUnitsCount = 0;
		g_selectionType = SELECTIONTYPE_STRUCTURE;
		GUI_3DS_RenderBottomScreen();
		/* Lower button area (y=150..215, x=176..310) must stay plain TC_FACE (no SELECT ALL (X) button). */
		for (int py = 150; py <= 215; py++) {
			for (int px = 176; px <= 310; px++) assert(g_bottomScreenBuffer[py * BOTTOM_WIDTH + px] == TC_FACE);
		}
		/* Enemy structure shows DMG bar (x=258..309, y=63..78) and leaves both button areas (y=120..215) empty. */
		wt.o.houseID = HOUSE_HARKONNEN;
		GUI_3DS_RenderBottomScreen();
		assert(g_bottomScreenBuffer[70 * BOTTOM_WIDTH + 265] == TC_GREEN);
		for (int py = 120; py <= 215; py++) {
			for (int px = 176; px <= 310; px++) assert(g_bottomScreenBuffer[py * BOTTOM_WIDTH + px] == TC_FACE);
		}
		selectedStructure = NULL;

		/* Enemy unit inspection: bottom screen shows DMG bar with no action buttons; top screen draws corner brackets with no HP bar above. */
		Unit enemy = {0};
		enemy.o.flags.s.allocated = true;
		enemy.o.type = UNIT_TANK;
		enemy.o.houseID = HOUSE_HARKONNEN;
		enemy.o.seenByHouses = (uint8)(1 << g_playerHouseID);
		enemy.o.hitpoints = 80;
		g_table_unitInfo[UNIT_TANK].o.hitpoints = 100;
		g_table_unitInfo[UNIT_TANK].o.spriteID = 16;
		g_table_unitInfo[UNIT_TANK].movementType = MOVEMENT_TRACKED;
		g_unitSelected = &enemy;
		g_selectedUnitsCount = 0;
		g_selectionType = SELECTIONTYPE_UNIT;
		unitOnScreen = true; /* Map_IsPositionInViewport -> (120, 80) -> top screen (200, 60) */
		memset(screens[0], 42, sizeof(screens[0]));
		GUI_3DS_RenderTopScreen();
		GUI_3DS_RenderBottomScreen();
		/* Bottom screen DMG bar visible and action button area (y=118..215) empty. */
		assert(g_bottomScreenBuffer[70 * BOTTOM_WIDTH + 265] == TC_GREEN);
		for (int py = 118; py <= 215; py++) {
			for (int px = 176; px <= 310; px++) assert(g_bottomScreenBuffer[py * BOTTOM_WIDTH + px] == TC_FACE);
		}
		/* Top screen: corner bracket at (200 - 13, 120 - 13) = (187, 107) is TC_WHITE, while HP bar row y=102 is untouched battlefield (not TACTICAL_UI_MASK). */
		assert(g_topScreenBuffer[107 * TOP_WIDTH + 187] == TC_WHITE && g_topScreenUIMask[107 * TOP_WIDTH + 187] == TACTICAL_UI_MASK);
		assert(g_topScreenUIMask[102 * TOP_WIDTH + 200] != TACTICAL_UI_MASK);
		/* Friendly unit still draws its top HP bar at y=102. */
		enemy.o.houseID = g_playerHouseID;
		GUI_3DS_RenderTopScreen();
		assert(g_topScreenUIMask[102 * TOP_WIDTH + 200] == TACTICAL_UI_MASK);
		g_unitSelected = NULL;
		unitOnScreen = false;
	}

	GUI_3DS_Uninit();
	puts("3DS UI integration checks passed");
	return 0;
}
