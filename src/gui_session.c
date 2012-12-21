/*****************************************************************************
 *
 * gui_session.c
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
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pango/pango-font.h>
#include "phasex.h"
#include "config.h"
#include "patch.h"
#include "param.h"
#include "bank.h"
#include "session.h"
#include "gui_main.h"
#include "gui_session.h"
#include "gui_navbar.h"
#include "gui_patch.h"
#include "settings.h"
#include "engine.h"
#include "debug.h"


GtkWidget       *session_load_dialog        = NULL;
GtkWidget       *session_save_dialog        = NULL;

GtkWidget       *session_load_start_spin    = NULL;
GtkWidget       *session_save_start_spin    = NULL;

GtkObject       *session_io_start_adj       = NULL;

unsigned int    session_io_start            = 0;

int             session_load_in_progress    = 0;


/*****************************************************************************
 * get_session_directory_from_entry()
 *****************************************************************************/
char *
get_session_directory_from_entry(GtkEntry *entry)
{
	SESSION     *session                = get_current_session();
	const char  *name                   = NULL;
	char        *tmpname;
	char        directory[PATH_MAX];

	/* get name from entry widget */
	if (entry != NULL) {
		name = gtk_entry_get_text(GTK_ENTRY(entry));
	}

	if ((name == NULL) || (name[0] == '\0')) {
		/* if entry is empty, get current session directory */
		tmpname = get_session_name_from_directory(session->directory);

		/* build the directory name from the session name */
		snprintf(directory, PATH_MAX, "%s/%s", user_session_dir, tmpname);

		/* free up mem */
		free(tmpname);
	}
	else {
		/* build the directory from the session name */
		snprintf(directory, PATH_MAX, "%s/%s", user_session_dir, name);
	}

	/* return a copy */
	tmpname = strdup(directory);

	return tmpname;
}


/*****************************************************************************
 * check_session_overwrite()
 *****************************************************************************/
int
check_session_overwrite(char *directory)
{
	struct stat     filestat;
	GtkWidget       *dialog;
	GtkWidget       *label;
	char            filename[PATH_MAX];
	gint            response;
	static int      recurse = 0;

	if (recurse) {
		recurse = 0;
		return 0;
	}

	/* check to see if the first patch file exists, since the filechooser */
	/* likes to create directories automatically */
	snprintf(filename, sizeof(filename), "%s/phasex-01.phx", directory);
	if (stat(filename, &filestat) == 0) {
		recurse = 1;

		/* create dialog with buttons */
		dialog = gtk_dialog_new_with_buttons("WARNING:  Session directory exists",
		                                     GTK_WINDOW(main_window),
		                                     GTK_DIALOG_DESTROY_WITH_PARENT,
		                                     GTK_STOCK_CANCEL,
		                                     GTK_RESPONSE_CANCEL,
		                                     GTK_STOCK_SAVE_AS,
		                                     1,
		                                     GTK_STOCK_SAVE,
		                                     GTK_RESPONSE_YES,
		                                     NULL);
		gtk_window_set_wmclass(GTK_WINDOW(dialog), "phasex", "phasex-dialog");
		gtk_window_set_role(GTK_WINDOW(dialog), "verify-save");

		/* Alert message */
		label = gtk_label_new("This operation will overwrite an existing session "
		                      "with the same directory name.  Save anyway? ");
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_padding(GTK_MISC(label), 8, 8);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
		//widget_set_custom_font (dialog);
		gtk_widget_show_all(dialog);

		/* Run the dialog */
		response = gtk_dialog_run(GTK_DIALOG(dialog));

		/* all finished with dialog now */
		gtk_widget_destroy(dialog);

		switch (response) {

		case GTK_RESPONSE_YES:
			/* save */
			on_session_save_activate(NULL, directory);
			break;

		case 1:
			/* save as */
			run_session_save_as_dialog(NULL, directory);
			break;

		case GTK_RESPONSE_CANCEL:
			/* cancel */
			break;
		}

		recurse = 0;
		return 1;
	}

	return 0;
}


/*****************************************************************************
 * on_save_session()
 *****************************************************************************/
void
on_save_session(GtkWidget *UNUSED(widget), gpointer data)
{
	SESSION         *session            = get_current_session();
	GtkEntry        *entry              = GTK_ENTRY(data);
	const char      *name;
	char            directory[PATH_MAX] = "\0";

	/* get name from entry widget */
	name = gtk_entry_get_text(GTK_ENTRY(entry));

	/* rebuild the directory and select directory as best as possible */
	if ((session->parent_dir != NULL) && (session->parent_dir[0] != '\0')) {
		if ((name != NULL) && (name[0] != '\0')) {
			snprintf(directory, PATH_MAX, "%s/%s", session->parent_dir, name);
		}
		else {
			snprintf(directory, PATH_MAX, "%s/Untitled-%04d",
			         session->parent_dir, visible_sess_num);
		}
	}
	else {
		if ((name != NULL) && (name[0] != '\0')) {
			snprintf(directory, PATH_MAX, "%s/%s", user_session_dir, name);
		}
		else {
			snprintf(directory, PATH_MAX, "%s/Untitled-%04d", user_session_dir, visible_sess_num);
		}
	}

	/* if the patch is still untitled, run the save as dialog */
	if (strncmp(directory, "Untitled", 8) == 0) {
		run_session_save_as_dialog(NULL, directory);
	}

	/* otherwise, just save it */
	else {
		on_session_save_activate(NULL, directory);
	}
}


/*****************************************************************************
 * on_load_session()
 *
 * Handler for the 'Load Session' button in the navbar.
 *****************************************************************************/
void
on_load_session(GtkWidget *UNUSED(widget), gpointer data)
{
	SESSION         *session            = get_current_session();
	PATCH           *patch;
	GtkEntry        *entry              = GTK_ENTRY(data);
	GtkWidget       *dialog;
	GtkWidget       *label;
	const char      *name;
	char            directory[PATH_MAX];
	gint            response;
	int             need_load           = 0;
	unsigned int    part_num;

	/* build directory from entry widget and current session directory */
	name = gtk_entry_get_text(entry);
	if (session->parent_dir == NULL) {
		snprintf(directory, PATH_MAX, "%s/%s", user_session_dir, name);
	}
	else {
		snprintf(directory, PATH_MAX, "%s/%s", session->parent_dir, name);
	}

	/* handle saving of current session based on mem mode */
	switch (setting_bank_mem_mode) {

	case BANK_MEM_AUTOSAVE:
		/* save current session if modified */
		if (session->modified) {
			on_session_save_activate(NULL, NULL);
		}
		need_load = 1;
		break;

	case BANK_MEM_WARN:
		/* if modified, warn about losing current session */
		if (session->modified) {

			/* create dialog with buttons */
			dialog = gtk_dialog_new_with_buttons("WARNING:  Session Modified",
			                                     GTK_WINDOW(main_window),
			                                     GTK_DIALOG_DESTROY_WITH_PARENT,
			                                     GTK_STOCK_CANCEL,
			                                     GTK_RESPONSE_CANCEL,
			                                     "Ignore Changes",
			                                     GTK_RESPONSE_NO,
			                                     "Save and Load New",
			                                     GTK_RESPONSE_YES,
			                                     NULL);
			gtk_window_set_wmclass(GTK_WINDOW(dialog), "phasex", "phasex-dialog");
			gtk_window_set_role(GTK_WINDOW(dialog), "verify-save");

			/* Alert message */
			label = gtk_label_new("The current session has not been saved since it "
			                      "was last modified.  Save now before loading new "
			                      "session?");
			gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
			gtk_misc_set_padding(GTK_MISC(label), 8, 8);
			gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
			//widget_set_custom_font (dialog);
			gtk_widget_show_all(dialog);

			/* Run the dialog */
			response = gtk_dialog_run(GTK_DIALOG(dialog));
			switch (response) {
			case GTK_RESPONSE_YES:
				/* save session and get ready to load a new one */
				on_session_save_activate(NULL, NULL);
				need_load = 1;
				break;
			case GTK_RESPONSE_NO:
				/* no save, only set flag to load new session */
				need_load = 1;
				break;
			case GTK_RESPONSE_CANCEL:
				/* find old session name and revert text in entry */
				if ((session->name == NULL) && (session->directory != NULL)) {
					session->name = get_session_name_from_directory(session->directory);
				}
				update_gui_session_name();
				/* cancel out on loading new session */
				need_load = 0;
				break;
			}
			gtk_widget_destroy(dialog);
		}
		/* if not modified, just load new session */
		else {
			need_load = 1;
		}
		break;

	case BANK_MEM_PROTECT:
		/* explicitly don't save in protect mode */
		need_load = 1;
		break;
	}

	/* load session specified by entry or assign an Untitled name */
	if (need_load) {
		session_load_in_progress = 1;
		if ((need_load = load_session(directory, visible_sess_num)) != 0) {
			snprintf(directory, PATH_MAX, "%s/%s", user_session_dir, name);
			need_load = load_session(directory, visible_sess_num);
		}
		for (part_num = 0; part_num < MAX_PARTS; part_num++) {
			visible_prog_num[part_num] = 0;
			patch = set_active_patch(visible_sess_num, part_num, 0);
			init_patch_state(patch);
			if (part_num == visible_part_num) {
				update_gui_patch(patch, 0);
			}
		}
		if (session->name == NULL) {
			sprintf(directory, "Untitled-%04d", visible_sess_num);
			session->name = strdup(directory);
		}
		session->modified = 0;
		session_load_in_progress = 0;
		update_gui_patch(NULL, 0);
		save_session_bank();
	}
}


/*****************************************************************************
 * set_session_io_start()
 *****************************************************************************/
void
set_session_io_start(GtkWidget *widget, gpointer UNUSED(data))
{
	if (GTK_IS_ADJUSTMENT(widget)) {
		session_io_start =
			(unsigned int) gtk_adjustment_get_value(GTK_ADJUSTMENT(widget)) - 1;
	}
	else if (GTK_IS_SPIN_BUTTON(widget)) {
		session_io_start =
			(unsigned int) gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)) - 1;
	}
}


/*****************************************************************************
 * create_session_load_dialog()
 *****************************************************************************/
void
create_session_load_dialog(void)
{
	GError          *error  = NULL;
	GtkWidget       *hbox;
	GtkWidget       *label;
	int             new_adj = (session_io_start_adj == NULL);

	/* this should only need to happen once */
	if (session_load_dialog == NULL) {

		/* create dialog */
		session_load_dialog = gtk_file_chooser_dialog_new("PHASEX - Load Session",
		                                                  GTK_WINDOW(main_window),
		                                                  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		                                                  GTK_STOCK_CANCEL,
		                                                  GTK_RESPONSE_CANCEL,
		                                                  GTK_STOCK_OPEN,
		                                                  GTK_RESPONSE_ACCEPT,
		                                                  NULL);
		gtk_window_set_wmclass(GTK_WINDOW(session_load_dialog), "phasex", "phasex-load");
		gtk_window_set_role(GTK_WINDOW(session_load_dialog), "session-load");
		gtk_file_chooser_set_preview_widget_active
			(GTK_FILE_CHOOSER(session_load_dialog), FALSE);

		/* create spinbutton control for starting program number */
		hbox = gtk_hbox_new(FALSE, 8);
		label = gtk_label_new("Load multiple sessions staring at session #:");
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);

		if (new_adj) {
			session_io_start_adj = gtk_adjustment_new(0, 1, PATCH_BANK_SIZE, 1, 8, 0);
		}
		session_load_start_spin = gtk_spin_button_new(GTK_ADJUSTMENT(session_io_start_adj), 0, 0);
		gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(session_load_start_spin), TRUE);
		gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(session_load_start_spin),
		                                  GTK_UPDATE_IF_VALID);
		gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(session_load_start_spin), TRUE);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(session_io_start_adj), (visible_sess_num + 1));
		gtk_box_pack_start(GTK_BOX(hbox), session_load_start_spin, FALSE, FALSE, 8);

		if (new_adj) {
			g_signal_connect(GTK_OBJECT(session_load_start_spin), "value_changed",
			                 GTK_SIGNAL_FUNC(set_session_io_start),
			                 (gpointer) session_io_start_adj);
		}

		gtk_widget_show_all(hbox);
		gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(session_load_dialog), hbox);

		/* realize the file chooser before telling it about files */
		gtk_widget_realize(session_load_dialog);

		/* start in user session dir (usually ~/.phasex/user-sessions) */
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(session_load_dialog),
		                                    user_session_dir);

		/* allow selection of multiple sessions */
		gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(session_load_dialog), TRUE);

#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(session_load_dialog), TRUE);
#endif

		/* add user session dirs as shortcut folder */
		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(session_load_dialog),
		                                     user_session_dir, &error);
		if (error != NULL) {
			PHASEX_ERROR("Error %d: %s\n", error->code, error->message);
			g_error_free(error);
		}
	}
}


/*****************************************************************************
 * run_session_load_dialog()
 *
 * Callback for running the 'Load Session' dialog from the main menu.
 *****************************************************************************/
void
run_session_load_dialog(GtkWidget *UNUSED(widget), gpointer UNUSED(data))
{
	SESSION         *session    = get_current_session();
	PATCH           *patch;
	DIR_LIST        *pdir       = session_dir_list;
	char            *directory;
	GSList          *file_list;
	GSList          *cur;
	GError          *error;
	unsigned int    sess_num    = session_io_start;
	unsigned int    part_num;

	/* create dialog if needed */
	if (session_load_dialog == NULL) {
		create_session_load_dialog();
	}

	gtk_widget_show(session_load_dialog);

	/* add all session directories to shortcuts */
	while (pdir != NULL) {
		if (!pdir->load_shortcut) {
			error = NULL;
			gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(session_load_dialog),
			                                     pdir->name, &error);
			if (error != NULL) {
				PHASEX_ERROR("Error %d: %s\n", error->code, error->message);
				g_error_free(error);
			}
			pdir->load_shortcut = 1;
		}
		pdir = pdir->next;
	}

	/* set filename and current directory */
	if ((session->directory != NULL) && (* (session->directory) != '\0')) {
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(session_load_dialog), session->directory);
	}
	else if ((session->parent_dir != NULL) && (* (session->parent_dir) != '\0')) {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(session_load_dialog),
		                                    session->parent_dir);
	}
	else {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(session_load_dialog),
		                                    user_session_dir);
	}

	/* set position in patch bank to start loading patches */
	session_io_start = visible_sess_num;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(session_io_start_adj), (session_io_start + 1));

	/* run the dialog and load if necessary */
	if (gtk_dialog_run(GTK_DIALOG(session_load_dialog)) == GTK_RESPONSE_ACCEPT) {

		/* get list of selected files from chooser */
		file_list = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(session_load_dialog));
		if (file_list != NULL) {
			session_io_start =
				(unsigned int) gtk_adjustment_get_value(GTK_ADJUSTMENT(session_io_start_adj)) - 1;

			/* read in each session and increment session bank slot number */
			sess_num = session_io_start;
			session_load_in_progress = 1;
			cur = file_list;
			while (cur != NULL) {
				directory = (char *) cur->data;
				if (load_session(directory, sess_num) == 0) {
					if (sess_num == visible_sess_num) {
						for (part_num = 0; part_num < MAX_PARTS; part_num++) {
							visible_prog_num[part_num] = 0;
							patch = set_active_patch(visible_sess_num, part_num, 0);
							init_patch_state(patch);
							if (part_num == visible_part_num) {
								update_gui_patch(patch, 0);
							}
						}
					}
					sess_num++;
				}
				if (sess_num >= SESSION_BANK_SIZE) {
					break;
				}
				cur = g_slist_next(cur);
			}
			session_load_in_progress = 0;
			g_slist_free(file_list);

			update_gui_patch_name();
			update_gui_session_name();

			save_session_bank();
		}

	}

	/* save this widget for next time */
	gtk_widget_hide(session_load_dialog);
}


/*****************************************************************************
 * create_session_save_dialog()
 *****************************************************************************/
void
create_session_save_dialog(void)
{
	GError          *error  = NULL;
	GtkWidget       *hbox;
	GtkWidget       *label;
	int             new_adj = (session_io_start_adj == NULL);

	/* this should only need to happen once */
	if (session_save_dialog == NULL) {

		/* create dialog */
		session_save_dialog = gtk_file_chooser_dialog_new("PHASEX - Save Session",
		                                                  GTK_WINDOW(main_window),
		                                                  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		                                                  GTK_STOCK_CANCEL,
		                                                  GTK_RESPONSE_CANCEL,
		                                                  GTK_STOCK_SAVE,
		                                                  GTK_RESPONSE_ACCEPT,
		                                                  NULL);
		gtk_window_set_wmclass(GTK_WINDOW(session_save_dialog), "phasex", "phasex-save");
		gtk_window_set_role(GTK_WINDOW(session_save_dialog), "session-save");
		gtk_file_chooser_set_preview_widget_active
			(GTK_FILE_CHOOSER(session_save_dialog), FALSE);

		/* create spinbutton control of session number */
		hbox = gtk_hbox_new(FALSE, 8);
		label = gtk_label_new("Save into session #:");
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);

		if (new_adj) {
			session_io_start_adj = gtk_adjustment_new(0, 1, PATCH_BANK_SIZE, 1, 8, 0);
		}
		session_save_start_spin = gtk_spin_button_new(GTK_ADJUSTMENT(session_io_start_adj), 0, 0);
		gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(session_save_start_spin), TRUE);
		gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(session_save_start_spin),
		                                  GTK_UPDATE_IF_VALID);
		gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(session_save_start_spin), TRUE);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(session_io_start_adj), (session_io_start + 1));
		gtk_box_pack_start(GTK_BOX(hbox), session_save_start_spin, FALSE, FALSE, 8);

		if (new_adj) {
			g_signal_connect(GTK_OBJECT(session_save_start_spin), "value_changed",
			                 GTK_SIGNAL_FUNC(set_session_io_start),
			                 (gpointer) session_io_start_adj);
		}

		gtk_widget_show_all(hbox);
		gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(session_save_dialog), hbox);

		/* realize the file chooser before telling it about files */
		gtk_widget_realize(session_save_dialog);

#if GTK_CHECK_VERSION(2, 8, 0)
		/* this can go away once manual overwrite checks are proven to work properly */
		gtk_file_chooser_set_do_overwrite_confirmation
			(GTK_FILE_CHOOSER(session_save_dialog), TRUE);
#endif
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(session_save_dialog), TRUE);
#endif

		/* add user session dir as shortcut folder (user cannot write to sys) */
		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(session_save_dialog),
		                                     user_session_dir, &error);
		if (error != NULL) {
			PHASEX_ERROR("Error %d: %s\n", error->code, error->message);
			g_error_free(error);
		}

		/* start in user session dir (usually ~/.phasex/user-sessions) */
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(session_save_dialog),
		                                    user_session_dir);
	}
}


/*****************************************************************************
 * run_session_save_as_dialog()
 *****************************************************************************/
void
run_session_save_as_dialog(GtkWidget *UNUSED(widget), gpointer data)
{
	SESSION         *session        = get_current_session();
	char            *directory      = (char *) data;
	char            *session_dir;
	DIR_LIST        *pdir           = session_dir_list;
	GError          *error;

	/* create dialog if needed */
	if (session_save_dialog == NULL) {
		create_session_save_dialog();
	}

	/* add all session dirs as shortcut folders */
	while (pdir != NULL) {
		if ((!pdir->save_shortcut)) {
			error = NULL;
			gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(session_save_dialog),
			                                     pdir->name, &error);
			if (error != NULL) {
				PHASEX_ERROR("Error %d: %s\n", error->code, error->message);
				g_error_free(error);
			}
			pdir->save_shortcut = 1;
		}
		pdir = pdir->next;
	}

	/* set filename and current directory */
	if ((directory == NULL) || (*directory == '\0')) {
		directory = session->directory;
	}

	/* if we have a directory, and it's not the sessiondump, set and select it */
	if ((directory != NULL) && (strcmp(directory, user_session_dump_dir) != 0)) {
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(session_save_dialog), directory);
	}

	/* if there is no filename, try to set the current directory */
	else if ((session->parent_dir != NULL) && (* (session->parent_dir) != '\0')) {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(session_save_dialog),
		                                    session->parent_dir);
	}
	else {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(session_save_dialog),
		                                    user_session_dir);
	}

	/* set position in session bank to save session */
	/* session_io_start should already be set properly at this point. */
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(session_save_start_spin), (session_io_start + 1));

	/* run the dialog and save if necessary */
	if (gtk_dialog_run(GTK_DIALOG(session_save_dialog)) == GTK_RESPONSE_ACCEPT) {
		session_dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(session_save_dialog));

		session_io_start =
			(unsigned int) gtk_spin_button_get_value(GTK_SPIN_BUTTON(session_save_start_spin)) - 1;

		/* hide dialog and save session */
		gtk_widget_hide(session_save_dialog);
		switch (setting_bank_mem_mode) {
		case BANK_MEM_AUTOSAVE:
			/* save session without overwrite protection */
			if (save_session(session_dir, session_io_start) == 0) {
				update_gui_session_name();
				save_session_bank();
			}
			break;
		case BANK_MEM_WARN:
		case BANK_MEM_PROTECT:
			/* save session with overwrite protection */
			if (!check_session_overwrite(session_dir)) {
				if (save_session(session_dir, session_io_start) == 0) {
					update_gui_session_name();
					save_session_bank();
				}
			}
			break;
		}

		g_free(session_dir);
	}
	else {
		/* save this widget for next time */
		gtk_widget_hide(session_save_dialog);
	}
}


/*****************************************************************************
 * on_session_save_activate()
 *****************************************************************************/
void
on_session_save_activate(GtkWidget *UNUSED(widget), gpointer data)
{
	SESSION *session    = get_current_session();
	char    *directory  = (char *) data;

	/* if no directory was provided, use the current session directory */
	if ((directory == NULL) || (directory[0] == '\0')) {
		if ((session->directory != NULL) && (session->directory[0] != '\0')) {
			directory = get_session_directory_from_entry(GTK_ENTRY(session_entry));
			session->directory = directory;
		}
		else {
			directory = session->directory;
		}
	}

	/* if we still don't have a directory, run the save-as dialog */
	if (directory == NULL) {
		run_session_save_as_dialog(NULL, NULL);
	}

	/* save the session with the given directory */
	else {
		switch (setting_bank_mem_mode) {
		case BANK_MEM_AUTOSAVE:
			/* save session without overwrite protection */
			if (save_session(directory, session_io_start) == 0) {
				update_gui_session_name();
				save_session_bank();
			}
			break;
		case BANK_MEM_WARN:
		case BANK_MEM_PROTECT:
			/* save session with overwrite protection */
			if (!check_session_overwrite(directory)) {
				if (save_session(directory, session_io_start) == 0) {
					update_gui_session_name();
					save_session_bank();
				}
			}
			break;
		}
	}
}


/*****************************************************************************
 * select_session()
 *****************************************************************************/
void
select_session(GtkWidget *widget, gpointer data)
{
	SESSION         *session    = get_current_session();
	PATCH           *patch;
	GtkWidget       *dialog;
	GtkWidget       *label;
	GtkEntry        *entry      = NULL;
	char            *directory  = NULL;
	gint            response;
	int             need_select = 1;
	unsigned int    sess_num;
	unsigned int    part_num;

	/* called as a gui callback */
	/* the widgets we need to reference are callback args */
	if (data == NULL) {
		entry = GTK_ENTRY(session_entry);
	}
	else {
		entry = GTK_ENTRY(data);
	}
	/* get name from entry widget or strip current patch directory */
	directory = get_session_directory_from_entry(entry);

	/* get session number from spin button if called from gui */
	sess_num = (unsigned int)(gtk_adjustment_get_value(GTK_ADJUSTMENT(widget))) - 1;

	/* only continue if we're really changing the session number */
	if (sess_num != visible_sess_num) {

		/* whether or not to save depends on memory mode */
		switch (setting_bank_mem_mode) {

		case BANK_MEM_AUTOSAVE:
			/* no canceling in autosave mode */
			need_select = 1;

			/* save current session if modified */
			if (session->modified) {
				on_session_save_activate(NULL, directory);
			}

			break;

		case BANK_MEM_WARN:
			/* this is set now, and may be canceled */
			need_select = 1;

			/* if modified, warn about losing current patch and give option to save */
			if (session->modified) {

				/* create dialog with buttons */
				dialog = gtk_dialog_new_with_buttons("WARNING:  Session Modified",
				                                     GTK_WINDOW(main_window),
				                                     GTK_DIALOG_DESTROY_WITH_PARENT,
				                                     GTK_STOCK_CANCEL,
				                                     GTK_RESPONSE_CANCEL,
				                                     "Ignore Changes",
				                                     GTK_RESPONSE_NO,
				                                     "Save and Select New",
				                                     GTK_RESPONSE_YES,
				                                     NULL);
				gtk_window_set_wmclass(GTK_WINDOW(dialog), "phasex", "phasex-dialog");
				gtk_window_set_role(GTK_WINDOW(dialog), "verify-save");

				/* Alert message */
				label = gtk_label_new("The current session has not been saved "
				                      "since patches were last modified.  Save "
				                      "now before selecting new session?");
				gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
				gtk_misc_set_padding(GTK_MISC(label), 8, 8);
				gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
				//widget_set_custom_font (dialog);
				gtk_widget_show_all(dialog);

				/* Run the dialog */
				response = gtk_dialog_run(GTK_DIALOG(dialog));

				switch (response) {

				case GTK_RESPONSE_YES:
					/* save session and set flag to select session from bank */
					on_session_save_activate(NULL, NULL);
					need_select = 1;
					break;

				case GTK_RESPONSE_NO:
					/* don't save, but still set flag to select new */
					need_select = 1;
					break;

				case GTK_RESPONSE_CANCEL:
					/* set spin button back to
					   current session and don't
					   select new */
					gtk_adjustment_set_value(GTK_ADJUSTMENT(session_adj), (visible_sess_num + 1));
					need_select = 0;
					break;
				}

				/* all finished with dialog now */
				gtk_widget_destroy(dialog);
			}
			break;

		case BANK_MEM_PROTECT:
			/* explicitly don't save */
			need_select = 1;
			break;
		}

		/* free name, now that we're done with it */
		if (directory != NULL) {
			free(directory);
		}
	}
	else if (widget != NULL) {
		need_select = 0;
	}

	/* select session specified by spinbutton or init new session */
	if (need_select) {
		visible_sess_num = sess_num;
		session_io_start = sess_num;
		session = get_session(sess_num);
		session_load_in_progress = 1;

		/* place current session name into session entry */
		update_gui_session_name();

		for (part_num = 0; part_num < MAX_PARTS; part_num++) {
			visible_prog_num[part_num] = session->prog_num[part_num];

			/* set active patch for this part in engine */
			patch = set_active_patch(sess_num, part_num, session->prog_num[part_num]);
			init_patch_state(patch);

			/* GUI changes for visible part only */
			if (part_num == visible_part_num) {
				update_gui_patch(patch, 0);
			}
		}

		update_gui_session_modified();
		session_load_in_progress = 0;
	}
}
