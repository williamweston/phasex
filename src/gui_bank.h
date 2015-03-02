/*****************************************************************************
 *
 * gui_bank.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2009 William Weston <william.h.weston@gmail.com>
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
#ifndef _PHASEX_GUI_BANK_H_
#define _PHASEX_GUI_BANK_H_

#include <gtk/gtk.h>


extern int          program_change_request;

extern GtkWidget    *bank_autosave_button;
extern GtkWidget    *bank_warn_button;
extern GtkWidget    *bank_protect_button;

extern GtkWidget    *patch_load_dialog;
extern GtkWidget    *patch_save_dialog;

extern GtkWidget    *patch_load_start_spin;
extern GtkWidget    *patch_save_start_spin;

extern GtkObject    *patch_io_start_adj;

extern unsigned int patch_io_start;

extern char         *modified_label_text[2];


char *get_patch_filename_from_entry(GtkEntry *entry);

int check_patch_overwrite(char *filename);

void set_visible_part(GtkWidget *widget, gpointer data, GtkWidget *widget2);
void switch_part(GtkWidget *widget, gpointer UNUSED(data));

void select_program(GtkWidget *widget, gpointer data);
void save_program(GtkWidget *UNUSED(widget), gpointer data);
void load_program(GtkWidget *UNUSED(widget), gpointer data);

void bank_autosave_activate(GtkWidget *widget, gpointer UNUSED(data));
void bank_warn_activate(GtkWidget *widget, gpointer UNUSED(data));
void bank_protect_activate(GtkWidget *widget, gpointer UNUSED(data));

void set_patch_io_start(GtkWidget *widget, gpointer UNUSED(data));
void create_patch_load_dialog(void);
void run_patch_load_dialog(GtkWidget *UNUSED(widget), gpointer UNUSED(data));
void create_patch_save_dialog(void);
void run_patch_save_as_dialog(GtkWidget *UNUSED(widget), gpointer data);
void on_patch_save_activate(GtkWidget *UNUSED(widget), gpointer data);
void on_patch_reset_activate(GtkWidget *UNUSED(widget), gpointer data);


#endif /* _PHASEX_GUI_BANK_H_ */
