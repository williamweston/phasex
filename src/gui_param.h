/*****************************************************************************
 *
 * gui_param.h
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
#ifndef _PHASEX_GUI_PARAM_H_
#define _PHASEX_GUI_PARAM_H_

#include <gtk/gtk.h>
#include "param.h"


void update_gui_param(PARAM *param);
void gui_param_midi_update(PARAM *param, int cc_val);
int get_param_widget_val(GtkWidget *widget, PARAM *param);
void on_gui_param_update(GtkWidget *widget, gpointer *data);
void on_param_name_button_press(GtkWidget *widget, GdkEventButton *event);
int on_param_label_event(gpointer data1, gpointer data2);
gboolean on_param_label_focus_change(GtkWidget *UNUSED(widget),
                                     GdkEventFocus *event,
                                     gpointer data);
int on_param_button_event(gpointer data1, gpointer data2, gpointer data3);
void create_param_input(GtkWidget *UNUSED(main_window), GtkWidget *table,
                        guint col, guint row, PARAM *param, gint page_num);
void update_param_sensitivities(void);
void update_param_sensitivity(unsigned int part_num, unsigned int id);
int get_param_sensitivity(unsigned int UNUSED(part_num), unsigned int id);
void update_param_child_sensitivities(unsigned int part_num, unsigned int id);


#endif /* _PHASEX_GUI_PARAM_H_ */
