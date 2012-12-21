/*****************************************************************************
 *
 * gui_navbar.h
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
#ifndef _PHASEX_GUI_NAVBAR_H_
#define _PHASEX_GUI_NAVBAR_H_

#include <gtk/gtk.h>


extern GtkObject    *session_adj;
extern GtkWidget    *session_spin;
extern GtkWidget    *session_entry;

extern GtkObject    *program_adj;
extern GtkWidget    *program_spin;
extern GtkWidget    *program_entry;

extern GtkObject    *part_adj;
extern GtkWidget    *part_spin;
extern GtkWidget    *part_entry;

extern GtkWidget    *midi_channel_label;
extern GtkWidget    *midi_channel_event_box;
extern GtkObject    *midi_channel_adj;

extern GtkWidget    *patch_modified_label;
extern GtkWidget    *session_modified_label;

extern GtkWidget    *patch_io_start_spin;

extern int          show_patch_modified;
extern int          show_session_modified;

extern char         *modified_label_text[2];


void create_navbar(GtkWidget *UNUSED(main_window), GtkWidget *parent_vbox);
int midi_channel_label_handle_event(gpointer UNUSED(data1),
                                    gpointer data2,
                                    gpointer UNUSED(data3));
void queue_test_note(GtkWidget *UNUSED(widget), gpointer UNUSED(data));


#endif /* _PHASEX_GUI_NAVBAR_H_ */
