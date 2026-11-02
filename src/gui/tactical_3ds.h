/* Fixed colors for the tactical panel; original artwork keeps the game palette. */
#ifndef TACTICAL_3DS_H
#define TACTICAL_3DS_H
#define TACTICAL_UI_MASK 3
enum { TC_BLACK, TC_FACE, TC_LIGHT, TC_SHADOW, TC_TEXT, TC_WHITE,
       TC_ACTIVE, TC_GREEN, TC_YELLOW, TC_RED, TC_DOT, TC_DARK_GREEN, TC_NAVY, TC_GOLD, TC_GOLD_LIGHT, TC_BUTTON_EDGE, TC_COUNT };
static const unsigned char s_tacticalBGR[TC_COUNT][3] = {
 {0,0,0}, {145,190,185}, {197,255,250}, {75,101,101}, {50,69,69}, {255,255,255},
 {76,66,255}, {38,255,0}, {5,255,255}, {0,0,199}, {104,142,142}, {0,173,0}, {43,38,37}, {86,190,230}, {166,233,255}, {90,86,84}
};
#endif
