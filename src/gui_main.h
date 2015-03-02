/*****************************************************************************
 *
 * gui_main.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2007-2015 Willaim Weston <william.h.weston@gmail.com>
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
#ifndef _PHASEX_GTKUI_H_
#define _PHASEX_GTKUI_H_

#include <pthread.h>
#include <gtk/gtk.h>
#include "phasex.h"
#include "gtkknob.h"


#define JUSTIFY_LEFT    0
#define JUSTIFY_RIGHT   1
#define JUSTIFY_CENTER  2


extern pthread_mutex_t  gtkui_ready_mutex;
extern pthread_cond_t   gtkui_ready_cond;

extern GtkFileFilter    *file_filter_all;
extern GtkFileFilter    *file_filter_patches;
extern GtkFileFilter    *file_filter_map;

extern GtkWidget        *main_window;
#ifdef ENABLE_SPLASH_WINDOW
extern GtkWidget        *splash_window;
#endif
extern GtkWidget        *focus_widget;

extern GtkObject        *cc_edit_adj;

extern GtkKnobAnim      *knob_anim;
extern GtkKnobAnim      *detent_knob_anim;

extern int              forced_quit;
extern int              gtkui_restarting;
extern int              gtkui_ready;
extern int              start_gui;


void *gtkui_thread(void *arg);
void start_gtkui_thread(void);

void create_file_filters(void);

void phasex_gtkui_quit(void);
void phasex_gtkui_force_quit(void);

gboolean gui_main_loop_iteration(gpointer data);
void gui_main_loop_stopped(gpointer UNUSED(data));

void handle_window_state_event(GtkWidget *widget, GdkEventWindowState *state);

#ifdef ENABLE_SPLASH_WINDOW
void create_splash_window(void);
#endif
void create_main_window(void);

void restart_gtkui(void);

#if defined(ENABLE_BACKING_STORE) && defined(GDK_WINDOWING_X11)
void widget_set_backing_store(GtkWidget *widget);
void widget_set_backing_store_callback(GtkWidget *widget, void *UNUSED(data));
#else
void widget_set_backing_store(GtkWidget *UNUSED(widget));
#endif

void widget_set_custom_font(GtkWidget *widget, PangoFontDescription *desc);

void table_add_widget(GtkTable *table, GtkWidget *child,
                      guint col, guint row, guint size_x, guint size_y,
                      GtkAttachOptions xoptions, GtkAttachOptions yoptions,
                      guint xpadding, guint ypadding,
                      int justify);


#endif /* _PHASEX_GUI_MAIN_H_ */
