/*****************************************************************************
 *
 * gui_patch.c
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
#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>
#include "phasex.h"
#include "config.h"
#include "patch.h"
#include "param.h"
#include "engine.h"
#include "gui_main.h"
#include "gui_param.h"
#include "gui_patch.h"
#include "gui_navbar.h"
#include "gtkknob.h"
#include "param_cb.h"
#include "bank.h"
#include "session.h"
#include "settings.h"
#include "help.h"
#include "debug.h"


PATCH       gui_patch;
PATCH       *gp                     = NULL;

PATCH       *visible_patch          = NULL;
PATCH       *pending_visible_patch  = NULL;

PATCH_STATE gui_dummy_patch_state;


/*****************************************************************************
 * init_gui_patch() {
 *
 * Initializes the gui patch data, with parameters copied from the patch
 * that is marked as currently visible.  Patch and param data structures
 * are established as well.
 *****************************************************************************/
void
init_gui_patch(void)
{
	PATCH           *patch = get_visible_patch();
	PARAM           *param;
	PARAM           *gui_param;
	unsigned int    param_num;

	gp = &gui_patch;
	memset(gp, 0, sizeof(PATCH));
	memset(&gui_dummy_patch_state, 0, sizeof(PATCH_STATE));
	gp->state     = &gui_dummy_patch_state;
	gp->name      = NULL;
	gp->filename  = NULL;
	gp->directory = NULL;
	gp->sess_num  = 0;
	gp->part_num  = 0;
	gp->prog_num  = 0;
	gp->part      = get_part(0);
	for (param_num = 0; param_num < NUM_HELP_PARAMS; param_num++) {
		param                    = & (patch->param[param_num]);
		gui_param                = & (gp->param[param_num]);
		gui_param->info          = get_param_info_by_id(param_num);
		gui_param->patch         = gp;
		gui_param->value.cc_val  = param->value.cc_val;
		gui_param->value.int_val = param->value.int_val;
		gui_param->value.cc_prev = param->info->cc_default;
	}
}


/*****************************************************************************
 * update_gui_patch_name()
 *****************************************************************************/
void
update_gui_patch_name(void)
{
	PATCH   *patch = get_visible_patch();
	char    *name = "";
	char    tmpname[PATCH_NAME_LEN];

	if ((program_entry != NULL) && (GTK_IS_ENTRY(program_entry))) {
		if (patch->name == NULL) {
			snprintf(tmpname, PATCH_NAME_LEN, "Untitled-%04d",
			         (patch->prog_num + 1));
			name = tmpname;
		}
		else {
			name = patch->name;
		}
		gtk_entry_set_text(GTK_ENTRY(program_entry), name);
		patch_name_changed = 0;
	}
}

/*****************************************************************************
 * update_gui_session_name()
 *****************************************************************************/
void
update_gui_session_name(void)
{
	SESSION *session = get_current_session();
	char    *name = "";

	if ((session_entry != NULL) && (GTK_IS_ENTRY(session_entry))) {
		if (session->name != NULL) {
			name = session->name;
		}
		gtk_entry_set_text(GTK_ENTRY(session_entry), name);
		session_name_changed = 0;
	}
}

/*****************************************************************************
 * update_gui_session_number()
 *****************************************************************************/
void
update_gui_session_number(void)
{
	if ((session_spin != NULL) && (GTK_IS_SPIN_BUTTON(session_spin))) {
		gtk_adjustment_set_value(GTK_ADJUSTMENT(session_adj), (visible_sess_num + 1));
	}
}

/*****************************************************************************
 * update_gui_part_number()
 *****************************************************************************/
void
update_gui_part_number(void)
{
	if ((part_spin != NULL) && (GTK_IS_SPIN_BUTTON(part_spin))) {
		gtk_adjustment_set_value(GTK_ADJUSTMENT(part_adj), (visible_part_num + 1));
	}
}

/*****************************************************************************
 * update_gui_program_number()
 *****************************************************************************/
void
update_gui_program_number(void)
{
	if ((program_spin != NULL) && (GTK_IS_SPIN_BUTTON(program_spin))) {
		gtk_adjustment_set_value(GTK_ADJUSTMENT(program_adj),
		                         (get_visible_program_number() + 1));
	}
}

/*****************************************************************************
 * patch_visible()
 *****************************************************************************/
int
patch_visible(PATCH *patch)
{
	return (patch == get_visible_patch());
}

/*****************************************************************************
 * update_gui_patch_modified()
 *****************************************************************************/
void
update_gui_patch_modified(void)
{
	PATCH   *patch = get_visible_patch();

	if (patch->modified != show_patch_modified) {
		gtk_label_set_text(GTK_LABEL(patch_modified_label),
		                   modified_label_text[patch->modified]);
		show_patch_modified = patch->modified;
	}
}


/*****************************************************************************
 * update_gui_session_modified()
 *****************************************************************************/
void
update_gui_session_modified(void)
{
	SESSION *session = get_current_session();

	if ((session_modified_label != NULL) &&
	    (session->modified != show_session_modified)) {
		gtk_label_set_text(GTK_LABEL(session_modified_label),
		                   modified_label_text[session->modified]);
		show_session_modified = session->modified;
	}
}


/*****************************************************************************
 * update_gui_patch_changed()
 *****************************************************************************/
void
update_gui_patch_changed(PATCH *patch, int part_switch)
{
	int     param_num;

	/* Use NUM_PARAMS + 1 in order to get midi ch from extended params. */
	for (param_num = 0; param_num < (NUM_PARAMS + 1); param_num++) {
		/* only mark changed param values as updated */
		if (gp->param[param_num].value.cc_val != patch->param[param_num].value.cc_val) {
			/* Only changed non-locked params, since locked params always
			   follow GUI state). */
			if (part_switch || !(gp->param[param_num].info->locked)) {
				gp->param[param_num].value.cc_prev = gp->param[param_num].value.cc_val;
				gp->param[param_num].value.cc_val  = patch->param[param_num].value.cc_val;
				gp->param[param_num].value.int_val =
					patch->param[param_num].value.cc_val +
					patch->param[param_num].info->cc_offset;
				gp->param[param_num].updated = 1;
				/* Now that gp is updated, widgets get updated on the next
				   param update cycle. */
			}
		}
	}
}


/*****************************************************************************
 * update_gui_patch()
 *
 * Forces GUI update of patch and parameters.  To be called after either
 * visible_patch or pending_visible_patch are set to force a full GUI update.
 *****************************************************************************/
void
update_gui_patch(PATCH *patch, int part_switch)
{
	if (patch != NULL) {
		pending_visible_patch = NULL;
		visible_patch = patch;

		if (gp->name != NULL) {
			free(gp->name);
		}
		if (patch->name != NULL) {
			gp->name = strdup(patch->name);
		}
		else {
			gp->name = NULL;
		}
		gp->sess_num = visible_sess_num;
		gp->part_num = visible_part_num;
		gp->prog_num = visible_prog_num[visible_part_num];

		update_gui_patch_changed(patch, part_switch);
		update_gui_part_number();
	}
	update_gui_patch_name();
	update_gui_session_number();
	update_gui_program_number();
	update_gui_patch_modified();
	update_gui_session_modified();
}
