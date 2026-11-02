/** Native 3DS save-description input. Called from the main loop, never HID polling. */
#include "types.h"
#include <3ds.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "gui_3ds.h"
#include "../input/input.h"
#include "../opendune.h"

bool GUI_3DS_InputText(char *description, uint16 maxLength, const char *hint, const char *confirm)
{
 SwkbdState keyboard;
 char text[256];
 if (maxLength == 0 || maxLength >= sizeof(text)) return false;
 size_t capacity = maxLength + 1;
 snprintf(text,capacity,"%s",description);
 swkbdInit(&keyboard,SWKBD_TYPE_NORMAL,2,maxLength);
 swkbdSetValidation(&keyboard,SWKBD_NOTEMPTY_NOTBLANK,0,0);
 swkbdSetHintText(&keyboard,hint);
 swkbdSetButton(&keyboard,SWKBD_BUTTON_LEFT,"Cancel",false);
 swkbdSetButton(&keyboard,SWKBD_BUTTON_RIGHT,confirm,true);
 swkbdSetInitialText(&keyboard,text);
 SwkbdButton result=swkbdInputText(&keyboard,text,capacity);
 text[capacity-1]=0;
 Input_History_Clear();
 hidScanInput();
 g_viewport_forceRedraw=true;
 if(result!=SWKBD_BUTTON_RIGHT) return false;
 bool nonblank=false;
 for(const unsigned char *p=(const unsigned char *)text;*p;p++) if(!isspace(*p)) nonblank=true;
 if(!nonblank) return false;
 memcpy(description,text,strlen(text)+1);
 return true;
}

bool GUI_3DS_InputSaveName(char *description)
{
 return GUI_3DS_InputText(description,50,"Saved game description","Save");
}
