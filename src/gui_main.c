/*****************************************************************************
 *
 * gui_main.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2007-2012 William Weston <whw@linuxmail.org>
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
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <pango/pango.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include "phasex.h"
#include "config.h"
#include "patch.h"
#include "param.h"
#include "midimap.h"
#include "midi_process.h"
#include "alsa_pcm.h"
#include "alsa_seq.h"
#include "rawmidi.h"
#include "jack.h"
#include "gui_main.h"
#include "gui_menubar.h"
#include "gui_navbar.h"
#include "gui_layout.h"
#include "gui_patch.h"
#include "gui_param.h"
#include "gui_midimap.h"
#include "gui_bank.h"
#include "gui_session.h"
#include "gtkknob.h"
#include "bank.h"
#include "session.h"
#include "settings.h"
#include "help.h"
#include "debug.h"

#ifndef WITHOUT_LASH
# include "lash.h"
#endif

#ifdef GDK_WINDOWING_X11
# include <gdk/gdkx.h>
# include <X11/Xlib.h>
#endif


pthread_mutex_t     gtkui_ready_mutex;
pthread_cond_t      gtkui_ready_cond        = PTHREAD_COND_INITIALIZER;

GtkFileFilter       *file_filter_all        = NULL;
GtkFileFilter       *file_filter_patches    = NULL;
GtkFileFilter       *file_filter_map        = NULL;

GtkWidget           *main_window            = NULL;
GtkWidget           *splash_window          = NULL;
GtkWidget           *focus_widget           = NULL;

GtkKnobAnim         *knob_anim              = NULL;
GtkKnobAnim         *detent_knob_anim       = NULL;

int                 alpha_support           = 0;
int                 composite_support       = 0;

GdkPixbuf           *splash_pixbuf          = NULL;

int                 splash_width            = 256;
int                 splash_height           = 256;

int                 forced_quit             = 0;
int                 gtkui_restarting        = 0;
int                 gtkui_ready             = 0;
int                 start_gui               = 0;


/*****************************************************************************
 * gtkui_thread()
 *
 * The entire GTK UI operates in this thread.
 *****************************************************************************/
void *
gtkui_thread(void *UNUSED(arg))
{
	static int  once = 1;
	GdkWindow   *window;

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	gtk_set_locale();
	set_theme_env();
	gtk_init(NULL, NULL);
	if (!gtk_init_check(NULL, NULL)) {
		PHASEX_ERROR("gtk_init_check() failure!\n");
	}

	/* create splash screen and sit in gtk_main until the rest is ready */
	create_splash_window();

	/* iterate over pending events to display the splash window */
	while (gtk_events_pending()) {
		gtk_main_iteration_do(FALSE);
		usleep(1000);
	}
	while (!gtk_main_iteration_do(FALSE) || !start_gui) {
		usleep(125000);
	}

	PHASEX_DEBUG(DEBUG_CLASS_GUI, "Splash Done!\n");

	/* continue on with GUI setup */
	file_filter_all         = NULL;
	file_filter_patches     = NULL;
	file_filter_map         = NULL;
	main_window             = NULL;
	main_menubar            = NULL;
	midimap_load_dialog     = NULL;
	midimap_save_dialog     = NULL;
	cc_edit_dialog          = NULL;
	cc_edit_spin            = NULL;

	cc_edit_adj             = NULL;

	if (knob_anim != NULL) {
		gtk_knob_animation_destroy(knob_anim);
		knob_anim = NULL;
	}
	if (detent_knob_anim != NULL) {
		gtk_knob_animation_destroy(detent_knob_anim);
		detent_knob_anim = NULL;
	}

	menu_item_fullscreen    = NULL;
	menu_item_notebook      = NULL;
	menu_item_one_page      = NULL;
	menu_item_autoconnect   = NULL;
	menu_item_manualconnect = NULL;
	menu_item_autosave      = NULL;
	menu_item_warn          = NULL;
	menu_item_protect       = NULL;

	if (once) {
		init_help();
	}
	init_gui_patch();

	/* allow main window to appear on top */
#if GTK_CHECK_VERSION(2, 14, 0)
	window = gtk_widget_get_window(splash_window);
#else
	window = splash_window->window;
#endif
	gdk_window_set_keep_above(window, FALSE);
	while (gtk_events_pending()) {
		gtk_main_iteration_do(FALSE);
		usleep(1000);
	}

	/* everything gets created from here */
	create_main_window();

	create_file_filters();
#ifdef ENABLE_CONFIG_DIALOG
	create_config_dialog();
	close_config_dialog(main_window, (gpointer)((long int) config_is_open));
#endif
	create_patch_load_dialog();
	create_patch_save_dialog();
	create_midimap_load_dialog();
	create_midimap_save_dialog();
	create_session_load_dialog();
	create_session_save_dialog();

	/* add idle handler or timer callback to update widgets */
	g_timeout_add_full(G_PRIORITY_HIGH_IDLE + 25,
	                   (guint) setting_refresh_interval,
	                   &gui_main_loop_iteration,
	                   (gpointer)((long int) setting_refresh_interval),
	                   gui_main_loop_stopped);

	if (gtkui_restarting) {
		gtkui_restarting = 0;
	}
	else {
		/* on first start, broadcast the gtkui ready condition */
		pthread_mutex_lock(&gtkui_ready_mutex);
		gtkui_ready = 1;
		pthread_cond_broadcast(&gtkui_ready_cond);
		pthread_mutex_unlock(&gtkui_ready_mutex);
	}

	update_param_sensitivities();

	gtk_widget_destroy(splash_window);
	splash_window = NULL;

	/* gtkui thread sits and runs here */
	gtk_main();

	gtkui_ready = 0;

	save_settings(NULL);

	/* cleanup and shut everything else down */
	phasex_shutdown("Thank you for using PHASEX!\n"
	                "(C) 1999-2012 William Weston <whw@linuxmail.org> and others.\n"
	                "Released under the GNU Public License, Ver. 3\n");

	/* end of gtkui thread */
	return NULL;
}


/*****************************************************************************
 * start_gtkui_thread()
 *****************************************************************************/
void
start_gtkui_thread(void)
{
	init_rt_mutex(&gtkui_ready_mutex, 1);
	if (pthread_create(&gtkui_thread_p, NULL, &gtkui_thread, NULL) != 0) {
		phasex_shutdown("Unable to start gtkui thread.\n");
	}
}


/*****************************************************************************
 * create_file_filters()
 *****************************************************************************/
void
create_file_filters(void)
{
	if (file_filter_all != NULL) {
		g_object_unref(G_OBJECT(file_filter_all));
	}
	file_filter_all = gtk_file_filter_new();
	gtk_file_filter_set_name(file_filter_all, "[*] All Files");
	gtk_file_filter_add_pattern(file_filter_all, "*");
	g_object_ref_sink(G_OBJECT(file_filter_all));

	if (file_filter_patches != NULL) {
		g_object_unref(G_OBJECT(file_filter_patches));
	}
	file_filter_patches = gtk_file_filter_new();
	gtk_file_filter_set_name(file_filter_patches, "[*.phx] PHASEX Patches");
	gtk_file_filter_add_pattern(file_filter_patches, "*.phx");
	g_object_ref_sink(G_OBJECT(file_filter_patches));

	if (file_filter_map != NULL) {
		g_object_unref(GTK_OBJECT(file_filter_map));
	}
	file_filter_map = gtk_file_filter_new();
	gtk_file_filter_set_name(file_filter_map, "[*.map] PHASEX MIDI Maps");
	gtk_file_filter_add_pattern(file_filter_map, "*.map");
	g_object_ref_sink(G_OBJECT(file_filter_map));
}


/*****************************************************************************
 * phasex_gtkui_quit()
 *****************************************************************************/
void
phasex_gtkui_quit(void)
{
	PATCH       *patch    = get_visible_patch();
	SESSION     *session  = get_current_session();
	GtkWidget   *dialog;
	GtkWidget   *label;
	gint        response;
	int         need_quit = 1;

	/* for multiple parts, check for modified session, and save if desired */
	/* TODO:  add config option for checking if _any_ session is modified */
	if ((MAX_PARTS > 1) && session->modified) {
		/* create session modified dialog */
		dialog = gtk_dialog_new_with_buttons("WARNING:  Session Modified",
		                                     GTK_WINDOW(main_window),
		                                     GTK_DIALOG_DESTROY_WITH_PARENT,
		                                     GTK_STOCK_CANCEL,
		                                     GTK_RESPONSE_CANCEL,
		                                     "Ignore Changes",
		                                     GTK_RESPONSE_NO,
		                                     "Save and Quit",
		                                     GTK_RESPONSE_YES,
		                                     NULL);
		gtk_window_set_wmclass(GTK_WINDOW(dialog), "phasex", "phasex-dialog");
		gtk_window_set_role(GTK_WINDOW(dialog), "verify-quit");

		label = gtk_label_new("The current session has not been saved since "
		                      "patches were last modified.  Save now?");
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
		//widget_set_custom_font (dialog);
		gtk_widget_show_all(dialog);

		/* run session modified dialog */
		response = gtk_dialog_run(GTK_DIALOG(dialog));
		switch (response) {
		case GTK_RESPONSE_YES:
			on_session_save_activate(NULL, NULL);
			need_quit = 1;
			break;
		case GTK_RESPONSE_NO:
			need_quit = 1;
			break;
		case GTK_RESPONSE_CANCEL:
			need_quit = 0;
			break;
		}
		gtk_widget_destroy(dialog);
	}
	/* for a single part, check for modified patch, and save if desired */
	/* TODO:  add config option for checking if _any_ patch is modified */
	else if (patch->modified) {

		/* create patch modified dialog */
		dialog = gtk_dialog_new_with_buttons("WARNING:  Patch Modified",
		                                     GTK_WINDOW(main_window),
		                                     GTK_DIALOG_DESTROY_WITH_PARENT,
		                                     GTK_STOCK_CANCEL,
		                                     GTK_RESPONSE_CANCEL,
		                                     "Ignore Changes",
		                                     GTK_RESPONSE_NO,
		                                     "Save and Quit",
		                                     GTK_RESPONSE_YES,
		                                     NULL);
		gtk_window_set_wmclass(GTK_WINDOW(dialog), "phasex", "phasex-dialog");
		gtk_window_set_role(GTK_WINDOW(dialog), "verify-quit");

		label = gtk_label_new("The current patch has not been saved since "
		                      "it was last modified.  Save now?");
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
		//widget_set_custom_font (dialog);
		gtk_widget_show_all(dialog);

		/* run patch modified dialog */
		response = gtk_dialog_run(GTK_DIALOG(dialog));
		switch (response) {
		case GTK_RESPONSE_YES:
			on_patch_save_activate(NULL, NULL);
			need_quit = 1;
			break;
		case GTK_RESPONSE_NO:
			need_quit = 1;
			break;
		case GTK_RESPONSE_CANCEL:
			need_quit = 0;
			break;
		}
		gtk_widget_destroy(dialog);
	}

	/* check for modified midimap, and save if desired */
	if (midimap_modified) {

		/* create midimap modified dialog */
		dialog = gtk_dialog_new_with_buttons("WARNING:  MIDI Map Modified",
		                                     GTK_WINDOW(main_window),
		                                     GTK_DIALOG_DESTROY_WITH_PARENT,
		                                     GTK_STOCK_CANCEL,
		                                     GTK_RESPONSE_CANCEL,
		                                     "Ignore Changes",
		                                     GTK_RESPONSE_NO,
		                                     "Save and Quit",
		                                     GTK_RESPONSE_YES,
		                                     NULL);
		gtk_window_set_wmclass(GTK_WINDOW(dialog), "phasex", "phasex-dialog");
		gtk_window_set_role(GTK_WINDOW(dialog), "verify-save");

		label = gtk_label_new("The current MIDI map has not been saved since "
		                      "it was last modified.  Save now?");
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
		//widget_set_custom_font (dialog);
		gtk_widget_show_all(dialog);

		/* run midimap modified dialog */
		response = gtk_dialog_run(GTK_DIALOG(dialog));
		switch (response) {
		case GTK_RESPONSE_YES:
			on_midimap_save_activate();
			need_quit = 1;
			break;
		case GTK_RESPONSE_NO:
			need_quit = 1;
			break;
		case GTK_RESPONSE_CANCEL:
			need_quit = 0;
			break;
		}
		gtk_widget_destroy(dialog);
	}

	/* quit only if cancel wasn't pressed */
	if (need_quit) {
		gtkui_ready = 0;
		gtk_main_quit();
	}
}


/*****************************************************************************
 * phasex_gtkui_forced_quit()
 *****************************************************************************/
void
phasex_gtkui_forced_quit(void)
{
	forced_quit = 1;
	phasex_gtkui_quit();
}


/*****************************************************************************
 * gui_main_loop_iteration()
 *****************************************************************************/
gboolean
gui_main_loop_iteration(gpointer data)
{
	PATCH           *patch      = get_visible_patch();
	int             interval    = (int)((long int) data % 1000000);
	static int      counter     = 0;
#ifdef WALKING_UPDATE
	static int      walking     = 0;
#endif
	int             num_updated = 0;
	int             param_num;

	/* watch for global shutdown */
	if (pending_shutdown) {
		gdk_threads_leave();
		gtk_main_quit();
		return FALSE;
	}

	/* handle pending gui updates for changes from MIDI side. */
	/* MIDI should already have updated synth engine side. */
	if (pending_visible_patch != NULL) {
		update_gui_patch(pending_visible_patch, 0);
		visible_patch = pending_visible_patch;
		pending_visible_patch = NULL;
	}
	else {
		/* change patch modified label, if needed */
		update_gui_patch_modified();
		update_gui_session_modified();

		if (patch_name_changed) {
			update_gui_patch_name();
		}
		if (session_name_changed) {
			update_gui_session_name();
		}

		/* avoid simultaneous updates */
		periodic_update_in_progress = 1;

		/* Update widgets for up to 150 parameters per cycle
		   go up to NUM_PARAMS + 1 to also get midi_channel in
		   the extended params. */
		for (param_num = 0; param_num < (NUM_PARAMS + 1); param_num++) {
			if (gtkui_restarting || (num_updated >= MAX_PARAMS)) {
				break;
			}
			if (gp->param[param_num].updated) {
				update_gui_param(& (patch->param[param_num]));
				num_updated++;
			}
		}

		periodic_update_in_progress = 0;
	}

	/* check for changes in audio/midi connection lists */
	/* TODO:  rebuild menu items when hardware or port lists change */
	if (alsa_pcm_hw_changed || alsa_seq_ports_changed || alsa_rawmidi_hw_changed) {
		alsa_pcm_hw_changed     = 0;
		alsa_seq_ports_changed  = 0;
		alsa_rawmidi_hw_changed = 0;
	}
	if (jack_midi_ports_changed) {
		jack_midi_ports_changed = 0;
	}

	/* check for active cc edit */
	if (cc_edit_active) {
		if (cc_edit_cc_num > -1) {
			if (cc_edit_spin != NULL) {
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(cc_edit_spin), cc_edit_cc_num);
				cc_edit_cc_num = -1;
			}

		}
	}

	/* for visible part switching, refocus widget that was focused before part change */
	if (focus_widget != NULL) {
		gtk_widget_grab_focus(focus_widget);
		focus_widget = NULL;
	}

	/* dump the session and midimap every 100x through here */
	counter++;
	if (counter == 50) {
		save_midimap(user_midimap_dump_file);
	}
	else if (counter == 100) {
		counter = 0;
		save_session(user_session_dump_dir, visible_sess_num, 0);

		/* TODO: allow feeding of new patches through
		   /tmp/patchload-## or /tmp/sessionload */
	}

	/* if config dialog was open before restarting, then open it again */
#ifdef ENABLE_CONFIG_DIALOG
	if (config_is_open >= 1) {
		config_is_open = 1;
		show_config_dialog();
	}
#endif

	/* If refresh interval changes, return false so that this
	   timer does not get called again.  The destroy callback will
	   start a new one. */
	if (interval != setting_refresh_interval) {
		return FALSE;
	}

	return TRUE;
}


/*****************************************************************************
 * gui_main_loop_stopped()
 *****************************************************************************/
void
gui_main_loop_stopped(gpointer UNUSED(data))
{
	if (!pending_shutdown && !gtkui_restarting) {
		g_timeout_add_full(G_PRIORITY_HIGH_IDLE + 25,
		                   (guint) setting_refresh_interval,
		                   &gui_main_loop_iteration,
		                   (gpointer)((long int) setting_refresh_interval),
		                   gui_main_loop_stopped);
	}
}


/*****************************************************************************
 * handle_window_state_event()
 *****************************************************************************/
void
handle_window_state_event(GtkWidget *UNUSED(widget), GdkEventWindowState *state)
{
	if (state->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
		if (state->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
			setting_maximize = MAXIMIZE_ON;
		}
		else {
			setting_maximize = MAXIMIZE_OFF;
		}
	}
	if (state->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
		if (state->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
			setting_fullscreen = FULLSCREEN_ON;
			if ((menu_item_fullscreen != NULL)) {
				gtk_check_menu_item_set_active(menu_item_fullscreen, TRUE);
			}
		}
		else {
			setting_fullscreen = FULLSCREEN_OFF;
			if ((menu_item_fullscreen != NULL)) {
				gtk_check_menu_item_set_active(menu_item_fullscreen, FALSE);
			}
		}
	}
}


/*****************************************************************************
 * splash_expose()
 *
 * Paints pixbuf data to window, using alpha if supported.
 *****************************************************************************/
gint
splash_expose(GtkWidget *widget, GdkEventExpose *event)
{
	cairo_t         *cr;
	GdkWindow       *window;

	if (event->count > 0) {
		return FALSE;
	}

#if GTK_CHECK_VERSION(2, 8, 0)
# if GTK_CHECK_VERSION(2, 14, 0)
	window = gtk_widget_get_window(widget);
# else
	window = widget->window;
# endif
	cr = gdk_cairo_create(GDK_DRAWABLE(window));
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

	gdk_cairo_set_source_pixbuf(cr, splash_pixbuf, 0.0, 0.0);
	if (alpha_support) {
		cairo_paint_with_alpha(cr, 1.0);
	}
	else {
		cairo_paint(cr);
	}
	cairo_destroy(cr);
#else
	gdk_draw_pixbuf(widget->window, widget->style->bg_gc[GTK_STATE_NORMAL], splash_pixbuf,
	                0, 0, 0, 0, splash_width, splash_height, GDK_RGB_DITHER_NONE, 0, 0);
#endif

	return FALSE;
}


/*****************************************************************************
 * create_splash_window()
 *
 * Creates splash window with transparent background.  If alpha channels and
 * compositing are supported, opaque areas in image are used as the mask for
 * setting the window shape.
 *****************************************************************************/
void
create_splash_window(void)
{
	GdkScreen       *screen;
	GdkColormap     *colormap;
	GdkWindow       *window;
	GdkBitmap       *mask_bitmap;
	GError          *gerror             = NULL;
	cairo_t         *cr;
	cairo_surface_t *image_surface;
	char            filename[PATH_MAX];
	unsigned char   *mask_data;
	unsigned char   *image_data;
	int             image_stride;
	int             mask_stride;
	int             x;
	int             y;

	/* create a new window always on top with no decorations */
	splash_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(splash_window), "Starting phasex...");
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_window_set_opacity(GTK_WINDOW(splash_window), 1.0);
#endif
	gtk_window_set_decorated(GTK_WINDOW(splash_window), FALSE);
	gtk_window_set_position(GTK_WINDOW(splash_window), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW(splash_window), FALSE);
	gtk_window_set_icon_from_file(GTK_WINDOW(splash_window), PIXMAP_DIR"/phasex-icon.png", NULL);
	gtk_window_set_default_icon_from_file(PIXMAP_DIR"/phasex-icon.png", NULL);
	gtk_container_set_border_width(GTK_CONTAINER(splash_window), 0);
	gtk_widget_set_app_paintable(splash_window, TRUE);

	/* check for alpha and compositing support */
	screen = gtk_widget_get_screen(splash_window);
	colormap = gdk_screen_get_rgba_colormap(screen);
	if (colormap == NULL) {
		colormap = gdk_screen_get_rgb_colormap(screen);
		alpha_support = 0;
	}
	else {
		PHASEX_DEBUG(DEBUG_CLASS_GUI, "Display supports alpha channels!\n");
		alpha_support = 1;
	}
	gtk_widget_set_colormap(splash_window, colormap);
#if GTK_CHECK_VERSION(2, 10, 0)
	if (gtk_widget_is_composited(splash_window)) {
		PHASEX_DEBUG(DEBUG_CLASS_GUI, "Display supports compositing!\n");
		composite_support = 1;
	}
#endif

	/* create pixbuf from either splash image or transparent image */
	if (alpha_support && composite_support) {
		snprintf(filename, sizeof(filename), "%s/phasex-icon.png", PIXMAP_DIR);
	}
	else {
		snprintf(filename, sizeof(filename), "%s/phasex-splash.png", PIXMAP_DIR);
	}
#if GTK_CHECK_VERSION(2, 10, 0)
	splash_pixbuf = gdk_pixbuf_new_from_file_at_size(filename, splash_width,
	                                                 splash_height, &gerror);
#else
	splash_pixbuf = gdk_pixbuf_new_from_file(filename, &gerror);
#endif
	if (splash_pixbuf == NULL) {
		PHASEX_WARN("Unable to make pixbuf from splash file '%s'.\n", filename);
		return;
	}

	/* set window size based on pixbuf dimensions */
	splash_width  = gdk_pixbuf_get_width(splash_pixbuf);
	splash_height = gdk_pixbuf_get_height(splash_pixbuf);
	gtk_widget_set_size_request(splash_window, splash_width, splash_height);
	PHASEX_DEBUG(DEBUG_CLASS_GUI, "width=%d  height=%d\n", splash_width, splash_height);

	/* connect minimal signals */
	g_signal_connect(G_OBJECT(splash_window), "expose-event",
	                 G_CALLBACK(splash_expose),
	                 (gpointer) NULL);

	/* show window now so we can work with its GdkWindow */
	gtk_widget_show_all(splash_window);

#if GTK_CHECK_VERSION(2, 14, 0)
	window = gtk_widget_get_window(splash_window);
#else
	window = splash_window->window;
#endif
	gdk_window_set_keep_above(window, TRUE);

	/* if compositing and alpha channels are supported, set the window shape */
	if (alpha_support && composite_support) {
		gdk_window_set_events(window, GDK_ALL_EVENTS_MASK);

		/* use cairo to get the raw pixbuf data */
		image_stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, splash_width);
		if ((image_data = malloc((size_t)(image_stride * splash_height))) == NULL) {
			phasex_shutdown("Out of Memory!\n");
		}
		memset(image_data, 0, ((size_t)(image_stride * splash_height)));
		image_surface = cairo_image_surface_create_for_data(image_data,
		                                                    CAIRO_FORMAT_ARGB32,
		                                                    splash_width,
		                                                    splash_height,
		                                                    image_stride);
		cr = cairo_create(image_surface);
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		gdk_cairo_set_source_pixbuf(cr, splash_pixbuf, 0.0, 0.0);
		cairo_paint_with_alpha(cr, 1.0);
		cairo_surface_flush(image_surface);

		/* allocate bitmap mask data */
		mask_stride = cairo_format_stride_for_width(CAIRO_FORMAT_A1, splash_width);
		if ((mask_data = malloc((size_t)(mask_stride * splash_height))) == NULL) {
			phasex_shutdown("Out of Memory!\n");
		}
		memset(mask_data, 0, (size_t)(mask_stride * splash_height));

		/* set mask bitmap data from image data alpha channel */
		for (x = 0; x < splash_width; x++) {
			for (y = 0; y < splash_height; y++) {
				if (image_data[(x * 4) + (image_stride * y) + 3] != 0) {
					mask_data[((x / 8) + (mask_stride * y))] |= (unsigned char)(1 << (x % 8));
				}
			}
		}

		/* create mask bitmap and use to set window's shape and input shape */
		mask_bitmap = gdk_bitmap_create_from_data(NULL,
		                                          (const gchar *) mask_data,
		                                          splash_width,
		                                          splash_height);
		gtk_widget_shape_combine_mask(splash_window, mask_bitmap, 0, 0);
#if GTK_CHECK_VERSION(2, 10, 0)
		gtk_widget_input_shape_combine_mask(splash_window, mask_bitmap, 0, 0);
#endif

		cairo_destroy(cr);
	}
}


/*****************************************************************************
 * create_main_window()
 *****************************************************************************/
void
create_main_window(void)
{
	GtkWidget           *main_vbox;
	GtkWidget           *event;
	char                knob_file[PATH_MAX];
	char                window_title[10];

	/* create main window */
	main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	/* Handle maximize and fullscreen modes before anything else
	   gets a sense of geometry. */
	if (setting_maximize) {
		gtk_window_set_decorated(GTK_WINDOW(main_window), TRUE);
		gtk_window_maximize(GTK_WINDOW(main_window));
	}
	if (setting_fullscreen) {
		gtk_window_set_decorated(GTK_WINDOW(main_window), FALSE);
		gtk_window_fullscreen(GTK_WINDOW(main_window));
	}
	if (!setting_maximize && !setting_fullscreen) {
		gtk_window_set_decorated(GTK_WINDOW(main_window), TRUE);
		gtk_window_unfullscreen(GTK_WINDOW(main_window));
		gtk_window_unmaximize(GTK_WINDOW(main_window));
	}

	/* set class, role, name, title, and icon */
	gtk_window_set_wmclass(GTK_WINDOW(main_window), "phasex", "phasex");
	gtk_window_set_role(GTK_WINDOW(main_window), "main");
	widget_set_backing_store(main_window);
	gtk_object_set_data(GTK_OBJECT(main_window), "main_window", main_window);
	gtk_widget_set_name(main_window, "main_window");
	snprintf(window_title, sizeof(window_title), "phasex");
	gtk_window_set_title(GTK_WINDOW(main_window), window_title);
	gtk_window_set_icon_from_file(GTK_WINDOW(main_window), PIXMAP_DIR"/phasex-icon.png", NULL);
	gtk_window_set_default_icon_from_file(PIXMAP_DIR"/phasex-icon.png", NULL);

	/* put a vbox in it */
	event = gtk_event_box_new();
	widget_set_backing_store(event);

	main_vbox = gtk_vbox_new(FALSE, 0);
	widget_set_backing_store(main_vbox);

	gtk_container_add(GTK_CONTAINER(event), main_vbox);
	gtk_container_add(GTK_CONTAINER(main_window), event);

	/* put the menu in the vbox first */
	create_menubar(main_window, main_vbox);

	/* preload knob animation images */
	if (knob_anim != NULL) {
		gtk_knob_animation_destroy(knob_anim);
	}
	snprintf(knob_file, PATH_MAX, "%s/knob-%dx%d.png",
	         setting_knob_dir,
	         knob_width[setting_knob_size],
	         knob_height[setting_knob_size]);
	knob_anim = gtk_knob_animation_new_from_file(knob_file);

	if (detent_knob_anim != NULL) {
		gtk_knob_animation_destroy(detent_knob_anim);
	}
	snprintf(knob_file, PATH_MAX, "%s/detent-knob-%dx%d.png",
	         setting_detent_knob_dir,
	         knob_width[setting_knob_size],
	         knob_height[setting_knob_size]);
	detent_knob_anim = gtk_knob_animation_new_from_file(knob_file);

	/* put parameter table or notebook in the vbox next */
	switch (setting_window_layout) {
	case LAYOUT_NOTEBOOK:
		create_param_notebook(main_window, main_vbox, param_page);
		break;
	case LAYOUT_ONE_PAGE:
		create_param_one_page(main_window, main_vbox, param_page);
		break;
	case LAYOUT_WIDESCREEN:
		create_param_widescreen(main_window, main_vbox, param_page);
		break;
	}

	/* turn the param nav list into a circular queue */
	param_nav_tail->next = param_nav_head;
	param_nav_head->prev = param_nav_tail;

	/* window doesn't appear until now */
	gtk_widget_show_all(main_window);

	if (debug_class & (DEBUG_CLASS_INIT | DEBUG_CLASS_GUI)) {
		gint width;
		gint height;

		gtk_window_get_size(GTK_WINDOW(main_window), &width, &height);
		PHASEX_DEBUG((DEBUG_CLASS_INIT | DEBUG_CLASS_GUI),
		             "GTKUI:  Window size = %d x %d\n", width, height);
	}

	if ((MAX_PARTS > 1) && (setting_window_layout == LAYOUT_WIDESCREEN)) {
		gtk_widget_grab_focus(GTK_WIDGET(session_spin));
	}
	else {
		gtk_widget_grab_focus(GTK_WIDGET(program_spin));
	}

	/* connect delete-event (but not destroy!) signal */
	g_signal_connect(G_OBJECT(main_window), "delete-event",
	                 G_CALLBACK(phasex_gtkui_forced_quit),
	                 (gpointer) NULL);

	/* connect (un)maximize events */
	g_signal_connect(G_OBJECT(main_window), "window-state-event",
	                 G_CALLBACK(handle_window_state_event),
	                 (gpointer) NULL);
}


/*****************************************************************************
 * restart_gtkui()
 *
 * Shuts down gtk_main(), re-inits, reloads rc files, and rebuilds
 * all windows and widgets.
 *****************************************************************************/
void
restart_gtkui(void)
{
	GdkScreen       *screen;
	GtkSettings     *settings;
	int             j;

	/* save settings so they can be reloaded */
	save_settings(NULL);

	set_theme_env();
	screen = gtk_window_get_screen(GTK_WINDOW(main_window));
	settings = gtk_settings_get_for_screen(screen);
	gtk_rc_reset_styles(settings);

	/* last step to shutting down gtkui */
	gtkui_ready = 0;
	gtk_main_quit();

	cc_edit_spin               = NULL;
	cc_edit_adj                = NULL;

	session_adj                = NULL;
	session_spin               = NULL;
	session_entry              = NULL;

	program_adj                = NULL;
	program_spin               = NULL;
	program_entry              = NULL;

	part_adj                   = NULL;
	part_spin                  = NULL;
	part_entry                 = NULL;

	midi_channel_label         = NULL;
	midi_channel_event_box     = NULL;
	midi_channel_adj           = NULL;

	patch_modified_label       = NULL;
	session_modified_label     = NULL;

	patch_io_start_adj         = NULL;
	session_io_start_adj       = NULL;
	patch_load_start_spin      = NULL;
	session_load_start_spin    = NULL;
	patch_save_start_spin      = NULL;
	session_save_start_spin    = NULL;

	menu_item_autosave         = NULL;
	menu_item_warn             = NULL;
	menu_item_protect          = NULL;

	for (j = 0; j < 3; j++) {
		bank_mem_menu_item[j] = NULL;
	}

	for (j = 0; j < 17; j++) {
		menu_item_part[j] = NULL;
	}

	/* destroy windows */
#ifdef ENABLE_CONFIG_DIALOG
	close_config_dialog(main_window, (gpointer)((long int) config_is_open));
#endif

	if ((patch_load_dialog != NULL) && GTK_IS_DIALOG(patch_load_dialog)) {
		gtk_widget_destroy(patch_load_dialog);
		patch_load_dialog = NULL;
	}
	if ((patch_save_dialog != NULL) && GTK_IS_DIALOG(patch_save_dialog)) {
		gtk_widget_destroy(patch_save_dialog);
		patch_save_dialog = NULL;
	}
	if ((midimap_load_dialog != NULL) && GTK_IS_DIALOG(midimap_load_dialog)) {
		gtk_widget_destroy(midimap_load_dialog);
		midimap_load_dialog = NULL;
	}
	if ((midimap_save_dialog != NULL) && GTK_IS_DIALOG(midimap_save_dialog)) {
		gtk_widget_destroy(midimap_save_dialog);
		midimap_save_dialog = NULL;
	}

	if ((main_window != NULL) && GTK_IS_WINDOW(main_window)) {
		gtk_widget_hide(main_window);
		gtk_widget_destroy(main_window);
		main_window = NULL;
	}

	/* free memory */
	if (knob_anim != NULL) {
		gtk_knob_animation_destroy(knob_anim);
		knob_anim = NULL;
	}
	if (detent_knob_anim != NULL) {
		gtk_knob_animation_destroy(detent_knob_anim);
		detent_knob_anim = NULL;
	}

	/* re-initialize GTK */
	gtk_init(NULL, NULL);

	/* recreate windows */
	create_main_window();

	create_patch_load_dialog();
	create_patch_save_dialog();
	create_midimap_load_dialog();
	create_midimap_save_dialog();
	create_session_load_dialog();
	create_session_save_dialog();

	update_param_sensitivities();

	/* back in business */
	gtkui_ready = 1;
	gtk_main();
}


/*****************************************************************************
 * widget_set_backing_store()
 *
 * enable backing store on the selected widget (if possible)
 *****************************************************************************/
#if defined(ENABLE_BACKING_STORE) && defined(GDK_WINDOWING_X11)
void
widget_set_backing_store(GtkWidget *widget)
{

	if (setting_backing_store) {
		g_signal_connect(widget, "realize",
		                 G_CALLBACK(widget_set_backing_store_callback),
		                 NULL);
	}
}
#else
void
widget_set_backing_store(GtkWidget *UNUSED(widget))
{
}
#endif


/*****************************************************************************
 * widget_set_backing_store_callback()
 *
 * callback to enable backing store on the selected widget (if possible)
 * connected to a widget's realize signal by widget_set_backing_store()
 *****************************************************************************/
#ifdef GDK_WINDOWING_X11
void
widget_set_backing_store_callback(GtkWidget *widget, void *UNUSED(data))
{
	GdkWindow           *window     = NULL;
# if GTK_CHECK_VERSION(2, 14, 0)
	GtkWindowGroup      *group;
	GList               *list       = NULL;
	GList               *cur;
# endif
	XSetWindowAttributes    attributes;
	unsigned long       attr_mask   = (CWBackingStore);


	if (widget->window == NULL) {
		return;
	}

	window = widget->window;

	attributes.backing_store  = Always;
	XChangeWindowAttributes(GDK_WINDOW_XDISPLAY(window),
	                        GDK_WINDOW_XWINDOW(window),
	                        attr_mask, &attributes);

	/* special case for the spin button which has a giganormitude of windows */
	if (GTK_IS_SPIN_BUTTON(widget)) {
		window = GTK_SPIN_BUTTON(widget)->panel;
		if (window != NULL) {
			XChangeWindowAttributes(GDK_WINDOW_XDISPLAY(window),
			                        GDK_WINDOW_XWINDOW(window),
			                        attr_mask, &attributes);
		}

		window = GTK_ENTRY(widget)->text_area;
		if (window != NULL) {
			XChangeWindowAttributes(GDK_WINDOW_XDISPLAY(window),
			                        GDK_WINDOW_XWINDOW(window),
			                        attr_mask, &attributes);
		}
	}
	/* special case for label mnemonic window and selection window */
	else if (GTK_IS_LABEL(widget)) {
		window = GDK_WINDOW(GTK_LABEL(widget)->mnemonic_window);
		if (window != NULL) {
			XChangeWindowAttributes(GDK_WINDOW_XDISPLAY(window),
			                        GDK_WINDOW_XWINDOW(window),
			                        attr_mask, &attributes);
		}

		window = GDK_WINDOW(GTK_LABEL(widget)->select_info);
		if (window != NULL) {
			XChangeWindowAttributes(GDK_WINDOW_XDISPLAY(window),
			                        GDK_WINDOW_XWINDOW(window),
			                        attr_mask, &attributes);
		}
	}
	/* special case for entry text area window */
	else if (GTK_IS_ENTRY(widget)) {
		window = GTK_ENTRY(widget)->text_area;
		if (window != NULL) {
			XChangeWindowAttributes(GDK_WINDOW_XDISPLAY(window),
			                        GDK_WINDOW_XWINDOW(window),
			                        attr_mask, &attributes);
		}
	}
	/* special case for gtk event box */
	else if (GTK_IS_EVENT_BOX(widget)) {
		window = widget->window;
		if (window != NULL) {
			XChangeWindowAttributes(GDK_WINDOW_XDISPLAY(window),
			                        GDK_WINDOW_XWINDOW(window),
			                        attr_mask, &attributes);
		}
	}
	/* special case for notebook event window */
	else if (GTK_IS_NOTEBOOK(widget)) {
		window = GTK_NOTEBOOK(widget)->event_window;
		if (window != NULL) {
			XChangeWindowAttributes(GDK_WINDOW_XDISPLAY(window),
			                        GDK_WINDOW_XWINDOW(window),
			                        attr_mask, &attributes);
		}
	}
	/* special case for normal gtk window */
	else if (GTK_IS_WINDOW(widget)) {
		window = GTK_WINDOW(widget)->frame;
		if (window != NULL) {
			XChangeWindowAttributes(GDK_WINDOW_XDISPLAY(window),
			                        GDK_WINDOW_XWINDOW(window),
			                        attr_mask, &attributes);
		}
# if GTK_CHECK_VERSION(2, 14, 0)
		group = gtk_window_get_group(GTK_WINDOW(widget));
		list = gtk_window_group_list_windows(group);
		cur = g_list_first(list);
		while (cur != NULL) {
			if ((cur->data != NULL) && GTK_IS_WIDGET(cur->data)) {
				window = gtk_widget_get_window(GTK_WIDGET(cur->data));
				if (window != NULL) {
					XChangeWindowAttributes(GDK_WINDOW_XDISPLAY(window),
					                        GDK_WINDOW_XWINDOW(window),
					                        attr_mask, &attributes);
				}
			}
			cur = g_list_next(cur);
		}
		if (list != NULL) {
			g_list_free(list);
		}
# endif
	}
}
#endif


/*****************************************************************************
 * widget_set_custom_font()
 *
 * override widget's font with a custom font
 *
 * *NOTE: this function recurses over containers
 *****************************************************************************/
void
widget_set_custom_font(GtkWidget *widget, PangoFontDescription *desc)
{
	GList   *list   = NULL;
	GList   *cur;

	if (desc != NULL) {
		/* modify label font directly */
		if (GTK_IS_LABEL(widget)) {
			gtk_widget_modify_font(GTK_WIDGET(widget), desc);
		}
		/* modify font of label widget inside button */
		else if (GTK_IS_BUTTON(widget)) {
			list = gtk_container_get_children(GTK_CONTAINER(widget));
			cur = g_list_first(list);
			while (cur != NULL) {
				if (GTK_IS_LABEL(cur->data)) {
					gtk_widget_modify_font(GTK_WIDGET(cur->data), desc);
				}
				cur = g_list_next(cur);
			}
		}
		/* recurse through menubar and menushell */
		else if (GTK_IS_MENU_BAR(widget) || GTK_IS_MENU_SHELL(widget)) {
			list = GTK_MENU_SHELL(widget)->children;
			cur = g_list_first(list);
			while (cur != NULL) {
				widget_set_custom_font(GTK_WIDGET(cur->data), desc);
				cur = g_list_next(cur);
			}
		}
		/* recurse to bin's child */
		else if (GTK_IS_BIN(widget)) {
			widget_set_custom_font(gtk_bin_get_child(GTK_BIN(widget)), desc);
		}
		/* recurse through children */
		else if (GTK_IS_CONTAINER(widget)) {
			list = gtk_container_get_children(GTK_CONTAINER(widget));
			cur = g_list_first(list);
			while (cur != NULL) {
				if (GTK_IS_LABEL(cur->data)) {
					gtk_widget_modify_font(GTK_WIDGET(cur->data), desc);
				}
				else {
					widget_set_custom_font(GTK_WIDGET(cur->data), desc);
				}
				cur = g_list_next(cur);
			}
		}
		/* recurse through event box's bin and child */
		else if (GTK_IS_EVENT_BOX(widget)) {
			widget = gtk_bin_get_child
				(GTK_BIN(GTK_WIDGET(& (GTK_EVENT_BOX(widget))->bin)));
			if (GTK_IS_WIDGET(widget) || GTK_IS_CONTAINER(widget)) {
				widget_set_custom_font(GTK_WIDGET(widget), desc);
			}
		}
		/* modify fonts of any other widget directly */
		else if (GTK_IS_WIDGET(widget)) {
			gtk_widget_modify_font(widget, desc);
		}
	}

	if (list != NULL) {
		g_list_free(NULL);
	}
}


/*****************************************************************************
 * table_add_widget()
 *
 * Wraps widgets placed into a GtkTable with an hbox for horizontal
 * justification within a table cell.  Used for building the navbar.
 *****************************************************************************/
void
table_add_widget(GtkTable *table, GtkWidget *child,
                 guint col, guint row, guint size_x, guint size_y,
                 GtkAttachOptions xoptions, GtkAttachOptions yoptions,
                 guint xpadding, guint ypadding,
                 int justify)
{
	GtkWidget   *hbox;
	GtkWidget   *event;
	GtkWidget   *event2;

	hbox = gtk_hbox_new(FALSE, 0);
	widget_set_backing_store(hbox);
	event = gtk_event_box_new();

	switch (justify) {
	case JUSTIFY_LEFT:
		gtk_box_pack_start(GTK_BOX(hbox), child, FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(hbox), event, TRUE, TRUE, 0);
		break;
	case JUSTIFY_RIGHT:
		gtk_box_pack_start(GTK_BOX(hbox), event, TRUE, TRUE, 0);
		gtk_box_pack_end(GTK_BOX(hbox), child, FALSE, FALSE, 0);
		break;
	case JUSTIFY_CENTER:
		event2 = gtk_event_box_new();
		gtk_box_pack_start(GTK_BOX(hbox), event, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), child, FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(hbox), event2, TRUE, TRUE, 0);
		break;
	}

	gtk_table_attach(table, hbox,
	                 col, (col + size_x), row, (row + size_y),
	                 xoptions, yoptions,
	                 xpadding, ypadding);
}
