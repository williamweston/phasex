/*****************************************************************************
 *
 * gui_session.h
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
#ifndef _PHASEX_GUI_SESSION_H_
#define _PHASEX_GUI_SESSION_H_

#include "phasex.h"
#include "patch.h"


extern GtkWidget    *session_load_dialog;
extern GtkWidget    *session_save_dialog;

extern GtkWidget    *session_load_start_spin;
extern GtkWidget    *session_save_start_spin;

extern GtkObject    *session_io_start_adj;

extern unsigned int session_io_start;

extern int          session_load_in_progress;


char *get_session_directory_from_entry(GtkEntry *entry);
int check_session_overwrite(char *directory);
void set_session_io_start(GtkWidget *widget, gpointer UNUSED(data));
void create_session_load_dialog(void);
void run_session_load_dialog(GtkWidget *UNUSED(widget), gpointer UNUSED(data));
void create_session_save_dialog(void);
void run_session_save_as_dialog(GtkWidget *UNUSED(widget), gpointer data);
void on_session_save_activate(GtkWidget *UNUSED(widget), gpointer data);
void select_session(GtkWidget *widget, gpointer data);
void on_save_session(GtkWidget *UNUSED(widget), gpointer data);
void on_load_session(GtkWidget *UNUSED(widget), gpointer data);


#endif /* _PHASEX_GUI_SESSION_H_ */
