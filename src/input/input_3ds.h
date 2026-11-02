/** @file src/input/input_3ds.h Nintendo 3DS physical controls and touchscreen input driver. */

#ifndef INPUT_3DS_H
#define INPUT_3DS_H

#include "types.h"

void Input_3DS_Init(void);
void Input_3DS_Uninit(void);
void Input_3DS_Poll(void);

#endif /* INPUT_3DS_H */
