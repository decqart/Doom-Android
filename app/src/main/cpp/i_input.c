//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include <stdio.h>

#include "doomkeys.h"
#include "doomgeneric.h"
#include "d_event.h"

int vanilla_keyboard_mapping = 1;

void I_GetEvent(void)
{
    event_t event;
    int pressed;
    unsigned char key;

    while (DG_GetKey(&pressed, &key))
    {
        // process event
        if (pressed)
        {
            // data1 has the key pressed, data2 has the character
            // (shift-translated, etc)
            event.type = ev_keydown;
            event.data1 = key;
            event.data2 = key;

            if (event.data1 != 0)
                D_PostEvent(&event);
        }
        else
        {
            event.type = ev_keyup;
            event.data1 = key;

            // data2 is just initialized to zero for ev_keyup.
            // For ev_keydown it's the shifted Unicode character
            // that was typed, but if something wants to detect
            // key releases it should do so based on data1
            // (key ID), not the printable char.
            event.data2 = 0;

            if (event.data1 != 0)
                D_PostEvent(&event);
            break;
        }
    }
}
