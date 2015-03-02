/*****************************************************************************
 *
 * gui_bank.c
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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pango/pango-font.h>
#include "phasex.h"
#include "config.h"
#include "patch.h"
#include "param.h"
#include "bank.h"
#include "session.h"
#include "settings.h"
#include "engine.h"
#include "midi_process.h"
#include "midi_event.h"
#include "timekeeping.h"
#include "buffer.h"
#include "gui_main.h"
#include "gui_menubar.h"
#include "gui_bank.h"
#include "gui_session.h"
#include "gui_param.h"
#include "gui_patch.h"
#include "gui_navbar.h"
#include "gtkknob.h"
#include "debug.h"


GtkWidget       *patch_load_dialog      = NULL;
GtkWidget       *patch_save_dialog      = NULL;

GtkWidget       *patch_load_start_spin  = NULL;
GtkWidget       *patch_save_start_spin  = NULL;

GtkObject       *patch_io_start_adj     = NULL;

unsigned int    patch_io_start          = 0;


/*****************************************************************************
 * get_patch_filename_from_entry()
 *****************************************************************************/
char *
get_patch_filename_from_entry(GtkEntry *entry)
{
	PATCH       *patch;
	const char  *name;
	char        *tmpname;
	char        filename[PATH_MAX];

	/* get name from entry widget */
	name = gtk_entry_get_text(GTK_ENTRY(entry));

	if ((name == NULL) || (name[0] == '\0')) {
		patch = get_visible_patch();

		/* if entry is empty, get current patch filename */
		tmpname = get_patch_name_from_filename(patch->filename);

		/* build the filename from the patch name */
		snprintf(filename, PATH_MAX, "%s/%s.phx", user_patch_dir, tmpname);

		/* free up mem */
		free(tmpname);
	}
	else {
		/* build the filename from the patch name */
		snprintf(filename, PATH_MAX, "%s/%s.phx", user_patch_dir, name);
	}

	/* return a copy */
	tmpname = strdup(filename);
	return tmpname;
}


/*****************************************************************************
 * check_patch_overwrite()
 *****************************************************************************/
int
check_patch_overwrite(char *filename)
{
	struct stat     filestat;
	GtkWidget       *dialog;
	GtkWidget       *label;
	gint            response;
	static int      recurse = 0;

	if (recurse) {
		recurse = 0;
		return 0;
	}

	/* check to see if file exists */
	if (stat(filename, &filestat) == 0) {
		recurse = 1;

		/* create dialog with buttons */
		dialog = gtk_dialog_new_with_buttons("WARNING:  Patch file exists",
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
		label = gtk_label_new("This operation will overwrite an existing "
		                      "patch with the same filename.  Save anyway? ");
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
			on_patch_save_activate(NULL, filename);
			break;

		case 1:
			/* save as */
			run_patch_save_as_dialog(NULL, filename);
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
 * set_visible_part()
 *****************************************************************************/
void
set_visible_part(GtkWidget *widget, gpointer data, GtkWidget *widget2)
{
	unsigned int    new_part    = (long unsigned int) data % MAX_PARTS;

	/* called from menu */
	if (widget == NULL) {
		if ((widget2 == NULL) || !GTK_IS_CHECK_MENU_ITEM(widget2) ||
		    !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget2))) {
			return;
		}

		/* set menu radiobutton */
		if ((menu_item_part[new_part] != NULL) &&
		    !gtk_check_menu_item_get_active(menu_item_part[new_part])) {
			gtk_check_menu_item_set_active(menu_item_part[new_part], TRUE);
		}

		/* set adjustment for part selector spin button */
		if (part_adj != NULL) {
			gtk_adjustment_set_value(GTK_ADJUSTMENT(part_adj), (new_part + 1));
		}
	}

	/* called from spinbutton */
	else {
		new_part = (unsigned int) gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)) - 1;
	}

	/* only deal with real changes */
	if (new_part != visible_part_num) {
		switch_part(widget, NULL);
	}
}


/*****************************************************************************
 * switch_part()
 *****************************************************************************/
void
switch_part(GtkWidget *widget, gpointer UNUSED(data))
{
	PATCH           *patch;
	PART            *new_part;
	unsigned int    old_part_num    = visible_part_num;
	unsigned int    new_part_num;
	unsigned int    prog;

	if (widget != NULL) {
		new_part_num = (unsigned int) gtk_adjustment_get_value(GTK_ADJUSTMENT(widget)) - 1;
	}
	else {
		new_part_num = (unsigned int)
			gtk_adjustment_get_value(GTK_ADJUSTMENT(part_adj)) - 1;
	}

	if (old_part_num != new_part_num) {
		/* keep track of currently focused so part selector doesn't grab */
		/* when keyboard accelerators are used. */
		focus_widget = gtk_window_get_focus(GTK_WINDOW(main_window));

		new_part = get_part(new_part_num);

		visible_part_num = new_part_num;

		prog = get_visible_program_number();
		patch_io_start = prog;

		/* set adjustments and not widget values so callbacks don't get called */
		gtk_adjustment_set_value(GTK_ADJUSTMENT(program_adj), (prog + 1));
		gtk_adjustment_set_value(GTK_ADJUSTMENT(midi_channel_adj), new_part->midi_channel);

		/* get the patch that is supposed to now be visible */
		patch = get_visible_patch();

		/* make it the visible patch */
		update_gui_patch(patch, 1);
	}
}


/*****************************************************************************
 * select_program()
 *****************************************************************************/
void
select_program(GtkWidget *widget, gpointer data)
{
	SESSION         *session    = get_current_session();
	PATCH           *patch      = get_visible_patch();
	GtkWidget       *dialog;
	GtkWidget       *label;
	GtkEntry        *entry      = NULL;
	char            *filename   = NULL;
	gint            response;
	int             need_select = 1;
	unsigned int    prog;

	if (data == NULL) {
		entry = GTK_ENTRY(program_entry);
	}
	else {
		entry = GTK_ENTRY(data);
	}
	/* get name from entry widget or strip current patch filename */
	filename = get_patch_filename_from_entry(entry);

	/* get program number from spin button */
	prog = (unsigned int)(gtk_adjustment_get_value(GTK_ADJUSTMENT(widget))) - 1;

	/* only continue if we're really changing the program number */
	if (prog != visible_prog_num[visible_part_num]) {

		/* whether or not to save depends on memory mode */
		switch (setting_bank_mem_mode) {

		case BANK_MEM_AUTOSAVE:
			/* save current patch if modified */
			if (patch->modified) {
				on_patch_save_activate(NULL, filename);
			}

			/* no canceling in autosave mode */
			need_select = 1;

			break;

		case BANK_MEM_WARN:
			/* this is set now, and may be canceled */
			need_select = 1;

			/* if modified, warn about losing current patch and give option to save */
			if (patch->modified) {

				/* create dialog with buttons */
				dialog = gtk_dialog_new_with_buttons("WARNING:  Patch Modified",
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
				label = gtk_label_new("The current patch has not been saved since "
				                      "it was last modified.  Save now before "
				                      "selecting new patch?");
				gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
				gtk_misc_set_padding(GTK_MISC(label), 8, 8);
				gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
				//widget_set_custom_font (dialog);
				gtk_widget_show_all(dialog);

				/* Run the dialog */
				response = gtk_dialog_run(GTK_DIALOG(dialog));

				switch (response) {

				case GTK_RESPONSE_YES:
					/* save patch and set flag to select new from bank */
					on_patch_save_activate(NULL, filename);
					need_select = 1;
					break;

				case GTK_RESPONSE_NO:
					/* don't save, but still set flag to select new */
					need_select = 1;
					break;

				case GTK_RESPONSE_CANCEL:
					/* Set spin button back to current
					   program and don't select new. */
					gtk_adjustment_set_value
						(GTK_ADJUSTMENT(widget),
						 (visible_prog_num[visible_part_num] + 1));
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
		if (filename != NULL) {
			free(filename);
		}
	}
	else if (widget != NULL) {
		need_select = 0;
	}

	/* select patch specified by spinbutton */
	if (need_select) {
		patch_io_start                      = prog;
		visible_prog_num[visible_part_num]  = prog;
		session->prog_num[visible_part_num] = prog;
		/* set active patch in engine */
		patch = set_active_patch(visible_sess_num, visible_part_num, prog);
		init_patch_state(patch);
		/* set visible patch in gui */
		update_gui_patch(patch, 0);
		if (!session_load_in_progress) {
			session->modified = 1;
		}
		update_gui_session_modified();
	}
}


/*****************************************************************************
 * save_program()
 *****************************************************************************/
void
save_program(GtkWidget *UNUSED(widget), gpointer data)
{
	const char      *name;
	PATCH           *patch              = get_visible_patch();
	GtkEntry        *entry              = GTK_ENTRY(data);
	unsigned int    prog                = get_visible_program_number();
	char            filename[PATH_MAX]  = "\0";

	/* get name from entry widget */
	name = gtk_entry_get_text(GTK_ENTRY(entry));

	/* rebuild the filename and select directory as best as possible */
	if ((patch->directory != NULL) &&
	    (patch->directory[0] != '\0') &&
	    (strcmp(patch->directory, PATCH_DIR) != 0)) {
		if ((name != NULL) && (name[0] != '\0')) {
			snprintf(filename, PATH_MAX, "%s/%s.phx",
			         patch->directory, name);
		}
		else {
			snprintf(filename, PATH_MAX, "%s/Untitled-%04d.phx",
			         patch->directory, (prog + 1));
		}
	}
	else {
		if ((name != NULL) && (name[0] != '\0')) {
			snprintf(filename, PATH_MAX, "%s/%s.phx",
			         user_patch_dir, name);
		}
		else {
			snprintf(filename, PATH_MAX, "%s/Untitled-%04d.phx",
			         user_patch_dir, (prog + 1));
		}
	}

	/* if the patch is still untitled, run the save as dialog */
	if (strncmp(filename, "Untitled", 8) == 0) {
		run_patch_save_as_dialog(NULL, filename);
	}

	/* otherwise, just save it */
	else {
		on_patch_save_activate(NULL, filename);
	}
}


/*****************************************************************************
 * load_program()
 *****************************************************************************/
void
load_program(GtkWidget *UNUSED(widget), gpointer data)
{
	SESSION         *session    = get_current_session();
	PATCH           *patch      = get_visible_patch();
	GtkEntry        *entry      = GTK_ENTRY(data);
	GtkWidget       *dialog;
	GtkWidget       *label;
	const char      *name;
	char            filename[PATH_MAX];
	gint            response;
	int             need_load   = 0;

	/* build filename from entry widget and current patch directory */
	name = gtk_entry_get_text(entry);
	if (patch->directory != NULL) {
		snprintf(filename, PATH_MAX, "%s/%s.phx", patch->directory, name);
	}
	else {
		snprintf(filename, PATH_MAX, "%s/%s.phx", user_patch_dir, name);
	}

	/* handle saving of current patch based on mem mode */
	switch (setting_bank_mem_mode) {

	case BANK_MEM_AUTOSAVE:
		/* save current patch if modified */
		if (patch->modified) {
			on_patch_save_activate(NULL, NULL);
		}
		need_load = 1;
		break;

	case BANK_MEM_WARN:
		/* if modified, warn about losing current patch */
		if (patch->modified) {

			/* create dialog with buttons */
			dialog = gtk_dialog_new_with_buttons("WARNING:  Patch Modified",
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
			label = gtk_label_new("The current patch has not been saved since "
			                      "it was last modified.  Save now before "
			                      "loading new patch?");
			gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
			gtk_misc_set_padding(GTK_MISC(label), 8, 8);
			gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
			//widget_set_custom_font (dialog);
			gtk_widget_show_all(dialog);

			/* Run the dialog */
			response = gtk_dialog_run(GTK_DIALOG(dialog));
			switch (response) {
			case GTK_RESPONSE_YES:
				/* save patch and get ready to load a new one */
				on_patch_save_activate(NULL, NULL);
				need_load = 1;
				break;
			case GTK_RESPONSE_NO:
				/* no save, only set flag to load new patch */
				need_load = 1;
				break;
			case GTK_RESPONSE_CANCEL:
				/* find old patch name and revert text in entry */
				if ((patch->name == NULL) && (patch->filename != NULL)) {
					patch->name =
						get_patch_name_from_filename(patch->filename);
				}
				update_gui_patch_name();
				/* cancel out on loading new patch */
				need_load = 0;
				break;
			}
			gtk_widget_destroy(dialog);
		}
		/* if not modified, just load new patch */
		else {
			need_load = 1;
		}
		break;

	case BANK_MEM_PROTECT:
		/* explicitly don't save in protect mode */
		need_load = 1;
		break;
	}

	/* load patch specified by entry or init new patch */
	if (need_load) {
		if (read_patch(filename, patch) != 0) {
			snprintf(filename, PATH_MAX, "%s/%s.phx", user_patch_dir, name);
			if (read_patch(filename, patch) != 0) {
				snprintf(filename, PATH_MAX, "%s/%s.phx", PATCH_DIR, name);
				if (read_patch(filename, patch) != 0) {
					if (read_patch(user_default_patch, patch) != 0) {
						read_patch(sys_default_patch, patch);
					}
				}
			}
		}
		init_patch_state(patch);
		if (patch->name == NULL) {
			sprintf(filename, "Untitled-%04d",
			        (visible_prog_num[visible_part_num] + 1));
			patch->name = strdup(filename);
		}
		update_gui_patch(patch, 0);
		session->modified = 1;
	}
}


/*****************************************************************************
 * bank_autosave_activate()
 *****************************************************************************/
void
bank_autosave_activate(GtkWidget *widget, gpointer UNUSED(data))
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		setting_bank_mem_mode = BANK_MEM_AUTOSAVE;
	}
}


/*****************************************************************************
 * bank_warn_activate()
 *****************************************************************************/
void
bank_warn_activate(GtkWidget *widget, gpointer UNUSED(data))
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		setting_bank_mem_mode = BANK_MEM_WARN;
	}
}


/*****************************************************************************
 * bank_protect_activate()
 *****************************************************************************/
void
bank_protect_activate(GtkWidget *widget, gpointer UNUSED(data))
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		setting_bank_mem_mode = BANK_MEM_PROTECT;
	}
}


/*****************************************************************************
 * set_patch_io_start()
 *****************************************************************************/
void
set_patch_io_start(GtkWidget *widget, gpointer UNUSED(data))
{
	if (GTK_IS_ADJUSTMENT(widget)) {
		patch_io_start = (unsigned int) gtk_adjustment_get_value(GTK_ADJUSTMENT(widget)) - 1;
	}
	else if (GTK_IS_SPIN_BUTTON(widget)) {
		patch_io_start = (unsigned int) gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)) - 1;
	}
}


/*****************************************************************************
 * create_patch_load_dialog()
 *****************************************************************************/
void
create_patch_load_dialog(void)
{
	GError          *error  = NULL;
	GtkWidget       *hbox;
	GtkWidget       *label;
	int             new_adj = (patch_io_start_adj == NULL);

	/* this should only need to happen once */
	if (patch_load_dialog == NULL) {

		/* create dialog */
		patch_load_dialog = gtk_file_chooser_dialog_new("PHASEX - Load Patch",
		                                                GTK_WINDOW(main_window),
		                                                GTK_FILE_CHOOSER_ACTION_OPEN,
		                                                GTK_STOCK_CANCEL,
		                                                GTK_RESPONSE_CANCEL,
		                                                GTK_STOCK_OPEN,
		                                                GTK_RESPONSE_ACCEPT,
		                                                NULL);
		gtk_window_set_wmclass(GTK_WINDOW(patch_load_dialog), "phasex", "phasex-load");
		gtk_window_set_role(GTK_WINDOW(patch_load_dialog), "patch-load");
		gtk_file_chooser_set_preview_widget_active
			(GTK_FILE_CHOOSER(patch_load_dialog), FALSE);

		/* create spinbutton control for starting program number */
		hbox = gtk_hbox_new(FALSE, 8);
		label = gtk_label_new("Load multiple patches staring at program #:");
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);

		if (new_adj) {
			patch_io_start_adj = gtk_adjustment_new(0, 1, PATCH_BANK_SIZE, 1, 8, 0);
		}
		patch_load_start_spin =
			gtk_spin_button_new(GTK_ADJUSTMENT(patch_io_start_adj), 0, 0);
		gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(patch_load_start_spin), TRUE);
		gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(patch_load_start_spin),
		                                  GTK_UPDATE_IF_VALID);
		gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(patch_load_start_spin), TRUE);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(patch_io_start_adj),
		                         (visible_prog_num[visible_part_num] + 1));
		gtk_box_pack_start(GTK_BOX(hbox), patch_load_start_spin, FALSE, FALSE, 8);

		if (new_adj) {
			g_signal_connect(GTK_OBJECT(patch_load_start_spin), "value_changed",
			                 GTK_SIGNAL_FUNC(set_patch_io_start),
			                 (gpointer) patch_load_start_spin);
		}

		gtk_widget_show_all(hbox);
		gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(patch_load_dialog), hbox);

		/* realize the file chooser before telling it about files */
		gtk_widget_realize(patch_load_dialog);

		/* start in user patch dir (usually ~/.phasex/user-patches) */
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(patch_load_dialog), user_patch_dir);

		/* allow multiple patches to be selected */
		gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(patch_load_dialog), TRUE);

#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(patch_load_dialog), TRUE);
#endif

		/* add system and user patch dirs as shortcut folders */
		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(patch_load_dialog),
		                                     PATCH_DIR, &error);
		if (error != NULL) {
			PHASEX_ERROR("Error %d: %s\n", error->code, error->message);
			g_error_free(error);
		}
		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(patch_load_dialog),
		                                     user_patch_dir, &error);
		if (error != NULL) {
			PHASEX_ERROR("Error %d: %s\n", error->code, error->message);
			g_error_free(error);
		}

		/* add filename filters for .phx and all files */
		if (file_filter_all != NULL) {
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(patch_load_dialog), file_filter_all);
		}
		if (file_filter_patches != NULL) {
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(patch_load_dialog), file_filter_patches);
			gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(patch_load_dialog), file_filter_patches);
		}
	}
}


/*****************************************************************************
 * run_patch_load_dialog()
 *****************************************************************************/
void
run_patch_load_dialog(GtkWidget *UNUSED(widget), gpointer UNUSED(data))
{
	PATCH           *patch      = get_visible_patch();
	DIR_LIST        *pdir       = patch_dir_list;
	char            *filename;
	GSList          *file_list;
	GSList          *cur;
	GError          *error;
	unsigned int    prog        = patch_io_start;

	/* create dialog if needed */
	if (patch_load_dialog == NULL) {
		create_patch_load_dialog();
	}

	gtk_widget_show(patch_load_dialog);

	/* add all patch directories to shortcuts */
	while (pdir != NULL) {
		if (!pdir->load_shortcut) {
			error = NULL;
			gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(patch_load_dialog),
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
	if ((patch->filename != NULL) && (* (patch->filename) != '\0') &&
	    (strncmp(patch->filename, "/tmp/patch", 10) != 0)) {
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(patch_load_dialog), patch->filename);
	}
	else if ((patch->directory != NULL) && (* (patch->directory) != '\0')) {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(patch_load_dialog), patch->directory);
	}
	else {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(patch_load_dialog), user_patch_dir);
	}

	/* set filter and hope that it takes */
	if (file_filter_patches != NULL) {
		gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(patch_load_dialog), file_filter_patches);
	}

	/* set position in patch bank to start loading patches */
	patch_io_start = get_visible_program_number();
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(patch_load_start_spin), (patch_io_start + 1));

	/* run the dialog and load if necessary */
	if (gtk_dialog_run(GTK_DIALOG(patch_load_dialog)) == GTK_RESPONSE_ACCEPT) {

		/* get list of selected files from chooser */
		file_list = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(patch_load_dialog));
		if (file_list != NULL) {
			patch_io_start = (unsigned int) gtk_spin_button_get_value
				(GTK_SPIN_BUTTON(patch_load_start_spin)) - 1;

			/* read in each patch and increment bank slot number */
			prog = patch_io_start;
			cur = file_list;
			while (cur != NULL) {
				filename = (char *) cur->data;
				patch = get_patch(visible_sess_num, visible_part_num, prog);
				if (read_patch(filename, patch) == 0) {
					if (visible_prog_num[visible_part_num] == prog) {
						init_patch_state(patch);
						update_gui_patch(patch, 0);
					}
					prog++;
				}
				if (prog >= PATCH_BANK_SIZE) {
					break;
				}
				cur = g_slist_next(cur);
			}

			g_slist_free(file_list);

			save_patch_bank(NULL);
		}
	}

	/* save this widget for next time */
	gtk_widget_hide(patch_load_dialog);
}


/*****************************************************************************
 * create_patch_save_dialog()
 *****************************************************************************/
void
create_patch_save_dialog(void)
{
	GError          *error  = NULL;
	GtkWidget       *hbox;
	GtkWidget       *label;
	unsigned int    prog    = get_visible_program_number();
	int             new_adj = (patch_io_start_adj == NULL);

	/* this should only need to happen once */
	if (patch_save_dialog == NULL) {

		/* create dialog */
		patch_save_dialog = gtk_file_chooser_dialog_new("PHASEX - Save Patch",
		                                                GTK_WINDOW(main_window),
		                                                GTK_FILE_CHOOSER_ACTION_SAVE,
		                                                GTK_STOCK_CANCEL,
		                                                GTK_RESPONSE_CANCEL,
		                                                GTK_STOCK_SAVE,
		                                                GTK_RESPONSE_ACCEPT,
		                                                NULL);
		gtk_window_set_wmclass(GTK_WINDOW(patch_save_dialog), "phasex", "phasex-save");
		gtk_window_set_role(GTK_WINDOW(patch_save_dialog), "patch-save");
		gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(patch_save_dialog), FALSE);

		/* create spinbutton control for setting alternate program number */
		hbox = gtk_hbox_new(FALSE, 8);
		label = gtk_label_new("Save patch into program #:");
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 8);

		if (new_adj) {
			patch_io_start_adj = gtk_adjustment_new(0, 1, PATCH_BANK_SIZE, 1, 8, 0);
		}
		patch_save_start_spin = gtk_spin_button_new(GTK_ADJUSTMENT(patch_io_start_adj), 0, 0);
		gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(patch_save_start_spin), TRUE);
		gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(patch_save_start_spin),
		                                  GTK_UPDATE_IF_VALID);
		gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(patch_save_start_spin), TRUE);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(patch_io_start_adj), (prog + 1));
		gtk_box_pack_start(GTK_BOX(hbox), patch_save_start_spin, FALSE, FALSE, 8);

		if (new_adj) {
			g_signal_connect(GTK_OBJECT(patch_io_start_adj), "value_changed",
			                 GTK_SIGNAL_FUNC(set_patch_io_start),
			                 (gpointer) patch_save_start_spin);
		}

		gtk_widget_show_all(hbox);
		gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(patch_save_dialog), hbox);

		/* realize the file chooser before telling it about files */
		gtk_widget_realize(patch_save_dialog);

#if GTK_CHECK_VERSION(2, 8, 0)
		/* this can go away once manual overwrite checks are proven to work properly */
		gtk_file_chooser_set_do_overwrite_confirmation
			(GTK_FILE_CHOOSER(patch_save_dialog), TRUE);
#endif
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(patch_save_dialog), TRUE);
#endif

		/* add user patch dir as shortcut folder (user cannot write to sys) */
		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(patch_save_dialog),
		                                     user_patch_dir, &error);
		if (error != NULL) {
			PHASEX_ERROR("Error %d: %s\n", error->code, error->message);
			g_error_free(error);
		}

		/* start in user patch dir (usually ~/.phasex/user-patches) */
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(patch_save_dialog), user_patch_dir);

		/* add filename filters for .phx and all files */
		if (file_filter_all != NULL) {
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(patch_save_dialog), file_filter_all);
		}
		if (file_filter_patches != NULL) {
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(patch_save_dialog), file_filter_patches);
			gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(patch_save_dialog), file_filter_patches);
		}
	}
}


/*****************************************************************************
 * run_patch_save_as_dialog()
 *
 * Callback for 'Patch / Save As' from the main menu.
 *****************************************************************************/
void
run_patch_save_as_dialog(GtkWidget *UNUSED(widget), gpointer data)
{
	PATCH           *patch    = get_visible_patch();
	char            *filename = (char *) data;
	DIR_LIST        *pdir     = patch_dir_list;
	GError          *error;
	char            patchfile[PATH_MAX];

	/* create dialog if needed */
	if (patch_save_dialog == NULL) {
		create_patch_save_dialog();
	}

	/* add all patch dirs as shortcut folders */
	while (pdir != NULL) {
		if ((!pdir->save_shortcut) && (strcmp(pdir->name, PATCH_DIR) != 0)) {
			error = NULL;
			gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(patch_save_dialog),
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
	if ((filename == NULL) || (*filename == '\0')) {
		filename = patch->filename;
	}

	/* if we have a filename, and it's not the patchdump, set and select it */
	if ((filename != NULL) &&
	    (strcmp(filename, user_patchdump_file[visible_part_num]) != 0)) {
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(patch_save_dialog), filename);
	}

	/* if there is no filename, try to set the current directory */
	else if ((patch->directory != NULL) && (* (patch->directory) != '\0')) {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(patch_save_dialog), patch->directory);
	}
	else {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(patch_save_dialog), user_patch_dir);
	}

	/* set filter and hope that it takes */
	if (file_filter_patches != NULL) {
		gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(patch_save_dialog), file_filter_patches);
	}

	/* set position in patch bank to save patch */
	/* patch_io_start should already be set properly. */
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(patch_save_start_spin), (patch_io_start + 1));

	/* run the dialog and save if necessary */
	if (gtk_dialog_run(GTK_DIALOG(patch_save_dialog)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(patch_save_dialog));

		patch_io_start =
			(unsigned int) gtk_spin_button_get_value(GTK_SPIN_BUTTON(patch_save_start_spin)) - 1;

		/* add .phx extension if necessary */
		snprintf(patchfile, PATH_MAX, "%s%s",
		         filename, (strstr(filename, ".phx\0") == NULL ? ".phx" : ""));
		g_free(filename);

		/* hide dialog and save patch */
		gtk_widget_hide(patch_save_dialog);
		switch (setting_bank_mem_mode) {
		case BANK_MEM_AUTOSAVE:
			/* save patch without overwrite protection */
			if (save_patch(patchfile, patch) == 0) {
				update_gui_patch_name();
				update_gui_patch_modified();
				save_patch_bank(NULL);
			}
			break;
		case BANK_MEM_WARN:
		case BANK_MEM_PROTECT:
			/* save patch with overwrite protection */
			if (!check_patch_overwrite(filename)) {
				if (save_patch(patchfile, patch) == 0) {
					update_gui_patch_name();
					update_gui_patch_modified();
					save_patch_bank(NULL);
				}
			}
			break;
		}
	}
	else {
		/* save this widget for next time */
		gtk_widget_hide(patch_save_dialog);
	}
}


/*****************************************************************************
 * on_patch_save_activate()
 *
 * Callback for 'Patch / Save' from main menu or for the 'Save' button
 * in patch save verification dialogs.
 *****************************************************************************/
void
on_patch_save_activate(GtkWidget *UNUSED(widget), gpointer data)
{
	PATCH   *patch    = get_visible_patch();
	char    *filename = (char *) data;

	/* if no filename was provided, use the current patch filename */
	if ((filename == NULL) || (filename[0] == '\0')) {
		filename = patch->filename;
	}

	/* if we still don't have a filename, run the save-as dialog */
	if (filename == NULL) {
		run_patch_save_as_dialog(NULL, NULL);
	}

	/* save the patch with the given filename */
	else {
		switch (setting_bank_mem_mode) {
		case BANK_MEM_AUTOSAVE:
			/* save patch without overwrite protection */
			if (save_patch(filename, patch) == 0) {
				update_gui_patch_name();
				update_gui_patch_modified();
				save_patch_bank(NULL);
			}
			break;
		case BANK_MEM_WARN:
		case BANK_MEM_PROTECT:
			/* save patch with overwrite protection */
			if (!check_patch_overwrite(filename)) {
				if (save_patch(filename, patch) == 0) {
					update_gui_patch_name();
					update_gui_patch_modified();
					save_patch_bank(NULL);
				}
			}
			break;
		}
	}
}


/*****************************************************************************
 * on_patch_clear_activate()
 *****************************************************************************/
void
on_patch_reset_activate(GtkWidget *UNUSED(widget), gpointer data)
{
	PATCH   *patch        = get_visible_patch();
	char    *filename     = (char *) data;
	char    *tmp;
	char    tmp_name[16];

	if (setting_bank_mem_mode == BANK_MEM_AUTOSAVE) {
		if (save_patch(filename, patch) == 0) {
			save_patch_bank(NULL);
		}
	}
	if (read_patch(user_default_patch, patch) != 0) {
		read_patch(sys_default_patch, patch);
	}

	if (patch->name != NULL) {
		tmp = patch->name;
		patch->name = NULL;
		free(tmp);
	}
	sprintf(tmp_name, "Untitled-%04d", (get_visible_program_number() + 1));
	patch->name = strdup(tmp_name);

	init_patch_state(patch);

	update_gui_patch(patch, 0);
	update_gui_patch_name();
	update_gui_patch_modified();
	update_gui_session_modified();
}
