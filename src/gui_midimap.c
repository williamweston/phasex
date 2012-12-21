/*****************************************************************************
 *
 * gui_midimap.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2012 William Weston <whw@linuxmail.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include "gui_main.h"
#include "gui_menubar.h"
#include "gui_bank.h"
#include "gui_session.h"
#include "gui_patch.h"
#include "gui_param.h"
#include "gui_midimap.h"
#include "phasex.h"
#include "config.h"
#include "patch.h"
#include "param.h"
#include "bank.h"
#include "session.h"
#include "midimap.h"
#include "settings.h"
#include "help.h"
#include "debug.h"


GtkWidget       *midimap_load_dialog    = NULL;
GtkWidget       *midimap_save_dialog    = NULL;
GtkWidget       *cc_edit_dialog         = NULL;
GtkWidget       *cc_edit_spin           = NULL;

GtkObject       *cc_edit_adj            = NULL;

int             cc_edit_active          = 0;
int             cc_edit_ignore_midi     = 0;
int             cc_edit_cc_num          = -1;
int             cc_edit_param_id        = -1;


/*****************************************************************************
 * update_param_cc_map()
 *****************************************************************************/
void
update_param_cc_map(GtkWidget *widget, gpointer data)
{
	PARAM       *param = (PARAM *) data;
	short       old_cc;
	short       new_cc;
	short       j;
	short       k;
	int         id;

	if (param != NULL) {
		id     = (int) param->info->id;
		old_cc = (short) param->info->cc_num;
		new_cc = (char)(floor(GTK_ADJUSTMENT(widget)->value)) & 0x7F;

		if ((new_cc  >= 0) && (new_cc < 128)) {
			/* unmap old cc num from ccmatrix */
			j = 0;
			while ((ccmatrix[old_cc][j] >= 0) && (j < 16)) {
				if (ccmatrix[old_cc][j] == id) {
					for (k = j; k < 16; k++) {
						ccmatrix[old_cc][k] = ccmatrix[old_cc][k + 1];
					}
				}
				j++;
			}

			/* map new cc num into ccmatrix */
			j = 0;
			while ((j < 16) && (ccmatrix[new_cc][j] >= 0)) {
				j++;
			}
			if (j < 16) {
				ccmatrix[new_cc][j] = id;
				j++;
			}
			if (j < 16) {
				ccmatrix[new_cc][j] = -1;
			}

			/* keep track of cc num in param too */
			if (param->info->cc_num != new_cc) {
				midimap_modified = 1;
				param->info->cc_num = new_cc;
			}
		}
	}
}


/*****************************************************************************
 * update_param_locked()
 *****************************************************************************/
void
update_param_locked(GtkWidget *widget, gpointer data)
{
	PARAM   *param = (PARAM *) data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		param->info->locked = 1;
	}
	else {
		param->info->locked = 0;
	}
}


/*****************************************************************************
 * update_param_ignore()
 *****************************************************************************/
void
update_param_ignore(GtkWidget *widget, gpointer UNUSED(data))
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		cc_edit_ignore_midi = 1;
	}
	else {
		cc_edit_ignore_midi = 0;
	}
}


/*****************************************************************************
 * close_cc_edit_dialog()
 *****************************************************************************/
void
close_cc_edit_dialog(GtkWidget *UNUSED(widget), gpointer UNUSED(data))
{
	cc_edit_active      = 0;
	cc_edit_ignore_midi = 0;
	cc_edit_cc_num      = -1;
	cc_edit_param_id    = -1;
	gtk_widget_destroy(cc_edit_dialog);
	cc_edit_dialog      = NULL;
	cc_edit_adj         = NULL;
	cc_edit_spin        = NULL;
}


/*****************************************************************************
 * param_label_button_press()
 *
 * Callback for a button press event on a param label event box.
 *****************************************************************************/
void
param_label_button_press(GtkWidget *widget, GdkEventButton *event)
{
	PARAM           *param;
	GtkWidget       *hbox;
	GtkWidget       *label;
	GtkWidget       *separator;
	GtkWidget       *lock_button;
	GtkWidget       *ignore_button;
	unsigned int    param_num;
	int             id              = -1;
	int             same_param_id   = 0;
	const char      *widget_name;

	/* find param id by looking up name from widget in param table */
	widget_name = gtk_widget_get_name(widget);
	for (param_num = 0; param_num < NUM_HELP_PARAMS; param_num++) {
		param = get_param(visible_part_num, param_num);
		if (strcmp(widget_name, param->info->name) == 0) {
			id = (int) param_num;
			break;
		}
	}

	/* return now if matching parameter is not found */
	if (id == -1) {
		return;
	}

	/* check which button got clicked */
	switch (event->button) {
	case 1:
		/* nothing for left button */
		break;

	case 2:
		/* display parameter help for middle click */
		display_param_help(id);
		break;

	case 3:
		/* only midi-map real paramaters */
		if (id >= NUM_PARAMS) {
			return;
		}

		/* destroy current edit window */
		if (cc_edit_dialog != NULL) {
			if (id == cc_edit_param_id) {
				same_param_id = 1;
			}
			gtk_widget_destroy(cc_edit_dialog);
		}

		/* only create new edit window if it's a new or different parameter */
		if (!same_param_id) {
			cc_edit_dialog = gtk_dialog_new_with_buttons("Update MIDI Controller",
			                                             GTK_WINDOW(main_window),
			                                             GTK_DIALOG_DESTROY_WITH_PARENT,
			                                             GTK_STOCK_CLOSE,
			                                             GTK_RESPONSE_CLOSE,
			                                             NULL);
			gtk_window_set_wmclass(GTK_WINDOW(cc_edit_dialog), "phasex", "phasex-edit");
			gtk_window_set_role(GTK_WINDOW(cc_edit_dialog), "controller-edit");

			label = gtk_label_new("Enter a new MIDI controller number "
			                      "below or simply touch the desired "
			                      "controller to map the parameter "
			                      "automatically.  Locked parameters "
			                      "allow only explicit updates from the "
			                      "user interface.");
			gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
			gtk_misc_set_padding(GTK_MISC(label), 8, 0);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox),
			                   label, TRUE, FALSE, 8);

			separator = gtk_hseparator_new();
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox),
			                   separator, TRUE, FALSE, 0);

			/* parameter name */
			label = gtk_label_new(param_help[id].label);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox),
			                   label, TRUE, FALSE, 8);

			separator = gtk_hseparator_new();
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox),
			                   separator, TRUE, FALSE, 0);

			/* select midi controller */
			hbox = gtk_hbox_new(FALSE, 0);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox),
			                   hbox, TRUE, FALSE, 8);

			cc_edit_adj = gtk_adjustment_new(param->info->cc_num, -1, 127, 1, 10, 0);

			cc_edit_spin = gtk_spin_button_new(GTK_ADJUSTMENT(cc_edit_adj), 0, 0);
			gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(cc_edit_spin), TRUE);
			gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(cc_edit_spin), GTK_UPDATE_IF_VALID);
			gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(cc_edit_spin), TRUE);
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(cc_edit_spin), param->info->cc_num);
			gtk_box_pack_end(GTK_BOX(hbox), cc_edit_spin, FALSE, FALSE, 8);

			label = gtk_label_new("MIDI Controller #");
			gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
			gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 8);

			/* parameter locking */
			hbox = gtk_hbox_new(FALSE, 0);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox), hbox, TRUE, TRUE, 8);

			lock_button = gtk_check_button_new();
			if (param->info->locked) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lock_button), TRUE);
			}
			else {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lock_button), FALSE);
			}
			gtk_box_pack_end(GTK_BOX(hbox), lock_button, FALSE, FALSE, 8);

			label = gtk_label_new("Lock parameter (manual adjustment only)?");
			gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
			gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 8);

			/* midi ignore */
			hbox = gtk_hbox_new(FALSE, 0);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox), hbox, TRUE, TRUE, 8);

			ignore_button = gtk_check_button_new();
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ignore_button), FALSE);
			gtk_box_pack_end(GTK_BOX(hbox), ignore_button, FALSE, FALSE, 8);

			label = gtk_label_new("Ignore inbound MIDI for this dialog?");
			gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
			gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 8);

			//widget_set_custom_font (cc_edit_dialog);
			gtk_widget_show_all(cc_edit_dialog);

			/* connect signals */
			g_signal_connect(GTK_OBJECT(cc_edit_adj), "value_changed",
			                 GTK_SIGNAL_FUNC(update_param_cc_map),
			                 (gpointer) param);

			g_signal_connect(GTK_OBJECT(lock_button), "toggled",
			                 GTK_SIGNAL_FUNC(update_param_locked),
			                 (gpointer) param);

			g_signal_connect(GTK_OBJECT(ignore_button), "toggled",
			                 GTK_SIGNAL_FUNC(update_param_ignore),
			                 (gpointer) param);

			g_signal_connect_swapped(G_OBJECT(cc_edit_dialog), "response",
			                         GTK_SIGNAL_FUNC(close_cc_edit_dialog),
			                         (gpointer) cc_edit_dialog);

			g_signal_connect(G_OBJECT(cc_edit_dialog), "destroy",
			                 GTK_SIGNAL_FUNC(close_cc_edit_dialog),
			                 (gpointer) cc_edit_dialog);

			/* set internal info */
			cc_edit_cc_num   = -1;
			cc_edit_param_id = id;
			cc_edit_active   = 1;
		}

		/* otherwise, just get out of edit mode completely */
		else {
			/* set internal info */
			cc_edit_cc_num   = -1;
			cc_edit_param_id = -1;
			cc_edit_active   = 0;
		}
	}
}


/*****************************************************************************
 * create_midimap_load_dialog()
 *****************************************************************************/
void
create_midimap_load_dialog(void)
{
	GError      *error = NULL;

	/* create new dialog only if it doesn't already exist */
	if (midimap_load_dialog == NULL) {
		midimap_load_dialog = gtk_file_chooser_dialog_new("PHASEX - Load Midimap",
		                                                  GTK_WINDOW(main_window),
		                                                  GTK_FILE_CHOOSER_ACTION_OPEN,
		                                                  GTK_STOCK_CANCEL,
		                                                  GTK_RESPONSE_CANCEL,
		                                                  GTK_STOCK_OPEN,
		                                                  GTK_RESPONSE_ACCEPT,
		                                                  NULL);
		//widget_set_custom_font (midimap_load_dialog);
		gtk_window_set_wmclass(GTK_WINDOW(midimap_load_dialog), "phasex", "phasex-dialog");
		gtk_window_set_role(GTK_WINDOW(midimap_load_dialog), "midimap-load");

		/* realize widget before making it deal with actual file trees */
		gtk_widget_realize(midimap_load_dialog);

#if GTK_CHECK_VERSION(2, 6, 0)
		/* show hidden files, if we can */
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(midimap_load_dialog), TRUE);
#endif

		/* add system and user midimap dirs as shortcuts */
		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(midimap_load_dialog),
		                                     MIDIMAP_DIR, &error);
		if (error != NULL) {
			PHASEX_ERROR("Error %d: %s\n", error->code, error->message);
			g_error_free(error);
		}
		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(midimap_load_dialog),
		                                     user_midimap_dir, &error);

		if (error != NULL) {
			if (debug) {
				PHASEX_ERROR("Error %d: %s\n", error->code, error->message);
			}
			g_error_free(error);
		}
		if (file_filter_map != NULL) {
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(midimap_load_dialog), file_filter_map);
			gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(midimap_load_dialog), file_filter_map);
		}
	}
}


/*****************************************************************************
 * run_midimap_load_dialog()
 *****************************************************************************/
void
run_midimap_load_dialog(void)
{
	char        *filename;

	/* create dialog if needed */
	if (midimap_load_dialog == NULL) {
		create_midimap_load_dialog();
	}

	/* set filename and current directory */
	if ((midimap_filename != NULL) &&
	    (strncmp(midimap_filename, user_midimap_dump_file, 10) != 0)) {
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(midimap_load_dialog), midimap_filename);
	}
	else {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(midimap_load_dialog),
		                                    user_midimap_dir);
	}

	/* run the dialog and load if necessary */
	if (gtk_dialog_run(GTK_DIALOG(midimap_load_dialog)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(midimap_load_dialog));
		read_midimap(filename);
		if (setting_midimap_file != NULL) {
			free(setting_midimap_file);
		}
		setting_midimap_file = strdup(filename);
		save_settings(NULL);
		g_free(filename);
	}

	/* save this widget for next time */
	gtk_widget_hide(midimap_load_dialog);
}


/*****************************************************************************
 * create_midimap_save_dialog()
 *****************************************************************************/
void
create_midimap_save_dialog(void)
{
	GError      *error = NULL;

	/* create new dialog only if it doesn't already exist */
	if (midimap_save_dialog == NULL) {
		midimap_save_dialog = gtk_file_chooser_dialog_new("PHASEX - Save Midimap",
		                                                  GTK_WINDOW(main_window),
		                                                  GTK_FILE_CHOOSER_ACTION_SAVE,
		                                                  GTK_STOCK_CANCEL,
		                                                  GTK_RESPONSE_CANCEL,
		                                                  GTK_STOCK_SAVE,
		                                                  GTK_RESPONSE_ACCEPT,
		                                                  NULL);
		//widget_set_custom_font (midimap_save_dialog);
		gtk_window_set_wmclass(GTK_WINDOW(midimap_save_dialog), "phasex", "phasex-dialog");
		gtk_window_set_role(GTK_WINDOW(midimap_save_dialog), "midimap-save");

#if GTK_CHECK_VERSION(2, 8, 0)
		/* perform overwrite confirmation, if we can */
		gtk_file_chooser_set_do_overwrite_confirmation
			(GTK_FILE_CHOOSER(midimap_save_dialog), TRUE);
#endif
#if GTK_CHECK_VERSION(2, 6, 0)
		/* show hidden files, if we can */
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(midimap_save_dialog), TRUE);
#endif

		/* add user midimap dir as shortcut (users cannot write to sys anyway) */
		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(midimap_save_dialog),
		                                     user_midimap_dir, &error);

		/* handle errors */
		if (error != NULL) {
			PHASEX_ERROR("Error %d: %s\n", error->code, error->message);
			g_error_free(error);
		}
	}
}


/*****************************************************************************
 * run_midimap_save_as_dialog()
 *****************************************************************************/
void
run_midimap_save_as_dialog(void)
{
	char        *filename;

	/* create dialog if needed */
	if (midimap_save_dialog == NULL) {
		create_midimap_save_dialog();
	}

	/* set filename and current directory */
	if ((midimap_filename != NULL) &&
	    (strcmp(midimap_filename, user_midimap_dump_file) != 0)) {
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(midimap_save_dialog),
		                              midimap_filename);
	}
	else {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(midimap_save_dialog),
		                                    user_midimap_dir);
	}

	/* run the dialog and save if necessary */
	if (gtk_dialog_run(GTK_DIALOG(midimap_save_dialog)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(midimap_save_dialog));
		save_midimap(filename);
		if (setting_midimap_file != NULL) {
			free(setting_midimap_file);
		}
		setting_midimap_file = strdup(filename);
		save_settings(NULL);
		g_free(filename);
	}

	/* save this widget for next time */
	gtk_widget_hide(midimap_save_dialog);
}


/*****************************************************************************
 * on_midimap_save_activate()
 *****************************************************************************/
void
on_midimap_save_activate(void)
{
	if ((midimap_filename != NULL) &&
	    (strcmp(midimap_filename, user_midimap_dump_file) != 0)) {
		save_midimap(midimap_filename);
	}
	else {
		run_midimap_save_as_dialog();
	}
}
