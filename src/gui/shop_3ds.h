/* Fixed colors for the purchase (factory / starport) window overlay.
 * Backgrounds, crests, previews and unit icons keep the game palette (UI mask 0);
 * everything the shop draws on top of them (buttons, selection frames, row tint,
 * text) uses this private palette so it is immune to palette fades and animation. */
#ifndef SHOP_3DS_H
#define SHOP_3DS_H
#define SHOP_UI_MASK 4
enum { SP_BLACK, SP_WHITE, SP_FRAME, SP_BTN_FILL, SP_BTN_HL, SP_BTN_SH, SP_SLOT, SP_TILE_FACE, SP_TEXT_DIM,
       SP_SEL_RED, SP_SEL_BLUE, SP_SEL_GREEN, SP_TINT_RED, SP_TINT_BLUE, SP_TINT_GREEN, SP_COUNT };
/* BGR order. Selection colours per house; tints are the selection colour at 25% over SP_SLOT (#38382C). */
static const unsigned char s_shopBGR[SP_COUNT][3] = {
 {0,0,0},        /* SP_BLACK      text shadow                    */
 {255,255,255},  /* SP_WHITE      text, tile border              */
 {0,35,65},      /* SP_FRAME      #412300 button frame           */
 {0,139,214},    /* SP_BTN_FILL   #D68B00                        */
 {0,178,255},    /* SP_BTN_HL     #FFB200 top/left bevel         */
 {0,102,163},    /* SP_BTN_SH     #A36600 bottom/right bevel     */
 {44,56,56},     /* SP_SLOT       #38382C recessed list area     */
 {142,185,170},  /* SP_TILE_FACE  #AAB98E behind transparent icon pixels */
 {0,92,143},     /* SP_TEXT_DIM   #8F5C00 disabled label         */
 {0,0,255},      /* SP_SEL_RED    Harkonnen  #FF0000             */
 {255,110,40},   /* SP_SEL_BLUE   Atreides   #286EFF             */
 {40,230,0},     /* SP_SEL_GREEN  Ordos      #00E628             */
 {33,42,106},    /* SP_TINT_RED   #6A2A21                        */
 {97,70,52},     /* SP_TINT_BLUE  #344661                        */
 {43,99,42}      /* SP_TINT_GREEN #2A632B                        */
};
#endif
