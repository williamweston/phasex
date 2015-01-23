/*****************************************************************************
 *
 * help.c
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
#include <gtk/gtk.h>
#include <errno.h>
#include "phasex.h"
#include "config.h"
#include "param.h"
#include "gui_main.h"
#include "help.h"
#include "debug.h"


#define     LICENSE_SIZE                    40960
#define     LINEBUF_SIZE                    256
#define     TEXTBUF_SIZE                    32768


PARAM_HELP  param_help[NUM_HELP_PARAMS];

GtkWidget   *param_help_dialog              = NULL;
GtkWidget   *about_dialog                   = NULL;

int         param_help_param_id             = -1;


/*****************************************************************************
 *
 * about_phasex_dialog()
 *
 *****************************************************************************/
void
about_phasex_dialog(void)
{
	gchar           *license;
	FILE            *license_file;
#if !GTK_CHECK_VERSION(2, 8, 0)
	GtkWidget       *label;
#else
	const gchar     *authors[] = {
		"* William Weston <whw@linuxmail.org>:\n"
		"    Original PHASEX code, artwork, patches, and samples.\n",
		"* Tony Garnock-Jones:\n"
		"    Original GTKKnob code.\n",
		"* Sean Bolton:\n"
		"    Contributions to the GTKKnob code.\n",
		"* Peter Shorthose <zenadsl6252@zen.co.uk>:\n"
		"    Contributions to GTKKnob and PHASEX GUI.\n",
		"* Anton Kormakov:\n"
		"    LASH, JACK Transport, hold-pedal support, and MIDI panic.\n",
		NULL
	};
#endif
	const gchar     *copyright =
		"Source code, all artwork, and all audio samples:\n"
		"Copyright (C) 1999-2013 William Weston <whw@linuxmail.org>\n"
		"With portions of the source code:\n"
		"Copyright (C) 2010 Anton Kormakov <assault64@gmail.com>\n"
		"Copyright (C) 2007 Peter Shorthose <zenadsl6252@zen.co.uk>\n"
		"Copyright (C) 2004 Sean Bolton\n"
		"Copyright (C) 1999 Tony Garnock-Jones\n";
	const gchar     *short_license =
		"PHASEX is Free and Open Source Software released under the\n"
		"GNU Genereal Public License (GPL), Version 3.\n\n"
		"All audio samples are to be considered part of the\n"
		"PHASEX source code, and also subject to the GPL.\n"
		"All included synth-patches released as Public Domian.\n"
		"Knob images may be used in any free and open source\n"
		"software project.\n";
	const gchar     *comments =
		"[P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment\n"
		"A MIDI software synthesizer for Linux, ALSA, & JACK.";
	const gchar     *website =
		"https://github.com/downloads/williamweston/phasex/";
	size_t          j = 0;

	/* build new dialog window if needed */
	if ((about_dialog == NULL) ||
	    !GTK_IS_WIDGET(about_dialog) ||
	    !gtk_widget_get_realized(about_dialog)) {

		/* read in license from phasex documentation */
		if ((license = malloc(LICENSE_SIZE)) == NULL) {
			phasex_shutdown("Out of Memory!\n");
		}
		memset(license, '\0', LICENSE_SIZE);
		if ((license_file = fopen(PHASEX_LICENSE_FILE, "r")) == NULL) {
			PHASEX_WARN("Error opening phasex license file '%s'.\nErrno: %d.  Error: %s\n",
			            PHASEX_LICENSE_FILE, errno, strerror(errno));
			if ((license_file = fopen(ALT_LICENSE_FILE, "r")) == NULL) {
				strncpy(license, short_license, LICENSE_SIZE);
				PHASEX_WARN("Error opening phasex license file '%s'.\nErrno: %d.  Error: %s\n",
				            ALT_LICENSE_FILE, errno, strerror(errno));
			}
		}
		if (license_file != NULL) {
			if ((j = fread(license, 1, LICENSE_SIZE, license_file) == 0) &&
			    ferror(license_file)) {
				PHASEX_DEBUG(DEBUG_CLASS_INIT, "Error reading phasex license file.\n");
			}
			fclose(license_file);
		}
		j = strlen(license);

		/* read in GPLv3 license text */
		if ((license_file = fopen(GPLv3_LICENSE_FILE, "r")) == NULL) {
			PHASEX_WARN("Error opening GPLv3 license file '%s'.\nErrno: %d.  Error: %s\n",
			            GPLv3_LICENSE_FILE, errno, strerror(errno));
			if ((license_file = fopen(ALT_GPLv3_FILE, "r")) == NULL) {
				PHASEX_WARN("Error opening GPLv3 license file '%s'.\nErrno: %d.  Error: %s\n",
				            ALT_GPLv3_FILE, errno, strerror(errno));
			}
		}
		if (license_file != NULL) {
			if ((fread(license + j, 1, LICENSE_SIZE - j, license_file) == 0) &&
			    ferror(license_file)) {
				PHASEX_WARN("Error reading GPLv3 text from '%s'.\n", GPLv3_LICENSE_FILE);
			}
			fclose(license_file);
		}

		/* play tricks with whitespace so GTK can wrap the license text */
		for (j = 0; (j < LICENSE_SIZE) && (license[j] != '\0'); j++) {
			if (license[j] == '\n') {
				if ((license[j + 1] == '\n') || (license[j + 1] == ' ') ||
				    (license[j + 1] == '\t') || (license[j + 1] == '-')) {
					j++;
				}
				else if (license[j + 1] == '') {
					license[++j] = ' ';
				}
				else {
					license[j] = ' ';
				}
			}
			else if (license[j] == '') {
				license[j] = ' ';
			}
		}

#if GTK_CHECK_VERSION(2, 8, 0)

		/* only newer GTK gets to do it the easy way */
		about_dialog = gtk_about_dialog_new();
		gtk_window_set_icon_from_file(GTK_WINDOW(about_dialog), PIXMAP_DIR"/phasex-icon.png", NULL);
		widget_set_backing_store(about_dialog);

		/* set data */
		gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(about_dialog), PACKAGE_NAME);
		gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about_dialog), PACKAGE_VERSION);
		gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about_dialog), copyright);
		gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(about_dialog), license);
		gtk_about_dialog_set_wrap_license(GTK_ABOUT_DIALOG(about_dialog), TRUE);
		gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about_dialog), website);
		gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about_dialog), authors);
		gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about_dialog), comments);

#else /* !GTK_CHECK_VERSION(2, 8, 0) */

		/* for old versions, use a plain old dialog with labels */
		about_dialog = gtk_dialog_new_with_buttons("About PHASEX",
		                                           GTK_WINDOW(main_window),
		                                           GTK_DIALOG_DESTROY_WITH_PARENT,
		                                           GTK_STOCK_CLOSE,
		                                           GTK_RESPONSE_CLOSE,
		                                           NULL);
		gtk_window_set_wmclass(GTK_WINDOW(about_dialog), "phasex", "phasex-about");
		gtk_window_set_role(GTK_WINDOW(about_dialog), "about");
		widget_set_backing_store(about_dialog);

		label = gtk_label_new("<span size=\"larger\">PHASEX "PACKAGE_VERSION"</span>");
		widget_set_backing_store(label);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_widget_show(label);

		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(about_dialog)->vbox),
		                   label, TRUE, FALSE, 8);

		label = gtk_label_new(comments);
		widget_set_backing_store(label);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(about_dialog)->vbox), label, TRUE, FALSE, 8);

		label = gtk_label_new(copyright);
		widget_set_backing_store(label);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(about_dialog)->vbox), label, TRUE, FALSE, 8);

		label = gtk_label_new(short_license);
		widget_set_backing_store(label);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(about_dialog)->vbox), label, TRUE, FALSE, 8);

		label = gtk_label_new(website);
		widget_set_backing_store(label);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(about_dialog)->vbox), label, TRUE, FALSE, 8);

#endif /* GTK_CHECK_VERSION(2, 8, 0) */

		/* make sure the dialog gets hidden when the user responds,
		   and destroyed when window is deleted. */
		g_signal_connect(G_OBJECT(main_window), "delete-event",
		                 G_CALLBACK(close_about_phasex_dialog),
		                 NULL);
		g_signal_connect(G_OBJECT(main_window), "destroy",
		                 G_CALLBACK(close_about_phasex_dialog),
		                 NULL);
		g_signal_connect_swapped(about_dialog, "response",
		                         G_CALLBACK(gtk_widget_hide),
		                         about_dialog);
	}

	/* show the user */
	gtk_widget_show_all(about_dialog);
}


/*****************************************************************************
 *
 * init_help()
 *
 *****************************************************************************/
void
init_help(void)
{
	PARAM           *param;
	FILE            *help_f;
	char            linebuf[LINEBUF_SIZE];
	char            *textbuf;
	char            *name           = NULL;
	char            *label          = NULL;
	char            *p;
	char            *q;
	char            *r;
	unsigned int    param_num;
	int             in_param_text   = 0;

	/* bail out if there's no gui to display help. */
	if (use_gui == 0) {
		return;
	}

	/* open the parameter help file */
	if ((help_f = fopen(PARAM_HELPFILE, "rt")) == NULL) {
		return;
	}

	/* read help entries */
	if ((textbuf = malloc(TEXTBUF_SIZE)) == NULL) {
		phasex_shutdown("Out of Memory!\n");
	}
	memset(textbuf, '\0', TEXTBUF_SIZE * sizeof(char));
	memset(linebuf, '\0', LINEBUF_SIZE * sizeof(char));
	while (fgets(linebuf, LINEBUF_SIZE, help_f) != NULL) {

		/* help text can span multiple lines */
		if (in_param_text) {

			/* help text ends with a dot on its own line */
			if ((linebuf[0] == '.') &&
			    ((linebuf[1] == '\n') || (linebuf[1] == '\0'))) {

				/* find parameter(s) matching name */
				if (strncmp(name, "osc#_", 5) == 0) {
					for (param_num = 0; param_num < NUM_PARAMS; param_num++) {
						param = get_param(0, param_num);
						if ((strncmp(param->info->name, "osc", 3) == 0) &&
						    (strcmp(& (param->info->name[4]), & (name[4])) == 0)) {
							param_help[param_num].label = strdup(label);
							param_help[param_num].text  = strdup(textbuf);
						}
					}
				}
				else if (strncmp(name, "lfo#_", 5) == 0) {
					for (param_num = 0; param_num < NUM_PARAMS; param_num++) {
						param = get_param(0, param_num);
						if ((strncmp(param->info->name, "lfo", 3) == 0) &&
						    (strcmp(& (param->info->name[4]), & (name[4])) == 0)) {
							param_help[param_num].label = strdup(label);
							param_help[param_num].text  = strdup(textbuf);
						}
					}
				}
				else {
					for (param_num = 0; param_num < NUM_HELP_PARAMS; param_num++) {
						param = get_param(0, param_num);
						if (strcmp(param->info->name, name) == 0) {
							param_help[param_num].label = strdup(label);
							param_help[param_num].text  = strdup(textbuf);
							break;
						}
					}
				}

				/* prepare to read another parameter name and label */
				in_param_text = 0;
			}

			/* If still in help text for this param, add
			   current line to help text. */
			else {
				if (linebuf[0] == '\n') {
					strcat(textbuf, "\n");
				}
				else if ((p = index((linebuf + 1), '\n')) != NULL) {
					*p = ' ';
				}
				strcat(textbuf, linebuf);
			}
		}

		/* searching for :param_name:Param Label: line */
		else {
			/* discard anything not starting with a colon */
			if (linebuf[0] != ':') {
				continue;
			}
			p = & (linebuf[1]);

			/* discard line if second colon is not found */
			if ((q = index(p, ':')) == NULL) {
				continue;
			}
			*q = '\0';
			q++;

			/* discard line if last colon is not found */
			if ((r = index(q, ':')) == NULL) {
				continue;
			}
			r++;
			*r = '\0';

			/* make copies of name and label because linebuf will be overwritten */
			if (name != NULL) {
				free(name);
			}
			name  = strdup(p);
			if (label != NULL) {
				free(label);
			}
			label = strdup(q);

			/* prepare to read in multiple lines of text for this param */
			in_param_text = 1;
			memset(textbuf, '\0', TEXTBUF_SIZE * sizeof(char));
		}
		memset(linebuf, '\0', LINEBUF_SIZE * sizeof(char));
	}

	/* done parsing */
	free(textbuf);
	fclose(help_f);

	if (name != NULL) {
		free(name);
	}
	if (label != NULL) {
		free(label);
	}
}


/*****************************************************************************
 *
 * close_param_help_dialog()
 *
 *****************************************************************************/
void
close_param_help_dialog(GtkWidget *widget, gpointer UNUSED(data))
{
	gtk_widget_destroy(widget);
	param_help_dialog = NULL;
}


/*****************************************************************************
 *
 * display_param_help()
 *
 *****************************************************************************/
void
display_param_help(int param_id)
{
	GtkWidget       *label;
	int             same_param_id = 0;

	/* destroy current help window */
	if (param_help_dialog != NULL) {
		if (param_id == param_help_param_id) {
			same_param_id = 1;
		}
		gtk_widget_destroy(param_help_dialog);
		param_help_dialog   = NULL;
		param_help_param_id = -1;
	}

	/* only create new help window if it's a new or different parameter */
	if (!same_param_id) {
		param_help_dialog = gtk_dialog_new_with_buttons("Parameter Description",
		                                                GTK_WINDOW(main_window),
		                                                GTK_DIALOG_DESTROY_WITH_PARENT,
		                                                GTK_STOCK_CLOSE,
		                                                GTK_RESPONSE_CLOSE,
		                                                NULL);
		gtk_window_set_wmclass(GTK_WINDOW(param_help_dialog), "phasex", "phasex-help");
		gtk_window_set_role(GTK_WINDOW(param_help_dialog), "help");

		/* full parameter name */
		label = gtk_label_new(param_help[param_id].label);
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		gtk_misc_set_padding(GTK_MISC(label), 8, 0);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(param_help_dialog)->vbox), label, TRUE, FALSE, 8);

		/* parameter help text */
		label = gtk_label_new(param_help[param_id].text);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_padding(GTK_MISC(label), 8, 0);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(param_help_dialog)->vbox), label, TRUE, FALSE, 8);

		/* connect signals */
		g_signal_connect(G_OBJECT(param_help_dialog), "destroy",
		                 GTK_SIGNAL_FUNC(close_param_help_dialog),
		                 (gpointer) param_help_dialog);
		g_signal_connect_swapped(G_OBJECT(param_help_dialog), "response",
		                         GTK_SIGNAL_FUNC(close_param_help_dialog),
		                         (gpointer) param_help_dialog);

		/* display to the user */
		gtk_widget_show_all(param_help_dialog);

		/* set internal info */
		param_help_param_id = param_id;
	}
}


/*****************************************************************************
 *
 * display_phasex_help()
 *
 *****************************************************************************/
void
display_phasex_help(void)
{
	GtkWidget       *label;
	GtkWidget       *window;
	GdkGeometry     hints = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	/* destroy current help window */
	if (param_help_dialog != NULL) {
		gtk_widget_destroy(param_help_dialog);
		param_help_param_id = -1;
	}

	param_help_dialog = gtk_dialog_new_with_buttons("Using PHASEX",
	                                                GTK_WINDOW(main_window),
	                                                GTK_DIALOG_DESTROY_WITH_PARENT,
	                                                GTK_STOCK_CLOSE,
	                                                GTK_RESPONSE_CLOSE,
	                                                NULL);
	gtk_window_set_wmclass(GTK_WINDOW(param_help_dialog), "phasex", "phasex-help");
	gtk_window_set_role(GTK_WINDOW(param_help_dialog), "help");

	/* scrolling window */
	window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(window),
	                               GTK_POLICY_NEVER,
	                               GTK_POLICY_AUTOMATIC);

	/* main help text */
	label = gtk_label_new(param_help[PARAM_PHASEX_HELP].text);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_label_set_width_chars(GTK_LABEL(label), 78);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_padding(GTK_MISC(label), 2, 0);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(window), label);

	/* attach to dialog's vbox */
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(param_help_dialog)->vbox), window, TRUE, TRUE, 2);

	/* set window geometry hints */
	hints.min_width   = 480;
	hints.min_height  = 480;
	gtk_window_set_geometry_hints(GTK_WINDOW(param_help_dialog), NULL, &hints, GDK_HINT_MIN_SIZE);

	/* connect signals */
	g_signal_connect(G_OBJECT(param_help_dialog), "destroy",
	                 GTK_SIGNAL_FUNC(close_param_help_dialog),
	                 (gpointer) param_help_dialog);
	g_signal_connect_swapped(G_OBJECT(param_help_dialog), "response",
	                         GTK_SIGNAL_FUNC(close_param_help_dialog),
	                         (gpointer) param_help_dialog);

	/* display to the user */
	gtk_widget_show_all(param_help_dialog);

	/* set internal info */
	param_help_param_id = PARAM_PHASEX_HELP;
}


/*****************************************************************************
 *
 * close_about_phasex_dialog()
 *
 *****************************************************************************/
void
close_about_phasex_dialog(GtkWidget *UNUSED(dialog), gpointer UNUSED(data))
{
	//gtk_widget_destroy(GTK_WIDGET(about_dialog));
	//about_dialog = NULL;
	gtk_widget_hide(GTK_WIDGET(about_dialog));
}
