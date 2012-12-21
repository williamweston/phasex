/*****************************************************************************
 *
 * gui_navbar.c
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
#include "param_cb.h"
#include "param_strings.h"
#include "bank.h"
#include "session.h"
#include "settings.h"
#include "engine.h"
#include "gui_main.h"
#include "gui_bank.h"
#include "gui_session.h"
#include "gui_param.h"
#include "gui_patch.h"
#include "gui_midimap.h"
#include "gui_navbar.h"
#include "gtkknob.h"
#include "midi_process.h"
#include "midi_event.h"
#include "timekeeping.h"
#include "buffer.h"
#include "debug.h"


GtkObject   *session_adj                = NULL;
GtkWidget   *session_spin               = NULL;
GtkWidget   *session_entry              = NULL;

GtkObject   *program_adj                = NULL;
GtkWidget   *program_spin               = NULL;
GtkWidget   *program_entry              = NULL;

GtkObject   *part_adj                   = NULL;
GtkWidget   *part_spin                  = NULL;
GtkWidget   *part_entry                 = NULL;

GtkWidget   *midi_channel_label         = NULL;
GtkWidget   *midi_channel_event_box     = NULL;
GtkObject   *midi_channel_adj           = NULL;

GtkWidget   *patch_modified_label       = NULL;
GtkWidget   *session_modified_label     = NULL;

int         show_patch_modified         = 0;
int         show_session_modified       = 0;

char        *modified_label_text[2]     = { " ", "!" };


guint navbar_table_coords[3][11][2] = {
	{
		// Notebook
		{ 0, 0 },   // session spin
		{ 0, 1 },   // program spin
		{ 1, 0 },   // session name
		{ 2, 0 },   // session load / save
		{ 3, 0 },   // session modified
		{ 1, 1 },   // program name
		{ 2, 1 },   // program load / save
		{ 3, 1 },   // patch modified
		{ 4, 1 },   // test note / sound off
		{ 5, 1 },   // midi channel
		{ 5, 0 }    // part spin
	},
	{
		// One Page
		{ 0, 0 },   // session spin
		{ 0, 1 },   // program spin
		{ 1, 0 },   // session name
		{ 2, 0 },   // session load / save
		{ 3, 0 },   // session modified
		{ 1, 1 },   // program name
		{ 2, 1 },   // program load / save
		{ 3, 1 },   // patch modified
		{ 4, 1 },   // test note / sound off
		{ 5, 1 },   // midi channel
		{ 5, 0 },   // part spin
	},
	{
		// Widescreen
		{ 7, 0 },   // session spin
		{ 2, 0 },   // program spin
		{ 8, 0 },   // session name
		{ 9, 0 },   // session load / save
		{ 10, 0 },  // session modified
		{ 3, 0 },   // program name
		{ 4, 0 },   // program load / save
		{ 5, 0 },   // patch modified
		{ 6, 0 },   // test note / sound off
		{ 1, 0 },   // midi channel
		{ 0, 0 }    // part spin
	}
};

guint navbar_table_size[3][2] = {
	{ 7, 2 },
	{ 7, 2 },
	{ 11, 2 }
};

/*****************************************************************************
 * create_navbar()
 *
 * Create the group of controls for selecting patches from the bank.
 *****************************************************************************/
void
create_navbar(GtkWidget *UNUSED(main_window), GtkWidget *parent_vbox)
{
	SESSION         *session = get_current_session();
	PATCH           *patch   = get_visible_patch();
	PART            *part    = get_visible_part();
	GtkWidget       *frame;
	GtkWidget       *frame_event;
	GtkWidget       *box;
	GtkWidget       *vbox;
	GtkWidget       *event;
	GtkWidget       *label;
	GtkWidget       *button;
	GtkWidget       *knob;
	GtkWidget       *table;
	char            label_text[64];
	unsigned int    prog     = get_visible_program_number();
	int             j        = setting_window_layout;
	int             k        = 0;

	/* Create frame with label and table, then pack in controls */
	frame = gtk_frame_new(NULL);
	gtk_widget_set_name(frame, "PatchGroup");
	widget_set_backing_store(frame);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);
	gtk_box_pack_start(GTK_BOX(parent_vbox), frame, FALSE, FALSE, 0);

	/* Frame label */
	event = gtk_event_box_new();
	widget_set_backing_store(event);
	snprintf(label_text, sizeof(label_text),
	         "<b>phasex v%s (developer's release)</b>",
	         PACKAGE_VERSION);
	label = gtk_label_new(label_text);
	gtk_widget_set_name(label, "GroupName");
	widget_set_backing_store(label);
	widget_set_custom_font(label, phasex_font_desc);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_container_add(GTK_CONTAINER(event), label);
	gtk_frame_set_label_widget(GTK_FRAME(frame), event);
	gtk_frame_set_label_align(GTK_FRAME(frame), 1.0, 0.5);

	/* Frame event box */
	frame_event = gtk_event_box_new();
	gtk_widget_set_name(frame_event, "PatchGroup");
	widget_set_backing_store(frame_event);
	gtk_container_add(GTK_CONTAINER(frame), frame_event);

	/* Main Table */
	table = gtk_table_new(navbar_table_size[k][1], navbar_table_size[k][0], FALSE);
	gtk_container_add(GTK_CONTAINER(frame_event), table);

	/* no session handling with only a single part */
	if (MAX_PARTS > 1) {
		/* *** Session selector box (label + spin) */
		box = gtk_hbox_new(FALSE, 0);
		widget_set_backing_store(box);
		table_add_widget(GTK_TABLE(table), box,
		                 navbar_table_coords[j][k][0], navbar_table_coords[j][k][1], 1, 1,
		                 (GtkAttachOptions)(GTK_FILL),
		                 (GtkAttachOptions)(GTK_FILL),
		                 0, 0, JUSTIFY_RIGHT);

		/* Clickable Session selector label */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		gtk_widget_set_name(event, "session_number");
		label = gtk_label_new("Session #:");
		gtk_widget_set_name(label, "PatchParam");
		widget_set_custom_font(label, phasex_font_desc);
		widget_set_backing_store(label);
		gtk_container_add(GTK_CONTAINER(event), label);
		gtk_box_pack_start(GTK_BOX(box), event, FALSE, FALSE, 0);

		/* connect signal for clicking session number label */
		g_signal_connect(G_OBJECT(event), "button_press_event",
		                 GTK_SIGNAL_FUNC(param_label_button_press),
		                 (gpointer) event);

		/* Session selector adjustment */
		session_adj = gtk_adjustment_new((visible_sess_num + 1), 1, PATCH_BANK_SIZE, 1, 8, 0);

		/* Session selector spin button */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		session_spin = gtk_spin_button_new(GTK_ADJUSTMENT(session_adj), 0, 0);
		gtk_widget_set_name(session_spin, "NumericSpin");
		widget_set_custom_font(session_spin, numeric_font_desc);
		widget_set_backing_store(session_spin);
		gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(session_spin), TRUE);
		gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(session_spin), GTK_UPDATE_IF_VALID);
		gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(session_spin), TRUE);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(session_spin), (visible_sess_num + 1));
		gtk_container_add(GTK_CONTAINER(event), session_spin);
		gtk_box_pack_start(GTK_BOX(box), event, FALSE, FALSE, 0);
	}
	k++;

	/* *** Program selector box (label + spin) */
	box = gtk_hbox_new(FALSE, 0);
	widget_set_backing_store(box);
	table_add_widget(GTK_TABLE(table), box,
	                 navbar_table_coords[j][k][0], navbar_table_coords[j][k][1], 1, 1,
	                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
	                 (GtkAttachOptions)(GTK_FILL),
	                 0, 0, JUSTIFY_RIGHT);
	k++;

	/* Clickable Program selector label */
	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_widget_set_name(event, "program_number");
	label = gtk_label_new("Program #:");
	gtk_widget_set_name(label, "PatchParam");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_container_add(GTK_CONTAINER(event), label);
	gtk_box_pack_start(GTK_BOX(box), event, FALSE, FALSE, 0);

	/* connect signal for clicking program number label */
	g_signal_connect(G_OBJECT(event), "button_press_event",
	                 GTK_SIGNAL_FUNC(param_label_button_press),
	                 (gpointer) event);

	/* Program selector adjustment */
	program_adj = gtk_adjustment_new((prog + 1), 1, PATCH_BANK_SIZE, 1, 8, 0);

	/* Program selector spin button */
	event = gtk_event_box_new();
	widget_set_backing_store(event);
	program_spin = gtk_spin_button_new(GTK_ADJUSTMENT(program_adj), 0, 0);
	gtk_widget_set_name(program_spin, "NumericSpin");
	widget_set_custom_font(program_spin, numeric_font_desc);
	widget_set_backing_store(program_spin);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(program_spin), TRUE);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(program_spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(program_spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(program_spin), (prog + 1));
	gtk_container_add(GTK_CONTAINER(event), program_spin);
	gtk_box_pack_start(GTK_BOX(box), event, FALSE, FALSE, 0);

	/* no session handling with only a single part */
	if (MAX_PARTS > 1) {
		/* *** Session name box (label + entry) */
		box = gtk_hbox_new(FALSE, 0);
		widget_set_backing_store(box);
		table_add_widget(GTK_TABLE(table), box,
		                 navbar_table_coords[j][k][0], navbar_table_coords[j][k][1], 1, 1,
		                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
		                 (GtkAttachOptions)(GTK_FILL),
		                 0, 0, JUSTIFY_RIGHT);
		k++;

		/* Clickable session name label */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		gtk_widget_set_name(event, "session_name");
		label = gtk_label_new("Session:");
		gtk_widget_set_name(label, "PatchParam");
		widget_set_custom_font(label, phasex_font_desc);
		widget_set_backing_store(label);
		gtk_container_add(GTK_CONTAINER(event), label);
		gtk_box_pack_start(GTK_BOX(box), event, FALSE, FALSE, 0);

		/* connect signal for clicking session name label */
		g_signal_connect(G_OBJECT(event), "button_press_event",
		                 GTK_SIGNAL_FUNC(param_label_button_press),
		                 (gpointer) event);

		/* Session name text entry */
		session_entry = gtk_entry_new_with_max_length(SESSION_NAME_LEN);
		widget_set_custom_font(session_entry, numeric_font_desc);
		widget_set_backing_store(session_entry);
		if (session->name == NULL) {
			session->name = get_session_name_from_directory(session->directory);
		}
		gtk_entry_set_text(GTK_ENTRY(session_entry), session->name);

		gtk_entry_set_width_chars(GTK_ENTRY(session_entry), SESSION_NAME_LEN);
		gtk_entry_set_editable(GTK_ENTRY(session_entry), TRUE);
		gtk_box_pack_start(GTK_BOX(box), session_entry, FALSE, FALSE, 0);

		/* Wait until after session name text entry creation
		   so that it can be passed in as the argument for the
		   session_adj callback. */
		g_signal_connect(GTK_OBJECT(session_adj), "value_changed",
		                 GTK_SIGNAL_FUNC(select_session),
		                 (gpointer) session_entry);

		/* *** Session load & save buttons */
		box = gtk_hbox_new(TRUE, 0);
		widget_set_backing_store(box);
		table_add_widget(GTK_TABLE(table), box,
		                 navbar_table_coords[j][k][0], navbar_table_coords[j][k][1], 1, 1,
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 0, 0, JUSTIFY_LEFT);
		k++;

		/* Load button */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		button = gtk_button_new();
		widget_set_backing_store(button);
		label = gtk_label_new("<small>Load\nSession</small>");
		gtk_widget_set_name(label, "PhasexButton");
		widget_set_custom_font(label, phasex_font_desc);
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_label_set_width_chars(GTK_LABEL(label), 7);
#else
		gtk_widget_set_size_request(label, 72, 22);
#endif
		gtk_button_set_image(GTK_BUTTON(button), label);
		gtk_container_add(GTK_CONTAINER(event), button);
		gtk_box_pack_start(GTK_BOX(box), event, TRUE, TRUE, 1);

		g_signal_connect(GTK_OBJECT(button), "clicked",
		                 GTK_SIGNAL_FUNC(on_load_session),
		                 (gpointer) session_entry);

		/* Save button */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		button = gtk_button_new();
		widget_set_backing_store(button);
		label = gtk_label_new("<small>Save\nSession</small>");
		gtk_widget_set_name(label, "PhasexButton");
		widget_set_custom_font(label, phasex_font_desc);
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_label_set_width_chars(GTK_LABEL(label), 7);
#else
		gtk_widget_set_size_request(label, 72, 22);
#endif
		gtk_button_set_image(GTK_BUTTON(button), label);
		gtk_container_add(GTK_CONTAINER(event), button);
		gtk_box_pack_start(GTK_BOX(box), event, TRUE, TRUE, 1);

		g_signal_connect(GTK_OBJECT(button), "clicked",
		                 GTK_SIGNAL_FUNC(on_save_session),
		                 (gpointer) session_entry);

		/* *** Session Modified label */
		vbox = gtk_vbox_new(FALSE, 0);
		event = gtk_event_box_new();
		gtk_widget_set_name(event, "IndicatorLabel");
		widget_set_backing_store(event);
		session_modified_label = gtk_label_new(modified_label_text[show_session_modified]);
		gtk_widget_set_name(session_modified_label, "IndicatorLabel");
		widget_set_custom_font(session_modified_label, numeric_font_desc);
		widget_set_backing_store(session_modified_label);
#if GTK_CHECK_VERSION(2, 6, 0)
		gtk_label_set_width_chars(GTK_LABEL(session_modified_label), 1);
#else
		gtk_widget_set_size_request(session_modified_label, 12, 12);
#endif
		gtk_container_add(GTK_CONTAINER(event), session_modified_label);
		gtk_box_pack_start(GTK_BOX(vbox), gtk_event_box_new(), TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), event, FALSE, FALSE, 0);
		gtk_box_pack_end(GTK_BOX(vbox), gtk_event_box_new(), TRUE, TRUE, 0);
		table_add_widget(GTK_TABLE(table), vbox,
		                 navbar_table_coords[j][k][0], navbar_table_coords[j][k][1], 1, 1,
		                 (GtkAttachOptions)(GTK_FILL),
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 0, 0, JUSTIFY_LEFT);
		k++;
	}
	else {
		k += 3;
	}

	/* *** Patch name box (label + entry) */
	box = gtk_hbox_new(FALSE, 0);
	widget_set_backing_store(box);
	table_add_widget(GTK_TABLE(table), box,
	                 navbar_table_coords[j][k][0], navbar_table_coords[j][k][1], 1, 1,
	                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
	                 (GtkAttachOptions)(GTK_EXPAND),
	                 0, 0, JUSTIFY_RIGHT);
	k++;

	/* Clickable patch name label */
	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_widget_set_name(event, "patch_name");
	label = gtk_label_new("Patch:");
	gtk_widget_set_name(label, "PatchParam");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_container_add(GTK_CONTAINER(event), label);
	gtk_box_pack_start(GTK_BOX(box), event, FALSE, FALSE, 0);

	/* connect signal for clicking patch name label */
	g_signal_connect(G_OBJECT(event), "button_press_event",
	                 GTK_SIGNAL_FUNC(param_label_button_press),
	                 (gpointer) event);

	/* Patch name text entry */
	program_entry = gtk_entry_new_with_max_length(PATCH_NAME_LEN);
	widget_set_custom_font(program_entry, numeric_font_desc);
	widget_set_backing_store(program_entry);
	if (patch->name == NULL) {
		patch->name = get_patch_name_from_filename(patch->filename);
	}
	gtk_entry_set_text(GTK_ENTRY(program_entry), patch->name);

	gtk_entry_set_width_chars(GTK_ENTRY(program_entry), PATCH_NAME_LEN);
	gtk_entry_set_editable(GTK_ENTRY(program_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(box), program_entry, FALSE, FALSE, 0);

	/* Wait until after patch name text entry creation so that it can be
	   passed in as the argument for the program_adj callback. */
	g_signal_connect(GTK_OBJECT(program_adj), "value_changed",
	                 GTK_SIGNAL_FUNC(select_program),
	                 (gpointer) program_entry);

	/* *** Patch load & save buttons */
	vbox = gtk_vbox_new(FALSE, 0);
	box = gtk_hbox_new(TRUE, 0);
	widget_set_backing_store(box);
	table_add_widget(GTK_TABLE(table), box,
	                 navbar_table_coords[j][k][0], navbar_table_coords[j][k][1], 1, 1,
	                 (GtkAttachOptions)(GTK_EXPAND),
	                 (GtkAttachOptions)(GTK_EXPAND),
	                 0, 0, JUSTIFY_LEFT);
	k++;

	/* Load button */
	event = gtk_event_box_new();
	widget_set_backing_store(event);
	button = gtk_button_new();
	widget_set_backing_store(button);
	label = gtk_label_new("<small>Load\nPatch</small>");
	gtk_widget_set_name(label, "PhasexButton");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 7);
#else
	gtk_widget_set_size_request(label, 72, 22);
#endif
	gtk_button_set_image(GTK_BUTTON(button), label);
	gtk_container_add(GTK_CONTAINER(event), button);
	gtk_box_pack_start(GTK_BOX(box), event, TRUE, TRUE, 1);

	g_signal_connect(GTK_OBJECT(button), "clicked",
	                 GTK_SIGNAL_FUNC(load_program),
	                 (gpointer) program_entry);

	/* Save button */
	event = gtk_event_box_new();
	widget_set_backing_store(event);
	button = gtk_button_new();
	widget_set_backing_store(button);
	label = gtk_label_new("<small>Save\nPatch</small>");
	gtk_widget_set_name(label, "PhasexButton");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 7);
#else
	gtk_widget_set_size_request(label, 72, 22);
#endif
	gtk_button_set_image(GTK_BUTTON(button), label);
	gtk_container_add(GTK_CONTAINER(event), button);
	gtk_box_pack_start(GTK_BOX(box), event, TRUE, TRUE, 1);

	g_signal_connect(GTK_OBJECT(button), "clicked",
	                 GTK_SIGNAL_FUNC(save_program),
	                 (gpointer) program_entry);

	/* *** Patch Modified label */
	event = gtk_event_box_new();
	gtk_widget_set_name(event, "IndicatorLabel");
	widget_set_backing_store(event);
	patch_modified_label = gtk_label_new(modified_label_text[show_patch_modified]);
	gtk_widget_set_name(patch_modified_label, "IndicatorLabel");
	widget_set_custom_font(patch_modified_label, numeric_font_desc);
	widget_set_backing_store(patch_modified_label);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(patch_modified_label), 1);
#else
	gtk_widget_set_size_request(patch_modified_label, 12, 12);
#endif
	gtk_container_add(GTK_CONTAINER(event), patch_modified_label);
	gtk_box_pack_start(GTK_BOX(vbox), gtk_event_box_new(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), event, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), gtk_event_box_new(), TRUE, TRUE, 0);
	table_add_widget(GTK_TABLE(table), vbox,
	                 navbar_table_coords[j][k][0], navbar_table_coords[j][k][1], 1, 1,
	                 (GtkAttachOptions)(GTK_FILL),
	                 (GtkAttachOptions)(GTK_EXPAND),
	                 0, 0, JUSTIFY_LEFT);
	k++;

	/* *** Test and Panic buttons */
	box = gtk_hbox_new(TRUE, 0);
	widget_set_backing_store(box);
	table_add_widget(GTK_TABLE(table), box,
	                 navbar_table_coords[j][k][0], navbar_table_coords[j][k][1], 1, 1,
	                 (GtkAttachOptions)(GTK_EXPAND),
	                 (GtkAttachOptions)(GTK_EXPAND),
	                 0, 0, JUSTIFY_CENTER);
	k++;

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	button = gtk_button_new();
	widget_set_backing_store(button);
	label = gtk_label_new("<small>Test\nNote</small>");
	gtk_widget_set_name(label, "PhasexButton");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 8);
#else
	gtk_widget_set_size_request(label, 72, 22);
#endif
	gtk_button_set_image(GTK_BUTTON(button), label);
	gtk_container_add(GTK_CONTAINER(event), button);
	gtk_box_pack_start(GTK_BOX(box), event, TRUE, TRUE, 1);

	g_signal_connect(GTK_OBJECT(button), "clicked",
	                 GTK_SIGNAL_FUNC(queue_test_note),
	                 (gpointer) NULL);

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	button = gtk_button_new();
	widget_set_custom_font(button, phasex_font_desc);
	widget_set_backing_store(button);
	label = gtk_label_new("<small>Notes\nOff</small>");
	gtk_widget_set_name(label, "PhasexButton");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 8);
#else
	gtk_widget_set_size_request(label, 72, 22);
#endif
	gtk_button_set_image(GTK_BUTTON(button), label);
	gtk_container_add(GTK_CONTAINER(event), button);
	gtk_box_pack_start(GTK_BOX(box), event, TRUE, TRUE, 1);

	g_signal_connect(GTK_OBJECT(button), "clicked",
	                 GTK_SIGNAL_FUNC(broadcast_notes_off),
	                 (gpointer) NULL);

	/* *** MIDI CH selector box (label + knob + label) */
	box = gtk_hbox_new(FALSE, 0);
	widget_set_backing_store(box);
	table_add_widget(GTK_TABLE(table), box,
	                 navbar_table_coords[j][k][0], navbar_table_coords[j][k][1], 1, 1,
	                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
	                 (GtkAttachOptions)(GTK_EXPAND),
	                 0, 0, JUSTIFY_RIGHT);
	k++;

	/* parameter name label */
	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_widget_set_name(event, "midi_channel");
	label = gtk_label_new("MIDI Ch:");
	gtk_widget_set_name(label, "PatchParam");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_misc_set_padding(GTK_MISC(label), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.8, 0.5);
	gtk_container_add(GTK_CONTAINER(event), label);
	gtk_box_pack_start(GTK_BOX(box), event, FALSE, FALSE, 0);

	/* connect signal for midi channel label */
	g_signal_connect(G_OBJECT(event), "button_press_event",
	                 GTK_SIGNAL_FUNC(param_label_button_press),
	                 (gpointer) event);

	/* create adjustment for knob widget */
	midi_channel_adj = gtk_adjustment_new(part->midi_channel, 0, 16, 1, 1, 0);

	gp->param[PARAM_MIDI_CHANNEL].info->adj = midi_channel_adj;

	/* create knob widget */
	vbox = gtk_vbox_new(FALSE, 0);
	event = gtk_event_box_new();
	widget_set_backing_store(event);
	knob = gtk_knob_new(GTK_ADJUSTMENT(midi_channel_adj), detent_knob_anim);
	widget_set_backing_store(knob);
	gtk_container_add(GTK_CONTAINER(event), knob);
	gtk_box_pack_start(GTK_BOX(vbox), event, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), vbox, FALSE, FALSE, 0);

	/* create label widget for text values */
	vbox = gtk_vbox_new(FALSE, 0);

	midi_channel_event_box = gtk_event_box_new();
	gtk_widget_set_name(midi_channel_event_box, "DetentLabel");
	g_object_set(G_OBJECT(midi_channel_event_box), "can-focus", TRUE, NULL);
	widget_set_backing_store(midi_channel_event_box);

	gp->param[PARAM_MIDI_CHANNEL].info->event = midi_channel_event_box;

	midi_channel_label = gtk_label_new(midi_ch_labels[part->midi_channel]);
	g_object_set(G_OBJECT(midi_channel_label), "can-focus", TRUE, NULL);
	gtk_widget_set_name(midi_channel_label, "DetentLabel");
	widget_set_custom_font(midi_channel_label, numeric_font_desc);
	gtk_misc_set_alignment(GTK_MISC(midi_channel_label), 0.2, 0.5);
	widget_set_backing_store(label);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(midi_channel_label), 4);
#else
	gtk_widget_set_size_request(midi_channel_label, 4, -1);
#endif
	gtk_container_add(GTK_CONTAINER(midi_channel_event_box), midi_channel_label);
	gtk_box_pack_start(GTK_BOX(vbox), gtk_event_box_new(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), midi_channel_event_box, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), gtk_event_box_new(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(box), vbox, FALSE, FALSE, 0);

	gp->param[PARAM_MIDI_CHANNEL].info->text = midi_channel_label;

	gp->param[PARAM_MIDI_CHANNEL].info->frame = frame;

	/* connect knob signals */
	g_signal_connect(GTK_OBJECT(midi_channel_adj), "value_changed",
	                 GTK_SIGNAL_FUNC(set_midi_channel),
	                 (gpointer)(knob));

	/* connect value label event box signals */
	g_signal_connect_swapped(G_OBJECT(midi_channel_event_box), "button_press_event",
	                         GTK_SIGNAL_FUNC(midi_channel_label_handle_event),
	                         (gpointer) midi_channel_event_box);
	g_signal_connect_swapped(G_OBJECT(midi_channel_event_box), "scroll_event",
	                         GTK_SIGNAL_FUNC(midi_channel_label_handle_event),
	                         (gpointer) midi_channel_event_box);
	g_signal_connect_swapped(G_OBJECT(midi_channel_event_box), "key_press_event",
	                         GTK_SIGNAL_FUNC(midi_channel_label_handle_event),
	                         (gpointer) midi_channel_event_box);
	g_signal_connect_swapped(G_OBJECT(midi_channel_event_box), "enter_notify_event",
	                         GTK_SIGNAL_FUNC(midi_channel_label_handle_event),
	                         (gpointer) midi_channel_event_box);
	g_signal_connect_swapped(G_OBJECT(midi_channel_event_box), "leave_notify_event",
	                         GTK_SIGNAL_FUNC(midi_channel_label_handle_event),
	                         (gpointer) midi_channel_event_box);
	g_signal_connect(G_OBJECT(midi_channel_event_box), "focus-in-event",
	                 GTK_SIGNAL_FUNC(on_param_label_focus_change),
	                 (gpointer) & (gp->param[PARAM_MIDI_CHANNEL]));
	g_signal_connect(G_OBJECT(midi_channel_event_box), "focus-out-event",
	                 GTK_SIGNAL_FUNC(on_param_label_focus_change),
	                 (gpointer) & (gp->param[PARAM_MIDI_CHANNEL]));

	if (MAX_PARTS > 1) {
		/* *** Part selector box (label + spin) */
		box = gtk_hbox_new(FALSE, 0);
		widget_set_backing_store(box);
		table_add_widget(GTK_TABLE(table), box,
		                 navbar_table_coords[j][k][0], navbar_table_coords[j][k][1], 1, 1,
		                 (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
		                 (GtkAttachOptions)(GTK_EXPAND),
		                 0, 0, JUSTIFY_RIGHT);

		/* Clickable Part selector label */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		gtk_widget_set_name(event, "part_number");
		label = gtk_label_new("Part #:");
		gtk_widget_set_name(label, "PatchParam");
		widget_set_custom_font(label, phasex_font_desc);
		widget_set_backing_store(label);
		gtk_container_add(GTK_CONTAINER(event), label);
		gtk_box_pack_start(GTK_BOX(box), event, FALSE, FALSE, 0);

		/* connect signal for clicking part number label */
		g_signal_connect(G_OBJECT(event), "button_press_event",
		                 GTK_SIGNAL_FUNC(param_label_button_press),
		                 (gpointer) event);

		/* Part selector adjustment */
		part_adj = gtk_adjustment_new((visible_part_num + 1), 1, MAX_PARTS, 1, 1, 0);

		/* Part selector spin button */
		event = gtk_event_box_new();
		widget_set_backing_store(event);
		part_spin = gtk_spin_button_new(GTK_ADJUSTMENT(part_adj), 0, 0);
		gtk_widget_set_name(part_spin, "NumericSpin");
		widget_set_custom_font(part_spin, numeric_font_desc);
		widget_set_backing_store(part_spin);
		gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(part_spin), TRUE);
		gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(part_spin), GTK_UPDATE_IF_VALID);
		gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(part_spin), TRUE);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(part_spin), (visible_part_num + 1));
		gtk_container_add(GTK_CONTAINER(event), part_spin);
		gtk_box_pack_start(GTK_BOX(box), event, FALSE, FALSE, 0);

		g_signal_connect(GTK_OBJECT(part_adj), "value_changed",
		                 GTK_SIGNAL_FUNC(switch_part),
		                 (gpointer) GTK_WIDGET(part_spin));
	}
	k++;

	/* show the entire bank group at once */
	gtk_widget_show_all(frame);
}


/*****************************************************************************
 * midi_channel_label_handle_event()
 *****************************************************************************/
int
midi_channel_label_handle_event(gpointer UNUSED(data1),
                                gpointer data2,
                                gpointer UNUSED(data3))
{
	PARAM_INFO      *info       = get_param_info_by_id(PARAM_MIDI_CHANNEL);
	PART            *part       = get_visible_part();
	PATCH           *patch      = get_visible_patch();
	GtkStateType    state       = GTK_STATE_NORMAL;
	GdkEventAny     *event      = (GdkEventAny *) data2;
	GdkEventButton  *button     = (GdkEventButton *) data2;
	GdkEventScroll  *scroll     = (GdkEventScroll *) data2;
	GdkEventKey     *key        = (GdkEventKey *) data2;
	GdkEventFocus   *focus      = (GdkEventFocus *) data2;
	int             grab        = 0;
	int             new_channel = part->midi_channel;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		switch (button->button) {
		case 1:     /* left button */
			if (new_channel > 0) {
				new_channel--;
			}
			grab = 1;
			break;
		case 2:     /* middle button */
			new_channel = 9;
			grab = 1;
			break;
		case 3:     /* right button */
			if (new_channel < 16) {
				new_channel++;
			}
			grab = 1;
			break;
		}
		break;
	case GDK_SCROLL:
		switch (scroll->direction) {
		case GDK_SCROLL_DOWN:
			if (new_channel > 0) {
				new_channel--;
			}
			grab = 1;
			break;
		case GDK_SCROLL_UP:
			if (new_channel < 16) {
				new_channel++;
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
			if (new_channel > 0) {
				new_channel--;
			}
			grab = 1;
			break;
		case GDK_Up:
		case GDK_KP_Up:
			if (new_channel < 16) {
				new_channel++;
			}
			grab = 1;
			break;
		case GDK_Tab:
		case GDK_KP_Tab:
		case GDK_ISO_Left_Tab:
			info->focused = 0;
			gtk_widget_child_focus(gtk_widget_get_toplevel(info->text),
			                       (key->keyval == GDK_ISO_Left_Tab) ?
			                       GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
			if (info->prelight) {
				state = GTK_STATE_PRELIGHT;
			}
			else {
				state = GTK_STATE_NORMAL;
			}
			gtk_widget_set_state(midi_channel_label, state);
			gp->param[info->id].updated = 1;
			return 1;
			break;
		case GDK_Return:
		case GDK_KP_Enter:
		default:
			break;
		}
	case GDK_ENTER_NOTIFY:
		if (!key->keyval) {
			info->prelight = 1;
			gtk_widget_set_state(midi_channel_label, GTK_STATE_PRELIGHT);
		}
		break;
	case GDK_LEAVE_NOTIFY:
		info->prelight = 0;
		if (gtk_widget_has_focus(midi_channel_event_box)) {
			info->focused = 1;
			state = GTK_STATE_SELECTED;
		}
		else {
			info->focused = 0;
			state = GTK_STATE_NORMAL;
		}
		gtk_widget_set_state(midi_channel_label, state);
		break;
	case GDK_FOCUS_CHANGE:
		if (focus->in) {
			info->focused  = 1;
		}
		else if (info->prelight) {
			info->focused  = 0;
		}
		else {
			info->focused  = 0;
			info->prelight = 0;
		}
		gp->param[info->id].updated = 1;
		break;
	default:
		/* must handle all enumerations for event type to keep gtk quiet */
		break;
	}

	/* only deal with real changes */
	if (part->midi_channel != new_channel) {
		part->midi_channel = new_channel;
		gp->param[info->id].value.cc_prev = gp->param[info->id].value.cc_val;
		gp->param[info->id].value.cc_val  = new_channel;
		gp->param[info->id].value.int_val = new_channel + gp->param[info->id].info->cc_offset;
		gp->param[info->id].updated = 1;
		patch->param[info->id].value.cc_prev = patch->param[info->id].value.cc_val;
		patch->param[info->id].value.cc_val  = new_channel;
		patch->param[info->id].value.int_val = new_channel + patch->param[info->id].info->cc_offset;
		patch->param[info->id].updated = 1;

		/* set menu radiobutton */
#ifdef MIDI_CHANNEL_IN_MENU
		if ((menu_item_midi_ch[new_channel] != NULL) &&
		    !gtk_check_menu_item_get_active(menu_item_midi_ch[new_channel])) {
			gtk_check_menu_item_set_active(menu_item_midi_ch[new_channel], TRUE);
		}
#endif
		/* set detent label text and adjustment for knob */
		//gtk_label_set_text (GTK_LABEL (midi_channel_label),
		//                  midi_ch_labels[part->midi_channel]);
		//gtk_adjustment_set_value (GTK_ADJUSTMENT (midi_channel_adj),
		//                        part->midi_channel);
	}

	/* grab focus if we need to */
	if (grab) {
		gtk_widget_grab_focus(midi_channel_event_box);
	}

	return 0;
}


/*****************************************************************************
 * queue_test_note()
 *
 * For now, use the same MIDI event queue as the MIDI thread.  At some point,
 * the GUI should be given its own event queue to be used for passing events
 * to the engine.
 *****************************************************************************/
void
queue_test_note(GtkWidget *UNUSED(widget), gpointer UNUSED(data))
{
	MIDI_EVENT      event;
	timecalc_t      delta_nsec;
	struct timespec now;
	unsigned int    index;
	unsigned int    tmp_index;
	unsigned int    cycle_frame;
	PART            *part = get_visible_part();

	event.state    = -1;
	event.type     = MIDI_EVENT_NOTE_ON;
	event.channel  = (unsigned char)(part->midi_channel);
	event.note     = 64;
	event.velocity = 64;
	event.next     = NULL;

	tmp_index   = get_midi_index();
	delta_nsec  = get_time_delta(&now);
	cycle_frame = get_midi_cycle_frame(delta_nsec);

	/* If index suddenly changes, set cycle frame to the start of
	   the period.  Since the engine sees the index change, this
	   is the safe way to do it. */
	if (tmp_index != (index = get_midi_index())) {
		cycle_frame = 0;
	}

	queue_midi_event(visible_part_num, &event, cycle_frame, index);

	if (delta_nsec >= 0.0) {
		inc_midi_index();
	}
}
