/*****************************************************************************
 *
 * gui_param.c
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
#include <pango/pango.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include "phasex.h"
#include "config.h"
#include "patch.h"
#include "param.h"
#include "param_parse.h"
#include "engine.h"
#include "buffer.h"
#include "timekeeping.h"
#include "midi_event.h"
#include "gui_main.h"
#include "gui_param.h"
#include "gui_patch.h"
#include "gui_midimap.h"
#include "gui_layout.h"
#include "gtkknob.h"
#include "param_cb.h"
#include "bank.h"
#include "session.h"
#include "midimap.h"
#include "settings.h"
#include "help.h"
#include "debug.h"


int periodic_update_in_progress = 0;


/*****************************************************************************
 * upadate_gui_param()
 *
 * To be called by gui code only.  This is the function that actually
 * updates parameter widgets.  Widget callbacks are disabled when necessary.
 *****************************************************************************/
void
update_gui_param(PARAM *param)
{
	unsigned int    id          = param->info->id;
	PARAM           *gui_param  = & (gp->param[id]);
	GtkStateType    state       = GTK_STATE_NORMAL;
	int             button_num;

	if (param->patch == gp) {
		PHASEX_ERROR("WARNING:  update_gui_param() called with param->patch == gp !\n");
	}

	update_param_child_sensitivities(param->patch->part_num, gui_param->info->id);

	switch (gui_param->info->type) {
	case PARAM_TYPE_INT:
	case PARAM_TYPE_DTNT:
	case PARAM_TYPE_REAL:
	case PARAM_TYPE_RATE:
		if (gui_param->info->prelight) {
			state = GTK_STATE_PRELIGHT;
		}
		else if (gui_param->info->focused || gtk_widget_has_focus(gui_param->info->event)) {
			state = GTK_STATE_SELECTED;
		}
		else {
			state = GTK_STATE_NORMAL;
		}
		gtk_widget_set_state(gui_param->info->text, state);

		gtk_label_set_text(GTK_LABEL(gui_param->info->text),
		                   (gui_param->info->list_labels[gui_param->value.cc_val]));

		g_signal_handlers_block_by_func(GTK_OBJECT(gui_param->info->adj),
		                                GTK_SIGNAL_FUNC(on_gui_param_update),
		                                (gpointer) gui_param);
		gtk_adjustment_set_value(GTK_ADJUSTMENT(gui_param->info->adj), gui_param->value.int_val);
		g_signal_handlers_unblock_by_func(GTK_OBJECT(gui_param->info->adj),
		                                  GTK_SIGNAL_FUNC(on_gui_param_update),
		                                  (gpointer) gui_param);
		break;
	case PARAM_TYPE_BOOL:
		if ((gui_param->value.cc_val == 0) &&
		    (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui_param->info->button[0])))) {
			g_signal_handlers_block_by_func(GTK_OBJECT(gui_param->info->button[0]),
			                                GTK_SIGNAL_FUNC(on_gui_param_update),
			                                (gpointer) gui_param);
			g_signal_handlers_block_by_func(GTK_OBJECT(gui_param->info->button[1]),
			                                GTK_SIGNAL_FUNC(on_gui_param_update),
			                                (gpointer) gui_param);

			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui_param->info->button[0]), TRUE);

			g_signal_handlers_unblock_by_func(GTK_OBJECT(gui_param->info->button[0]),
			                                  GTK_SIGNAL_FUNC(on_gui_param_update),
			                                  (gpointer) gui_param);
			g_signal_handlers_unblock_by_func(GTK_OBJECT(gui_param->info->button[1]),
			                                  GTK_SIGNAL_FUNC(on_gui_param_update),
			                                  (gpointer) gui_param);
		}
		else if ((gui_param->value.cc_val == 1) &&
		         (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(gui_param->info->button[1])))) {
			g_signal_handlers_block_by_func(GTK_OBJECT(gui_param->info->button[1]),
			                                GTK_SIGNAL_FUNC(on_gui_param_update),
			                                (gpointer) gui_param);
			g_signal_handlers_block_by_func(GTK_OBJECT(gui_param->info->button[0]),
			                                GTK_SIGNAL_FUNC(on_gui_param_update),
			                                (gpointer) gui_param);

			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gui_param->info->button[1]), TRUE);

			g_signal_handlers_unblock_by_func(GTK_OBJECT(gui_param->info->button[1]),
			                                  GTK_SIGNAL_FUNC(on_gui_param_update),
			                                  (gpointer) gui_param);
			g_signal_handlers_unblock_by_func(GTK_OBJECT(gui_param->info->button[0]),
			                                  GTK_SIGNAL_FUNC(on_gui_param_update),
			                                  (gpointer) gui_param);
		}
		for (button_num = 0; button_num <= 1; button_num++) {
			if (gui_param->info->focused == button_num) {
				gtk_widget_set_state(gui_param->info->button[button_num], GTK_STATE_SELECTED);
			}
			else if (gui_param->info->prelight == button_num) {
				gtk_widget_set_state(gui_param->info->button[button_num], GTK_STATE_PRELIGHT);
			}
			else {
				gtk_widget_set_state(gui_param->info->button[button_num], GTK_STATE_NORMAL);
			}
		}
		break;
	case PARAM_TYPE_BBOX:
		if (!gtk_toggle_button_get_active
		    (GTK_TOGGLE_BUTTON(gui_param->info->button[gui_param->value.cc_val]))) {
			for (button_num = 0; gui_param->info->button[button_num] != NULL; button_num++) {
				g_signal_handlers_block_by_func
					(GTK_OBJECT(gui_param->info->button[button_num]),
					 GTK_SIGNAL_FUNC(on_gui_param_update),
					 (gpointer) gui_param);
			}

			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(gui_param->info->button[gui_param->value.cc_val]), TRUE);

			for (button_num = 0; gui_param->info->button[button_num] != NULL; button_num++) {
				g_signal_handlers_unblock_by_func
					(GTK_OBJECT(gui_param->info->button[button_num]),
					 GTK_SIGNAL_FUNC(on_gui_param_update),
					 (gpointer) gui_param);
			}
		}
		for (button_num = 0; gui_param->info->button[button_num] != NULL; button_num++) {
			if (gui_param->info->focused == button_num) {
				gtk_widget_set_state(gui_param->info->button[button_num], GTK_STATE_SELECTED);
			}
			else if (gui_param->info->prelight == button_num) {
				gtk_widget_set_state(gui_param->info->button[button_num], GTK_STATE_PRELIGHT);
			}
			else {
				gtk_widget_set_state(gui_param->info->button[button_num], GTK_STATE_NORMAL);
			}
		}
		break;
	}

	if (param->updated > 0) {
		param->updated--;
	}
	if (gui_param->updated > 0) {
		gui_param->updated--;
	}
}


/*****************************************************************************
 * gui_param_midi_update()
 *
 * Handle gui updates for parameter changes * received via midi.
 *****************************************************************************/
void
gui_param_midi_update(PARAM *param, int cc_val)
{
	unsigned int    id = param->info->id;

	/* ignore midi updates for locked params */
	if ((!param->info->locked) && (gtkui_thread_p > 0)) {
		/* update gui param so gui can update state */
		if (gp->part_num == param->patch->part_num) {
			if (gp->param[id].value.cc_val != cc_val) {
				gp->param[id].value.cc_prev = gp->param[id].value.cc_val;
				gp->param[id].value.cc_val  = cc_val;
				gp->param[id].value.int_val = cc_val + param->info->cc_offset;
				gp->param[id].updated = 1;
				gp->modified = 1;
			}
		}
	}
	PHASEX_DEBUG(DEBUG_CLASS_GUI,
	             "  GUI Param MIDI Update ---- Part %d -- [%1d] "
	             "-- old ( cc_val = %03d ) "
	             "-- new ( cc_val = %03d ) -- <%s>\n",
	             (param->patch->part_num + 1),
	             param->info->id,
	             param->value.cc_prev,
	             param->value.cc_val,
	             param->info->name);
}


/*****************************************************************************
 * get_param_widget_val()
 *
 * Return the current integer value of a widget.
 *****************************************************************************/
int
get_param_widget_val(GtkWidget *widget, PARAM *param)
{
	int     val;
	int     j;

	/* set val to int value from param in case a new val is not caught here */
	val = param->value.int_val;

	switch (param->info->type) {
	case PARAM_TYPE_INT:
	case PARAM_TYPE_REAL:
		/* get the adjustment value used for both knob and spin */
		val = (int) (gtk_adjustment_get_value(GTK_ADJUSTMENT(widget)) + 0.0);
		/* while we're here, set the value label next to the knob */
		gtk_label_set_text(GTK_LABEL(param->info->text),
		                   param->info->list_labels[val - param->info->cc_offset]);
		break;
	case PARAM_TYPE_RATE:
	case PARAM_TYPE_DTNT:
		/* get value from adjustment for knob widget that was modified */
		if (GTK_OBJECT(widget) == param->info->adj) {
			val = (int) (gtk_adjustment_get_value(GTK_ADJUSTMENT(widget)) + 0.0);
		}
		/* while we're here, set the value label next to the knob */
		gtk_label_set_text(GTK_LABEL(param->info->text), param->info->list_labels[val]);
		break;
	case PARAM_TYPE_BOOL:
		/* Two radio buttons work a bit differently than three or more */
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(param->info->button[1]))) {
			val = 1;
		}
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(param->info->button[0]))) {
			val = 0;
		}
		else {
			val = PARAM_VAL_IGNORE;
		}
		break;
	case PARAM_TYPE_BBOX:
		/* Make sure only a button toggling on actually does anything */
		/* figure out which button this is */
		/* for buttons toggling off, return out of bounds value that gets caught */
		val = PARAM_VAL_IGNORE;
		for (j = 0; j <= param->info->cc_limit; j++) {
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(param->info->button[j]))) {
				val = j;
				break;
			}
		}
		break;
	}

	return val;
}


/*****************************************************************************
 * on_gui_param_update()
 *
 * Generic callback for all synth param widgets.  Handles updates from the
 * GUI only.  Session, Part, and Program are always the current visible.
 *****************************************************************************/
void
on_gui_param_update(GtkWidget *widget, gpointer *data)
{
	SESSION             *session     = get_current_session();
	PARAM               *gui_param   = ((PARAM *)(data));
	unsigned int        id           = gui_param->info->id;
	PATCH               *patch       = get_visible_patch();
	PARAM               *param       = & (patch->param[id]);
	int                 val;

	if ((val = get_param_widget_val(widget, gui_param)) != PARAM_VAL_IGNORE) {
		gui_param->value.int_val = val;
		gui_param->value.cc_prev = gui_param->value.cc_val;
		gui_param->value.cc_val  = gui_param->value.int_val - gui_param->info->cc_offset;

		/* ignore when value doesn't change */
		val = 0;
		if (gui_param->value.cc_val != gui_param->value.cc_prev) {
			val = 1;
			gui_param->updated = 1;

			/* changing non-locked paramaters show the patch as being modified */
			if (!param->info->locked) {
				patch->modified   = 1;
				session->modified = 1;
			}

			/* timestamp and queue event for engine */
			queue_midi_param_event(patch->part_num, id, gui_param->value.cc_val);
		}

		/* handle remaining necessary gui changes */
		update_param_child_sensitivities(visible_part_num, param->info->id);

		PHASEX_DEBUG(DEBUG_CLASS_GUI,
		             "on_gui_param_update(): %s Part %d -- [%1d] "
		             "-- old = %03d  -- new = %03d  -- <%s>\n",
		             (val ? "(value changed)" : "---------------"),
		             (param->patch->part_num + 1),
		             gui_param->info->id,
		             gui_param->value.cc_prev,
		             gui_param->value.cc_val,
		             gui_param->info->name);
	}
}


/*****************************************************************************
 * on_param_name_button_press()
 *
 * Callback for a button press event on a param label event box.
 *****************************************************************************/
void
on_param_name_button_press(GtkWidget *widget, GdkEventButton *event)
{
	PARAM           *param;
	GtkWidget       *hbox;
	GtkWidget       *label;
	GtkWidget       *separator;
	GtkWidget       *lock_button;
	GtkWidget       *ignore_button;
	int             id              = -1;
	int             same_param_id   = 0;
	const char      *widget_name;

	/* find param id by looking up name from widget in param table */
	widget_name = gtk_widget_get_name(widget);
	id = (int) get_param_id_by_name((char *) widget_name);
	param = get_param(visible_part_num, (unsigned int) id);

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
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox), label, TRUE, FALSE, 8);

			separator = gtk_hseparator_new();
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox),
			                   separator, TRUE, FALSE, 0);

			/* parameter name */
			label = gtk_label_new(param_help[id].label);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox), label, TRUE, FALSE, 8);

			separator = gtk_hseparator_new();
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox),
			                   separator, TRUE, FALSE, 0);

			/* select midi controller */
			hbox = gtk_hbox_new(FALSE, 0);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cc_edit_dialog)->vbox), hbox, TRUE, FALSE, 8);

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
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lock_button),
			                             param->info->locked ? TRUE : FALSE);
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
			g_signal_connect(G_OBJECT(cc_edit_dialog), "destroy",
			                 GTK_SIGNAL_FUNC(close_cc_edit_dialog),
			                 (gpointer) cc_edit_dialog);
			g_signal_connect_swapped(G_OBJECT(cc_edit_dialog), "response",
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
 * new_param_value()
 *****************************************************************************/
void
new_param_value(PARAM *gui_param, PARAM *param, int cc_val)
{
	SESSION *session = get_current_session();

	gui_param->value.cc_prev = gui_param->value.cc_val;
	gui_param->value.cc_val  = cc_val;
	gui_param->value.int_val = cc_val + gui_param->info->cc_offset;
	if (gui_param->value.cc_val != gui_param->value.cc_prev) {
		queue_midi_param_event(visible_part_num, param->info->id, gui_param->value.cc_val);
		gui_param->updated     = 1;
		param->patch->modified = 1;
		session->modified      = 1;
		PHASEX_DEBUG(DEBUG_CLASS_GUI,
		             "  GUI Param Update ---- Part %d -- [%1d] "
		             "-- old ( cc_val = %03d ) "
		             "-- new ( cc_val = %03d ) -- <%s>\n",
		             (param->patch->part_num + 1),
		             gui_param->info->id,
		             gui_param->value.cc_prev,
		             gui_param->value.cc_val,
		             gui_param->info->name);
	}
}


/*****************************************************************************
 * on_param_label_event()
 *
 * Callback for a button press event on a detent parameter value label.
 *****************************************************************************/
int
on_param_label_event(gpointer data1, gpointer data2)
{
	PARAM_NAV_LIST      *param_nav;
	PARAM               *gui_param      = (PARAM *) data1;
	unsigned int        id              = gui_param->info->id;
	PATCH               *patch          = get_visible_patch();
	PARAM               *param          = & (patch->param[id]);
	GdkEventAny         *event          = (GdkEventAny *) data2;
	GdkEventButton      *button         = (GdkEventButton *) data2;
	GdkEventScroll      *scroll         = (GdkEventScroll *) data2;
	GdkEventKey         *key            = (GdkEventKey *) data2;
	GtkWidget           *grab_widget    = NULL;
	GtkStateType        state           = GTK_STATE_NORMAL;
	GtkDirectionType    direction;
	int                 grab            = 0;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		switch (button->button) {
		case 1:     /* left button */
			if (gui_param->value.cc_val > 0) {
				new_param_value(gui_param, param, (gui_param->value.cc_val - 1));
			}
			grab = 1;
			break;
		case 2:     /* middle button */
			new_param_value(gui_param, param, ((gui_param->info->cc_limit + 1) / 2));
			grab = 1;
			break;
		case 3:     /* right button */
			if ((gui_param->value.cc_val) < (gui_param->info->cc_limit)) {
				new_param_value(gui_param, param, (gui_param->value.cc_val + 1));
			}
			grab = 1;
			break;
		}
		break;
	case GDK_SCROLL:
		switch (scroll->direction) {
		case GDK_SCROLL_DOWN:
			if (gui_param->value.cc_val > 0) {
				new_param_value(gui_param, param, (gui_param->value.cc_val - 1));
			}
			grab = 1;
			break;
		case GDK_SCROLL_UP:
			if ((gui_param->value.cc_val) < (gui_param->info->cc_limit)) {
				new_param_value(gui_param, param, (gui_param->value.cc_val + 1));
			}
			grab = 1;
			break;
		default:
			/* must handle default case for scroll direction to keep gtk quiet */
			break;
		}
		break;
	case GDK_KEY_PRESS:
		switch (key->keyval) {
		case GDK_Down:
		case GDK_KP_Down:
		case GDK_Left:
		case GDK_KP_Left:
			gui_param->info->focused = 1;
			if (gui_param->value.cc_val > 0) {
				new_param_value(gui_param, param, (gui_param->value.cc_val - 1));
			}
			return 1;
			grab = 1;
			break;
		case GDK_Up:
		case GDK_KP_Up:
		case GDK_Right:
		case GDK_KP_Right:
			gui_param->info->focused = 1;
			if ((gui_param->value.cc_val) < (gui_param->info->cc_limit)) {
				new_param_value(gui_param, param, (gui_param->value.cc_val + 1));
			}
			return 1;
			grab = 1;
			break;
		case GDK_Tab:
		case GDK_KP_Tab:
		case GDK_ISO_Left_Tab:
			gui_param->info->focused = 0;
			if (key->keyval == GDK_ISO_Left_Tab) {
				param_nav = gui_param->info->param_nav;
				param_nav = param_nav->prev;
				while (!param_nav->param_info->sensitive) {
					param_nav = param_nav->prev;
				}
				while (!param_nav->param_info->sensitive ||
				       (param_nav->page_num != param_nav->next->page_num)) {
					param_nav = param_nav->next;
				}
				grab_widget = param_nav->widget;
				direction = GTK_DIR_TAB_BACKWARD;
			}
			else {
				param_nav = gui_param->info->param_nav;
				param_nav = param_nav->next;
				while (!param_nav->param_info->sensitive) {
					param_nav = param_nav->next;
				}
				while (!param_nav->param_info->sensitive ||
				       (param_nav->page_num != param_nav->prev->page_num)) {
					param_nav = param_nav->prev;
				}
				grab_widget = param_nav->widget;
				direction = GTK_DIR_TAB_FORWARD;
			}
			gtk_widget_child_focus(grab_widget, direction);
			if (gui_param->info->prelight) {
				state = GTK_STATE_PRELIGHT;
			}
			else {
				state = GTK_STATE_NORMAL;
			}
			gtk_widget_set_state(gui_param->info->text, state);
			gui_param->updated = 1;
			return 1;
			break;
		case GDK_Return:
		case GDK_KP_Enter:
		default:
			break;
		}
	case GDK_ENTER_NOTIFY:
		if (!key->keyval) {
			gui_param->info->prelight = 1;
			gtk_widget_set_state(gui_param->info->text, GTK_STATE_PRELIGHT);
		}
		break;
	case GDK_LEAVE_NOTIFY:
		gui_param->info->prelight = 0;
		if ((gtk_widget_has_focus(gui_param->info->event))) {
			gui_param->info->focused = 1;
			state = GTK_STATE_SELECTED;
		}
		else {
			gui_param->info->focused = 0;
			state = GTK_STATE_NORMAL;
		}
		gtk_widget_set_state(gui_param->info->text, state);
		break;
	default:
		/* must handle all enumerations for event type to keep gtk quiet */
		break;
	}

	/* handle grab if this widget has been newly selected */
	if (grab) {
		switch (gui_param->info->type) {
		case PARAM_TYPE_INT:
		case PARAM_TYPE_DTNT:
		case PARAM_TYPE_RATE:
		case PARAM_TYPE_REAL:
			grab_widget = gui_param->info->event;
			break;
		}
		if ((grab_widget != NULL) && GTK_IS_WIDGET(grab_widget)) {
			gtk_widget_grab_focus(grab_widget);
		}
	}

	return 0;
}


/*****************************************************************************
 * on_param_label_focus_change()
 *
 * Callback for a focus change event on a detent label.  Widget state cannot
 * be set here, or navigation gets messed up.  Set param info and let the
 * idle update handler take care of state changes on the widgets.
 *****************************************************************************/
gboolean
on_param_label_focus_change(GtkWidget     *UNUSED(widget),
                            GdkEventFocus *event,
                            gpointer      data)
{
	PARAM       *param  = (PARAM *) data;

	/* focus in event */
	if (event->in) {
		PHASEX_DEBUG(DEBUG_CLASS_GUI, "focus in event:  focused = %d    prelight = %d\n",
		             param->info->focused, param->info->prelight);
		param->info->focused  = 1;
	}
	/* focus out event with active prelight */
	else if (param->info->prelight) {
		PHASEX_DEBUG(DEBUG_CLASS_GUI, "focus out event:  focused = %d    prelight = %d\n",
		             param->info->focused, param->info->prelight);
		param->info->focused  = 0;
	}
	/* focus out event with no prelight */
	else {
		PHASEX_DEBUG(DEBUG_CLASS_GUI, "focus out event:  focused = %d    prelight = %d\n",
		             param->info->focused, param->info->prelight);
		param->info->focused  = 0;
		param->info->prelight = 0;
	}
	param->updated = 1;

	return param->info->focused;
}


/*****************************************************************************
 * on_param_button_event()
 *
 * Callback for a button hover event (enter or leave) on a button or button label.
 *****************************************************************************/
int
on_param_button_event(gpointer data1, gpointer data2, gpointer data3)
{
	PARAM_NAV_LIST      *param_nav;
	PARAM               *gui_param      = (PARAM *) data1;
	unsigned int        id              = gui_param->info->id;
	PATCH               *patch          = get_visible_patch();
	PARAM               *param          = & (patch->param[id]);
	GdkEventAny         *event          = (GdkEventAny *) data2;
	GdkEventButton      *button         = (GdkEventButton *) data2;
	GdkEventScroll      *scroll         = (GdkEventScroll *) data2;
	GdkEventKey         *key            = (GdkEventKey *) data2;
	GtkWidget           *grab_widget    = NULL;
	GtkStateType        state           = GTK_STATE_NORMAL;
	GtkDirectionType    direction;
	int                 button_num;
	int                 grab            = 0;

	PHASEX_DEBUG(DEBUG_CLASS_GUI,
	             "  Param Button Event ---- [%03d] "
	             "-- old ( cc_val = %03d ) "
	             "-- new ( cc_val = %03d ) -- <%s>\n",
	             param->info->id,
	             param->value.cc_prev,
	             param->value.cc_val,
	             param->info->name);

	for (button_num = 0; gui_param->info->button[button_num] != NULL; button_num++) {
		if ((data3 == (gpointer) gui_param->info->button[button_num]) ||
		    (data3 == (gpointer) gui_param->info->button_event[button_num][0]) ||
		    (data3 == (gpointer) gui_param->info->button_event[button_num][1])) {
			break;
		}
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		switch (button->button) {
		case 1:     /* left button */
		case 2:     /* middle button */
		case 3:     /* right button */
			gui_param->info->focused = button_num;
			gtk_widget_set_state(gui_param->info->button[gui_param->info->focused],
			                     GTK_STATE_SELECTED);
			new_param_value(gui_param, param, button_num);
			break;
		}
		grab = 1;
		break;
	case GDK_SCROLL:
		switch (scroll->direction) {
		case GDK_SCROLL_DOWN:
			if ((gui_param->value.cc_val > 0) &&
			    (gui_param->info->button[gui_param->value.cc_val - 1] != NULL)) {
				gui_param->info->focused = gui_param->value.cc_val - 1;
				gtk_widget_set_state(gui_param->info->button[gui_param->info->focused],
				                     GTK_STATE_SELECTED);
				new_param_value(gui_param, param, (gui_param->value.cc_val - 1));
			}
			grab = 1;
			break;
		case GDK_SCROLL_UP:
			if ((gui_param->value.cc_val < gui_param->info->cc_limit) &&
			    (gui_param->info->button[gui_param->value.cc_val + 1] != NULL)) {
				gui_param->info->focused = gui_param->value.cc_val + 1;
				gtk_widget_set_state(gui_param->info->button[gui_param->info->focused],
				                     GTK_STATE_SELECTED);
				new_param_value(gui_param, param, (gui_param->value.cc_val + 1));
			}
			grab = 1;
			break;
		default:
			/* must handle all enumerations for scroll direction to keep gtk quiet */
			break;
		}
		break;
	case GDK_KEY_PRESS:
		switch (key->keyval) {
		case GDK_Down:
		case GDK_KP_Down:
		case GDK_Left:
		case GDK_KP_Left:
			if ((gui_param->value.cc_val > 0) &&
			    (gui_param->info->button[gui_param->value.cc_val - 1] != NULL)) {
				gui_param->info->focused = gui_param->value.cc_val - 1;
				gtk_widget_set_state(gui_param->info->button[gui_param->info->focused],
				                     GTK_STATE_SELECTED);
				new_param_value(gui_param, param, (gui_param->value.cc_val - 1));
				grab = 1;
			}
			grab = 1;
			break;
		case GDK_Up:
		case GDK_KP_Up:
		case GDK_Right:
		case GDK_KP_Right:
			if ((gui_param->value.cc_val < gui_param->info->cc_limit) &&
			    (gui_param->info->button[gui_param->value.cc_val + 1] != NULL)) {
				gui_param->info->focused = gui_param->value.cc_val + 1;
				gtk_widget_set_state(gui_param->info->button[gui_param->info->focused],
				                     GTK_STATE_SELECTED);
				new_param_value(gui_param, param, (gui_param->value.cc_val + 1));
				grab = 1;
			}
			grab = 1;
			break;
		case GDK_Tab:
		case GDK_KP_Tab:
		case GDK_ISO_Left_Tab:
			gui_param->info->focused = -1;
			if (key->keyval == GDK_ISO_Left_Tab) {
				param_nav = gui_param->info->param_nav;
				param_nav = param_nav->prev;
				while (!param_nav->param_info->sensitive) {
					param_nav = param_nav->prev;
				}
				while (!param_nav->param_info->sensitive ||
				       (param_nav->page_num != param_nav->next->page_num)) {
					param_nav = param_nav->next;
				}
				grab_widget = param_nav->widget;
				direction = GTK_DIR_TAB_BACKWARD;
			}
			else {
				param_nav = gui_param->info->param_nav;
				param_nav = param_nav->next;
				while (!param_nav->param_info->sensitive) {
					param_nav = param_nav->next;
				}
				while (!param_nav->param_info->sensitive ||
				       (param_nav->page_num != param_nav->prev->page_num)) {
					param_nav = param_nav->prev;
				}
				grab_widget = param_nav->widget;
				direction = GTK_DIR_TAB_FORWARD;
			}
			gtk_widget_child_focus(grab_widget, direction);
			if (gui_param->info->prelight == button_num) {
				state = GTK_STATE_PRELIGHT;
			}
			else {
				state = GTK_STATE_NORMAL;
			}
			gtk_widget_set_state(gui_param->info->button[button_num], state);
			gtk_widget_set_state(gui_param->info->button[gui_param->info->focused],
			                     GTK_STATE_NORMAL);
			gui_param->updated  = 1;
			return 1;
			break;
		case GDK_Return:
		case GDK_KP_Enter:
		default:

			break;
		}
	case GDK_ENTER_NOTIFY:
		if (gui_param->info->prelight < 0) {
			gui_param->info->prelight = button_num;
			gtk_widget_set_state
				(gui_param->info->button[button_num], GTK_STATE_PRELIGHT);
			gui_param->updated  = 1;
		}
		break;
	case GDK_LEAVE_NOTIFY:
		if (gui_param->info->prelight >= 0) {
			gtk_widget_set_state(gui_param->info->button[gui_param->info->prelight],
			                     GTK_STATE_NORMAL);
			gui_param->info->prelight = -1;
		}
		if (gui_param->info->focused == button_num) {
			state = GTK_STATE_SELECTED;
		}
		else {
			state = GTK_STATE_NORMAL;
		}
		gtk_widget_set_state(gui_param->info->button[button_num], state);
		gui_param->updated  = 1;
		break;
	default:
		/* must handle all enumerations for event type to keep gtk quiet */
		break;
	}

	/* Grab currently selected button */
	if (grab) {
		grab_widget = gui_param->info->button[gui_param->value.cc_val];
		gui_param->info->focused = gui_param->value.cc_val;
		if ((gui_param->info->focused >= 0) &&
		    (grab_widget != NULL) && GTK_IS_WIDGET(grab_widget)) {
			gtk_widget_grab_focus(grab_widget);
			return 1;
		}
	}

	return 0;
}


/*****************************************************************************
 * create_param_input()
 *
 * Create a parameter input widget based on PARAM entry.
 * Designed to run out of a loop.
 *****************************************************************************/
void
create_param_input(GtkWidget *UNUSED(main_window),
                   GtkWidget *table,
                   guint     col,
                   guint     row,
                   PARAM     *param,
                   gint      page_num)
{
	PARAM_NAV_LIST  *cur;
	GtkWidget       *event;
	GtkWidget       *label;
	GtkWidget       *knob;
	GtkWidget       *hbox;
	GtkWidget       *button_table;
	GtkWidget       *button;
	GtkObject       *adj;
	GSList          *button_group;
	unsigned int    j;
	unsigned int    k;

	param->info->table = table;

	/* event box for clickable param label */
	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_widget_set_name(event, param->info->name);
	gtk_table_attach(GTK_TABLE(table), event,
	                 col, col + 1, row, row + 1,
	                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
	                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
	                 0, 0);

	/* parameter name label */
	label = gtk_label_new(param->info->label_text);
	gtk_widget_set_name(label, "ParamName");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_misc_set_padding(GTK_MISC(label), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_container_add(GTK_CONTAINER(event), label);

	/* connect signal for clicking param name label */
	g_signal_connect(G_OBJECT(event), "button_press_event",
	                 GTK_SIGNAL_FUNC(on_param_name_button_press),
	                 (gpointer) event);

	/* keep track of param name label widget */
	param->info->label = label;

	/* create different widget combos depending on parameter type */
	switch (param->info->type) {
	case PARAM_TYPE_INT:
	case PARAM_TYPE_REAL:
		/* create adjustment for real/integer knob and spin widgets */
		adj = gtk_adjustment_new(param->value.int_val,
		                         (0 + param->info->cc_offset),
		                         (param->info->cc_limit + param->info->cc_offset),
		                         1, param->info->leap, 0);
		param->info->adj = adj;

		/* create knob widget */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		knob = gtk_knob_new(GTK_ADJUSTMENT(adj), knob_anim);
		widget_set_backing_store(knob);
		gtk_container_add(GTK_CONTAINER(event), knob);
		gtk_table_attach(GTK_TABLE(table), event,
		                 col + 1, col + 2, row, row + 1,
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 0, 0);
		param->info->knob = GTK_KNOB(knob);

		/* create label widget for text values */
		event = gtk_event_box_new();
		gtk_widget_set_name(event, "NumericLabel");
		g_object_set(G_OBJECT(event), "can-focus", TRUE, NULL);
		widget_set_backing_store(event);

		param->info->event = event;

		label = gtk_label_new(param->info->list_labels[param->value.cc_val]);
		gtk_widget_set_name(label, "NumericLabel");
		widget_set_custom_font(label, numeric_font_desc);
		widget_set_backing_store(label);
		g_object_set(G_OBJECT(label), "can-focus", TRUE, NULL);
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_label_set_width_chars(GTK_LABEL(label), 4);
#else
		gtk_widget_set_size_request(label, 80, -1);
#endif
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_container_add(GTK_CONTAINER(event), label);

		gtk_table_attach(GTK_TABLE(table), event,
		                 col + 2, col + 3, row, row + 1,
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 0, 0);
		param->info->text = label;

		/* connect knob signals */
		g_signal_connect(GTK_OBJECT(adj), "value_changed",
		                 GTK_SIGNAL_FUNC(on_gui_param_update),
		                 (gpointer) param);

		/* connect value label event box signals */
		g_signal_connect_swapped(G_OBJECT(event), "button_press_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "scroll_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "key_press_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "enter_notify_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "leave_notify_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect(G_OBJECT(event), "focus-in-event",
		                 GTK_SIGNAL_FUNC(on_param_label_focus_change),
		                 (gpointer) param);
		g_signal_connect(G_OBJECT(event), "focus-out-event",
		                 GTK_SIGNAL_FUNC(on_param_label_focus_change),
		                 (gpointer) param);
		break;
	case PARAM_TYPE_RATE:
		/* create adjustment for knob widget */
		adj = gtk_adjustment_new(param->value.int_val, 0, param->info->cc_limit,
		                         1, param->info->leap, 0);
		param->info->adj = adj;

		/* create knob widget */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		knob = gtk_knob_new(GTK_ADJUSTMENT(adj), knob_anim);
		widget_set_backing_store(knob);
		gtk_container_add(GTK_CONTAINER(event), knob);
		gtk_table_attach(GTK_TABLE(table), event,
		                 col + 1, col + 2, row, row + 1,
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 0, 0);
		param->info->knob = GTK_KNOB(knob);

		/* create label widget for text values */
		event = gtk_event_box_new();
		gtk_widget_set_name(event, "DetentLabel");
		g_object_set(G_OBJECT(event), "can-focus", TRUE, NULL);
		widget_set_backing_store(event);

		param->info->event = event;

		label = gtk_label_new(param->info->list_labels[param->value.cc_val]);
		gtk_widget_set_name(label, "DetentLabel");
		widget_set_custom_font(label, numeric_font_desc);
		widget_set_backing_store(label);
		g_object_set(G_OBJECT(label), "can-focus", TRUE, NULL);
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_label_set_width_chars(GTK_LABEL(label), 5);
#else
		gtk_widget_set_size_request(label, 80, -1);
#endif
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_container_add(GTK_CONTAINER(event), label);

		gtk_table_attach(GTK_TABLE(table), event,
		                 col + 2, col + 3, row, row + 1,
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 0, 0);
		param->info->text = label;

		/* connect knob signals */
		g_signal_connect(GTK_OBJECT(adj), "value_changed",
		                 GTK_SIGNAL_FUNC(on_gui_param_update),
		                 (gpointer) param);

		/* connect value label event box signals */
		g_signal_connect_swapped(G_OBJECT(event), "button_press_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "scroll_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "key_press_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "enter_notify_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "leave_notify_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect(G_OBJECT(event), "focus-in-event",
		                 GTK_SIGNAL_FUNC(on_param_label_focus_change),
		                 (gpointer) param);
		g_signal_connect(G_OBJECT(event), "focus-out-event",
		                 GTK_SIGNAL_FUNC(on_param_label_focus_change),
		                 (gpointer) param);
		break;
	case PARAM_TYPE_DTNT:
		/* create adjustment for knob widget */
		adj = gtk_adjustment_new(param->value.int_val, 0, param->info->cc_limit,
		                         1, param->info->leap, 0);
		param->info->adj = adj;

		/* create knob widget */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		knob = gtk_knob_new(GTK_ADJUSTMENT(adj), detent_knob_anim);
		widget_set_backing_store(knob);
		gtk_container_add(GTK_CONTAINER(event), knob);
		gtk_table_attach(GTK_TABLE(table), event,
		                 col + 1, col + 2, row, row + 1,
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 0, 0);
		param->info->knob = GTK_KNOB(knob);

		/* create label widget for text values */
		event = gtk_event_box_new();
		gtk_widget_set_name(event, "DetentLabel");
		g_object_set(G_OBJECT(event), "can-focus", TRUE, NULL);
		widget_set_backing_store(event);

		param->info->event = event;

		label = gtk_label_new(param->info->list_labels[param->value.cc_val]);
		gtk_widget_set_name(label, "DetentLabel");
		widget_set_custom_font(label, numeric_font_desc);
		widget_set_backing_store(label);
		g_object_set(G_OBJECT(label), "can-focus", TRUE, NULL);
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_label_set_width_chars(GTK_LABEL(label), 10);
#else
		gtk_widget_set_size_request(label, 80, -1);
#endif
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
		gtk_container_add(GTK_CONTAINER(event), label);

		gtk_table_attach(GTK_TABLE(table), event,
		                 col + 2, col + 3, row, row + 1,
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 0, 0);
		param->info->text = label;

		/* connect knob signals */
		g_signal_connect(GTK_OBJECT(adj), "value_changed",
		                 GTK_SIGNAL_FUNC(on_gui_param_update),
		                 (gpointer) param);

		/* connect value label event box signals */
		g_signal_connect_swapped(G_OBJECT(event), "button_press_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "scroll_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "key_press_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "enter_notify_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect_swapped(G_OBJECT(event), "leave_notify_event",
		                         GTK_SIGNAL_FUNC(on_param_label_event),
		                         (gpointer) param);
		g_signal_connect(G_OBJECT(event), "focus-in-event",
		                 GTK_SIGNAL_FUNC(on_param_label_focus_change),
		                 (gpointer) param);
		g_signal_connect(G_OBJECT(event), "focus-out-event",
		                 GTK_SIGNAL_FUNC(on_param_label_focus_change),
		                 (gpointer) param);
		break;
	case PARAM_TYPE_BOOL:
		/* create button box */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		hbox = gtk_hbox_new(FALSE, 0);
		widget_set_backing_store(hbox);
		gtk_container_add(GTK_CONTAINER(event), hbox);
		gtk_table_attach(GTK_TABLE(table), event,
		                 col + 1, col + 3, row, row + 1,
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 0, 0);
		param->info->event = event;

		/* create buttons */
		button = NULL;
		button_group = NULL;
		param->info->button[0] = NULL;
		for (j = 0; param->info->list_labels[j] != NULL; j++) {
			/* create individual button widget */
			event = gtk_event_box_new();
			widget_set_backing_store(event);
			button = gtk_radio_button_new(NULL);
			if ((strcasecmp(param->info->list_labels[j], "off") == 0) ||
			    (strcasecmp(param->info->list_labels[j], "<small>off</small>") == 0)) {
				gtk_widget_set_name(button, "OffButton");
			}
			else {
				gtk_widget_set_name(button, "ParamButton");
			}
			widget_set_backing_store(button);
			gtk_container_add(GTK_CONTAINER(event), button);
			gtk_box_pack_start(GTK_BOX(hbox), event, TRUE, TRUE, 0);
			param->info->button_event[j][0] = event;

			/* create label to sit next to button */
			event = gtk_event_box_new();
			widget_set_backing_store(event);
			label = gtk_label_new(param->info->list_labels[j]);
			gtk_widget_set_name(label, "ButtonLabel");
			widget_set_custom_font(label, phasex_font_desc);
			widget_set_backing_store(label);
			gtk_container_add(GTK_CONTAINER(event), label);
			gtk_box_pack_start(GTK_BOX(hbox), event, TRUE, TRUE, 0);
			param->info->button_event[j][1] = event;

			param->info->button[j]       = button;
			param->info->button_label[j] = label;

			gtk_radio_button_set_group(GTK_RADIO_BUTTON(button), button_group);
			button_group = g_slist_append(button_group, button);
		}

		/* set active button before connecting signals */
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(param->info->button[param->value.cc_val]), TRUE);

		/* connect toggled signal for each button */
		for (j = 0; param->info->list_labels[j] != NULL; j++) {
			g_signal_connect(G_OBJECT(param->info->button[j]), "toggled",
			                 GTK_SIGNAL_FUNC(on_gui_param_update),
			                 (gpointer) param);

			for (k = 0; k <= 1; k++) {
				g_signal_connect_swapped(G_OBJECT(param->info->button_event[j][k]),
				                         "button_press_event",
				                         GTK_SIGNAL_FUNC(on_param_button_event),
				                         (gpointer) param);
				g_signal_connect_swapped(G_OBJECT(param->info->button_event[j][k]),
				                         "scroll_event",
				                         GTK_SIGNAL_FUNC(on_param_button_event),
				                         (gpointer) param);
				g_signal_connect_swapped(G_OBJECT(param->info->button_event[j][k]),
				                         "key_press_event",
				                         GTK_SIGNAL_FUNC(on_param_button_event),
				                         (gpointer) param);
				g_signal_connect_swapped(G_OBJECT(param->info->button_event[j][k]),
				                         "enter_notify_event",
				                         GTK_SIGNAL_FUNC(on_param_button_event),
				                         (gpointer) param);
				g_signal_connect_swapped(G_OBJECT(param->info->button_event[j][k]),
				                         "leave_notify_event",
				                         GTK_SIGNAL_FUNC(on_param_button_event),
				                         (gpointer) param);
			}
		}

		break;
	case PARAM_TYPE_BBOX:
		/* create button box */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		button_table = gtk_table_new(2, (guint)(param->info->cc_limit + 1), FALSE);
		widget_set_backing_store(button_table);
		gtk_container_add(GTK_CONTAINER(event), button_table);
		gtk_table_attach(GTK_TABLE(table), event,
		                 col + 1, col + 3, row, row + 1,
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 0, 0);
		param->info->event = event;

		/* create buttons */
		button = NULL;
		button_group = NULL;
		param->info->button[0] = NULL;
		for (j = 0; param->info->list_labels[j] != NULL; j++) {
			/* create label to sit over the button */
			event = gtk_event_box_new();
			widget_set_backing_store(event);
			label = gtk_label_new(param->info->list_labels[j]);
			gtk_widget_set_name(label, "ButtonLabel");
			widget_set_custom_font(label, phasex_font_desc);
			widget_set_backing_store(label);
			gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
			gtk_misc_set_alignment(GTK_MISC(label), 0.0, 1.0);
			gtk_container_add(GTK_CONTAINER(event), label);
			gtk_table_attach(GTK_TABLE(button_table), event,
			                 j, (j + 1), 0, 1,
			                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			                 0, 0);
			param->info->button_event[j][0] = event;

			/* create individual button widget */
			event = gtk_event_box_new();
			widget_set_backing_store(event);
			button = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(button));
			if ((strcasecmp(param->info->list_labels[j], "off") == 0) ||
			    (strcasecmp(param->info->list_labels[j], "<small>off</small>") == 0)) {
				gtk_widget_set_name(button, "OffButton");
			}
			else {
				gtk_widget_set_name(button, "ParamButton");
			}
			widget_set_backing_store(button);
			gtk_container_add(GTK_CONTAINER(event), button);
			gtk_table_attach(GTK_TABLE(button_table), event,
			                 j, (j + 1), 1, 2,
			                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			                 0, 0);
			param->info->button_event[j][1] = event;

			param->info->button[j]       = button;
			param->info->button_label[j] = label;
		}

		/* set active button before connecting signals */
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(param->info->button[param->value.cc_val]), TRUE);

		/* connect toggled signal for each button */
		for (j = 0; param->info->button[j] != NULL; j++) {
			g_signal_connect(G_OBJECT(param->info->button[j]),
			                 "toggled",
			                 GTK_SIGNAL_FUNC(on_gui_param_update),
			                 (gpointer) param);

			for (k = 0; k <= 1; k++) {
				g_signal_connect_swapped(G_OBJECT(param->info->button_event[j][k]),
				                         "button_press_event",
				                         GTK_SIGNAL_FUNC(on_param_button_event),
				                         (gpointer) param);
				g_signal_connect_swapped(G_OBJECT(param->info->button_event[j][k]),
				                         "scroll_event",
				                         GTK_SIGNAL_FUNC(on_param_button_event),
				                         (gpointer) param);
				g_signal_connect_swapped(G_OBJECT(param->info->button_event[j][k]),
				                         "key_press_event",
				                         GTK_SIGNAL_FUNC(on_param_button_event),
				                         (gpointer) param);
				g_signal_connect_swapped(G_OBJECT(param->info->button_event[j][k]),
				                         "enter_notify_event",
				                         GTK_SIGNAL_FUNC(on_param_button_event),
				                         (gpointer) param);
				g_signal_connect_swapped(G_OBJECT(param->info->button_event[j][k]),
				                         "leave_notify_event",
				                         GTK_SIGNAL_FUNC(on_param_button_event),
				                         (gpointer) param);
			}
		}

		break;
	}

	/* Add this parameter's primary event box to the nav list */
	if ((cur = malloc(sizeof(PARAM_NAV_LIST))) == NULL) {
		phasex_shutdown("Out of Memory!\n");
	}
	cur->widget     = param->info->event;
	cur->param_info = param->info;
	cur->page_num   = page_num;
	cur->next       = NULL;
	cur->prev       = param_nav_tail;
	if (param_nav_head == NULL) {
		param_nav_head = cur;
	}
	else {
		param_nav_tail->next = cur;
	}
	param_nav_tail = cur;
	param->info->param_nav = cur;
}


/*****************************************************************************
 * update_param_sensitivities()
 *****************************************************************************/
void
update_param_sensitivities(void)
{
	unsigned int    param_num;

	for (param_num = PARAM_CHORUS_AMOUNT; param_num < NUM_PARAMS; param_num++) {
		update_param_sensitivity(visible_part_num, param_num);
		update_param_sensitivity(visible_part_num, param_num);
		get_param(visible_part_num, param_num)->updated = 1;
	}
}


/*****************************************************************************
 * update_param_sensitivity()
 *****************************************************************************/
void
update_param_sensitivity(unsigned int part_num, unsigned int id)
{
	PARAM_INFO  *param_info = get_param_info_by_id(id);
	int         j;
	int         k;

	/* if parameter needs to be sensitive, make it so */
	if (get_param_sensitivity(part_num, id)) {
		if (param_info->knob != NULL) {
			gtk_widget_set_sensitive(GTK_WIDGET(param_info->knob), TRUE);
			gtk_widget_set_state(GTK_WIDGET(param_info->knob), GTK_STATE_NORMAL);
		}
		if (param_info->text != NULL) {
			gtk_widget_set_sensitive(param_info->text, TRUE);
			gtk_widget_set_state(param_info->text, GTK_STATE_NORMAL);
		}
		if (param_info->event != NULL) {
			gtk_widget_set_sensitive(param_info->event, TRUE);
			gtk_widget_set_state(param_info->event, GTK_STATE_NORMAL);
		}
		for (j = 0; param_info->button[j] != NULL; j++) {
			gtk_widget_set_sensitive(param_info->button[j], TRUE);
			gtk_widget_set_state(param_info->button[j], GTK_STATE_NORMAL);
		}
		for (j = 0; param_info->button_event[j][1] != NULL; j++) {
			for (k = 0; k <= 1; k++) {
				gtk_widget_set_sensitive(param_info->button_event[j][k], TRUE);
				gtk_widget_set_state(param_info->button_event[j][k], GTK_STATE_NORMAL);
			}
		}
		for (j = 0; param_info->button_label[j] != NULL; j++) {
			gtk_widget_set_sensitive(param_info->button_label[j], TRUE);
			gtk_widget_set_state(param_info->button_label[j], GTK_STATE_NORMAL);
		}
		param_info->sensitive = 1;
	}
	/* if parameter needs to be insensitive, make it so */
	else {
		if (param_info->knob != NULL) {
			gtk_widget_set_sensitive(GTK_WIDGET(param_info->knob), FALSE);
			gtk_widget_set_state(GTK_WIDGET(param_info->knob), GTK_STATE_INSENSITIVE);
		}
		if (param_info->text != NULL) {
			gtk_widget_set_sensitive(param_info->text, FALSE);
			gtk_widget_set_state(param_info->text, GTK_STATE_INSENSITIVE);
		}
		if (param_info->event != NULL) {
			gtk_widget_set_sensitive(param_info->event, FALSE);
			gtk_widget_set_state(param_info->event, GTK_STATE_INSENSITIVE);
		}
		for (j = 0; param_info->button[j] != NULL; j++) {
			gtk_widget_set_sensitive(param_info->button[j], FALSE);
			gtk_widget_set_state(param_info->button[j], GTK_STATE_INSENSITIVE);
		}
		for (j = 0; param_info->button_event[j][1] != NULL; j++) {
			for (k = 0; k <= 1; k++) {
				gtk_widget_set_sensitive(param_info->button_event[j][k], FALSE);
				gtk_widget_set_state(param_info->button_event[j][k], GTK_STATE_INSENSITIVE);
			}
		}
		for (j = 0; param_info->button_label[j] != NULL; j++) {
			gtk_widget_set_sensitive(param_info->button_label[j], FALSE);
			gtk_widget_set_state(param_info->button_label[j], GTK_STATE_INSENSITIVE);
		}
		param_info->sensitive = 0;
	}
}


/*****************************************************************************
 * get_param_sensitivity()
 *****************************************************************************/
int
get_param_sensitivity(unsigned int part_num, unsigned int id)
{
	int     sensitive = 1;
	PATCH   *patch    = get_patch(visible_sess_num, part_num, visible_prog_num[part_num]);

	switch (id) {
	case PARAM_CHORUS_AMOUNT:
	case PARAM_CHORUS_TIME:
	case PARAM_CHORUS_FEED:
	case PARAM_CHORUS_CROSSOVER:
	case PARAM_CHORUS_LFO_WAVE:
	case PARAM_CHORUS_LFO_RATE:
	case PARAM_CHORUS_PHASE_RATE:
	case PARAM_CHORUS_PHASE_BALANCE:
		if (patch->param[PARAM_CHORUS_MIX].value.cc_val == 0) {
			sensitive = 0;
		}
		break;
	case PARAM_DELAY_FEED:
	case PARAM_DELAY_TIME:
	case PARAM_DELAY_CROSSOVER:
	case PARAM_DELAY_LFO:
		if (patch->param[PARAM_DELAY_MIX].value.cc_val == 0) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_POLARITY:
	case PARAM_OSC2_POLARITY:
	case PARAM_OSC3_POLARITY:
	case PARAM_OSC4_POLARITY:
		if (patch->param[id - 1].value.cc_val == MOD_TYPE_OFF) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_FREQ_BASE:
	case PARAM_OSC2_FREQ_BASE:
	case PARAM_OSC3_FREQ_BASE:
	case PARAM_OSC4_FREQ_BASE:
		if (patch->param[id - 2].value.cc_val == MOD_TYPE_OFF) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_WAVE:
	case PARAM_OSC2_WAVE:
	case PARAM_OSC3_WAVE:
	case PARAM_OSC4_WAVE:
		if ((patch->param[id - 3].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 1].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 1].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_RATE:
	case PARAM_OSC2_RATE:
	case PARAM_OSC3_RATE:
	case PARAM_OSC4_RATE:
		if ((patch->param[id - 4].value.cc_val == MOD_TYPE_OFF) ||
		    (patch->param[id - 2].value.cc_val < FREQ_BASE_TEMPO)) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_INIT_PHASE:
	case PARAM_OSC2_INIT_PHASE:
	case PARAM_OSC3_INIT_PHASE:
	case PARAM_OSC4_INIT_PHASE:
		if ((patch->param[id - 5].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 3].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 3].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_TRANSPOSE:
	case PARAM_OSC2_TRANSPOSE:
	case PARAM_OSC3_TRANSPOSE:
	case PARAM_OSC4_TRANSPOSE:
		if ((patch->param[id - 6].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 4].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 4].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_FINE_TUNE:
	case PARAM_OSC2_FINE_TUNE:
	case PARAM_OSC3_FINE_TUNE:
	case PARAM_OSC4_FINE_TUNE:
		if ((patch->param[id - 7].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 5].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 5].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_PITCHBEND:
	case PARAM_OSC2_PITCHBEND:
	case PARAM_OSC3_PITCHBEND:
	case PARAM_OSC4_PITCHBEND:
		if ((patch->param[id - 8].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 6].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 6].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_AM_LFO:
	case PARAM_OSC2_AM_LFO:
	case PARAM_OSC3_AM_LFO:
	case PARAM_OSC4_AM_LFO:
		if (patch->param[id - 9].value.cc_val == MOD_TYPE_OFF) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_AM_LFO_AMOUNT:
	case PARAM_OSC2_AM_LFO_AMOUNT:
	case PARAM_OSC3_AM_LFO_AMOUNT:
	case PARAM_OSC4_AM_LFO_AMOUNT:
		if ((patch->param[id - 10].value.cc_val == MOD_TYPE_OFF) ||
		    (patch->param[id - 1].value.cc_val == 0)) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_FREQ_LFO:
	case PARAM_OSC2_FREQ_LFO:
	case PARAM_OSC3_FREQ_LFO:
	case PARAM_OSC4_FREQ_LFO:
		if ((patch->param[id - 11].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 9].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 9].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_FREQ_LFO_AMOUNT:
	case PARAM_OSC2_FREQ_LFO_AMOUNT:
	case PARAM_OSC3_FREQ_LFO_AMOUNT:
	case PARAM_OSC4_FREQ_LFO_AMOUNT:
		if ((patch->param[id - 12].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 10].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 10].value.cc_val < FREQ_BASE_TEMPO)) ||
		    (patch->param[id - 1].value.cc_val == 0)) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_FREQ_LFO_FINE:
	case PARAM_OSC2_FREQ_LFO_FINE:
	case PARAM_OSC3_FREQ_LFO_FINE:
	case PARAM_OSC4_FREQ_LFO_FINE:
		if ((patch->param[id - 13].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 11].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 11].value.cc_val < FREQ_BASE_TEMPO)) ||
		    (patch->param[id - 2].value.cc_val == 0)) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_PHASE_LFO:
	case PARAM_OSC2_PHASE_LFO:
	case PARAM_OSC3_PHASE_LFO:
	case PARAM_OSC4_PHASE_LFO:
		if ((patch->param[id - 14].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 12].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 12].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_PHASE_LFO_AMOUNT:
	case PARAM_OSC2_PHASE_LFO_AMOUNT:
	case PARAM_OSC3_PHASE_LFO_AMOUNT:
	case PARAM_OSC4_PHASE_LFO_AMOUNT:
		if ((patch->param[id - 15].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 13].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 13].value.cc_val < FREQ_BASE_TEMPO)) ||
		    (patch->param[id - 1].value.cc_val == 0)) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_WAVE_LFO:
	case PARAM_OSC2_WAVE_LFO:
	case PARAM_OSC3_WAVE_LFO:
	case PARAM_OSC4_WAVE_LFO:
		if ((patch->param[id - 16].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 14].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 14].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	case PARAM_OSC1_WAVE_LFO_AMOUNT:
	case PARAM_OSC2_WAVE_LFO_AMOUNT:
	case PARAM_OSC3_WAVE_LFO_AMOUNT:
	case PARAM_OSC4_WAVE_LFO_AMOUNT:
		if ((patch->param[id - 17].value.cc_val == MOD_TYPE_OFF) ||
		    ((patch->param[id - 15].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 15].value.cc_val < FREQ_BASE_TEMPO)) ||
		    (patch->param[id - 1].value.cc_val == 0)) {
			sensitive = 0;
		}
		break;
	case PARAM_LFO1_WAVE:
	case PARAM_LFO2_WAVE:
	case PARAM_LFO3_WAVE:
	case PARAM_LFO4_WAVE:
		if (((patch->param[id - 1].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 1].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	case PARAM_LFO1_RATE:
	case PARAM_LFO2_RATE:
	case PARAM_LFO3_RATE:
	case PARAM_LFO4_RATE:
		if (patch->param[id - 2].value.cc_val < FREQ_BASE_TEMPO) {
			sensitive = 0;
		}
		break;
	case PARAM_LFO1_INIT_PHASE:
	case PARAM_LFO2_INIT_PHASE:
	case PARAM_LFO3_INIT_PHASE:
	case PARAM_LFO4_INIT_PHASE:
		if (((patch->param[id - 3].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 3].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	case PARAM_LFO1_TRANSPOSE:
	case PARAM_LFO2_TRANSPOSE:
	case PARAM_LFO3_TRANSPOSE:
	case PARAM_LFO4_TRANSPOSE:
		if (((patch->param[id - 4].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 4].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	case PARAM_LFO1_PITCHBEND:
	case PARAM_LFO2_PITCHBEND:
	case PARAM_LFO3_PITCHBEND:
	case PARAM_LFO4_PITCHBEND:
		if (((patch->param[id - 5].value.cc_val > FREQ_BASE_MIDI_KEY) &&
		     (patch->param[id - 5].value.cc_val < FREQ_BASE_TEMPO))) {
			sensitive = 0;
		}
		break;
	}

	return sensitive;
}


/*****************************************************************************
 * update_param_child_sensitivities()
 *****************************************************************************/
void
update_param_child_sensitivities(unsigned int part_num, unsigned int id)
{
	unsigned int    param_id;
	unsigned int    start = NUM_PARAMS;
	unsigned int    end   = NUM_PARAMS;

	if (gtkui_ready) {
		switch (id) {
		case PARAM_CHORUS_MIX:
			start = id + 1;
			end   = id + 8;
			break;
		case PARAM_DELAY_MIX:
			start = id + 1;
			end   = id + 5;
			break;
		case PARAM_OSC1_MODULATION:
		case PARAM_OSC2_MODULATION:
		case PARAM_OSC3_MODULATION:
		case PARAM_OSC4_MODULATION:
			start = id + 1;
			end   = id + 17;
			break;
		case PARAM_OSC1_FREQ_BASE:
		case PARAM_OSC2_FREQ_BASE:
		case PARAM_OSC3_FREQ_BASE:
		case PARAM_OSC4_FREQ_BASE:
			start = id + 1;
			end   = id + 15;
			break;
		case PARAM_OSC1_AM_LFO:
		case PARAM_OSC2_AM_LFO:
		case PARAM_OSC3_AM_LFO:
		case PARAM_OSC4_AM_LFO:
			start = id + 1;
			end   = id + 1;
			break;
		case PARAM_OSC1_FREQ_LFO:
		case PARAM_OSC2_FREQ_LFO:
		case PARAM_OSC3_FREQ_LFO:
		case PARAM_OSC4_FREQ_LFO:
			start = id + 1;
			end   = id + 2;
			break;
		case PARAM_OSC1_PHASE_LFO:
		case PARAM_OSC2_PHASE_LFO:
		case PARAM_OSC3_PHASE_LFO:
		case PARAM_OSC4_PHASE_LFO:
			start = id + 1;
			end   = id + 1;
			break;
		case PARAM_OSC1_WAVE_LFO:
		case PARAM_OSC2_WAVE_LFO:
		case PARAM_OSC3_WAVE_LFO:
		case PARAM_OSC4_WAVE_LFO:
			start = id + 1;
			end   = id + 1;
			break;
		case PARAM_LFO1_FREQ_BASE:
		case PARAM_LFO2_FREQ_BASE:
		case PARAM_LFO3_FREQ_BASE:
		case PARAM_LFO4_FREQ_BASE:
			start = id + 1;
			end   = id + 5;
			break;
		}
		if (start < NUM_PARAMS) {
			for (param_id = start; param_id <= end; param_id++) {
				update_param_sensitivity(part_num, param_id);
			}
		}
	}
}
