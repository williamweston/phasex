/*****************************************************************************
 *
 * gui_menubar.h
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
#ifndef _PHASEX_GUI_MENUBAR_H_
#define _PHASEX_GUI_MENUBAR_H_

#include <gtk/gtk.h>


extern GtkWidget        *main_menubar;

extern GtkCheckMenuItem *menu_item_fullscreen;
extern GtkCheckMenuItem *menu_item_notebook;
extern GtkCheckMenuItem *menu_item_one_page;
extern GtkCheckMenuItem *menu_item_widescreen;
extern GtkCheckMenuItem *menu_item_autoconnect;
extern GtkCheckMenuItem *menu_item_manualconnect;
extern GtkCheckMenuItem *menu_item_stereo_out;
extern GtkCheckMenuItem *menu_item_multi_out;
extern GtkCheckMenuItem *menu_item_autosave;
extern GtkCheckMenuItem *menu_item_warn;
extern GtkCheckMenuItem *menu_item_protect;

extern GtkCheckMenuItem *bank_mem_menu_item[3];

extern GtkCheckMenuItem *menu_item_part[17];


void create_menubar(GtkWidget *main_window, GtkWidget *box);
void on_test_menu_activate(gpointer data, guint action, GtkWidget *widget);
void on_alsa_menu_activate(GtkMenuItem *parent_menu_item, gpointer data);
void on_menu_item_activate(GtkMenuItem *menu_item, gpointer data);


#endif /* _PHASEX_GUI_MENUBAR_H_ */
