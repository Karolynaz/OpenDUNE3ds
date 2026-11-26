/* Fixed colors and layout for the 3DS top-screen popup dialog (Figma node 165:815). */
#ifndef POPUP_3DS_H
#define POPUP_3DS_H

#define POPUP_UI_MASK 5

#ifdef __NDS__
#define POPUP_WIDTH  236
#define POPUP_HEIGHT 152
#else
#define POPUP_WIDTH  284
#define POPUP_HEIGHT 173
#endif

enum {
	POPUP_BLACK,        /* 0: #000000 text shadow */
	POPUP_BG,           /* 1: #a9a9c2 main background */
	POPUP_BORDER_DARK,  /* 2: #55557c dark purple bevel */
	POPUP_WHITE,        /* 3: #ffffff white bevel and text */
	POPUP_COUNT
};

/* BGR color values for the 3DS display driver */
static const unsigned char s_popupBGR[POPUP_COUNT][3] = {
	{  0,   0,   0}, /* POPUP_BLACK: #000000 */
	{194, 169, 169}, /* POPUP_BG: #a9a9c2 (R=169, G=169, B=194) */
	{124,  85,  85}, /* POPUP_BORDER_DARK: #55557c (R=85, G=85, B=124) */
	{255, 255, 255}  /* POPUP_WHITE: #ffffff */
};

#endif
