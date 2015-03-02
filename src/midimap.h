/*****************************************************************************
 *
 * patch.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2015 Willaim Weston <william.h.weston@gmail.com>
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
#ifndef _PHASEX_MIDIMAP_H_
#define _PHASEX_MIDIMAP_H_


extern int      ccmatrix[128][16];

extern char     *midimap_filename;
extern int      midimap_modified;


void build_ccmatrix(void);
void set_midi_channel_for_part(unsigned int part_num, int new_channel);
int read_midimap(char *filename);
int save_midimap(char *filename);


#endif /* _PHASEX_MIDIMAP_H_ */
