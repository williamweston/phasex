/*****************************************************************************
 *
 * gui_alsa.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012 William Weston <whw@linuxmail.org>
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
#include <string.h>
#include <pango/pango.h>
#include <gtk/gtk.h>
#include "gui_alsa.h"
#include "gui_main.h"
#include "gui_menubar.h"
#include "phasex.h"
#include "config.h"
#include "alsa_pcm.h"
#include "alsa_seq.h"
#include "rawmidi.h"
#include "settings.h"
#include "help.h"
#include "debug.h"


GtkMenuItem     *alsa_menu_item             = NULL;
GtkMenuItem     *alsa_pcm_hw_menu_item      = NULL;
GtkMenuItem     *alsa_seq_hw_menu_item      = NULL;
GtkMenuItem     *alsa_seq_sw_menu_item      = NULL;
GtkMenuItem     *alsa_rawmidi_menu_item     = NULL;


/*****************************************************************************
 * on_alsa_menu_activate()
 *****************************************************************************/
void
on_alsa_menu_activate(GtkMenuItem *UNUSED(parent_menu_item),
                      gpointer UNUSED(data))
{
	ALSA_PCM_HW_INFO        *pcm_hwinfo         = NULL;
	ALSA_SEQ_PORT           *seq_port_info      = NULL;
	ALSA_RAWMIDI_HW_INFO    *rawmidi_hw_info    = NULL;
	GSList                  *button_group;
	GtkWidget               *submenu;
	GtkWidget               *menu_item;
	char                    label_text[80];
	unsigned int            port_type;

	/* build ALSA PCM HW submenu */
	if ((alsa_pcm_info != NULL) && (alsa_pcm_playback_hw != NULL)) {
		submenu = gtk_menu_new();
		menu_item = NULL;
		button_group = NULL;
		pcm_hwinfo = alsa_pcm_playback_hw;
		while (pcm_hwinfo != NULL) {
			sprintf(label_text, "[%s] %s: %s",
			        pcm_hwinfo->alsa_name,
			        pcm_hwinfo->card_name,
			        pcm_hwinfo->device_name);
			menu_item = gtk_radio_menu_item_new_with_label(button_group, label_text);
			widget_set_custom_font(GTK_WIDGET(menu_item), phasex_font_desc);
			button_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menu_item));

			/* check to see if this is the current hardware device */
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item),
			                               (strcmp(pcm_hwinfo->alsa_name,
			                                       alsa_pcm_info->pcm_name) == 0) ? TRUE : FALSE);

			gtk_menu_append(GTK_MENU(submenu), menu_item);
			gtk_widget_show(menu_item);

			g_signal_connect(G_OBJECT(menu_item), "toggled",
			                 GTK_SIGNAL_FUNC(on_select_alsa_pcm_hw_device),
			                 (gpointer) pcm_hwinfo);

			pcm_hwinfo = pcm_hwinfo->next;
		}
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(alsa_pcm_hw_menu_item), submenu);
		gtk_widget_set_sensitive(GTK_WIDGET(alsa_pcm_hw_menu_item), TRUE);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(alsa_pcm_hw_menu_item), FALSE);
	}

	/* build ALSA Seq HW and SW submenus */
	if ((alsa_seq_info != NULL) && (alsa_seq_info->capture_ports != NULL)) {
		/* Always keep port type set to hardware.  Some software ports are not
		   SND_SEQ_PORT_TYPE_SOFTWARE, so a check for !port_type is used
		   instead. */
		port_type = SND_SEQ_PORT_TYPE_HARDWARE;

		/* build ALSA Seq HW submenu */
		submenu = gtk_menu_new();
		menu_item = NULL;
		seq_port_info = alsa_seq_info->capture_ports;
		while (seq_port_info != NULL) {
			/* ignore other phasex clients */
			if ((strstr(seq_port_info->client_name, "PHASEX") != NULL) ||
			    (strstr(seq_port_info->client_name, "phasex") != NULL)) {
				seq_port_info = seq_port_info->next;
				continue;
			}
			/* check for valid client and type match */
			if ((seq_port_info->client >= 0) &&
			    (seq_port_info->client != SND_SEQ_ADDRESS_SUBSCRIBERS) &&
			    ((seq_port_info->type & port_type) == port_type)) {

				sprintf(label_text, "[%s] %s: %s",
				        seq_port_info->alsa_name,
				        seq_port_info->client_name,
				        seq_port_info->port_name);
				menu_item = gtk_check_menu_item_new_with_label(label_text);
				widget_set_custom_font(GTK_WIDGET(menu_item), phasex_font_desc);

				/* check for active subscription */
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item),
				                               (seq_port_info->subs == NULL) ?
				                               FALSE : TRUE);

				gtk_menu_append(GTK_MENU(submenu), menu_item);
				gtk_widget_show(menu_item);

				g_signal_connect(G_OBJECT(menu_item), "toggled",
				                 GTK_SIGNAL_FUNC(on_select_alsa_seq_port),
				                 (gpointer) seq_port_info);
			}
			seq_port_info = seq_port_info->next;
		}
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(alsa_seq_hw_menu_item), submenu);

		if (menu_item == NULL) {
			gtk_widget_set_sensitive(GTK_WIDGET(alsa_seq_hw_menu_item), FALSE);
		}
		else {
			gtk_widget_set_sensitive(GTK_WIDGET(alsa_seq_hw_menu_item), TRUE);
		}

		/* build ALSA Seq SW submenu */
		submenu = gtk_menu_new();
		menu_item = NULL;
		seq_port_info = alsa_seq_info->capture_ports;
		while (seq_port_info != NULL) {
			/* ignore other phasex clients */
			if ((strstr(seq_port_info->client_name, "PHASEX") != NULL) ||
			    (strstr(seq_port_info->client_name, "phasex") != NULL)) {
				seq_port_info = seq_port_info->next;
				continue;
			}
			/* check for valid client and type match */
			if ((seq_port_info->client > 0) &&
			    (seq_port_info->client != SND_SEQ_ADDRESS_SUBSCRIBERS) &&
			    ((seq_port_info->type & port_type) != port_type)) {

				sprintf(label_text, "[%s] %s: %s",
				        seq_port_info->alsa_name,
				        seq_port_info->client_name,
				        seq_port_info->port_name);
				menu_item = gtk_check_menu_item_new_with_label(label_text);
				widget_set_custom_font(GTK_WIDGET(menu_item), phasex_font_desc);

				/* check for active subscription */
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item),
				                               (seq_port_info->subs == NULL) ?
				                               FALSE : TRUE);

				gtk_menu_append(GTK_MENU(submenu), menu_item);
				gtk_widget_show(menu_item);

				g_signal_connect(G_OBJECT(menu_item), "toggled",
				                 GTK_SIGNAL_FUNC(on_select_alsa_seq_port),
				                 (gpointer) seq_port_info);
			}
			seq_port_info = seq_port_info->next;
		}
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(alsa_seq_sw_menu_item), submenu);

		gtk_widget_set_sensitive(GTK_WIDGET(alsa_seq_sw_menu_item),
		                         (menu_item == NULL) ? FALSE : TRUE);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(alsa_seq_hw_menu_item), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(alsa_seq_sw_menu_item), FALSE);
	}

	/* build ALSA Raw MIDI submenu */
	if ((rawmidi_info != NULL) && (alsa_rawmidi_hw != NULL)) {
		submenu = gtk_menu_new();
		menu_item = NULL;
		button_group = NULL;
		rawmidi_hw_info = alsa_rawmidi_hw;
		while (rawmidi_hw_info != NULL) {
			sprintf(label_text, "[%s] %s: %s",
			        rawmidi_hw_info->alsa_name,
			        rawmidi_hw_info->device_name,
			        rawmidi_hw_info->subdevice_name);
			menu_item = gtk_radio_menu_item_new_with_label(button_group, label_text);
			widget_set_custom_font(GTK_WIDGET(menu_item), phasex_font_desc);
			button_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menu_item));

			/* check to see if this is the current hardware device */
			if ((rawmidi_info != NULL) && (rawmidi_info->device != NULL) &&
			    (strcmp(rawmidi_hw_info->alsa_name, rawmidi_info->device) == 0)) {
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
			}
			else {
				gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), FALSE);
			}

			gtk_menu_append(GTK_MENU(submenu), menu_item);
			gtk_widget_show(menu_item);

			g_signal_connect(G_OBJECT(menu_item), "toggled",
			                 GTK_SIGNAL_FUNC(on_select_alsa_rawmidi_device),
			                 (gpointer) rawmidi_hw_info);

			rawmidi_hw_info = rawmidi_hw_info->next;
		}
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(alsa_rawmidi_menu_item), submenu);
		gtk_widget_set_sensitive(GTK_WIDGET(alsa_rawmidi_menu_item), TRUE);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(alsa_rawmidi_menu_item), FALSE);
	}
}


/*****************************************************************************
 * on_select_alsa_pcm_hw_device()
 *****************************************************************************/
void
on_select_alsa_pcm_hw_device(GtkCheckMenuItem *menu_item, gpointer data)
{
	ALSA_PCM_HW_INFO    *hwinfo = alsa_pcm_playback_hw;

	if (gtk_check_menu_item_get_active(menu_item)) {
		while (hwinfo != NULL) {
			if (data == (gpointer) hwinfo) {
				hwinfo->connect_request    = 1;
				hwinfo->disconnect_request = 0;
			}
			hwinfo = hwinfo->next;
		}
	}
	else {
		while (hwinfo != NULL) {
			if (data == (gpointer) hwinfo) {
				hwinfo->connect_request    = 0;
				hwinfo->disconnect_request = 1;
			}
			hwinfo = hwinfo->next;
		}
	}
}


/*****************************************************************************
 * on_select_alsa_seq_port()
 *****************************************************************************/
void
on_select_alsa_seq_port(GtkCheckMenuItem *menu_item, gpointer data)
{
	ALSA_SEQ_PORT   *seq_port = alsa_seq_info->capture_ports;

	if (gtk_check_menu_item_get_active(menu_item)) {
		while (seq_port != NULL) {
			if (data == (gpointer) seq_port) {
				seq_port->subscribe_request   = 1;
				seq_port->unsubscribe_request = 0;
				break;
			}
			seq_port = seq_port->next;
		}
	}
	else {
		while (seq_port != NULL) {
			if (data == (gpointer) seq_port) {
				seq_port->subscribe_request   = 0;
				seq_port->unsubscribe_request = 1;
				break;
			}
			seq_port = seq_port->next;
		}
	}
}


/*****************************************************************************
 * on_select_alsa_rawmidi_device()
 *****************************************************************************/
void
on_select_alsa_rawmidi_device(GtkCheckMenuItem *menu_item, gpointer data)
{
	ALSA_RAWMIDI_HW_INFO    *hwinfo = alsa_rawmidi_hw;

	if (gtk_check_menu_item_get_active(menu_item)) {
		while (hwinfo != NULL) {
			if (data == (gpointer) hwinfo) {
				hwinfo->connect_request    = 1;
				hwinfo->disconnect_request = 0;
				break;
			}
			hwinfo = hwinfo->next;
		}
	}
	else {
		while (hwinfo != NULL) {
			if (data == (gpointer) hwinfo) {
				hwinfo->connect_request    = 0;
				hwinfo->disconnect_request = 1;
				break;
			}
			hwinfo = hwinfo->next;
		}
	}
}
