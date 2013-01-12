/*****************************************************************************
 *
 * gui_midimap.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2013 William Weston <whw@linuxmail.org>
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
#ifndef _PHASEX_GUI_MIDIMAP_H_
#define _PHASEX_GUI_MIDIMAP_H_

#include <gtk/gtk.h>


extern GtkWidget    *midimap_load_dialog;
extern GtkWidget    *midimap_save_dialog;
extern GtkWidget    *cc_edit_dialog;
extern GtkWidget    *cc_edit_spin;

extern GtkObject    *cc_edit_adj;

extern int          cc_edit_active;
extern int          cc_edit_ignore_midi;
extern int          cc_edit_cc_num;
extern int          cc_edit_param_id;


void update_param_cc_map(GtkWidget *widget, gpointer data);
void update_param_locked(GtkWidget *widget, gpointer data);
void update_param_ignore(GtkWidget *widget, gpointer UNUSED(data));
void close_cc_edit_dialog(GtkWidget *UNUSED(widget), gpointer UNUSED(data));
void create_midimap_load_dialog(void);
void run_midimap_load_dialog(void);
void create_midimap_save_dialog(void);
void run_midimap_save_as_dialog(void);
void on_midimap_save_activate(void);


#endif /* _PHASEX_GUI_MIDIMAP_H_ */
