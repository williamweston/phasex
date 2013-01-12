/*****************************************************************************
 *
 * gui_menubar.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2013 William Weston <whw@linuxmail.org>
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
#include "gui_main.h"
#include "gui_menubar.h"
#include "gui_bank.h"
#include "gui_session.h"
#include "gui_patch.h"
#include "gui_param.h"
#include "gui_midimap.h"
#include "gui_alsa.h"
#include "gui_jack.h"
#include "phasex.h"
#include "config.h"
#include "patch.h"
#include "param.h"
#include "bank.h"
#include "session.h"
#include "midi_process.h"
#include "settings.h"
#include "help.h"
#include "debug.h"


GtkWidget           *main_menubar               = NULL;

GtkCheckMenuItem    *menu_item_fullscreen       = NULL;
GtkCheckMenuItem    *menu_item_notebook         = NULL;
GtkCheckMenuItem    *menu_item_one_page         = NULL;
GtkCheckMenuItem    *menu_item_widescreen       = NULL;
GtkCheckMenuItem    *menu_item_autoconnect      = NULL;
GtkCheckMenuItem    *menu_item_manualconnect    = NULL;
GtkCheckMenuItem    *menu_item_stereo_out       = NULL;
GtkCheckMenuItem    *menu_item_multi_out        = NULL;
GtkCheckMenuItem    *menu_item_autosave         = NULL;
GtkCheckMenuItem    *menu_item_warn             = NULL;
GtkCheckMenuItem    *menu_item_protect          = NULL;

GtkCheckMenuItem    *bank_mem_menu_item[3]      = {
	NULL, NULL, NULL
};

GtkCheckMenuItem    *menu_item_part[17]     = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL
};


/*****************************************************************************
 * Menubar / Menu Items
 *****************************************************************************/
static GtkItemFactoryEntry menu_items[] = {
	{ "/_File",                       NULL,         NULL,                        0,                 "<Branch>",                    NULL },
	{ "/File/_Open",                  "<CTRL>O",    run_patch_load_dialog,       0,                 "<Item>",                      NULL },
	{ "/File/_Save",                  "<CTRL>S",    on_patch_save_activate,      0,                 "<Item>",                      NULL },
	{ "/File/Save _As",               NULL,         run_patch_save_as_dialog,    0,                 "<Item>",                      NULL },
	{ "/File/sep1",                   NULL,         NULL,                        0,                 "<Separator>",                 NULL },
#ifdef ENABLE_CONFIG_DIALOG
	{ "/File/_Preferences",           "<CTRL>P",    show_config_dialog,          0,                 "<Item>",                      NULL },
#endif
	{ "/File/sep2",                   NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/File/_Quit",                  "<CTRL>Q",    phasex_gtkui_quit,           0,                 "<Item>",                      NULL },
	{ "/_View",                       NULL,         NULL,                        0,                 "<Branch>",                    NULL },
	{ "/View/_Notebook",              NULL,         set_window_layout,           LAYOUT_NOTEBOOK,   "<RadioItem>",                 NULL },
	{ "/View/_One Page",              NULL,         set_window_layout,           LAYOUT_ONE_PAGE,   "/View/Notebook",              NULL },
	{ "/View/_WideScreen",            NULL,         set_window_layout,           LAYOUT_WIDESCREEN, "/View/One Page",              NULL },
	{ "/View/sep1",                   NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/View/_FullScreen",            "F11",        set_fullscreen_mode,         0,                 "<CheckItem>",                 NULL },
	{ "/View/sep2",                   NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/View/Fit 800 x 600",          NULL,         set_desktop_view,            VIEW_800x600,      "<Item>",                      NULL },
	{ "/View/Fit 1024 x 768",         NULL,         set_desktop_view,            VIEW_1024x768,     "<Item>",                      NULL },
	{ "/View/Fit 1280 x 960",         NULL,         set_desktop_view,            VIEW_1280x960,     "<Item>",                      NULL },
	{ "/View/Fit 1440 x 900",         NULL,         set_desktop_view,            VIEW_1440x900,     "<Item>",                      NULL },
	{ "/View/Fit 1680 x 1050",        NULL,         set_desktop_view,            VIEW_1680x1050,    "<Item>",                      NULL },
	{ "/View/Fit 1920 x 1080",        NULL,         set_desktop_view,            VIEW_1920x1080,    "<Item>",                      NULL },
	{ "/_Patch",                      NULL,         NULL,                        0,                 "<Branch>",                    NULL },
	{ "/Patch/_Load",                 "<CTRL>L",    run_patch_load_dialog,       0,                 "<Item>",                      NULL },
	{ "/Patch/_Save",                 "<CTRL>S",    on_patch_save_activate,      0,                 "<Item>",                      NULL },
	{ "/Patch/Save _As",              NULL,         run_patch_save_as_dialog,    0,                 "<Item>",                      NULL },
	{ "/Patch/sep1",                  NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/Patch/_Reset Patch",          "<CTRL>R",    on_patch_reset_activate,     0,                 "<Item>",                      NULL },
#if MAX_PARTS > 1
	{ "/Patch/sep2",                  NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/Patch/Load Session",          NULL,         run_session_load_dialog,     0,                 "<Item>",                      NULL },
	{ "/Patch/Save Session",          NULL,         on_session_save_activate,    0,                 "<Item>",                      NULL },
	{ "/Patch/Save Session As",       NULL,         run_session_save_as_dialog,  0,                 "<Item>",                      NULL },
#endif
	{ "/Patch/sep3",                  NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/Patch/Bank Memory Autosa_ve", NULL,         set_bank_mem_mode,           BANK_MEM_AUTOSAVE, "<RadioItem>",                 NULL },
	{ "/Patch/Bank Memory _Warn",     NULL,         set_bank_mem_mode,           BANK_MEM_WARN,     "/Patch/Bank Memory Autosave", NULL },
	{ "/Patch/Bank Memory _Protect",  NULL,         set_bank_mem_mode,           BANK_MEM_PROTECT,  "/Patch/Bank Memory Warn",     NULL },
	{ "/_MIDI",                       NULL,         NULL,                        0,                 "<Branch>",                    NULL },
	{ "/MIDI/All Notes Off",          "F9",         broadcast_notes_off,         0,                 "<Item>",                      NULL },
	{ "/MIDI/sep1",                   NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/MIDI/_Load MIDI Map",         NULL,         run_midimap_load_dialog,     0,                 "<Item>",                      NULL },
	{ "/MIDI/_Save MIDI Map",         NULL,         on_midimap_save_activate,    0,                 "<Item>",                      NULL },
	{ "/MIDI/Save MIDI Map _As",      NULL,         run_midimap_save_as_dialog,  0,                 "<Item>",                      NULL },
#if MAX_PARTS > 1
	{ "/_Part",                       NULL,         NULL,                        0,                 "<Branch>",                    NULL },
	{ "/Part/_1",                     "F1",         set_visible_part,            0,                 "<RadioItem>",                 NULL },
	{ "/Part/_2",                     "F2",         set_visible_part,            1,                 "/Part/1",                     NULL },
#endif
#if MAX_PARTS > 2
	{ "/Part/_3",                     "F3",         set_visible_part,            2,                 "/Part/2",                     NULL },
#endif
#if MAX_PARTS > 3
	{ "/Part/_4",                     "F4",         set_visible_part,            3,                 "/Part/3",                     NULL },
#endif
#if MAX_PARTS > 4
	{ "/Part/_5",                     "F5",         set_visible_part,            4,                 "/Part/4",                     NULL },
#endif
#if MAX_PARTS > 5
	{ "/Part/_6",                     "F6",         set_visible_part,            5,                 "/Part/5",                     NULL },
#endif
#if MAX_PARTS > 6
	{ "/Part/_7",                     "F7",         set_visible_part,            6,                 "/Part/6",                     NULL },
#endif
#if MAX_PARTS > 7
	{ "/Part/_8",                     "F8",         set_visible_part,            7,                 "/Part/7",                     NULL },
#endif
#if MAX_PARTS > 8
	{ "/Part/_9",                     "<CTRL>F1",   set_visible_part,            8,                 "/Part/8",                     NULL },
#endif
#if MAX_PARTS > 9
	{ "/Part/_10",                    "<CTRL>F1",   set_visible_part,            9,                 "/Part/9",                     NULL },
#endif
#if MAX_PARTS > 10
	{ "/Part/_11",                    "<CTRL>F3",   set_visible_part,            10,                "/Part/10",                    NULL },
#endif
#if MAX_PARTS > 11
	{ "/Part/_12",                    "<CTRL>F4",   set_visible_part,            11,                "/Part/11",                    NULL },
#endif
#if MAX_PARTS > 12
	{ "/Part/_13",                    "<CTRL>F5",   set_visible_part,            12,                "/Part/12",                    NULL },
#endif
#if MAX_PARTS > 13
	{ "/Part/_14",                    "<CTRL>F6",   set_visible_part,            13,                "/Part/13",                    NULL },
#endif
#if MAX_PARTS > 14
	{ "/Part/_15",                    "<CTRL>F7",   set_visible_part,            14,                "/Part/14",                    NULL },
#endif
#if MAX_PARTS > 15
	{ "/Part/_16",                    "<CTRL>F8",   set_visible_part,            15,                "/Part/15",                    NULL },
#endif
	{ "/_ALSA",                       NULL,         NULL,                        0,                 "<Branch>",                    NULL },
	{ "/_ALSA/ALSA PCM HW",           NULL,         NULL,                        0,                 "<Item>",                      NULL },
	{ "/ALSA/sep1",                   NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/_ALSA/ALSA Seq HW",           NULL,         NULL,                        0,                 "<Item>",                      NULL },
	{ "/_ALSA/ALSA Seq SW",           NULL,         NULL,                        0,                 "<Item>",                      NULL },
	{ "/ALSA/sep2",                   NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/_ALSA/ALSA Raw MIDI",         NULL,         NULL,                        0,                 "<Item>",                      NULL },
	{ "/_JACK",                       NULL,         NULL,                        0,                 "<Branch>",                    NULL },
#if (MAX_PARTS > 1)
	{ "/JACK/Stereo Output",          NULL,         set_jack_multi_out,          0,                 "<RadioItem>",                 NULL },
	{ "/JACK/Multi-Part Output",      NULL,         set_jack_multi_out,          1,                 "/JACK/Stereo Output",         NULL },
#endif /* (MAX_PARTS > 1) */
	{ "/JACK/sep1",                   NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/JACK/Autoconnect",            NULL,         set_jack_autoconnect,        1,                 "<RadioItem>",                 NULL },
	{ "/JACK/Manual Connect",         NULL,         set_jack_autoconnect,        0,                 "/JACK/Autoconnect",           NULL },
	{ "/JACK/sep2",                   NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/JACK/Reconnect",              NULL,         jack_restart,                0,                 "<Item>",                      NULL },
	{ "/JACK/sep3",                   NULL,         NULL,                        0,                 "<Separator>",                 NULL },
	{ "/JACK/MIDI",                   NULL,         NULL,                        0,                 "<Item>",                      NULL },
	{ "/_Help",                       NULL,         NULL,                        0,                 "<LastBranch>",                NULL },
	{ "/_Help/About PHASEX",          NULL,         about_phasex_dialog,         0,                 "<Item>",                      NULL },
	{ "/_Help/Using PHASEX",          NULL,         display_phasex_help,         0,                 "<Item>",                      NULL },

};
static guint nmenu_items = sizeof(menu_items) / sizeof(menu_items[0]);


/*****************************************************************************
 * create_menubar()
 *****************************************************************************/
void
create_menubar(GtkWidget *main_window, GtkWidget *box)
{
	GtkAccelGroup   *accel_group;
	GtkItemFactory  *item_factory;
#ifdef CUSTOM_FONTS_IN_MENUS
	GtkWidget       *widget;
#endif
	int             mem_mode        = setting_bank_mem_mode;

	/* create item factory with associated accel group */
	accel_group = gtk_accel_group_new();
	item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accel_group);

	/* build menubar and menus with the item factory */
	gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, NULL);

	/* handle check and radio items */

	/* fullscreen checkbox */
	menu_item_fullscreen = GTK_CHECK_MENU_ITEM
		(gtk_item_factory_get_item(item_factory, "/View/FullScreen"));
	gtk_check_menu_item_set_active(menu_item_fullscreen, setting_fullscreen);

	/* window layout radio group */
	menu_item_notebook   = GTK_CHECK_MENU_ITEM
		(gtk_item_factory_get_item(item_factory, "/View/Notebook"));
	menu_item_one_page   = GTK_CHECK_MENU_ITEM
		(gtk_item_factory_get_item(item_factory, "/View/One Page"));
	menu_item_widescreen = GTK_CHECK_MENU_ITEM
		(gtk_item_factory_get_item(item_factory, "/View/WideScreen"));

	switch (setting_window_layout) {
	case LAYOUT_NOTEBOOK:
		if (!gtk_check_menu_item_get_active(menu_item_notebook)) {
			gtk_check_menu_item_set_active(menu_item_notebook, TRUE);
		}
		break;
	case LAYOUT_ONE_PAGE:
		if (!gtk_check_menu_item_get_active(menu_item_one_page)) {
			gtk_check_menu_item_set_active(menu_item_one_page, TRUE);
		}
		break;
	case LAYOUT_WIDESCREEN:
		if (!gtk_check_menu_item_get_active(menu_item_widescreen)) {
			gtk_check_menu_item_set_active(menu_item_widescreen, TRUE);
		}
		break;
	}

	/* jack manual/auto radio group */
	menu_item_autoconnect   = GTK_CHECK_MENU_ITEM
		(gtk_item_factory_get_item(item_factory, "/JACK/Autoconnect"));
	menu_item_manualconnect = GTK_CHECK_MENU_ITEM
		(gtk_item_factory_get_item(item_factory, "/JACK/Manual Connect"));

	if (setting_jack_autoconnect) {
		if (!gtk_check_menu_item_get_active(menu_item_autoconnect)) {
			gtk_check_menu_item_set_active(menu_item_autoconnect, TRUE);
		}
	}
	else {
		if (!gtk_check_menu_item_get_active(menu_item_manualconnect)) {
			gtk_check_menu_item_set_active(menu_item_manualconnect, TRUE);
		}
	}

	/* jack stereo/multi radio group */
#if MAX_PARTS > 1
	menu_item_stereo_out    = GTK_CHECK_MENU_ITEM
		(gtk_item_factory_get_item(item_factory, "/JACK/Stereo Output"));
	menu_item_multi_out     = GTK_CHECK_MENU_ITEM
		(gtk_item_factory_get_item(item_factory, "/JACK/Multi-Part Output"));

	if (setting_jack_multi_out) {
		if (!gtk_check_menu_item_get_active(menu_item_multi_out)) {
			gtk_check_menu_item_set_active(menu_item_multi_out, TRUE);
		}
	}
	else {
		if (!gtk_check_menu_item_get_active(menu_item_stereo_out)) {
			gtk_check_menu_item_set_active(menu_item_stereo_out, TRUE);
		}
	}
#endif /* MAX_PARTS > 1 */

	/* midi channel radio group */
#if MAX_PARTS > 1
	menu_item_part[0]  = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/1"));
	menu_item_part[1]  = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/2"));
#endif
#if MAX_PARTS > 2
	menu_item_part[2]  = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/3"));
#endif
#if MAX_PARTS > 3
	menu_item_part[3]  = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/4"));
#endif
#if MAX_PARTS > 4
	menu_item_part[4]  = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/5"));
#endif
#if MAX_PARTS > 5
	menu_item_part[5]  = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/6"));
#endif
#if MAX_PARTS > 6
	menu_item_part[6]  = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/7"));
#endif
#if MAX_PARTS > 7
	menu_item_part[7]  = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/8"));
#endif
#if MAX_PARTS > 8
	menu_item_part[8]  = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/9"));
#endif
#if MAX_PARTS > 9
	menu_item_part[9]  = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/10"));
#endif
#if MAX_PARTS > 10
	menu_item_part[10] = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/11"));
#endif
#if MAX_PARTS > 11
	menu_item_part[11] = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/12"));
#endif
#if MAX_PARTS > 12
	menu_item_part[12] = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/13"));
#endif
#if MAX_PARTS > 13
	menu_item_part[13] = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/14"));
#endif
#if MAX_PARTS > 14
	menu_item_part[14] = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/15"));
#endif
#if MAX_PARTS > 15
	menu_item_part[15] = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/Part/16"));
#endif

#if MAX_PARTS > 1
	if (visible_part_num <= 16) {
		gtk_check_menu_item_set_active(menu_item_part[visible_part_num], TRUE);
	}
#endif

	/* Bank memory mode radio group */
	bank_mem_menu_item[BANK_MEM_AUTOSAVE] = menu_item_autosave = GTK_CHECK_MENU_ITEM
		(gtk_item_factory_get_item(item_factory, "/Patch/Bank Memory Autosave"));
	bank_mem_menu_item[BANK_MEM_WARN]     = menu_item_warn     = GTK_CHECK_MENU_ITEM
		(gtk_item_factory_get_item(item_factory, "/Patch/Bank Memory Warn"));
	bank_mem_menu_item[BANK_MEM_PROTECT]  = menu_item_protect  = GTK_CHECK_MENU_ITEM
		(gtk_item_factory_get_item(item_factory, "/Patch/Bank Memory Protect"));

	switch (mem_mode) {
	case BANK_MEM_AUTOSAVE:
		gtk_check_menu_item_set_active(menu_item_autosave, TRUE);
		break;
	case BANK_MEM_PROTECT:
		gtk_check_menu_item_set_active(menu_item_protect, TRUE);
		break;
	case BANK_MEM_WARN:
		gtk_check_menu_item_set_active(menu_item_warn, TRUE);
		break;
	}

	/* ALSA Menu */
	alsa_menu_item         = GTK_MENU_ITEM(gtk_item_factory_get_item(item_factory,
	                                                                 "/ALSA"));
	alsa_pcm_hw_menu_item  = GTK_MENU_ITEM(gtk_item_factory_get_item(item_factory,
	                                                                 "/ALSA/ALSA PCM HW"));
	alsa_seq_hw_menu_item  = GTK_MENU_ITEM(gtk_item_factory_get_item(item_factory,
	                                                                 "/ALSA/ALSA Seq HW"));
	alsa_seq_sw_menu_item  = GTK_MENU_ITEM(gtk_item_factory_get_item(item_factory,
	                                                                 "/ALSA/ALSA Seq SW"));
	alsa_rawmidi_menu_item = GTK_MENU_ITEM(gtk_item_factory_get_item(item_factory,
	                                                                 "/ALSA/ALSA Raw MIDI"));

	widget_set_custom_font(GTK_WIDGET(alsa_menu_item), phasex_font_desc);
	widget_set_custom_font(GTK_WIDGET(alsa_pcm_hw_menu_item), phasex_font_desc);
	widget_set_custom_font(GTK_WIDGET(alsa_seq_hw_menu_item), phasex_font_desc);
	widget_set_custom_font(GTK_WIDGET(alsa_seq_sw_menu_item), phasex_font_desc);
	widget_set_custom_font(GTK_WIDGET(alsa_rawmidi_menu_item), phasex_font_desc);

	if ((setting_midi_driver == MIDI_DRIVER_ALSA_SEQ) ||
	    (setting_midi_driver == MIDI_DRIVER_RAW_ALSA) ||
	    (setting_audio_driver == AUDIO_DRIVER_ALSA_PCM)) {
		gtk_widget_set_sensitive(GTK_WIDGET(alsa_menu_item), TRUE);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(alsa_menu_item), FALSE);
	}

	/* Connect handler to build ALSA submenus */
	g_signal_connect(G_OBJECT(alsa_menu_item),
	                 "activate",
	                 GTK_SIGNAL_FUNC(on_alsa_menu_activate),
	                 (gpointer) 0);

	/* JACK Menu */
	jack_menu_item = GTK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/JACK"));
	jack_midi_menu_item = GTK_MENU_ITEM(gtk_item_factory_get_item(item_factory, "/JACK/MIDI"));

	widget_set_custom_font(GTK_WIDGET(jack_menu_item), phasex_font_desc);
	widget_set_custom_font(GTK_WIDGET(jack_midi_menu_item), phasex_font_desc);

	if (setting_audio_driver == AUDIO_DRIVER_JACK) {
		gtk_widget_set_sensitive(GTK_WIDGET(jack_menu_item), TRUE);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(jack_menu_item), FALSE);
	}

	/* Connect handler to build JACK submenus */
	g_signal_connect(G_OBJECT(jack_menu_item),
	                 "activate",
	                 GTK_SIGNAL_FUNC(on_jack_menu_activate),
	                 (gpointer) 0);

	/* attach menubar's accel group to main window */
	gtk_window_add_accel_group(GTK_WINDOW(main_window), accel_group);

	/* add the menubar to the box passed in to this function */
	main_menubar = gtk_item_factory_get_widget(item_factory, "<main>");
	widget_set_custom_font(main_menubar, phasex_font_desc);
	widget_set_backing_store(main_menubar);
	gtk_box_pack_start(GTK_BOX(box), main_menubar, FALSE, FALSE, 0);

#ifdef CUSTOM_FONTS_IN_MENUS
	/* add custom fonts to the menus */
	widget = gtk_item_factory_get_widget(item_factory, "/File");
	widget_set_custom_font(widget, phasex_font_desc);

	widget = gtk_item_factory_get_widget(item_factory, "/View");
	widget_set_custom_font(widget, phasex_font_desc);

	widget = gtk_item_factory_get_widget(item_factory, "/Patch");
	widget_set_custom_font(widget, phasex_font_desc);

	widget = gtk_item_factory_get_widget(item_factory, "/MIDI");
	widget_set_custom_font(widget, phasex_font_desc);

	widget = gtk_item_factory_get_widget(item_factory, "/Part");
	widget_set_custom_font(widget, phasex_font_desc);

	widget = gtk_item_factory_get_widget(item_factory, "/JACK");
	widget_set_custom_font(widget, phasex_font_desc);

	widget = gtk_item_factory_get_widget(item_factory, "/Help");
	widget_set_custom_font(widget, phasex_font_desc);
#endif
}
