#include <assert.h>
#include "../src/gui/keyboard_3ds.c"
bool g_viewport_forceRedraw;
static int cleared;
void Input_History_Clear(void) {cleared++;}
int main(void)
{
 char text[51]="Old description";
 test_keyboard_text="New save";
 assert(GUI_3DS_InputSaveName(text) && strcmp(text,"New save")==0);
 assert(strcmp(test_keyboard_initial,"Old description")==0 && test_keyboard_limit==50);
 test_keyboard_result=SWKBD_BUTTON_LEFT;test_keyboard_text="Discarded";
 assert(!GUI_3DS_InputSaveName(text) && strcmp(text,"New save")==0);
 test_keyboard_result=SWKBD_BUTTON_NONE;
 assert(!GUI_3DS_InputSaveName(text));
 test_keyboard_result=SWKBD_BUTTON_RIGHT;test_keyboard_text="   ";
 assert(!GUI_3DS_InputSaveName(text) && strcmp(text,"New save")==0);
 char longText[100]; memset(longText,'a',99);longText[99]=0;test_keyboard_text=longText;
 assert(GUI_3DS_InputSaveName(text) && strlen(text)==50);
 assert(cleared==5 && test_keyboard_calls==5 && g_viewport_forceRedraw);
 char name[6]=""; test_keyboard_text="Commander";
 assert(GUI_3DS_InputText(name,5,"Name for the Hall of Fame","OK"));
 assert(strcmp(name,"Comma")==0 && test_keyboard_limit==5);
 char answer[81]=""; test_keyboard_text="Arrakis";
 assert(GUI_3DS_InputText(answer,80,"Enter your answer","OK") && strcmp(answer,"Arrakis")==0 && test_keyboard_limit==80);
 test_keyboard_result=SWKBD_BUTTON_LEFT;
 assert(!GUI_3DS_InputText(name,5,"Name","OK") && strcmp(name,"Comma")==0);
 assert(!GUI_3DS_InputText(name,256,"Name","OK"));
 puts("3DS native save, high-score and security keyboard checks passed");
}
