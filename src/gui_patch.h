/*****************************************************************************
 *
 * gui_patch.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2012 William Weston <whw@linuxmail.org>
 *
 * PHASEX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PHASEX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PHASEX.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/
#ifndef _PHASEX_GUI_PATCH_H_
#define _PHASEX_GUI_PATCH_H_

#include "patch.h"


extern PATCH    gui_patch;
extern PATCH    *gp;

extern PATCH    *visible_patch;
extern PATCH    *pending_visible_patch;


void init_gui_patch(void);
void update_gui_patch_name(void);
void update_gui_session_name(void);
void update_gui_session_number(void);
void update_gui_part_number(void);
void update_gui_program_number(void);
int patch_visible(PATCH *patch);
void update_gui_patch_modified(void);
void update_gui_session_modified(void);
void update_gui_patch_changed(PATCH *patch, int part_switch);
void update_gui_patch(PATCH *patch, int part_switch);


#endif /* _PHASEX_GUI_PATCH_H_ */
