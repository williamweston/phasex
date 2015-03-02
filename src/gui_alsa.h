/*****************************************************************************
 *
 * gui_alsa.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2009 William Weston <william.h.weston@gmail.com>
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
#ifndef _PHASEX_GUI_ALSA_H_
#define _PHASEX_GUI_ALSA_H_

#include <gtk/gtk.h>
#include "phasex.h"


extern GtkMenuItem      *alsa_menu_item;
extern GtkMenuItem      *alsa_pcm_hw_menu_item;
extern GtkMenuItem      *alsa_seq_hw_menu_item;
extern GtkMenuItem      *alsa_seq_sw_menu_item;
extern GtkMenuItem      *alsa_rawmidi_menu_item;


void on_alsa_menu_activate(GtkMenuItem *UNUSED(parent_menu_item), gpointer UNUSED(data));
void on_select_alsa_pcm_hw_device(GtkCheckMenuItem *menu_item, gpointer data);
void on_select_alsa_seq_port(GtkCheckMenuItem *menu_item, gpointer data);
void on_select_alsa_rawmidi_device(GtkCheckMenuItem *menu_item, gpointer data);


#endif /* _PHASEX_GUI_ALSA_H_ */
