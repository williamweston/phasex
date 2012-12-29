/*****************************************************************************
 *
 * bank.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2009 William Weston <whw@linuxmail.org>
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
#ifndef _PHASEX_BANK_H_
#define _PHASEX_BANK_H_

#include <gtk/gtk.h>
#include "phasex.h"
#include "patch.h"


#define BANK_MEM_AUTOSAVE   0
#define BANK_MEM_WARN       1
#define BANK_MEM_PROTECT    2

#define PATCH_BANK_SIZE     1024


extern PATCH        patch_bank[MAX_PARTS][PATCH_BANK_SIZE];
extern PATCH_STATE  state_bank[MAX_PARTS][PATCH_BANK_SIZE];

extern unsigned int visible_sess_num;
extern unsigned int visible_part_num;
extern unsigned int visible_prog_num[MAX_PARTS];


PATCH *get_patch_from_bank(unsigned int part_num, unsigned int prog_num);
PATCH *set_patch_from_bank(unsigned int part_num, unsigned int prog_num);

void init_patch_bank(char *filename);
void load_patch_bank(char *filename);
void save_patch_bank(char *filename);

void load_patch_list(char *patch_list);

unsigned int find_patch(char *name, unsigned int part_num);

char *get_patch_name_from_filename(char *filename);

void midi_select_program(unsigned int part_num, unsigned int prog_num);


#endif /* _PHASEX_BANK_H_ */
