/*****************************************************************************
 *
 * gui_jack.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012-2013 William Weston <whw@linuxmail.org>
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
#include <sys/types.h>
#include <ctype.h>
#include <gtk/gtk.h>
#include "gui_jack.h"
#include "gui_main.h"
#include "gui_menubar.h"
#include "phasex.h"
#include "config.h"
#include "jack.h"
#include "settings.h"
#include "help.h"
#include "debug.h"


GtkMenuItem     *jack_menu_item         = NULL;
GtkMenuItem     *jack_midi_menu_item    = NULL;


/*****************************************************************************
 * on_jack_menu_activate()
 *****************************************************************************/
void
on_jack_menu_activate(GtkMenuItem *UNUSED(parent_menu_item), gpointer UNUSED(data))
{
	JACK_PORT_INFO      *cur      = NULL;
	GtkWidget           *submenu;
	GtkWidget           *menu_item;

	/* build JACK MIDI submenu */
	if ((midi_driver == MIDI_DRIVER_JACK) && (jack_midi_ports != NULL)) {
		submenu = gtk_menu_new();
		menu_item = NULL;
		cur = jack_midi_ports;
		while (cur != NULL) {
			menu_item = gtk_check_menu_item_new_with_label(cur->name);
			widget_set_custom_font(GTK_WIDGET(menu_item), phasex_font_desc);

			/* set checkbutton active if port currently connected */
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item),
			                               cur->connected ? TRUE : FALSE);

			gtk_menu_append(GTK_MENU(submenu), menu_item);
			gtk_widget_show(menu_item);

			g_signal_connect(G_OBJECT(menu_item), "toggled",
			                 GTK_SIGNAL_FUNC(on_select_jack_midi_port),
			                 (gpointer) cur);

			cur = cur->next;
		}
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(jack_midi_menu_item), submenu);
		gtk_widget_set_sensitive(GTK_WIDGET(jack_midi_menu_item), TRUE);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(jack_midi_menu_item), FALSE);
	}
}


/*****************************************************************************
 * on_select_jack_midi_port()
 *****************************************************************************/
void
on_select_jack_midi_port(GtkCheckMenuItem *menu_item, gpointer data)
{
	JACK_PORT_INFO  *cur = jack_midi_ports;

	if (gtk_check_menu_item_get_active(menu_item)) {
		while (cur != NULL) {
			if (data == (gpointer) cur) {
				cur->connect_request    = 1;
				cur->disconnect_request = 0;
				break;
			}
			cur = cur->next;
		}
	}
	else {
		while (cur != NULL) {
			if (data == (gpointer) cur) {
				cur->connect_request    = 0;
				cur->disconnect_request = 1;
				break;
			}
			cur = cur->next;
		}
	}
}
