/*****************************************************************************
 *
 * settings.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2007-2013 William Weston <whw@linuxmail.org>
 * Copyright (C) 2010 Anton Kormakov <assault64@gmail.com>
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
/*****************************************************************************
 *
 * Please excuse the excessive run-on code in create_config_dialog and
 * load_config().  A new settings system needs to be designed to work more
 * like the synth parameters, with single-codepath-per-type where possible,
 * page grouping, sensitivity handling, proper value updates on the gui-side,
 * standardized callbacks for the non-gui side, etc.  Currently, this is not
 * at the top of the priority list (but still pretty close up there,
 * considering maintainability and and the number of hacks currently needed
 * for tracking config state).  Please bear with me while this mess gets
 * cleaned up.....
 *
 *****************************************************************************/
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <libgen.h>
#include <math.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <pango/pango-font.h>
#include "phasex.h"
#include "config.h"
#include "timekeeping.h"
#include "patch.h"
#include "param.h"
#include "param_parse.h"
#include "param_strings.h"
#include "buffer.h"
#include "engine.h"
#include "wave.h"
#include "filter.h"
#include "jack.h"
#include "jack_transport.h"
#include "gui_main.h"
#include "gui_menubar.h"
#include "gui_alsa.h"
#include "gui_jack.h"
#include "gui_navbar.h"
#include "gui_patch.h"
#include "bank.h"
#include "gtkknob.h"
#include "settings.h"
#include "driver.h"
#include "string_util.h"
#include "midimap.h"
#include "debug.h"


/* Current (most recently loaded) config file */
char                    *config_file                        = NULL;

/* Status flag variables */
int                     config_is_open                      = 0;
int                     config_page_num                     = 0;
int                     no_config                           = 1;
int                     config_changed                      = 0;
int                     sample_rate_changed                 = 0;

/* MIDI settings */
int                     setting_midi_driver                 = MIDI_DRIVER_ALSA_SEQ;
#ifdef ENABLE_RAWMIDI_OSS
char                    *setting_oss_midi_device            = NULL;
#endif
char                    *setting_generic_midi_device        = NULL;
char                    *setting_alsa_raw_midi_device       = NULL;
char                    *setting_alsa_seq_port              = NULL;
int                     setting_ignore_midi_program_change  = 0;
timecalc_t              setting_audio_phase_lock            = DEFAULT_AUDIO_PHASE_LOCK;
timecalc_t              setting_clock_constant              = 1.0;

/* Audio settings */
int                     setting_audio_driver;
unsigned int            setting_buffer_latency              = DEFAULT_LATENCY_PERIODS;

/* ALSA PCM settings */
char                    *setting_alsa_pcm_device            = NULL;
int                     setting_sample_rate                 = DEFAULT_SAMPLE_RATE;
unsigned int            setting_buffer_period_size          = DEFAULT_BUFFER_PERIOD_SIZE;
int                     setting_force_16bit                 = 0;
int                     setting_enable_mmap                 = 0;
int                     setting_enable_inputs               = 0;

/* JACK settings */
int                     setting_jack_autoconnect            = 1;
int                     setting_jack_multi_out              = 0;
int                     setting_jack_transport_mode         = JACK_TRANSPORT_OFF;

/* Synth settings */
sample_t                setting_tuning_freq                 = A4FREQ;
#ifdef NONSTANDARD_HARMONICS
double                  setting_harmonic_base               = 2.0;
double                  setting_harmonic_steps              = 12.0;
#endif
int                     setting_sample_rate_mode            = SAMPLE_RATE_NORMAL;
int                     setting_bank_mem_mode               = BANK_MEM_WARN;
int                     setting_polyphony                   = DEFAULT_POLYPHONY;

/* Interface settings */
int                     setting_fullscreen                  = 0;
int                     setting_maximize                    = 0;
int                     setting_window_layout               = LAYOUT_ONE_PAGE;
int                     setting_refresh_interval            = DEFAULT_REFRESH_INTERVAL;
int                     setting_knob_size                   = KNOB_SIZE_28x28;

/* System settings */
int                     setting_audio_priority              = AUDIO_THREAD_PRIORITY;
int                     setting_midi_priority               = MIDI_THREAD_PRIORITY;
int                     setting_engine_priority             = ENGINE_THREAD_PRIORITY;
int                     setting_sched_policy                = PHASEX_SCHED_POLICY;

/* Theme settings */
int                     setting_theme                       = PHASEX_THEME_DARK;
char                    *setting_custom_theme               = NULL;
char                    *setting_knob_dir                   = NULL;
char                    *setting_detent_knob_dir            = NULL;
char                    *setting_font                       = NULL;
char                    *setting_title_font                 = NULL;
char                    *setting_numeric_font               = NULL;

/* Hidden settings */
char                    *setting_midimap_file               = NULL;
int                     setting_backing_store               = 0;

/* Dialog widgets that we need to keep track of */
GtkWidget               *config_dialog                      = NULL;
GtkWidget               *custom_theme_button                = NULL;
GtkWidget               *config_notebook                    = NULL;
GtkWidget               *alsa_device_entry                  = NULL;
#ifdef ENABLE_RAWMIDI_OSS
GtkWidget               *oss_midi_device_entry              = NULL;
#endif
#ifdef ENABLE_RAWMIDI_GENERIC
GtkWidget               *generic_midi_device_entry          = NULL;
#endif
GtkWidget               *alsa_raw_midi_device_entry         = NULL;
GtkWidget               *alsa_seq_port_entry                = NULL;
GtkWidget               *bank_autosave_button               = NULL;
GtkWidget               *bank_warn_button                   = NULL;
GtkWidget               *bank_protect_button                = NULL;
GtkWidget               *jack_midi_button                   = NULL;
GtkWidget               *enable_input_button                = NULL;
GtkWidget               *audio_status_label                 = NULL;
GtkObject               *audio_phase_lock_adj               = NULL;

/* Internal font descriptions, based on settings */
PangoFontDescription    *phasex_font_desc                   = NULL;
PangoFontDescription    *title_font_desc                    = NULL;
PangoFontDescription    *numeric_font_desc                  = NULL;

/* Status message buffer, filled in by driver code */
char                    audio_driver_status_msg[256];

/* Strings for gui components */
char *sample_rate_mode_names[] = {
	"normal",
	"undersample",
	"oversample",
	NULL
};

char *bank_mode_names[] = {
	"autosave",
	"warn",
	"protect",
	NULL
};

char *layout_names[] = {
	"one_page",
	"notebook",
	"widescreen",
	NULL
};

char *theme_names[] = {
	"dark",
	"light",
	"system",
	"custom",
	NULL
};

char *knob_sizes[] = {
	"16x16",
	"20x20",
	"24x24",
	"28x28",
	"32x32",
	"36x36",
	"40x40",
	"44x44",
	"48x48",
	"52x52",
	"56x56",
	"60x60",
	NULL
};

char *jack_transport_mode_names[] = {
	"off",
	"tempo",
	"tempo_lfo",
	NULL
};


/*****************************************************************************
 * read_settings()
 *****************************************************************************/
int
read_settings(char *filename)
{
	FILE    *config_f;
	char    *old_config;
	char    *p;
	char    setting_name[64];
	char    setting_value[256];
	char    buffer[256];
	char    c;
	int     prio;
	int     line                = 0;

	/* use default config file location if no filename is supplied. */
	if (config_file == NULL) {
		old_config = NULL;
	}
	else {
		old_config = strdup(config_file);
	}
	if (filename == NULL) {
		if (config_file == NULL) {
			if (user_config_file == NULL) {
				PHASEX_WARN("No config file specified.  Configuration not read.\n");
				if (old_config != NULL) {
					free(old_config);
				}
				return -ENOENT;
			}
			config_file = strdup(user_config_file);
		}
	}
	else {
		if (config_file != NULL) {
			free(config_file);
		}
		config_file = strdup(filename);
	}

	/* open the config file */
	if ((config_f = fopen(config_file, "rt")) != NULL) {

		/* read config settings */
		while (fgets(buffer, sizeof(buffer), config_f) != NULL) {
			line++;

			/* discard comments and blank lines */
			if ((buffer[0] == '\n') || (buffer[0] == '#')) {
				continue;
			}

			/* strip comments */
			p = buffer;
			while ((p < (buffer + sizeof(buffer))) && ((c = *p) != '\0')) {
				if (c == '#') {
					*p = '\0';
				}
				p++;
			}

			/* get setting name */
			if ((p = get_next_token(buffer)) == NULL) {
				continue;
			}
			strncpy(setting_name, p, sizeof(setting_name));
			setting_name[sizeof(setting_name) - 1] = '\0';

			/* make sure there's an '=' */
			if ((p = get_next_token(buffer)) == NULL) {
				continue;
			}
			if (strcmp(p, "=") != 0) {
				while (get_next_token(buffer) != NULL);
				continue;
			}

			/* get setting value */
			if ((p = get_next_token(buffer)) == NULL) {
				continue;
			}
			strncpy(setting_value, p, sizeof(setting_value));
			setting_value[sizeof(setting_value) - 1] = '\0';

			/* make sure there's a ';' */
			if ((p = get_next_token(buffer)) == NULL) {
				continue;
			}
			if (strcmp(p, ";") != 0) {
				while (get_next_token(buffer) != NULL);
				continue;
			}

			/* flush remainder of line */
			while (get_next_token(buffer) != NULL);

			/* skip past null values that slipped into config file. */
			if (strcmp(setting_value, "(null)") == 0) {
				continue;
			}

			/* process setting */
			if (strcasecmp(setting_name, "fullscreen") == 0) {
				setting_fullscreen = get_boolean(setting_value, NULL, 0);
			}

			else if (strcasecmp(setting_name, "maximize") == 0) {
				setting_maximize = get_boolean(setting_value, NULL, 0);
			}

			else if (strcasecmp(setting_name, "window_layout") == 0) {
				if (strcasecmp(setting_value, "one_page") == 0) {
					setting_window_layout = LAYOUT_ONE_PAGE;
				}
				else if (strcasecmp(setting_value, "notebook") == 0) {
					setting_window_layout = LAYOUT_NOTEBOOK;
				}
				else if (strcasecmp(setting_value, "widescreen") == 0) {
					setting_window_layout = LAYOUT_WIDESCREEN;
				}
			}

			else if (strcasecmp(setting_name, "audio_driver") == 0) {
				if (strcasecmp(setting_value, "alsa") == 0) {
					setting_audio_driver = AUDIO_DRIVER_ALSA_PCM;
				}
				else if (strcasecmp(setting_value, "jack") == 0) {
					setting_audio_driver = AUDIO_DRIVER_JACK;
				}
				else {
					setting_audio_driver = AUDIO_DRIVER_NONE;
				}
			}

			else if (strcasecmp(setting_name, "alsa_pcm_device") == 0) {
				if (setting_alsa_pcm_device != NULL) {
					free(setting_alsa_pcm_device);
				}
				setting_alsa_pcm_device = strdup(setting_value);
			}

			else if (strcasecmp(setting_name, "midi_driver") == 0) {
				if (strcasecmp(setting_value, "alsa-raw") == 0) {
					setting_midi_driver = MIDI_DRIVER_RAW_ALSA;
				}
				else if (strcasecmp(setting_value, "alsa-seq") == 0) {
					setting_midi_driver = MIDI_DRIVER_ALSA_SEQ;
				}
				else if (strcasecmp(setting_value, "jack") == 0) {
					setting_midi_driver = MIDI_DRIVER_JACK;
				}
#ifdef ENABLE_RAWMIDI_GENERIC
				else if (strcasecmp(setting_value, "generic") == 0) {
					setting_midi_driver = MIDI_DRIVER_RAW_GENERIC;
				}
#endif
#ifdef ENABLE_RAWMIDI_OSS
				else if (strcasecmp(setting_value, "oss") == 0) {
					setting_midi_driver = MIDI_DRIVER_RAW_OSS;
				}
#endif
				else {
					setting_midi_driver = MIDI_DRIVER_NONE;
				}
			}

#ifdef ENABLE_RAWMIDI_OSS
			else if (strcasecmp(setting_name, "oss_midi_device") == 0) {
				if (setting_oss_midi_device != NULL) {
					free(setting_oss_midi_device);
				}
				setting_oss_midi_device = strdup(setting_value);
			}
#endif
#ifdef ENABLE_RAWMIDI_GENERIC
			else if (strcasecmp(setting_name, "generic_midi_device") == 0) {
				if (setting_generic_midi_device != NULL) {
					free(setting_generic_midi_device);
				}
				setting_generic_midi_device = strdup(setting_value);
			}
#endif

			else if (strcasecmp(setting_name, "alsa_raw_midi_device") == 0) {
				if (setting_alsa_raw_midi_device != NULL) {
					free(setting_alsa_raw_midi_device);
				}
				setting_alsa_raw_midi_device = strdup(setting_value);
			}

			else if (strcasecmp(setting_name, "alsa_seq_port") == 0) {
				if (setting_alsa_seq_port != NULL) {
					free(setting_alsa_seq_port);
				}
				setting_alsa_seq_port = strdup(setting_value);
			}

			else if (strcasecmp(setting_name, "midi_audio_phase_lock") == 0) {
				setting_audio_phase_lock = (timecalc_t) atof(setting_value);
			}

			else if (strcasecmp(setting_name, "ignore_midi_program_change") == 0) {
				setting_ignore_midi_program_change = get_boolean(setting_value, NULL, 0);
			}

			else if (strcasecmp(setting_name, "sample_rate") == 0) {
				setting_sample_rate = atoi(setting_value);
				switch (setting_sample_rate) {
				case 22050:
				case 32000:
				case 44100:
				case 48000:
				case 64000:
				case 88200:
				case 96000:
				case 128000:
				case 176400:
				case 192000:
					break;
				default:
					setting_sample_rate = 44100;
					break;
				}
			}

			else if (strcasecmp(setting_name, "period_size") == 0) {
				setting_buffer_period_size = (unsigned int) atoi(setting_value);
				switch (setting_buffer_period_size) {
				case 32:
				case 64:
				case 128:
				case 256:
				case 512:
				case 1024:
				case 2048:
					break;
				default:
					setting_buffer_period_size = 256;
					break;
				}
			}

			else if (strcasecmp(setting_name, "force_16bit") == 0) {
				setting_force_16bit = get_boolean(setting_value, NULL, 0);
			}

			else if (strcasecmp(setting_name, "clock_constant") == 0) {
				setting_clock_constant = (timecalc_t) atof(setting_value);
			}

			else if (strcasecmp(setting_name, "jack_transport_mode") == 0) {
				if (strcasecmp(setting_value, "off") == 0) {
					setting_jack_transport_mode = JACK_TRANSPORT_OFF;
				}
				else if (strcasecmp(setting_value, "tempo_lfo") == 0) {
					setting_jack_transport_mode = JACK_TRANSPORT_TNP;
				}
				else if (strcasecmp(setting_value, "tempo") == 0) {
					setting_jack_transport_mode = JACK_TRANSPORT_TEMPO;
				}
				else {
					setting_jack_transport_mode = JACK_TRANSPORT_OFF;
				}
			}

			else if (strcasecmp(setting_name, "jack_autoconnect") == 0) {
				setting_jack_autoconnect = get_boolean(setting_value, NULL, 0);
			}

			else if (strcasecmp(setting_name, "jack_multi_out") == 0) {
				setting_jack_multi_out = get_boolean(setting_value, NULL, 0);
			}

			else if (strcasecmp(setting_name, "buffer_latency") == 0) {
				setting_buffer_latency = (unsigned int) atoi(setting_value);
				if ((setting_buffer_latency < 1) || (setting_buffer_latency > 3)) {
					setting_buffer_latency = DEFAULT_LATENCY_PERIODS;
				}
			}

			else if (strcasecmp(setting_name, "enable_mmap") == 0) {
				setting_enable_mmap = get_boolean(setting_value, NULL, 0);
			}

			else if (strcasecmp(setting_name, "enable_inputs") == 0) {
				setting_enable_inputs = get_boolean(setting_value, NULL, 0);
			}

			else if (strcasecmp(setting_name, "sample_rate_mode") == 0) {
				if (strcasecmp(setting_value, "normal") == 0) {
					setting_sample_rate_mode = SAMPLE_RATE_NORMAL;
				}
				else if (strcasecmp(setting_value, "undersample") == 0) {
					setting_sample_rate_mode = SAMPLE_RATE_UNDERSAMPLE;
				}
				else if (strcasecmp(setting_value, "oversample") == 0) {
					setting_sample_rate_mode = SAMPLE_RATE_OVERSAMPLE;
				}
				else {
					setting_sample_rate_mode = SAMPLE_RATE_NORMAL;
				}
			}

			else if (strcasecmp(setting_name, "bank_mem_mode") == 0) {
				if (strcasecmp(setting_value, "autosave") == 0) {
					setting_bank_mem_mode = BANK_MEM_AUTOSAVE;
				}
				else if (strcasecmp(setting_value, "warn") == 0) {
					setting_bank_mem_mode = BANK_MEM_WARN;
				}
				else if (strcasecmp(setting_value, "protect") == 0) {
					setting_bank_mem_mode = BANK_MEM_PROTECT;
				}
				else {
					setting_bank_mem_mode = BANK_MEM_WARN;
				}
			}

			else if (strcasecmp(setting_name, "gui_idle_sleep") == 0) {
				setting_refresh_interval = atoi(setting_value) / 1000;
				if (setting_refresh_interval < 20) {
					setting_refresh_interval = 20;
				}
				else if (setting_refresh_interval > 200) {
					setting_refresh_interval = 200;
				}
			}

			else if (strcasecmp(setting_name, "refresh_interval") == 0) {
				setting_refresh_interval = atoi(setting_value);
				if (setting_refresh_interval < 20) {
					setting_refresh_interval = 20;
				}
				else if (setting_refresh_interval > 200) {
					setting_refresh_interval = 200;
				}
			}

			else if (strcasecmp(setting_name, "midi_thread_priority") == 0) {
				setting_midi_priority = atoi(setting_value);
				prio = sched_get_priority_min(PHASEX_SCHED_POLICY);
				if (setting_midi_priority < prio) {
					setting_midi_priority = prio;
				}
				else {
					prio = sched_get_priority_max(PHASEX_SCHED_POLICY);
					if (setting_midi_priority > prio) {
						setting_midi_priority = prio;
					}
				}
			}

			else if (strcasecmp(setting_name, "engine_thread_priority") == 0) {
				setting_engine_priority = atoi(setting_value);
				prio = sched_get_priority_min(PHASEX_SCHED_POLICY);
				if (setting_engine_priority < prio) {
					setting_engine_priority = prio;
				}
				else {
					prio = sched_get_priority_max(PHASEX_SCHED_POLICY);
					if (setting_engine_priority > prio) {
						setting_engine_priority = prio;
					}
				}
			}

			else if (strcasecmp(setting_name, "audio_thread_priority") == 0) {
				setting_audio_priority = atoi(setting_value);
				prio = sched_get_priority_min(PHASEX_SCHED_POLICY);
				if (setting_audio_priority < prio) {
					setting_audio_priority = prio;
				}
				else {
					prio = sched_get_priority_max(PHASEX_SCHED_POLICY);
					if (setting_audio_priority > prio) {
						setting_audio_priority = prio;
					}
				}
			}

			else if (strcasecmp(setting_name, "sched_policy") == 0) {
				if (strcasecmp(setting_value, "sched_fifo") == 0) {
					setting_sched_policy = SCHED_FIFO;
				}
				else if (strcasecmp(setting_value, "sched_rr") == 0) {
					setting_sched_policy = SCHED_RR;
				}
				else {
					setting_sched_policy = PHASEX_SCHED_POLICY;
				}
			}

			else if (strcasecmp(setting_name, "midimap_file") == 0) {
				if (setting_midimap_file != NULL) {
					free(setting_midimap_file);
				}
				setting_midimap_file = strdup(setting_value);
			}

			else if (strcasecmp(setting_name, "polyphony") == 0) {
				setting_polyphony = atoi(setting_value);
				if (setting_polyphony <= 0) {
					setting_polyphony = DEFAULT_POLYPHONY;
				}
			}

			else if (strcasecmp(setting_name, "tuning_freq") == 0) {
				a4freq = atof(setting_value);
				setting_tuning_freq = (sample_t) a4freq;
			}

#ifdef NONSTANDARD_HARMONICS
			else if (strcasecmp(setting_name, "harmonic_base") == 0) {
				setting_harmonic_base = (sample_t) atof(setting_value);
			}

			else if (strcasecmp(setting_name, "harmonic_steps") == 0) {
				setting_harmonic_steps = (sample_t) atof(setting_value);
			}
#endif

			else if (strcasecmp(setting_name, "backing_store") == 0) {
				setting_backing_store = get_boolean(setting_value, NULL, 0);
			}

			else if (strcasecmp(setting_name, "theme") == 0) {
				if (strcasecmp(setting_value, "system") == 0) {
					setting_theme = PHASEX_THEME_SYSTEM;
				}
				else if (strcasecmp(setting_value, "dark") == 0) {
					setting_theme = PHASEX_THEME_DARK;
				}
				else if (strcasecmp(setting_value, "light") == 0) {
					setting_theme = PHASEX_THEME_LIGHT;
				}
				else if (strcasecmp(setting_value, "custom") == 0) {
					setting_theme = PHASEX_THEME_CUSTOM;
				}
				else {
					setting_theme = PHASEX_THEME_DARK;
				}
			}

			else if (strcasecmp(setting_name, "custom_theme") == 0) {
				if (setting_custom_theme != NULL) {
					free(setting_custom_theme);
				}
				setting_custom_theme = strdup(setting_value);
			}

			else if ((strcasecmp(setting_name, "font") == 0) &&
			         (strcmp(setting_value, "(null)") != 0)) {
				if (setting_font != NULL) {
					free(setting_font);
				}
				setting_font = strdup(setting_value);
				if (phasex_font_desc != NULL) {
					pango_font_description_free(phasex_font_desc);
				}
				phasex_font_desc = pango_font_description_from_string(setting_font);
			}

			else if ((strcasecmp(setting_name, "title_font") == 0) &&
			         (strcmp(setting_value, "(null)") != 0)) {
				if (setting_title_font != NULL) {
					free(setting_title_font);
				}
				setting_title_font = strdup(setting_value);
				if (title_font_desc != NULL) {
					pango_font_description_free(title_font_desc);
				}
				title_font_desc = pango_font_description_from_string(setting_title_font);
			}

			else if ((strcasecmp(setting_name, "numeric_font") == 0) &&
			         (strcmp(setting_value, "(null)") != 0)) {
				if (setting_numeric_font != NULL) {
					free(setting_numeric_font);
				}
				setting_numeric_font = strdup(setting_value);
				if (numeric_font_desc != NULL) {
					pango_font_description_free(numeric_font_desc);
				}
				numeric_font_desc = pango_font_description_from_string(setting_numeric_font);
			}

			else if (strcasecmp(setting_name, "knob_size") == 0) {
				if (strcasecmp(setting_value, "16x16") == 0) {
					setting_knob_size = KNOB_SIZE_16x16;
				}
				else if (strcasecmp(setting_value, "20x20") == 0) {
					setting_knob_size = KNOB_SIZE_20x20;
				}
				else if (strcasecmp(setting_value, "24x24") == 0) {
					setting_knob_size = KNOB_SIZE_24x24;
				}
				else if (strcasecmp(setting_value, "28x28") == 0) {
					setting_knob_size = KNOB_SIZE_28x28;
				}
				else if (strcasecmp(setting_value, "32x32") == 0) {
					setting_knob_size = KNOB_SIZE_32x32;
				}
				else if (strcasecmp(setting_value, "36x36") == 0) {
					setting_knob_size = KNOB_SIZE_36x36;
				}
				else if (strcasecmp(setting_value, "40x40") == 0) {
					setting_knob_size = KNOB_SIZE_40x40;
				}
				else if (strcasecmp(setting_value, "44x44") == 0) {
					setting_knob_size = KNOB_SIZE_44x44;
				}
				else if (strcasecmp(setting_value, "48x48") == 0) {
					setting_knob_size = KNOB_SIZE_48x48;
				}
				else if (strcasecmp(setting_value, "52x52") == 0) {
					setting_knob_size = KNOB_SIZE_52x52;
				}
				else if (strcasecmp(setting_value, "56x56") == 0) {
					setting_knob_size = KNOB_SIZE_56x56;
				}
				else if (strcasecmp(setting_value, "60x60") == 0) {
					setting_knob_size = KNOB_SIZE_60x60;
				}
				else {
					setting_knob_size = KNOB_SIZE_28x28;
				}
			}

			else if (strcasecmp(setting_name, "knob_dir") == 0) {
				if (setting_knob_dir != NULL) {
					free(setting_knob_dir);
				}
				setting_knob_dir = strdup(setting_value);
			}

			else if (strcasecmp(setting_name, "detent_knob_dir") == 0) {
				if (setting_detent_knob_dir != NULL) {
					free(setting_detent_knob_dir);
				}
				setting_detent_knob_dir = strdup(setting_value);
			}
		}

		/* done parsing */
		fclose(config_f);

		/* we now have a config */
		no_config = 0;
		if (old_config != NULL) {
			free(old_config);
			old_config = NULL;
		}
	} /*  end   if ((config_f = fopen (config_file, "rt")) != NULL) */

	/* if new config didn't open, revert back to old config */
	else {
		if (config_file != NULL) {
			free(config_file);
		}
		config_file = old_config;
	}

	/* set defaults for fonts if missing */
	if (setting_font == NULL) {
		setting_font = strdup(DEFAULT_ALPHA_FONT);
		if (phasex_font_desc != NULL) {
			pango_font_description_free(phasex_font_desc);
		}
		phasex_font_desc = pango_font_description_from_string(setting_font);
	}
	if (setting_numeric_font == NULL) {
		setting_numeric_font = strdup(DEFAULT_NUMERIC_FONT);
		if (numeric_font_desc != NULL) {
			pango_font_description_free(numeric_font_desc);
		}
		numeric_font_desc = pango_font_description_from_string(setting_numeric_font);
	}
	if (setting_title_font == NULL) {
		setting_title_font = strdup(DEFAULT_TITLE_FONT);
		if (title_font_desc != NULL) {
			pango_font_description_free(title_font_desc);
		}
		title_font_desc = pango_font_description_from_string(setting_title_font);
	}

	/* set defaults for knobs (if missing) based on theme */
	switch (setting_theme) {
	case PHASEX_THEME_SYSTEM:
	case PHASEX_THEME_LIGHT:
		if (setting_knob_dir == NULL) {
			setting_knob_dir = malloc(strlen(PIXMAP_DIR) + strlen(DEFAULT_LIGHT_KNOB_DIR) + 2);
			sprintf(setting_knob_dir, "%s/%s", PIXMAP_DIR, DEFAULT_LIGHT_KNOB_DIR);
		}
		if (setting_detent_knob_dir == NULL) {
			setting_detent_knob_dir = malloc(strlen(PIXMAP_DIR) +
			                                 strlen(DEFAULT_LIGHT_DETENT_KNOB_DIR) + 2);
			sprintf(setting_detent_knob_dir, "%s/%s", PIXMAP_DIR, DEFAULT_LIGHT_DETENT_KNOB_DIR);
		}
		break;
	case PHASEX_THEME_DARK:
	default:
		if (setting_knob_dir == NULL) {
			setting_knob_dir = malloc(strlen(PIXMAP_DIR) + strlen(DEFAULT_DARK_KNOB_DIR) + 2);
			sprintf(setting_knob_dir, "%s/%s", PIXMAP_DIR, DEFAULT_DARK_KNOB_DIR);
		}
		if (setting_detent_knob_dir == NULL) {
			setting_detent_knob_dir = malloc(strlen(PIXMAP_DIR) +
			                                 strlen(DEFAULT_DARK_DETENT_KNOB_DIR) + 2);
			sprintf(setting_detent_knob_dir, "%s/%s", PIXMAP_DIR, DEFAULT_DARK_DETENT_KNOB_DIR);
		}
		break;
	}

	return 0;
}


/*****************************************************************************
 * save_settings()
 *****************************************************************************/
int
save_settings(char *filename)
{
	FILE    *config_f;
	char    *old_config;

	/* use default config file location if no filename is supplied. */
	if (config_file == NULL) {
		old_config = NULL;
	}
	else {
		old_config = strdup(config_file);
	}
	if (filename == NULL) {
		if (config_file == NULL) {
			if (user_config_file == NULL) {
				PHASEX_WARN("No config file specified.  Configuration not written.\n");
				if (old_config != NULL) {
					free(old_config);
				}
				return -ENOENT;
			}
			config_file = strdup(user_config_file);
		}
	}
	else {
		if (config_file != NULL) {
			free(config_file);
		}
		config_file = strdup(filename);
	}

	/* open the config file */
	if ((config_f = fopen(config_file, "wt")) == NULL) {
		if (config_file != NULL) {
			free(config_file);
		}
		config_file = old_config;
		return -EIO;
	}

	/* write the settings */
	fprintf(config_f, "# PHASEX %s Configuration\n",           PACKAGE_VERSION);
	fprintf(config_f, "\taudio_driver\t\t\t= %s;\n",           audio_driver_names[setting_audio_driver]);
	fprintf(config_f, "# JACK:\n");
	fprintf(config_f, "\tjack_multi_out\t\t\t= %s;\n",         boolean_names[setting_jack_multi_out]);
	fprintf(config_f, "\tjack_autoconnect\t\t= %s;\n",         boolean_names[setting_jack_autoconnect]);
	fprintf(config_f, "\tjack_transport_mode\t\t= %s;\n",      jack_transport_mode_names[setting_jack_transport_mode]);
	fprintf(config_f, "# ALSA PCM:\n");
	fprintf(config_f, "\talsa_pcm_device\t\t\t= \"%s\";\n",    setting_alsa_pcm_device);
	fprintf(config_f, "\tsample_rate\t\t\t= %d;\n",            setting_sample_rate);
	fprintf(config_f, "\tperiod_size\t\t\t= %d;\n",            setting_buffer_period_size);
	fprintf(config_f, "\tforce_16bit\t\t\t= %s;\n",            boolean_names[setting_force_16bit]);
	fprintf(config_f, "\tenable_mmap\t\t\t= %s;\n",            boolean_names[setting_enable_mmap]);
	fprintf(config_f, "\tenable_inputs\t\t\t= %s;\n",          boolean_names[setting_enable_inputs]);
	fprintf(config_f, "\tbuffer_latency\t\t\t= %d;\n",         setting_buffer_latency);
	fprintf(config_f, "# MIDI:\n");
	fprintf(config_f, "\tmidi_driver\t\t\t= %s;\n",            midi_driver_names[setting_midi_driver]);
	fprintf(config_f, "\talsa_seq_port\t\t\t= \"%s\";\n",      setting_alsa_seq_port);
	fprintf(config_f, "\talsa_raw_midi_device\t\t= \"%s\";\n", setting_alsa_raw_midi_device);
#ifdef ENABLE_RAWMIDI_GENERIC
	fprintf(config_f, "\tgeneric_midi_device\t\t= \"%s\";\n",  setting_generic_midi_device);
#endif
#ifdef ENABLE_RAWMIDI_OSS
	fprintf(config_f, "\toss_midi_device\t\t\t= \"%s\";\n",    setting_oss_midi_device);
#endif
	fprintf(config_f, "\tmidi_audio_phase_lock\t\t= %f;\n",    setting_audio_phase_lock);
	fprintf(config_f, "\tignore_midi_program_change\t= %s;\n", boolean_names[setting_ignore_midi_program_change]);
	fprintf(config_f, "\tclock_constant\t\t\t= %.25f;\n",
	        (float)((timecalc_t) nsec_per_period /
	                (f_buffer_period_size * (timecalc_t) 1000000000.0 /
	                 (timecalc_t) f_sample_rate)));
	fprintf(config_f, "# Synth:\n");
	fprintf(config_f, "\ttuning_freq\t\t\t= %3.7f;\n",         (float)setting_tuning_freq);
#ifdef NONSTANDARD_HARMONICS
	fprintf(config_f, "\tharmonic_base\t\t\t= %f;\n",          setting_harmonic_base);
	fprintf(config_f, "\tharmonic_steps\t\t\t= %f;\n",         setting_harmonic_steps);
#endif
	fprintf(config_f, "\tpolyphony\t\t\t= %d;\n",              setting_polyphony);
	fprintf(config_f, "\tsample_rate_mode\t\t= %s;\n",         sample_rate_mode_names[setting_sample_rate_mode]);
	fprintf(config_f, "\tbank_mem_mode\t\t\t= %s;\n",          bank_mode_names[setting_bank_mem_mode]);
	fprintf(config_f, "# System:\n");
	fprintf(config_f, "\tmidi_thread_priority\t\t= %d;\n",     setting_midi_priority);
	fprintf(config_f, "\tengine_thread_priority\t\t= %d;\n",   setting_engine_priority);
	fprintf(config_f, "\taudio_thread_priority\t\t= %d;\n",    setting_audio_priority);
	fprintf(config_f, "\tsched_policy\t\t\t= %s;\n", ((setting_sched_policy == SCHED_RR) ? "sched_rr" : "sched_fifo"));
	fprintf(config_f, "# Interface:\n");
	fprintf(config_f, "\tfullscreen\t\t\t= %s;\n",             boolean_names[setting_fullscreen]);
	fprintf(config_f, "\tmaximize\t\t\t= %s;\n",               boolean_names[setting_maximize]);
	fprintf(config_f, "\twindow_layout\t\t\t= %s;\n",          layout_names[setting_window_layout]);
	fprintf(config_f, "\t# warning:  backing store may be broken\n");
	fprintf(config_f, "\tbacking_store\t\t\t= %s;\n",          boolean_names[setting_backing_store]);
	fprintf(config_f, "\trefresh_interval\t\t= %d;\n",         setting_refresh_interval);
	fprintf(config_f, "# Theme:\n");
	fprintf(config_f, "\tknob_size\t\t\t= %s;\n",              knob_sizes[setting_knob_size]);
	fprintf(config_f, "\tknob_dir\t\t\t= %s;\n",               setting_knob_dir);
	fprintf(config_f, "\tdetent_knob_dir\t\t\t= %s;\n",        setting_detent_knob_dir);
	fprintf(config_f, "\tfont\t\t\t\t= \"%s\";\n",             setting_font);
	fprintf(config_f, "\ttitle_font\t\t\t= \"%s\";\n",         setting_title_font);
	fprintf(config_f, "\tnumeric_font\t\t\t= \"%s\";\n",       setting_numeric_font);
	fprintf(config_f, "\ttheme\t\t\t\t= \"%s\";\n",            theme_names[setting_theme]);
	if (setting_custom_theme != NULL) {
		fprintf(config_f, "\tcustom_theme\t\t\t= \"%s\";\n",   setting_custom_theme);
	}
	if (setting_midimap_file != NULL) {
		fprintf(config_f, "\tmidimap_file\t\t\t= \"%s\";\n",   setting_midimap_file);
	}

	/* done */
	fclose(config_f);
	if (old_config != NULL) {
		free(old_config);
	}
	return 0;
}


/*****************************************************************************
 * set_midi_channel()
 *****************************************************************************/
void
set_midi_channel(GtkWidget *widget, gpointer data, GtkWidget *UNUSED(widget2))
{
	int     new_channel = (int)((long int) data);

	if (widget != NULL) {
		new_channel = (int) gtk_adjustment_get_value(GTK_ADJUSTMENT(widget));
		if ((midi_channel_label != NULL) && (GTK_IS_LABEL(midi_channel_label))) {
			gtk_label_set_text(GTK_LABEL(midi_channel_label), midi_ch_labels[new_channel]);
		}
		gtk_widget_grab_focus(midi_channel_event_box);
	}

	set_midi_channel_for_part(visible_part_num, new_channel);
}


/*****************************************************************************
 * set_fullscreen_mode()
 *****************************************************************************/
void
set_fullscreen_mode(GtkWidget *widget, gpointer UNUSED(data1), gpointer data2)
{
	int         new_mode = 0;
	int         old_mode = setting_fullscreen;
	GtkWidget   *button;

	/* callback used from two different places, so find the button */
	if (widget == NULL) {
		button = (GtkWidget *) data2;
	}
	else {
		button = (GtkWidget *) widget;
	}

	/* check to see if button is active */
	if (((GTK_IS_TOGGLE_BUTTON(button)) &&
	     (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))) ||
	    ((GTK_IS_CHECK_MENU_ITEM(button)) &&
	     (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(button))))) {
		new_mode = 1;
	}

	/* only (un)maximize if changing modes, and leave maximize setting alone */
	if (new_mode != old_mode) {
		if (new_mode) {
			setting_fullscreen = 1;
			gtk_window_set_decorated(GTK_WINDOW(main_window), FALSE);
			gtk_window_fullscreen(GTK_WINDOW(main_window));
			if ((widget != NULL) && (menu_item_fullscreen != NULL)) {
				gtk_check_menu_item_set_active(menu_item_fullscreen, TRUE);
			}
		}
		else {
			setting_fullscreen = 0;
			gtk_window_set_decorated(GTK_WINDOW(main_window), TRUE);
			gtk_window_unfullscreen(GTK_WINDOW(main_window));
			if ((widget != NULL) && (menu_item_fullscreen != NULL)) {
				gtk_check_menu_item_set_active(menu_item_fullscreen, FALSE);
			}
		}

		save_settings(NULL);
	}
}


/*****************************************************************************
 * set_maximize_mode()
 *****************************************************************************/
void
set_maximize_mode(GtkWidget *UNUSED(widget), gpointer data)
{
	int     new_mode = (int)((long int) data);

	/* only do something if changing modes */
	if (new_mode != setting_maximize) {
		if (new_mode) {
			setting_maximize = MAXIMIZE_ON;
			if (setting_fullscreen) {
				setting_fullscreen = FULLSCREEN_OFF;
				gtk_window_unfullscreen(GTK_WINDOW(main_window));
			}
			gtk_window_set_decorated(GTK_WINDOW(main_window), TRUE);
			gtk_window_maximize(GTK_WINDOW(main_window));
			if ((menu_item_fullscreen != NULL)) {
				gtk_check_menu_item_set_active(menu_item_fullscreen, FALSE);
			}
		}
		else {
			setting_maximize = MAXIMIZE_OFF;
			gtk_window_unmaximize(GTK_WINDOW(main_window));
		}

		save_settings(NULL);
	}
}


/*****************************************************************************
 * set_window_layout()
 *****************************************************************************/
void
set_window_layout(GtkWidget *widget, gpointer data, GtkWidget *widget2)
{
	int     layout = (int)((long int) data);

	/* make sure this is a button toggling ON and actually changing layout */
	if ((layout != setting_window_layout) &&
	    (((GTK_IS_TOGGLE_BUTTON(widget)) &&
	      (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))) ||
	     ((GTK_IS_CHECK_MENU_ITEM(widget2)) &&
	      (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget2)))))) {

		/* set new layout mode */
		switch (layout) {
		case LAYOUT_NOTEBOOK:
			setting_window_layout = LAYOUT_NOTEBOOK;
			if ((widget != NULL) && (menu_item_notebook != NULL)) {
				gtk_check_menu_item_set_active(menu_item_notebook, TRUE);
			}
			break;
		case LAYOUT_ONE_PAGE:
			setting_window_layout = LAYOUT_ONE_PAGE;
			if ((widget != NULL) && (menu_item_one_page != NULL)) {
				gtk_check_menu_item_set_active(menu_item_one_page, TRUE);
			}
			break;
		case LAYOUT_WIDESCREEN:
			setting_window_layout = LAYOUT_WIDESCREEN;
			if ((widget != NULL) && (menu_item_widescreen != NULL)) {
				gtk_check_menu_item_set_active(menu_item_widescreen, TRUE);
			}
			break;
		}

		/* save settings and restart gui */
		save_settings(NULL);
		restart_gtkui();
	}
}


/*****************************************************************************
 * set_knob_size()
 *****************************************************************************/
void
set_knob_size(GtkWidget *widget, gpointer data)
{
	int         new_size = (int)((long int) data);

	/* make sure this is a button toggling ON and actually changing new_size */
	if ((new_size != setting_knob_size) &&
	    (GTK_IS_TOGGLE_BUTTON(widget)) &&
	    (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))) {

		/* set new scheduling new_size setting */
		switch (new_size) {
		case KNOB_SIZE_16x16:
		case KNOB_SIZE_20x20:
		case KNOB_SIZE_24x24:
		case KNOB_SIZE_28x28:
		case KNOB_SIZE_32x32:
		case KNOB_SIZE_36x36:
		case KNOB_SIZE_40x40:
		case KNOB_SIZE_44x44:
		case KNOB_SIZE_48x48:
		case KNOB_SIZE_52x52:
		case KNOB_SIZE_56x56:
		case KNOB_SIZE_60x60:
			setting_knob_size = new_size;
			break;
		default:
			setting_knob_size = KNOB_SIZE_28x28;
			break;
		}

		/* save settings and restart gui */
		save_settings(NULL);
		restart_gtkui();
	}
}


/*****************************************************************************
 * set_desktop_view()
 *****************************************************************************/
void
set_desktop_view(GtkWidget *UNUSED(widget), gpointer data, GtkWidget *UNUSED(widget2))
{
	int     new_view = (int)((long int) data);

	if (setting_font != NULL) {
		free(setting_font);
	}
	if (setting_title_font != NULL) {
		free(setting_title_font);
	}
	if (setting_numeric_font != NULL) {
		free(setting_numeric_font);
	}

	switch (new_view) {
	case VIEW_1920x1080:
		setting_window_layout = LAYOUT_WIDESCREEN;
		setting_knob_size     = KNOB_SIZE_48x48;
		setting_font          = strdup("Sans 9");
		setting_title_font    = strdup("Sans 9");
		setting_numeric_font  = strdup("Fixed,Monospace 10");
		break;
	case VIEW_1680x1050:
		setting_window_layout = LAYOUT_WIDESCREEN;
		setting_knob_size     = KNOB_SIZE_44x44;
		setting_font          = strdup("Sans 8");
		setting_title_font    = strdup("Sans 8");
		setting_numeric_font  = strdup("Fixed,Monospace 9");
		break;
	case VIEW_1600x1200:
		setting_window_layout = LAYOUT_ONE_PAGE;
		setting_knob_size     = KNOB_SIZE_36x36;
		setting_font          = strdup("Sans 8");
		setting_title_font    = strdup("Sans 8");
		setting_numeric_font  = strdup("Fixed,Monospace 9");
		break;
	case VIEW_1440x900:
		setting_window_layout = LAYOUT_WIDESCREEN;
		setting_knob_size     = KNOB_SIZE_36x36;
		setting_font          = strdup("Sans 6");
		setting_title_font    = strdup("Sans 7");
		setting_numeric_font  = strdup("Fixed,Monospace 7");
		break;
	case VIEW_1280x960:
		setting_window_layout = LAYOUT_ONE_PAGE;
		setting_knob_size     = KNOB_SIZE_28x28;
		setting_font          = strdup("Sans 7");
		setting_title_font    = strdup("Sans 7");
		setting_numeric_font  = strdup("glisp,cure,lime,Monospace 7");
		break;
	case VIEW_1024x768:
		setting_window_layout = LAYOUT_ONE_PAGE;
		setting_knob_size     = KNOB_SIZE_24x24;
		setting_font          = strdup("Sans 6");
		setting_title_font    = strdup("Sans 7");
		setting_numeric_font  = strdup("glisp,cure,lime,Monospace 7");
		break;
	case VIEW_800x600:
		setting_window_layout = LAYOUT_NOTEBOOK;
		setting_knob_size     = KNOB_SIZE_24x24;
		setting_font          = strdup("cure,lime,goth_p,Sans 6");
		setting_title_font    = strdup("cure,lime,goth_p,Sans 6");
		setting_numeric_font  = strdup("cure,lime,goth_p,Monospace 6");
		break;
	default:
		setting_window_layout = LAYOUT_NOTEBOOK;
		setting_knob_size     = KNOB_SIZE_16x16;
		setting_font          = strdup("cure,lime,goth_p,Sans 6");
		setting_title_font    = strdup("cure,lime,goth_p,Sans 6");
		setting_numeric_font  = strdup("cure,lime,goth_p,Monospace,Sans 6");
		break;
	}

	if (phasex_font_desc != NULL) {
		pango_font_description_free(phasex_font_desc);
	}
	phasex_font_desc  = pango_font_description_from_string(setting_font);
	if (title_font_desc != NULL) {
		pango_font_description_free(title_font_desc);
	}
	title_font_desc   = pango_font_description_from_string(setting_title_font);
	if (numeric_font_desc != NULL) {
		pango_font_description_free(numeric_font_desc);
	}
	numeric_font_desc = pango_font_description_from_string(setting_numeric_font);

	save_settings(NULL);
	restart_gtkui();
}


/*****************************************************************************
 * set_refresh_interval()
 *****************************************************************************/
void
set_refresh_interval(GtkWidget *UNUSED(widget), gpointer data)
{
	setting_refresh_interval = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data));
}


/*****************************************************************************
 * set_midi_priority()
 *****************************************************************************/
void
set_midi_priority(GtkWidget *UNUSED(widget), gpointer data)
{
	struct sched_param  schedparam;

	setting_midi_priority = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data));

	if (midi_thread_p != 0) {
		memset(&schedparam, 0, sizeof(struct sched_param));
		schedparam.sched_priority = setting_midi_priority;
		pthread_setschedparam(midi_thread_p, setting_sched_policy, &schedparam);
	}
	save_settings(NULL);
}


/*****************************************************************************
 * set_engine_priority()
 *****************************************************************************/
void
set_engine_priority(GtkWidget *UNUSED(widget), gpointer data)
{
	struct sched_param  schedparam;
	int         i;

	if (data != NULL) {
		setting_engine_priority = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data));
	}

	memset(&schedparam, 0, sizeof(struct sched_param));
	schedparam.sched_priority = setting_engine_priority;

	for (i = 0; i < MAX_PARTS; i++) {
		pthread_setschedparam(engine_thread_p[i], setting_sched_policy, &schedparam);
	}
	save_settings(NULL);
}


/*****************************************************************************
 * set_audio_priority()
 *****************************************************************************/
void
set_audio_priority(GtkWidget *UNUSED(widget), gpointer data)
{
	struct sched_param  schedparam;

	setting_audio_priority = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data));

	if (audio_thread_p != 0) {
		memset(&schedparam, 0, sizeof(struct sched_param));
		schedparam.sched_priority = setting_audio_priority;
		pthread_setschedparam(audio_thread_p, setting_sched_policy, &schedparam);
	}
	save_settings(NULL);
}


/*****************************************************************************
 * set_sched_policy()
 *****************************************************************************/
void
set_sched_policy(GtkWidget *widget, gpointer data)
{
	struct sched_param  schedparam;
	int                 policy = (int)((long int) data);
	int                 i;

	/* make sure this is a button toggling ON and actually changing policy */
	if ((policy != setting_sched_policy) &&
	    (GTK_IS_TOGGLE_BUTTON(widget)) &&
	    (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))) {

		/* set new scheduling policy setting */
		switch (policy) {
		case SCHED_FIFO:
		case SCHED_RR:
			setting_sched_policy = policy;
			break;
		default:
			setting_sched_policy = PHASEX_SCHED_POLICY;
			break;
		}

		/* update scheduling policy of running midi and engine threads */
		memset(&schedparam, 0, sizeof(struct sched_param));
		schedparam.sched_priority = setting_audio_priority;
		if (audio_thread_p != 0) {
			pthread_setschedparam(audio_thread_p, setting_sched_policy, &schedparam);
		}
		schedparam.sched_priority = setting_midi_priority;
		pthread_setschedparam(midi_thread_p, setting_sched_policy, &schedparam);
		schedparam.sched_priority = setting_engine_priority;
		for (i = 0; i < MAX_PARTS; i++) {
			pthread_setschedparam(engine_thread_p[i], setting_sched_policy, &schedparam);
		}
	}
	save_settings(NULL);
}


/*****************************************************************************
 * set_audio_driver()
 *****************************************************************************/
void
set_audio_driver(GtkWidget *widget, gpointer data)
{
	int     driver = (int)((long int) data);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		switch (driver) {
		case AUDIO_DRIVER_ALSA_PCM:
			gtk_widget_set_sensitive(GTK_WIDGET(alsa_menu_item), TRUE);
			gtk_widget_set_sensitive(jack_midi_button, FALSE);
			gtk_widget_set_sensitive(GTK_WIDGET(jack_menu_item), FALSE);
			break;
		case AUDIO_DRIVER_JACK:
			gtk_widget_set_sensitive(jack_midi_button, TRUE);
			gtk_widget_set_sensitive(GTK_WIDGET(jack_menu_item), TRUE);
			if (setting_midi_driver == MIDI_DRIVER_JACK) {
				gtk_widget_set_sensitive(GTK_WIDGET(alsa_menu_item), FALSE);
			}
			else {
				gtk_widget_set_sensitive(GTK_WIDGET(alsa_menu_item), TRUE);
			}
			break;
		case AUDIO_DRIVER_NONE:
		default:
			driver = AUDIO_DRIVER_NONE;
			break;
		}

		setting_audio_driver = driver;
		if (audio_driver != setting_audio_driver) {
			config_changed = 1;
			stop_engine();
			stop_audio();
		}
	}
}


/*****************************************************************************
 * set_midi_driver()
 *****************************************************************************/
void
set_midi_driver(GtkWidget *widget, gpointer data)
{
	int     driver = (int)((long int) data);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		switch (driver) {
#ifdef ENABLE_RAWMIDI_GENERIC
		case MIDI_DRIVER_RAW_GENERIC:
#endif
#ifdef ENABLE_RAWMIDI_OSS
		case MIDI_DRIVER_RAW_OSS:
#endif
		case MIDI_DRIVER_RAW_ALSA:
		case MIDI_DRIVER_ALSA_SEQ:
			gtk_widget_set_sensitive(GTK_WIDGET(alsa_menu_item), TRUE);
			gtk_widget_set_sensitive(GTK_WIDGET(jack_menu_item),
			                         (setting_audio_driver == AUDIO_DRIVER_JACK) ?
			                         TRUE : FALSE);
			break;
		case MIDI_DRIVER_JACK:
			gtk_widget_set_sensitive(GTK_WIDGET(jack_menu_item), TRUE);
			gtk_widget_set_sensitive(GTK_WIDGET(alsa_menu_item),
			                         (setting_audio_driver == AUDIO_DRIVER_ALSA_PCM) ?
			                         TRUE : FALSE);
			break;
		case MIDI_DRIVER_NONE:
			break;
		default:
			driver = MIDI_DRIVER_ALSA_SEQ;
		}

		setting_midi_driver = driver;
		if (midi_driver != setting_midi_driver) {
			config_changed = 1;
			stop_midi();
		}
	}
}


/*****************************************************************************
 * set_alsa_pcm_device()
 *****************************************************************************/
void
set_alsa_pcm_device(GtkWidget *UNUSED(widget), gpointer UNUSED(data))
{
	const char  *dev;

	dev = gtk_entry_get_text(GTK_ENTRY(alsa_device_entry));
	if ((dev == NULL) || (*dev == '\0')) {
		gtk_entry_set_text(GTK_ENTRY(alsa_device_entry), setting_alsa_pcm_device);
	}
	if ((dev != NULL) && (*dev != '\0')) {
		if (setting_alsa_pcm_device != NULL) {
			free(setting_alsa_pcm_device);
		}
		setting_alsa_pcm_device = strdup(dev);
		if (audio_driver == AUDIO_DRIVER_ALSA_PCM) {
			config_changed = 1;
			stop_audio();
		}
	}
}


/*****************************************************************************
 * set_oss_midi_device()
 *****************************************************************************/
#ifdef ENABLE_RAWMIDI_OSS
void
set_oss_midi_device(GtkWidget *UNUSED(widget), gpointer UNUSED(data))
{
	const char  *dev;

	dev = gtk_entry_get_text(GTK_ENTRY(oss_midi_device_entry));
	if ((dev != NULL) || (*dev != '\0')) {
		if (setting_oss_midi_device != NULL) {
			free(setting_oss_midi_device);
		}
		setting_oss_midi_device = strdup(dev);
		if (midi_driver == MIDI_DRIVER_RAW_OSS) {
			config_changed = 1;
			stop_midi();
		}
	}
}
#endif


/*****************************************************************************
 * set_generic_midi_device()
 *****************************************************************************/
#ifdef ENABLE_RAWMIDI_GENERIC
void
set_generic_midi_device(GtkWidget *UNUSED(widget), gpointer UNUSED(data))
{
	const char  *dev;

	dev = gtk_entry_get_text(GTK_ENTRY(generic_midi_device_entry));
	if ((dev != NULL) || (*dev != '\0')) {
		if (setting_generic_midi_device != NULL) {
			free(setting_generic_midi_device);
		}
		setting_generic_midi_device = strdup(dev);
		if (midi_driver == MIDI_DRIVER_RAW_GENERIC) {
			config_changed = 1;
			stop_midi();
		}
	}
}
#endif


/*****************************************************************************
 * set_alsa_raw_midi_device()
 *****************************************************************************/
void
set_alsa_raw_midi_device(GtkWidget *UNUSED(widget), gpointer UNUSED(data))
{
	const char  *dev;

	dev = gtk_entry_get_text(GTK_ENTRY(alsa_raw_midi_device_entry));
	if ((dev != NULL) || (*dev != '\0')) {
		if (setting_alsa_raw_midi_device != NULL) {
			free(setting_alsa_raw_midi_device);
		}
		setting_alsa_raw_midi_device = strdup(dev);
		if (midi_driver == MIDI_DRIVER_RAW_ALSA) {
			config_changed = 1;
			stop_midi();
		}
	}
}


/*****************************************************************************
 * set_alsa_seq_port()
 *****************************************************************************/
void
set_alsa_seq_port(GtkWidget *UNUSED(widget), gpointer UNUSED(data))
{
	const char  *dev;

	dev = gtk_entry_get_text(GTK_ENTRY(alsa_seq_port_entry));
	if ((dev != NULL) || (*dev != '\0')) {
		if (setting_alsa_seq_port != NULL) {
			free(setting_alsa_seq_port);
		}
		setting_alsa_seq_port = strdup(dev);
		if (midi_driver == MIDI_DRIVER_ALSA_SEQ) {
			config_changed = 1;
			stop_midi();
		}
	}
}


/*****************************************************************************
 * set_midi_audio_phase_lock()
 *****************************************************************************/
void
set_midi_audio_phase_lock(GtkWidget *UNUSED(widget), gpointer data)
{
	setting_audio_phase_lock = (timecalc_t) gtk_spin_button_get_value(GTK_SPIN_BUTTON(data));
	audio_phase_lock = (timecalc_t)(setting_audio_phase_lock * f_buffer_period_size);
	if (audio_phase_lock > 1.0) {
		audio_phase_min  = audio_phase_lock - 2.0;
	}
	else {
		audio_phase_min = 0.0;
	}
	if (audio_phase_lock < (f_buffer_period_size - 2.0)) {
		audio_phase_max  = audio_phase_lock + 2.0;
	}
	else {
		audio_phase_max = (f_buffer_period_size - 2.0);
	}
	save_settings(NULL);
}


/*****************************************************************************
 * set_ignore_midi_program_change()
 *****************************************************************************/
void
set_ignore_midi_program_change(GtkWidget *UNUSED(widget), gpointer data)
{
	int         ignore  = 0;
	GtkWidget   *button = (GtkWidget *) data;

	if ((GTK_IS_TOGGLE_BUTTON(button)) &&
	    (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))) {
		ignore = 1;
	}

	if (ignore != setting_ignore_midi_program_change) {
		setting_ignore_midi_program_change = ignore;
		save_settings(NULL);
	}
}


/*****************************************************************************
 * set_knob_dir()
 *****************************************************************************/
void
set_knob_dir(GtkWidget *widget, gpointer UNUSED(data))
{
	char        *knob_dir;

	knob_dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));

	if (knob_dir != NULL) {
		if ((setting_knob_dir == NULL) || strcmp(knob_dir, setting_knob_dir) != 0) {
			if (setting_knob_dir != NULL) {
				free(setting_knob_dir);
			}
			setting_knob_dir = strdup(knob_dir);
		}
		save_settings(NULL);
		set_theme_env();
		gtk_rc_reparse_all_for_settings(gtk_settings_get_default(), TRUE);
		restart_gtkui();
	}
}


/*****************************************************************************
 * set_detent_knob_dir()
 *****************************************************************************/
void
set_detent_knob_dir(GtkWidget *widget, gpointer UNUSED(data))
{
	char        *detent_knob_dir;

	detent_knob_dir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));

	if (detent_knob_dir != NULL) {
		if ((setting_detent_knob_dir == NULL) || strcmp(detent_knob_dir, setting_detent_knob_dir) != 0) {
			if (setting_detent_knob_dir != NULL) {
				free(setting_detent_knob_dir);
			}
			setting_detent_knob_dir = strdup(detent_knob_dir);
		}
		save_settings(NULL);
		set_theme_env();
		gtk_rc_reparse_all_for_settings(gtk_settings_get_default(), TRUE);
		restart_gtkui();
	}
}


/*****************************************************************************
 * set_jack_transport_mode()
 *****************************************************************************/
void
set_jack_transport_mode(GtkWidget *widget, gpointer data)
{
	int     mode = (int)((long int) data);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		switch (mode) {
		case JACK_TRANSPORT_TEMPO:
			setting_jack_transport_mode = JACK_TRANSPORT_TEMPO;
			break;
		case JACK_TRANSPORT_TNP:
			setting_jack_transport_mode = JACK_TRANSPORT_TNP;
			break;
		case JACK_TRANSPORT_OFF:
		default:
			setting_jack_transport_mode = JACK_TRANSPORT_OFF;
			break;
		}
		save_settings(NULL);
	}
}


/*****************************************************************************
 * set_jack_autoconnect()
 *****************************************************************************/
void
set_jack_autoconnect(GtkWidget *widget, gpointer data, GtkWidget *widget2)
{
	int     autoconnect = (int)((long int) data);

	/* make sure this is a button toggling ON and actually changing connect mode */
	if ((autoconnect != setting_jack_autoconnect) &&
	    (((GTK_IS_TOGGLE_BUTTON(widget)) &&
	      (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))) ||
	     ((GTK_IS_CHECK_MENU_ITEM(widget2)) &&
	      (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget2)))))) {

		/* set new autoconnect mode */
		if (autoconnect) {
			setting_jack_autoconnect = 1;
			if ((widget != NULL) && (menu_item_autoconnect != NULL)) {
				gtk_check_menu_item_set_active(menu_item_autoconnect, TRUE);
			}
		}
		else {
			setting_jack_autoconnect = 0;
			if ((widget != NULL) && (menu_item_manualconnect != NULL)) {
				gtk_check_menu_item_set_active(menu_item_manualconnect, TRUE);
			}
		}

		/* save settings and restart JACK connection */
		if (audio_driver == AUDIO_DRIVER_JACK) {
			config_changed = 1;
			stop_audio();
		}
	}
}


/*****************************************************************************
 * set_jack_multi_out()
 *****************************************************************************/
void
set_jack_multi_out(GtkWidget *widget, gpointer data, GtkWidget *widget2)
{
	int     multi_out = (int)((long int) data);

	/* make sure this is a button toggling ON and actually changing connect mode */
	if ((multi_out != setting_jack_multi_out) &&
	    (((GTK_IS_TOGGLE_BUTTON(widget)) &&
	      (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))) ||
	     ((GTK_IS_CHECK_MENU_ITEM(widget2)) &&
	      (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget2)))))) {

		/* set new multi_out mode */
		if (multi_out) {
			setting_jack_multi_out = 1;
			if ((widget != NULL) && (menu_item_multi_out != NULL)) {
				gtk_check_menu_item_set_active(menu_item_multi_out, TRUE);
			}
		}
		else {
			setting_jack_multi_out = 0;
			if ((widget != NULL) && (menu_item_manualconnect != NULL)) {
				gtk_check_menu_item_set_active(menu_item_stereo_out, TRUE);
			}
		}

		/* save settings and restart JACK connection */
		if (audio_driver == AUDIO_DRIVER_JACK) {
			config_changed = 1;
			stop_audio();
		}
	}
}


/*****************************************************************************
 * set_buffer_period_size()
 *****************************************************************************/
void
set_buffer_period_size(GtkWidget *widget, gpointer data)
{
	unsigned int    nframes = (unsigned int)((long unsigned int) data);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		switch (nframes) {
		case 32:
		case 64:
		case 128:
		case 256:
		case 512:
		case 1024:
		case 2048:
			break;
		default:
			nframes = 256;
			break;
		}

		setting_buffer_period_size = nframes;
		if (setting_buffer_period_size != buffer_period_size) {
			config_changed = 1;
			stop_engine();
			stop_audio();
		}
	}
}


/*****************************************************************************
 * set_buffer_latency()
 *****************************************************************************/
void
set_buffer_latency(GtkWidget *UNUSED(widget), gpointer data)
{
	setting_buffer_latency = (unsigned int) gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data));
	config_changed = 1;
}


/*****************************************************************************
 * set_force_16bit
 *****************************************************************************/
void
set_force_16bit(GtkWidget *UNUSED(widget), gpointer data)
{
	int         force_16bit     = 0;
	GtkWidget   *button         = (GtkWidget *) data;

	/* check to see if button is active */
	if ((GTK_IS_TOGGLE_BUTTON(button)) &&
	    (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))) {
		force_16bit = 1;
	}

	/* only restart audio driver if changing 16bit mode */
	if (force_16bit != setting_force_16bit) {
		setting_force_16bit = force_16bit;
		if (audio_driver == AUDIO_DRIVER_ALSA_PCM) {
			config_changed = 1;
			stop_audio();
		}
	}
}


/*****************************************************************************
 * set_enable_mmap
 *****************************************************************************/
void
set_enable_mmap(GtkWidget *UNUSED(widget), gpointer data)
{
	int         enable_mmap     = 0;
	GtkWidget   *button         = (GtkWidget *) data;

	/* check to see if button is active */
	if ((GTK_IS_TOGGLE_BUTTON(button)) &&
	    (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))) {
		enable_mmap = 1;
	}

	/* only restart audio driver if changing 16bit mode */
	if (enable_mmap != setting_enable_mmap) {
		setting_enable_mmap = enable_mmap;
		if (audio_driver == AUDIO_DRIVER_ALSA_PCM) {
			config_changed = 1;
			stop_audio();
		}
		gtk_widget_set_sensitive(GTK_WIDGET(enable_input_button),
		                         enable_mmap ? TRUE : FALSE);
	}
}


/*****************************************************************************
 * set_enable_inputs
 *****************************************************************************/
void
set_enable_inputs(GtkWidget *UNUSED(widget), gpointer data)
{
	int         enable_inputs   = 0;
	GtkWidget   *button         = (GtkWidget *) data;

	/* check to see if button is active */
	if ((GTK_IS_TOGGLE_BUTTON(button)) &&
	    (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))) {
		enable_inputs = 1;
	}

	/* only restart audio driver if changing 16bit mode */
	if (enable_inputs != setting_enable_inputs) {
		setting_enable_inputs = enable_inputs;
		if (setting_enable_mmap && (audio_driver == AUDIO_DRIVER_ALSA_PCM)) {
			config_changed = 1;
			stop_audio();
		}
	}
}


/*****************************************************************************
 * set_sample_rate()
 *****************************************************************************/
void
set_sample_rate(GtkWidget *widget, gpointer data)
{
	int     rate = (int)((long int) data);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		switch (rate) {
		case 22050:
		case 32000:
		case 44100:
		case 48000:
		case 64000:
		case 88200:
		case 96000:
		case 128000:
		case 176400:
		case 192000:
			break;
		default:
			rate = DEFAULT_SAMPLE_RATE;
			break;
		}

		if (setting_sample_rate != rate) {
			setting_sample_rate = rate;
			if (audio_driver == AUDIO_DRIVER_ALSA_PCM) {
				sample_rate_changed = 1;
				config_changed = 1;
				stop_audio();
			}
		}
	}
}


/*****************************************************************************
 * set_sample_rate_mode()
 *****************************************************************************/
void
set_sample_rate_mode(GtkWidget *widget, gpointer data)
{
	int     mode     = (int)((long int) data);
	int     old_mode = setting_sample_rate_mode;

	/* set new mode from toggle buttons */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		switch (mode) {
		case SAMPLE_RATE_UNDERSAMPLE:
			setting_sample_rate_mode = SAMPLE_RATE_UNDERSAMPLE;
			break;
		case SAMPLE_RATE_OVERSAMPLE:
			setting_sample_rate_mode = SAMPLE_RATE_OVERSAMPLE;
			break;
		case SAMPLE_RATE_NORMAL:
		default:
			setting_sample_rate_mode = SAMPLE_RATE_NORMAL;
			break;
		}
		if (setting_sample_rate_mode != old_mode) {
			sample_rate_mode_changed = 1;
			config_changed = 1;
		}
	}
}


/*****************************************************************************
 * set_bank_mem_mode()
 *****************************************************************************/
void
set_bank_mem_mode(GtkWidget *widget, gpointer data, GtkWidget *UNUSED(widget2))
{
	int     mode = (int)((long int) data);

	/* if called from toggle button, make sure this is a button toggling ON */
	if ((widget != NULL) && (GTK_IS_TOGGLE_BUTTON(widget))) {
		if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
			return;
		}
	}
	/* otherwise, check for menu item actually toggling ON */
	else if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(bank_mem_menu_item[mode]))) {
		return;
	}

	/* make sure the mode is actually changing */
	if (mode != setting_bank_mem_mode) {
		switch (mode) {
		case BANK_MEM_AUTOSAVE:
			setting_bank_mem_mode = BANK_MEM_AUTOSAVE;
			if ((bank_autosave_button != NULL) && (GTK_IS_TOGGLE_BUTTON(bank_autosave_button)) &&
			    !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bank_autosave_button))) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bank_autosave_button), TRUE);
			}
			if ((menu_item_autosave != NULL)) {
				gtk_check_menu_item_set_active(menu_item_autosave, TRUE);
			}
			break;
		case BANK_MEM_PROTECT:
			setting_bank_mem_mode = BANK_MEM_PROTECT;
			if ((bank_protect_button != NULL) && (GTK_IS_TOGGLE_BUTTON(bank_protect_button)) &&
			    !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bank_protect_button))) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bank_protect_button), TRUE);
			}
			if ((menu_item_protect != NULL)) {
				gtk_check_menu_item_set_active(menu_item_protect, TRUE);
			}
			break;
		case BANK_MEM_WARN:
		default:
			setting_bank_mem_mode = BANK_MEM_WARN;
			if ((bank_warn_button != NULL) && (GTK_IS_TOGGLE_BUTTON(bank_warn_button)) &&
			    !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bank_warn_button))) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bank_warn_button), TRUE);
			}
			if ((menu_item_warn != NULL)) {
				gtk_check_menu_item_set_active(menu_item_warn, TRUE);
			}
			break;
		}

		save_settings(NULL);
	}
}


/*****************************************************************************
 * set_tuning_freq()
 *****************************************************************************/
void
set_tuning_freq(GtkWidget *UNUSED(widget), gpointer data)
{
	a4freq = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data));
	setting_tuning_freq = (sample_t) a4freq;
	build_freq_table();
	build_filter_tables();
}


#ifdef NONSTANDARD_HARMONICS
/*****************************************************************************
 * set_harmonic_base()
 *****************************************************************************/
void
set_harmonic_base(GtkWidget *UNUSED(widget), gpointer data)
{
	setting_harmonic_base = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data));
	build_freq_table();
	build_filter_tables();
}


/*****************************************************************************
 * set_harmonic_steps()
 *****************************************************************************/
void
set_harmonic_steps(GtkWidget *UNUSED(widget), gpointer data)
{
	setting_harmonic_steps = gtk_spin_button_get_value(GTK_SPIN_BUTTON(data));
	build_freq_table();
	build_filter_tables();
}
#endif /* NONSTANDARD_HARMONICS */


/*****************************************************************************
 * set_polyphony()
 *****************************************************************************/
void
set_polyphony(GtkWidget *UNUSED(widget), gpointer data)
{
	setting_polyphony = (int) gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data));
}


#ifdef SHOW_BACKING_STORE_SETTING
/*****************************************************************************
 * set_backing_store()
 *****************************************************************************/
void
set_backing_store(GtkWidget *UNUSED(widget), gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data))) {
		setting_backing_store = 1;
	}
	else {
		setting_backing_store = 0;
	}
	restart_gtkui();
}
#endif


/*****************************************************************************
 * set_theme()
 *****************************************************************************/
void
set_theme(GtkWidget *widget, gpointer data)
{
	int     theme = (int)((long int) data);
	int     old_theme = setting_theme;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		switch (theme) {
		case PHASEX_THEME_LIGHT:
			setting_theme = PHASEX_THEME_LIGHT;
			if (setting_knob_dir != NULL) {
				free(setting_knob_dir);
			}
			setting_knob_dir = malloc(strlen(PIXMAP_DIR) + strlen(DEFAULT_LIGHT_KNOB_DIR) + 2);
			sprintf(setting_knob_dir, "%s/%s", PIXMAP_DIR, DEFAULT_LIGHT_KNOB_DIR);
			if (setting_detent_knob_dir != NULL) {
				free(setting_detent_knob_dir);
			}
			setting_detent_knob_dir = malloc(strlen(PIXMAP_DIR) + strlen(DEFAULT_LIGHT_DETENT_KNOB_DIR) + 2);
			sprintf(setting_detent_knob_dir, "%s/%s", PIXMAP_DIR, DEFAULT_LIGHT_DETENT_KNOB_DIR);
			break;
		case PHASEX_THEME_SYSTEM:
			setting_theme = PHASEX_THEME_SYSTEM;
			break;
		case PHASEX_THEME_CUSTOM:
			setting_theme = PHASEX_THEME_CUSTOM;
			break;
		case PHASEX_THEME_DARK:
		default:
			setting_theme = PHASEX_THEME_DARK;
			if (setting_knob_dir != NULL) {
				free(setting_knob_dir);
			}
			setting_knob_dir = malloc(strlen(PIXMAP_DIR) + strlen(DEFAULT_DARK_KNOB_DIR) + 2);
			sprintf(setting_knob_dir, "%s/%s", PIXMAP_DIR, DEFAULT_DARK_KNOB_DIR);
			if (setting_detent_knob_dir != NULL) {
				free(setting_detent_knob_dir);
			}
			setting_detent_knob_dir = malloc(strlen(PIXMAP_DIR) + strlen(DEFAULT_DARK_DETENT_KNOB_DIR) + 2);
			sprintf(setting_detent_knob_dir, "%s/%s", PIXMAP_DIR, DEFAULT_DARK_DETENT_KNOB_DIR);
			break;
		}

		if (old_theme != setting_theme) {
			save_settings(NULL);

			set_theme_env();
			gtk_rc_reparse_all_for_settings(gtk_settings_get_default(), TRUE);
			restart_gtkui();
		}
	}
}


/*****************************************************************************
 * set_custom_theme()
 *****************************************************************************/
void
set_custom_theme(GtkWidget *widget, gpointer UNUSED(data))
{
	char        *theme_file;

	theme_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));

	if (theme_file != NULL) {
		if ((setting_custom_theme == NULL) || strcmp(theme_file, setting_custom_theme) != 0) {
			setting_theme = PHASEX_THEME_CUSTOM;
			if (setting_custom_theme != NULL) {
				free(setting_custom_theme);
			}
			setting_custom_theme = strdup(theme_file);
			if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(custom_theme_button))) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(custom_theme_button), TRUE);
			}
		}
		save_settings(NULL);
		set_theme_env();
		gtk_rc_reparse_all_for_settings(gtk_settings_get_default(), TRUE);
		restart_gtkui();
	}
}


/*****************************************************************************
 * set_theme_env()
 *****************************************************************************/
void
set_theme_env(void)
{
	char    *theme_selection    = NULL;
	gchar   rc_file_string[256] = "\0";
	gchar   *rc_file_list[3]    = { NULL, NULL, NULL };
#ifdef GTK_ENGINE_DIR
	gchar   *gtk_engine_path;
#endif
	int     rc_file_num         = 0;

	switch (setting_theme) {
	case PHASEX_THEME_CUSTOM:
		if (setting_custom_theme != NULL) {
			theme_selection = setting_custom_theme;
		}
		else {
			setting_theme = PHASEX_THEME_DARK;
			theme_selection = GTKRC_DARK;
		}
		break;
	case PHASEX_THEME_LIGHT:
		theme_selection = GTKRC_LIGHT;
		break;
	case PHASEX_THEME_DARK:
		theme_selection = GTKRC_DARK;
		break;
	}

	/* no theme selected, no rc files... use system defaults */
#ifdef GTK_ENGINE_DIR
	gtk_engine_path = g_module_build_path(GTK_ENGINE_DIR, PHASEX_GTK_ENGINE);
#endif
	if (theme_selection == NULL) {
		rc_file_string[0] = '\0';
	}
#ifdef GTK_ENGINE_DIR
	/* if GTK supports engines, make sure our engine is on the system */
	else if ((g_file_test(gtk_engine_path, G_FILE_TEST_EXISTS))) {
		snprintf(rc_file_string, sizeof(rc_file_string), "%s:%s",
		         PHASEX_GTK_ENGINE_RC, theme_selection);
		rc_file_list[rc_file_num] = PHASEX_GTK_ENGINE_RC;
		rc_file_num++;
		rc_file_list[rc_file_num] = theme_selection;
		rc_file_num++;
		PHASEX_DEBUG((DEBUG_CLASS_INIT | DEBUG_CLASS_GUI),
		             "Found GTK engine path '%s'.\n", gtk_engine_path);
	}
#endif
	/* no engine available, so just load the main theme rc */
	else {
		PHASEX_DEBUG((DEBUG_CLASS_INIT | DEBUG_CLASS_GUI),
		             "GTK engine path '%s' not found.\n  Using default radiobutton colors.\n",
		             gtk_engine_path);
		snprintf(rc_file_string, sizeof(rc_file_string), "%s", theme_selection);
		rc_file_list[rc_file_num] = theme_selection;
		rc_file_num++;
	}

	gtk_rc_set_default_files(rc_file_list);

#ifdef GTK_ENGINE_DIR
	g_free(gtk_engine_path);
#endif

	if (rc_file_string[0] == '\0') {
		unsetenv("GTK2_RC_FILES");
	}
	else {
		setenv("GTK2_RC_FILES", rc_file_string, 1);
	}
}


/*****************************************************************************
 * set_font()
 *****************************************************************************/
void
set_font(GtkFontButton *button, GtkWidget *UNUSED(widget))
{
	const gchar         *font;

	font = gtk_font_button_get_font_name(button);
	if (setting_font != NULL) {
		free(setting_font);
	}
	setting_font = strdup(font);
	if (phasex_font_desc != NULL) {
		pango_font_description_free(phasex_font_desc);
	}
	phasex_font_desc = pango_font_description_from_string(font);

	restart_gtkui();
}


/*****************************************************************************
 * set_numeric_font()
 *****************************************************************************/
void
set_numeric_font(GtkFontButton *button, GtkWidget *UNUSED(widget))
{
	const gchar         *font;

	font = gtk_font_button_get_font_name(button);
	if (setting_numeric_font != NULL) {
		free(setting_numeric_font);
	}
	setting_numeric_font = strdup(font);
	if (numeric_font_desc != NULL) {
		pango_font_description_free(numeric_font_desc);
	}
	numeric_font_desc = pango_font_description_from_string(font);

	restart_gtkui();
}


/*****************************************************************************
 * set_title_font()
 *****************************************************************************/
void
set_title_font(GtkFontButton *button, GtkWidget *UNUSED(widget))
{
	const gchar         *font;

	font = gtk_font_button_get_font_name(button);
	if (setting_title_font != NULL) {
		free(setting_title_font);
	}
	setting_title_font = strdup(font);
	if (title_font_desc != NULL) {
		pango_font_description_free(title_font_desc);
	}
	title_font_desc = pango_font_description_from_string(font);

	restart_gtkui();
}


/*****************************************************************************
 * on_restart_audio()
 *****************************************************************************/
void
on_restart_audio(GtkWidget *UNUSED(widget), gpointer UNUSED(data))
{
	stop_audio();
}


/*****************************************************************************
 * on_restart_midi()
 *****************************************************************************/
void
on_restart_midi(GtkWidget *UNUSED(widget), gpointer UNUSED(data))
{
	stop_midi();
}


#ifdef ENABLE_CONFIG_DIALOG
/*****************************************************************************
 * close_config_dialog()
 *****************************************************************************/
void
close_config_dialog(GtkWidget *widget, gpointer data)
{
	if (((long int) data != -1) && (widget != main_window)) {
		config_is_open   = 0;
	}
	save_settings(NULL);
	if ((config_dialog != NULL) && GTK_IS_DIALOG(config_dialog)) {
		config_page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK(config_notebook));
		gtk_widget_destroy(config_dialog);
	}
	config_dialog               = NULL;
	config_notebook             = NULL;
	custom_theme_button         = NULL;
	bank_autosave_button        = NULL;
	bank_warn_button            = NULL;
	bank_protect_button         = NULL;
	alsa_device_entry           = NULL;
#ifdef ENABLE_RAWMIDI_OSS
	oss_midi_device_entry       = NULL;
#endif
#ifdef ENABLE_RAWMIDI_GENERIC
	generic_midi_device_entry   = NULL;
#endif
	alsa_raw_midi_device_entry  = NULL;
	alsa_seq_port_entry         = NULL;
	jack_midi_button            = NULL;
	enable_input_button         = NULL;
	audio_status_label          = NULL;
}


/*****************************************************************************
 * on_config_notebook_page_switch()
 *****************************************************************************/
void
on_config_notebook_page_switch(GtkNotebook      *UNUSED(notebook),
                               GtkNotebookPage *UNUSED(page),
                               gint              page_num,
                               gpointer          UNUSED(user_data))
{
	config_page_num = page_num;
}


/*****************************************************************************
 * show_config_dialog()
 *****************************************************************************/
void
show_config_dialog(void)
{
	if (config_dialog == NULL) {
		create_config_dialog();
	}

	/* show the dialog now that it's built */
	gtk_widget_show_all(config_dialog);
	config_is_open = 1;

	/* open to page based on hint from last opening */
	gtk_notebook_set_current_page(GTK_NOTEBOOK(config_notebook), config_page_num);

	/* TODO:  update widgets with new settings */
}


/*****************************************************************************
 * create_config_dialog()
 *****************************************************************************/
void
create_config_dialog(void)
{
	static char     *theme_dir          = NULL;
	static char     *pixmap_dir         = NULL;
	char            *tmp_dir            = NULL;
	GtkWidget       *frame;
	GtkWidget       *page;
	GtkWidget       *event;
	GtkWidget       *hbox;
	GtkWidget       *page_hbox;
	GtkWidget       *vbox;
	GtkWidget       *box;
	GtkWidget       *sep;
	GtkWidget       *label;
	GtkWidget       *button1;
	GtkWidget       *button2;
	GtkWidget       *button3;
	GtkWidget       *button4;
	GtkWidget       *button5;
	GtkWidget       *button6;
	GtkWidget       *button7;
	GtkWidget       *button8;
	GtkWidget       *button9;
	GtkWidget       *button10;
	GtkWidget       *button11;
	GtkWidget       *button12;
	GtkWidget       *active_button      = NULL;
	GtkWidget       *spin;
	GtkObject       *adj;
	int             min_prio;
	int             max_prio;
	gint            page_num            = 0;
	gfloat          button_x            = 0.37;

	/* don't do anything if the dialog already exists */
	if (config_dialog != NULL) {
		return;
	}

	/* non-modal dialog w/ close button */
	config_dialog = gtk_dialog_new_with_buttons("PHASEX Preferences",
	                                            GTK_WINDOW(main_window),
	                                            GTK_DIALOG_DESTROY_WITH_PARENT,
	                                            GTK_STOCK_CLOSE,
	                                            GTK_RESPONSE_CLOSE,
	                                            NULL);
	gtk_window_set_wmclass(GTK_WINDOW(config_dialog), "phasex", "phasex");
	gtk_window_set_role(GTK_WINDOW(config_dialog), "preferences");

	/* create a notebook and attach it to the dialog's vbox */
	config_notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(config_notebook), GTK_POS_TOP);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(config_dialog)->vbox), config_notebook, TRUE, TRUE, 8);

	/* *********** "MIDI" tab *********** */
	frame = gtk_frame_new("");
	widget_set_backing_store(frame);

	label = gtk_label_new("MIDI");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 10);
#else
	gtk_widget_set_size_request(label, 80, -1);
#endif

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_container_add(GTK_CONTAINER(frame), event);
	gtk_widget_show(frame);
	gtk_container_add(GTK_CONTAINER(config_notebook), frame);

	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(config_notebook), 0);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(config_notebook), page, FALSE);
#endif
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(config_notebook), page, label);

	page_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(event), page_hbox);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page_hbox), vbox, TRUE, TRUE, 16);

	/* separator */
	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	/* *** MIDI Driver *** label + 4 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("MIDI Driver:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "JACK MIDI  (requires JACK Audio)");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1),
	                                                      "ALSA Sequencer  [client:port]");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	button3 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button2),
	                                                      "ALSA Raw MIDI  [hw:x,y]");
	widget_set_custom_font(button3, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button3), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button3, FALSE, FALSE, 0);

#ifdef ENABLE_RAWMIDI_GENERIC
	button4 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button3),
	                                                      "Generic MIDI  [/dev/midi]");
	widget_set_custom_font(button4, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button4), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button4, FALSE, FALSE, 0);
#endif

#ifdef ENABLE_RAWMIDI_OSS
	button5 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button4),
	                                                      "OSS  [/dev/midi?]  (untested!!!)");
	widget_set_custom_font(button5, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button5), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button5, FALSE, FALSE, 0);
#endif

	jack_midi_button = button1;
	if (audio_driver != AUDIO_DRIVER_JACK) {
		gtk_widget_set_sensitive(jack_midi_button, FALSE);
	}

	/* set active button before connecting signals */
	active_button = NULL;
	switch (setting_midi_driver) {
	case MIDI_DRIVER_JACK:
		active_button = button1;
		break;
	case MIDI_DRIVER_ALSA_SEQ:
		active_button = button2;
		break;
	case MIDI_DRIVER_RAW_ALSA:
		active_button = button3;
		break;
#ifdef ENABLE_RAWMIDI_GENERIC
	case MIDI_DRIVER_RAW_GENERIC:
		active_button = button4;
		break;
#endif
#ifdef ENABLE_RAWMIDI_OSS
	case MIDI_DRIVER_RAW_OSS:
		active_button = button5;
		break;
#endif
	}
	if (active_button != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_button), TRUE);
	}

	/* connect signals for midi driver radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_midi_driver),
	                 (gpointer) MIDI_DRIVER_JACK);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_midi_driver),
	                 (gpointer) MIDI_DRIVER_ALSA_SEQ);
	g_signal_connect(GTK_OBJECT(button3), "toggled",
	                 GTK_SIGNAL_FUNC(set_midi_driver),
	                 (gpointer) MIDI_DRIVER_RAW_ALSA);
#ifdef ENABLE_RAWMIDI_GENERIC
	g_signal_connect(GTK_OBJECT(button4), "toggled",
	                 GTK_SIGNAL_FUNC(set_midi_driver),
	                 (gpointer) MIDI_DRIVER_RAW_GENERIC);
#endif
#ifdef ENABLE_RAWMIDI_OSS
	g_signal_connect(GTK_OBJECT(button5), "toggled",
	                 GTK_SIGNAL_FUNC(set_midi_driver),
	                 (gpointer) MIDI_DRIVER_RAW_OSS);
#endif

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *** Restart MIDI Driver *** label + button */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Restart MIDI Driver:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	button1 = gtk_button_new_with_label(" Restart Now! ");
	widget_set_custom_font(button1, phasex_font_desc);
	widget_set_backing_store(button1);
	gtk_container_add(GTK_CONTAINER(event), button1);
	gtk_box_pack_start(GTK_BOX(box), event, TRUE, TRUE, 1);

	g_signal_connect(GTK_OBJECT(button1), "clicked",
	                 GTK_SIGNAL_FUNC(on_restart_midi),
	                 (gpointer) NULL);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *** ALSA Sequencer Port *** label + entry */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("ALSA Sequencer Port(s):");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	alsa_seq_port_entry = gtk_entry_new_with_max_length(256);

	if (setting_alsa_seq_port != NULL) {
		gtk_entry_set_text(GTK_ENTRY(alsa_seq_port_entry), setting_alsa_seq_port);
	}

	gtk_entry_set_width_chars(GTK_ENTRY(alsa_seq_port_entry), 32);
	gtk_entry_set_editable(GTK_ENTRY(alsa_seq_port_entry), TRUE);
	gtk_entry_set_activates_default(GTK_ENTRY(alsa_seq_port_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(box), alsa_seq_port_entry, FALSE, FALSE, 1);

	g_signal_connect(GTK_OBJECT(alsa_seq_port_entry), "activate",
	                 GTK_SIGNAL_FUNC(set_alsa_seq_port),
	                 (gpointer) alsa_seq_port_entry);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *** ALSA Raw MIDI Device *** label + entry */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("ALSA Raw MIDI Device:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	alsa_raw_midi_device_entry = gtk_entry_new_with_max_length(256);

	if (setting_alsa_raw_midi_device != NULL) {
		gtk_entry_set_text(GTK_ENTRY(alsa_raw_midi_device_entry),
		                   setting_alsa_raw_midi_device);
	}

	gtk_entry_set_width_chars(GTK_ENTRY(alsa_raw_midi_device_entry), 32);
	gtk_entry_set_editable(GTK_ENTRY(alsa_raw_midi_device_entry), TRUE);
	gtk_entry_set_activates_default(GTK_ENTRY(alsa_raw_midi_device_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(box), alsa_raw_midi_device_entry, FALSE, FALSE, 1);

	g_signal_connect(GTK_OBJECT(alsa_raw_midi_device_entry), "activate",
	                 GTK_SIGNAL_FUNC(set_alsa_raw_midi_device),
	                 (gpointer) alsa_raw_midi_device_entry);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *** Generic Raw MIDI Device *** label + entry */
#ifdef ENABLE_RAWMIDI_GENERIC
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Generic MIDI Device:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	generic_midi_device_entry = gtk_entry_new_with_max_length(256);

	if (setting_generic_midi_device != NULL) {
		gtk_entry_set_text(GTK_ENTRY(generic_midi_device_entry), setting_generic_midi_device);
	}

	gtk_entry_set_width_chars(GTK_ENTRY(generic_midi_device_entry), 32);
	gtk_entry_set_editable(GTK_ENTRY(generic_midi_device_entry), TRUE);
	gtk_entry_set_activates_default(GTK_ENTRY(generic_midi_device_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(box), generic_midi_device_entry, FALSE, FALSE, 1);

	g_signal_connect(GTK_OBJECT(generic_midi_device_entry), "activate",
	                 GTK_SIGNAL_FUNC(set_generic_midi_device),
	                 (gpointer) generic_midi_device_entry);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);
#endif /* ENABLE_RAWMIDI_GENERIC */

	/* *** OSS MIDI Device *** label + entry */
#ifdef ENABLE_RAWMIDI_OSS
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("OSS MIDI Device:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	oss_midi_device_entry = gtk_entry_new_with_max_length(256);

	if (setting_oss_midi_device != NULL) {
		gtk_entry_set_text(GTK_ENTRY(oss_midi_device_entry), setting_oss_midi_device);
	}

	gtk_entry_set_width_chars(GTK_ENTRY(oss_midi_device_entry), 32);
	gtk_entry_set_editable(GTK_ENTRY(oss_midi_device_entry), TRUE);
	gtk_entry_set_activates_default(GTK_ENTRY(oss_midi_device_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(box), oss_midi_device_entry, FALSE, FALSE, 1);

	g_signal_connect(GTK_OBJECT(oss_midi_device_entry), "activate",
	                 GTK_SIGNAL_FUNC(set_oss_midi_device),
	                 (gpointer) oss_midi_device_entry);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);
#endif /* ENABLE_RAWMIDI_OSS */

	/* MIDI Audio Phase Lock: hbox w/ label + spinbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("MIDI Clock - Audio Wakeup Phase Lock:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	audio_phase_lock_adj = gtk_adjustment_new(setting_audio_phase_lock,
	                                          0.0, 1.0,
	                                          (1.0 / f_buffer_period_size),
	                                          (1.0 / f_buffer_period_size),
	                                          0.0);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	spin = gtk_spin_button_new(GTK_ADJUSTMENT(audio_phase_lock_adj),
	                           (1.0 / f_buffer_period_size), 5);
	widget_set_custom_font(spin, numeric_font_desc);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), setting_audio_phase_lock);
	gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(audio_phase_lock_adj), "value_changed",
	                 GTK_SIGNAL_FUNC(set_midi_audio_phase_lock),
	                 (gpointer) spin);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* Ignore Program Change Messages: hbox w/ label + checkbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Ignore MIDI Program Change:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_check_button_new();
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_end(GTK_BOX(box), button1, TRUE, TRUE, 4);

	/* set active button before connecting signals */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button1),
	                             (setting_ignore_midi_program_change ? TRUE : FALSE));

	/* connect signal for backing store checkbutton */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_ignore_midi_program_change),
	                 (gpointer) button1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_end(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *********** "Audio" tab *********** */
	frame = gtk_frame_new("");
	widget_set_backing_store(frame);

	label = gtk_label_new("Audio");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 10);
#else
	gtk_widget_set_size_request(label, 80, -1);
#endif

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_container_add(GTK_CONTAINER(frame), event);
	gtk_widget_show(frame);
	gtk_container_add(GTK_CONTAINER(config_notebook), frame);

	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(config_notebook), 1);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(config_notebook), page, FALSE);
#endif
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(config_notebook), page, label);

	page_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(event), page_hbox);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page_hbox), vbox, TRUE, TRUE, 16);

	/* separator */
	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	/* *** Audio Driver *** label + 4 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Audio Driver:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "ALSA");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "JACK");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	switch (setting_audio_driver) {
	case AUDIO_DRIVER_ALSA_PCM:
		active_button = button1;
		break;
	case AUDIO_DRIVER_JACK:
		active_button = button2;
		break;
	default:
		active_button = button1;
		break;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_button), TRUE);


	/* connect signals for audio driver radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_audio_driver),
	                 (gpointer) AUDIO_DRIVER_ALSA_PCM);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_audio_driver),
	                 (gpointer) AUDIO_DRIVER_JACK);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *** Restart Audio Driver *** label + button */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Restart Audio Driver:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	button1 = gtk_button_new_with_label(" Restart Now! ");
	widget_set_custom_font(button1, phasex_font_desc);
	widget_set_backing_store(button1);
	gtk_container_add(GTK_CONTAINER(event), button1);
	gtk_box_pack_start(GTK_BOX(box), event, TRUE, TRUE, 1);

	g_signal_connect(GTK_OBJECT(button1), "clicked",
	                 GTK_SIGNAL_FUNC(on_restart_audio),
	                 (gpointer) NULL);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* Buffer Latency: hbox w/ label + spinbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Buffer Latency:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	adj = gtk_adjustment_new(setting_buffer_latency, 1, 3,
	                         1, 1, 0);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	widget_set_custom_font(spin, numeric_font_desc);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), setting_buffer_latency);
	gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(adj), "value_changed",
	                 GTK_SIGNAL_FUNC(set_buffer_latency),
	                 (gpointer) spin);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *** Audio Driver Status *** label + label */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Audio Driver Status:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	query_audio_driver_status(audio_driver_status_msg);
	audio_status_label = gtk_label_new(audio_driver_status_msg);
	widget_set_custom_font(audio_status_label, phasex_font_desc);
	gtk_box_pack_start(GTK_BOX(box), audio_status_label, TRUE, TRUE, 4);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	label = gtk_label_new("* Options for ALSA PCM and JACK audio can\n"
	                      "   be found under their respective tabs.");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 3);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_end(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *********** "ALSA" tab *********** */
	frame = gtk_frame_new("");
	widget_set_backing_store(frame);

	label = gtk_label_new("ALSA");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 10);
#else
	gtk_widget_set_size_request(label, 80, -1);
#endif

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_container_add(GTK_CONTAINER(frame), event);
	gtk_widget_show(frame);
	gtk_container_add(GTK_CONTAINER(config_notebook), frame);

	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(config_notebook), 2);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(config_notebook), page, FALSE);
#endif
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(config_notebook), page, label);

	page_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(event), page_hbox);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page_hbox), vbox, TRUE, TRUE, 16);

	/* separator */
	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	/* *** Playback Sample Rate *** label + 4 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Playback Sample Rate:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(TRUE, 4);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	hbox = box;

	box = gtk_vbox_new(TRUE, 4);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "22050");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "44100");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	button3 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button2), "48000");
	widget_set_custom_font(button3, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button3), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button3, FALSE, FALSE, 0);

	box = gtk_vbox_new(TRUE, 4);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button4 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button3), "64000");
	widget_set_custom_font(button4, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button4), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button4, FALSE, FALSE, 0);

	button5 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button4), "88200");
	widget_set_custom_font(button5, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button5), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button5, FALSE, FALSE, 0);

	button6 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button5), "96000");
	widget_set_custom_font(button6, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button6), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button6, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	active_button = NULL;
	switch (setting_sample_rate) {
	case 22050:
		active_button = button1;
		break;
	case 44100:
		active_button = button2;
		break;
	case 48000:
		active_button = button3;
		break;
	case 64000:
		active_button = button4;
		break;
	case 88200:
		active_button = button5;
		break;
	case 96000:
		active_button = button6;
		break;
	default:
		active_button = NULL;
		break;
	}
	if (active_button != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_button), TRUE);
	}

	/* connect signals for sample rate radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_sample_rate),
	                 (gpointer) 22050);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_sample_rate),
	                 (gpointer) 44100);
	g_signal_connect(GTK_OBJECT(button3), "toggled",
	                 GTK_SIGNAL_FUNC(set_sample_rate),
	                 (gpointer) 48000);
	g_signal_connect(GTK_OBJECT(button4), "toggled",
	                 GTK_SIGNAL_FUNC(set_sample_rate),
	                 (gpointer) 64000);
	g_signal_connect(GTK_OBJECT(button5), "toggled",
	                 GTK_SIGNAL_FUNC(set_sample_rate),
	                 (gpointer) 88200);
	g_signal_connect(GTK_OBJECT(button6), "toggled",
	                 GTK_SIGNAL_FUNC(set_sample_rate),
	                 (gpointer) 96000);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *** Buffer Period Size *** label + 4 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Buffer Period Size:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(TRUE, 4);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	hbox = box;

	box = gtk_vbox_new(TRUE, 4);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "32");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "64");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	button3 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button2), "128");
	widget_set_custom_font(button3, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button3), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button3, FALSE, FALSE, 0);

	box = gtk_vbox_new(TRUE, 4);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button4 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button3), "256");
	widget_set_custom_font(button4, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button4), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button4, FALSE, FALSE, 0);

	button5 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button4), "512");
	widget_set_custom_font(button5, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button5), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button5, FALSE, FALSE, 0);

	button6 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button5), "1024");
	widget_set_custom_font(button6, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button6), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button6, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	active_button = NULL;
	switch (setting_buffer_period_size) {
	case 32:
		active_button = button1;
		break;
	case 64:
		active_button = button2;
		break;
	case 128:
		active_button = button3;
		break;
	case 256:
		active_button = button4;
		break;
	case 512:
		active_button = button5;
		break;
	case 1024:
		active_button = button6;
		break;
	default:
		active_button = NULL;
		break;
	}
	if (active_button != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_button), TRUE);
	}

	/* connect signals for buffer period size radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_buffer_period_size),
	                 (gpointer) 32);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_buffer_period_size),
	                 (gpointer) 64);
	g_signal_connect(GTK_OBJECT(button3), "toggled",
	                 GTK_SIGNAL_FUNC(set_buffer_period_size),
	                 (gpointer) 128);
	g_signal_connect(GTK_OBJECT(button4), "toggled",
	                 GTK_SIGNAL_FUNC(set_buffer_period_size),
	                 (gpointer) 256);
	g_signal_connect(GTK_OBJECT(button5), "toggled",
	                 GTK_SIGNAL_FUNC(set_buffer_period_size),
	                 (gpointer) 512);
	g_signal_connect(GTK_OBJECT(button6), "toggled",
	                 GTK_SIGNAL_FUNC(set_buffer_period_size),
	                 (gpointer) 1024);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *** ALSA PCM Device *** label + entry */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("ALSA PCM Device:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	alsa_device_entry = gtk_entry_new_with_max_length(256);

	if (setting_alsa_pcm_device != NULL) {
		gtk_entry_set_text(GTK_ENTRY(alsa_device_entry), setting_alsa_pcm_device);
	}

	gtk_entry_set_width_chars(GTK_ENTRY(alsa_device_entry), 32);
	gtk_entry_set_editable(GTK_ENTRY(alsa_device_entry), TRUE);
	gtk_entry_set_activates_default(GTK_ENTRY(alsa_device_entry), TRUE);
	gtk_box_pack_start(GTK_BOX(box), alsa_device_entry, FALSE, FALSE, 1);

	g_signal_connect(GTK_OBJECT(alsa_device_entry), "activate",
	                 GTK_SIGNAL_FUNC(set_alsa_pcm_device),
	                 (gpointer) alsa_device_entry);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* Force 16bit: hbox w/ label + checkbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Force 16bit Audio Samples:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_check_button_new();
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_end(GTK_BOX(box), button1, TRUE, TRUE, 4);

	/* set active button before connecting signals */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button1),
	                             (setting_force_16bit ? TRUE : FALSE));

	/* connect signal for backing store checkbutton */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_force_16bit),
	                 (gpointer) button1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* Enable MMAP: hbox w/ label + checkbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Enable MMAP:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_check_button_new();
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_end(GTK_BOX(box), button1, TRUE, TRUE, 4);

	/* set active button before connecting signals */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button1),
	                             (setting_enable_mmap ? TRUE : FALSE));

	/* connect signal for backing store checkbutton */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_enable_mmap),
	                 (gpointer) button1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* Enable Inputs: hbox w/ label + checkbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Enable Inputs (requires MMAP):");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	enable_input_button = gtk_check_button_new();
	gtk_button_set_alignment(GTK_BUTTON(enable_input_button), button_x, 0.5);
	gtk_box_pack_end(GTK_BOX(box), enable_input_button, TRUE, TRUE, 4);

	gtk_widget_set_sensitive(GTK_WIDGET(enable_input_button),
	                         setting_enable_mmap ? TRUE : FALSE);

	/* set active button before connecting signals */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_input_button),
	                             (setting_enable_inputs ? TRUE : FALSE));

	/* connect signal for backing store checkbutton */
	g_signal_connect(GTK_OBJECT(enable_input_button), "toggled",
	                 GTK_SIGNAL_FUNC(set_enable_inputs),
	                 (gpointer) enable_input_button);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_end(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *********** "JACK" tab *********** */
	frame = gtk_frame_new("");
	widget_set_backing_store(frame);

	label = gtk_label_new("JACK");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 10);
#else
	gtk_widget_set_size_request(label, 80, -1);
#endif

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_container_add(GTK_CONTAINER(frame), event);
	gtk_widget_show(frame);
	gtk_container_add(GTK_CONTAINER(config_notebook), frame);

	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(config_notebook), 3);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(config_notebook), page, FALSE);
#endif
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(config_notebook), page, label);

	page_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(event), page_hbox);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page_hbox), vbox, TRUE, TRUE, 16);

	/* separator */
	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

#if (MAX_PARTS > 1)
	/* *** JACK Output Mode *** label + 2 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("JACK Output Mode:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "Stereo Output");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1),
	                                                      "Multi-Channel Output");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	gtk_toggle_button_set_active((setting_jack_multi_out ?
	                              GTK_TOGGLE_BUTTON(button2) :
	                              GTK_TOGGLE_BUTTON(button1)), TRUE);

	/* connect signals for jack multi_out radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_jack_multi_out),
	                 (gpointer) 0);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_jack_multi_out),
	                 (gpointer) 1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);
#endif /* (MAX_PARTS > 1) */

	/* *** JACK Autoconnect Mode *** label + 2 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("JACK Connect Mode:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "Manual");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "Autoconnect");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	gtk_toggle_button_set_active((setting_jack_autoconnect ?
	                              GTK_TOGGLE_BUTTON(button2) :
	                              GTK_TOGGLE_BUTTON(button1)), TRUE);

	/* connect signals for jack autoconnect radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_jack_autoconnect),
	                 (gpointer) 0);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_jack_autoconnect),
	                 (gpointer) 1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *** JACK Transport Mode *** label + 3 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("JACK Transport Mode (experimental):");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "Off");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "Tempo Only");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	button3 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button2), "Tempo & LFOs");
	widget_set_custom_font(button3, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button3), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button3, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	active_button = NULL;
	switch (setting_jack_transport_mode) {
	case JACK_TRANSPORT_TEMPO:
		active_button = button2;
		break;
	case JACK_TRANSPORT_TNP:
		active_button = button3;
		break;
	case JACK_TRANSPORT_OFF:
	default:
		active_button = button1;
		break;
	}
	if (active_button != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_button), TRUE);
	}

	/* connect signals for jack autoconnect radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_jack_transport_mode),
	                 (gpointer) JACK_TRANSPORT_OFF);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_jack_transport_mode),
	                 (gpointer) JACK_TRANSPORT_TEMPO);
	g_signal_connect(GTK_OBJECT(button3), "toggled",
	                 GTK_SIGNAL_FUNC(set_jack_transport_mode),
	                 (gpointer) JACK_TRANSPORT_TNP);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_end(GTK_BOX(vbox), sep, TRUE, TRUE, 3);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_end(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *********** "Synth" tab *********** */
	frame = gtk_frame_new("");
	widget_set_backing_store(frame);

	label = gtk_label_new("Synth");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 10);
#else
	gtk_widget_set_size_request(label, 80, -1);
#endif

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_container_add(GTK_CONTAINER(frame), event);
	gtk_widget_show(frame);
	gtk_container_add(GTK_CONTAINER(config_notebook), frame);

	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(config_notebook), 4);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(config_notebook), page, FALSE);
#endif
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(config_notebook), page, label);

	page_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(event), page_hbox);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page_hbox), vbox, TRUE, TRUE, 16);

	/* separator */
	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	/* *** A4 Tuning Frequency *** hbox w/ label + spinbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("A4 Frequency:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	adj = gtk_adjustment_new(setting_tuning_freq, 200.0, 968.0, 1.0, 10.0, 0);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 3);
	widget_set_custom_font(spin, numeric_font_desc);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), FALSE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), setting_tuning_freq);
	gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(adj), "value_changed",
	                 GTK_SIGNAL_FUNC(set_tuning_freq),
	                 (gpointer) spin);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

#ifdef NONSTANDARD_HARMONICS
	/* harmonic base ratio: hbox w/ label + spinbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Harmonic Base Ratio:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	adj = gtk_adjustment_new(setting_harmonic_base, 1.2, 60.0, 1.0, 1.0, 0);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	widget_set_custom_font(spin, numeric_font_desc);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), FALSE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), setting_harmonic_base);
	gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(adj), "value_changed",
	                 GTK_SIGNAL_FUNC(set_harmonic_base),
	                 (gpointer) spin);

	/* harmonic steps per division: hbox w/ label + spinbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Harmonic Divisions:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	adj = gtk_adjustment_new(setting_harmonic_steps, 1.0, 60.0, 1.0, 1.0, 0);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), setting_harmonic_steps);
	gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(adj), "value_changed",
	                 GTK_SIGNAL_FUNC(set_harmonic_steps),
	                 (gpointer) spin);
	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

#endif /* NONSTANDARD_HARMONICS */

	/* Polyphony: hbox w/ label + spinbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Polyphony:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	adj = gtk_adjustment_new(setting_polyphony, 2, MAX_VOICES, 1, DEFAULT_POLYPHONY, 0);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	widget_set_custom_font(spin, numeric_font_desc);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), setting_polyphony);
	gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(adj), "value_changed",
	                 GTK_SIGNAL_FUNC(set_polyphony),
	                 (gpointer) spin);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* sample rate mode: label + 3 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Sample Rate Mode:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	/* normal / undersample / oversample  radiobuttons */
	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "Normal");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "Undersample");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	button3 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button2), "Oversample");
	widget_set_custom_font(button3, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button3), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button3, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	active_button = NULL;
	switch (setting_sample_rate_mode) {
	case SAMPLE_RATE_NORMAL:
		active_button = button1;
		break;
	case SAMPLE_RATE_UNDERSAMPLE:
		active_button = button2;
		break;
	case SAMPLE_RATE_OVERSAMPLE:
		active_button = button3;
		break;
	}
	if (active_button != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_button), TRUE);
	}

	/* connect signals for sample rate mode radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_sample_rate_mode),
	                 (gpointer) SAMPLE_RATE_NORMAL);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_sample_rate_mode),
	                 (gpointer) SAMPLE_RATE_UNDERSAMPLE);
	g_signal_connect(GTK_OBJECT(button3), "toggled",
	                 GTK_SIGNAL_FUNC(set_sample_rate_mode),
	                 (gpointer) SAMPLE_RATE_OVERSAMPLE);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* bank mem mode: label + 3 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Bank Memory Mode:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	/* Autosave / Warn / Protect radiobuttons */
	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	bank_autosave_button = gtk_radio_button_new_with_label(NULL, "Autosave");
	widget_set_custom_font(bank_autosave_button, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(bank_autosave_button), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), bank_autosave_button, FALSE, FALSE, 0);

	bank_warn_button =
		gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(bank_autosave_button), "Warn");
	widget_set_custom_font(bank_warn_button, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(bank_warn_button), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), bank_warn_button, FALSE, FALSE, 0);

	bank_protect_button =
		gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(bank_warn_button), "Protect");
	widget_set_custom_font(bank_protect_button, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(bank_protect_button), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), bank_protect_button, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	active_button = NULL;
	switch (setting_bank_mem_mode) {
	case BANK_MEM_AUTOSAVE:
		active_button = bank_autosave_button;
		break;
	case BANK_MEM_WARN:
		active_button = bank_warn_button;
		break;
	case BANK_MEM_PROTECT:
		active_button = bank_protect_button;
		break;
	}
	if (active_button != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_button), TRUE);
	}

	/* connect signals for mem mode radiobuttons */
	g_signal_connect(GTK_OBJECT(bank_autosave_button), "toggled",
	                 GTK_SIGNAL_FUNC(set_bank_mem_mode),
	                 (gpointer) BANK_MEM_AUTOSAVE);
	g_signal_connect(GTK_OBJECT(bank_warn_button), "toggled",
	                 GTK_SIGNAL_FUNC(set_bank_mem_mode),
	                 (gpointer) BANK_MEM_WARN);
	g_signal_connect(GTK_OBJECT(bank_protect_button), "toggled",
	                 GTK_SIGNAL_FUNC(set_bank_mem_mode),
	                 (gpointer) BANK_MEM_PROTECT);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_end(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* *********** "System" tab *********** */
	frame = gtk_frame_new("");
	widget_set_backing_store(frame);

	label = gtk_label_new("System");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 10);
#else
	gtk_widget_set_size_request(label, 80, -1);
#endif

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_container_add(GTK_CONTAINER(frame), event);
	gtk_widget_show(frame);
	gtk_container_add(GTK_CONTAINER(config_notebook), frame);

	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(config_notebook), 5);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(config_notebook), page, FALSE);
#endif
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(config_notebook), page, label);

	page_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(event), page_hbox);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page_hbox), vbox, TRUE, TRUE, 16);

	/* separator */
	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	/* midi thread priority: hbox w/ label + spinbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("MIDI Priority:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	min_prio = sched_get_priority_min(PHASEX_SCHED_POLICY);
	max_prio = sched_get_priority_max(PHASEX_SCHED_POLICY);

	adj = gtk_adjustment_new(setting_midi_priority, min_prio, max_prio, 1, 10, 0);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	widget_set_custom_font(spin, numeric_font_desc);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), setting_midi_priority);
	gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(adj), "value_changed",
	                 GTK_SIGNAL_FUNC(set_midi_priority),
	                 (gpointer) spin);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* engine thread priority: hbox w/ label + spinbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Engine Priority:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	min_prio = sched_get_priority_min(PHASEX_SCHED_POLICY);
	max_prio = sched_get_priority_max(PHASEX_SCHED_POLICY);

	adj = gtk_adjustment_new(setting_engine_priority, min_prio, max_prio, 1, 10, 0);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	widget_set_custom_font(spin, numeric_font_desc);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), setting_engine_priority);
	gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(adj), "value_changed",
	                 GTK_SIGNAL_FUNC(set_engine_priority),
	                 (gpointer) spin);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* audio thread priority: hbox w/ label + spinbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Audio Priority:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	min_prio = sched_get_priority_min(PHASEX_SCHED_POLICY);
	max_prio = sched_get_priority_max(PHASEX_SCHED_POLICY);

	adj = gtk_adjustment_new(setting_audio_priority, min_prio, max_prio, 1, 10, 0);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	widget_set_custom_font(spin, numeric_font_desc);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), setting_audio_priority);
	gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(adj), "value_changed",
	                 GTK_SIGNAL_FUNC(set_audio_priority),
	                 (gpointer) spin);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* realtime scheduling policy: label + 2 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("RT Scheduling Policy:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "SCHED_FIFO");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "SCHED_RR");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	gtk_toggle_button_set_active(((setting_sched_policy == SCHED_RR) ?
	                              GTK_TOGGLE_BUTTON(button2) :
	                              GTK_TOGGLE_BUTTON(button1)), TRUE);

	/* connect signals for sched policy radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_sched_policy),
	                 (gpointer) SCHED_FIFO);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_sched_policy),
	                 (gpointer) SCHED_RR);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_end(GTK_BOX(vbox), sep, TRUE, TRUE, 3);

	/* *********** "Interface" tab *********** */
	frame = gtk_frame_new("");
	widget_set_backing_store(frame);

	label = gtk_label_new("Interface");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 10);
#else
	gtk_widget_set_size_request(label, 80, -1);
#endif

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_container_add(GTK_CONTAINER(frame), event);
	gtk_widget_show(frame);
	gtk_container_add(GTK_CONTAINER(config_notebook), frame);

	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(config_notebook), 6);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(config_notebook), page, FALSE);
#endif
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(config_notebook), page, label);

	page_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(event), page_hbox);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page_hbox), vbox, TRUE, TRUE, 16);

	/* separator */
	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	/* fullscreen enable: hbox w/ label + checkbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Fullscreen Mode:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_check_button_new();
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_end(GTK_BOX(box), button1, TRUE, TRUE, 4);

	/* set active button before connecting signals */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button1),
	                             (setting_fullscreen ? TRUE : FALSE));

	/* connect signal for backing store checkbutton */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_fullscreen_mode),
	                 (gpointer) button1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

#ifdef SHOW_BACKING_STORE_SETTING
	/* backing store enable: hbox w/ label + checkbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Backing Store:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_check_button_new();
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_end(GTK_BOX(box), button1, TRUE, TRUE, 4);

	/* set active button before connecting signals */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button1),
	                             (setting_backing_store ? TRUE : FALSE));

	/* connect signal for backing store checkbutton */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_backing_store),
	                 (gpointer) button1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);
#endif

	/* layout mode: label + 2 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Window Layout:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "Notebook");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "One Page");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	button3 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button2), "WideScreen");
	widget_set_custom_font(button3, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button3), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button3, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	active_button = NULL;
	switch (setting_window_layout) {
	case LAYOUT_NOTEBOOK:
		active_button = button1;
		break;
	case LAYOUT_ONE_PAGE:
		active_button = button2;
		break;
	case LAYOUT_WIDESCREEN:
		active_button = button3;
		break;
	}

	if (active_button != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_button), TRUE);
	}

	/* connect signals for layout mode radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_window_layout),
	                 (gpointer) LAYOUT_NOTEBOOK);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_window_layout),
	                 (gpointer) LAYOUT_ONE_PAGE);
	g_signal_connect(GTK_OBJECT(button3), "toggled",
	                 GTK_SIGNAL_FUNC(set_window_layout),
	                 (gpointer) LAYOUT_WIDESCREEN);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* knob size: hbox w/ label + radiobuttons w/ labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Knob Size:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	box = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	hbox = box;

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "16x16");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "20x20");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	button3 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button2), "24x24");
	widget_set_custom_font(button3, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button3), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button3, FALSE, FALSE, 0);

	button4 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button3), "28x28");
	widget_set_custom_font(button4, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button4), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button4, FALSE, FALSE, 0);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button5 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button4), "32x32");
	widget_set_custom_font(button5, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button5), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button5, FALSE, FALSE, 0);

	button6 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button5), "36x36");
	widget_set_custom_font(button6, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button6), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button6, FALSE, FALSE, 0);

	button7 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button6), "40x40");
	widget_set_custom_font(button7, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button7), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button7, FALSE, FALSE, 0);

	button8 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button7), "44x44");
	widget_set_custom_font(button8, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button8), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button8, FALSE, FALSE, 0);

	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button9 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button8), "48x48");
	widget_set_custom_font(button9, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button9), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button9, FALSE, FALSE, 0);

	button10 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button9), "52x52");
	widget_set_custom_font(button10, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button10), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button10, FALSE, FALSE, 0);

	button11 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button10), "56x56");
	widget_set_custom_font(button11, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button11), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button11, FALSE, FALSE, 0);

	button12 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button11), "60x60");
	widget_set_custom_font(button12, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button12), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button12, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	active_button = NULL;
	switch (setting_knob_size) {
	case KNOB_SIZE_16x16:
		active_button = button1;
		break;
	case KNOB_SIZE_20x20:
		active_button = button2;
		break;
	case KNOB_SIZE_24x24:
		active_button = button3;
		break;
	case KNOB_SIZE_28x28:
		active_button = button4;
		break;
	case KNOB_SIZE_32x32:
		active_button = button5;
		break;
	case KNOB_SIZE_36x36:
		active_button = button6;
		break;
	case KNOB_SIZE_40x40:
		active_button = button7;
		break;
	case KNOB_SIZE_44x44:
		active_button = button8;
		break;
	case KNOB_SIZE_48x48:
		active_button = button9;
		break;
	case KNOB_SIZE_52x52:
		active_button = button10;
		break;
	case KNOB_SIZE_56x56:
		active_button = button11;
		break;
	case KNOB_SIZE_60x60:
		active_button = button12;
		break;
	}
	if (active_button != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_button), TRUE);
	}

	/* connect signals for layout mode radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_16x16);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_20x20);
	g_signal_connect(GTK_OBJECT(button3), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_24x24);
	g_signal_connect(GTK_OBJECT(button4), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_28x28);
	g_signal_connect(GTK_OBJECT(button5), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_32x32);
	g_signal_connect(GTK_OBJECT(button6), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_36x36);
	g_signal_connect(GTK_OBJECT(button7), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_40x40);
	g_signal_connect(GTK_OBJECT(button8), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_44x44);
	g_signal_connect(GTK_OBJECT(button9), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_48x48);
	g_signal_connect(GTK_OBJECT(button10), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_52x52);
	g_signal_connect(GTK_OBJECT(button11), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_56x56);
	g_signal_connect(GTK_OBJECT(button12), "toggled",
	                 GTK_SIGNAL_FUNC(set_knob_size),
	                 (gpointer) KNOB_SIZE_60x60);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* refresh interval: hbox w/ label + spinbutton */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Refresh Interval (msec):");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	adj = gtk_adjustment_new(setting_refresh_interval, 5, 250, 5, 20, 0);

	box = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	spin = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 0, 0);
	widget_set_custom_font(spin, numeric_font_desc);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin), GTK_UPDATE_IF_VALID);
	gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), setting_refresh_interval);
	gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(adj), "value_changed",
	                 GTK_SIGNAL_FUNC(set_refresh_interval),
	                 (gpointer) spin);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_end(GTK_BOX(vbox), sep, TRUE, TRUE, 3);

	/* *********** "Theme" tab *********** */
	frame = gtk_frame_new("");
	widget_set_backing_store(frame);

	label = gtk_label_new("Theme");
	widget_set_custom_font(label, phasex_font_desc);
	widget_set_backing_store(label);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_label_set_width_chars(GTK_LABEL(label), 10);
#else
	gtk_widget_set_size_request(label, 80, -1);
#endif

	event = gtk_event_box_new();
	widget_set_backing_store(event);
	gtk_container_add(GTK_CONTAINER(frame), event);
	gtk_widget_show(frame);
	gtk_container_add(GTK_CONTAINER(config_notebook), frame);

	page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(config_notebook), 7);
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(config_notebook), page, FALSE);
#endif
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(config_notebook), page, label);

	page_hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(event), page_hbox);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(page_hbox), vbox, TRUE, TRUE, 16);

	/* separator */
	label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	/* theme: label + 4 radiobuttons w/labels */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("GTK Theme:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	/* dark / light / system / custom radiobuttons */
	box = gtk_vbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 4);

	button1 = gtk_radio_button_new_with_label(NULL, "Dark");
	widget_set_custom_font(button1, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button1), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button1, FALSE, FALSE, 0);

	button2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "Light");
	widget_set_custom_font(button2, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button2), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button2, FALSE, FALSE, 0);

	button3 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "System");
	widget_set_custom_font(button3, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button3), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button3, FALSE, FALSE, 0);

	button4 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button1), "Custom");
	widget_set_custom_font(button4, phasex_font_desc);
	gtk_button_set_alignment(GTK_BUTTON(button4), button_x, 0.5);
	gtk_box_pack_start(GTK_BOX(box), button4, FALSE, FALSE, 0);

	/* set active button before connecting signals */
	active_button = NULL;
	switch (setting_theme) {
	case PHASEX_THEME_DARK:
		active_button = button1;
		break;
	case PHASEX_THEME_LIGHT:
		active_button = button2;
		break;
	case PHASEX_THEME_SYSTEM:
		active_button = button3;
		break;
	case PHASEX_THEME_CUSTOM:
		active_button = button4;
		break;
	}
	if (active_button != NULL) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_button), TRUE);
	}

	/* connect signals for theme radiobuttons */
	g_signal_connect(GTK_OBJECT(button1), "toggled",
	                 GTK_SIGNAL_FUNC(set_theme),
	                 (gpointer) PHASEX_THEME_DARK);
	g_signal_connect(GTK_OBJECT(button2), "toggled",
	                 GTK_SIGNAL_FUNC(set_theme),
	                 (gpointer) PHASEX_THEME_LIGHT);
	g_signal_connect(GTK_OBJECT(button3), "toggled",
	                 GTK_SIGNAL_FUNC(set_theme),
	                 (gpointer) PHASEX_THEME_SYSTEM);
	g_signal_connect(GTK_OBJECT(button4), "toggled",
	                 GTK_SIGNAL_FUNC(set_theme),
	                 (gpointer) PHASEX_THEME_CUSTOM);

	custom_theme_button = button4;

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* custom theme: label + file chooser button */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Custom Theme:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	button1 = gtk_file_chooser_button_new("Select a theme", GTK_FILE_CHOOSER_ACTION_OPEN);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_file_chooser_button_set_width_chars(GTK_FILE_CHOOSER_BUTTON(button1), 11);
#endif

	if (setting_custom_theme != NULL) {
		if (theme_dir != NULL) {
			free(theme_dir);
		}
		theme_dir = dirname(strdup(setting_custom_theme));
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(button1), theme_dir);
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(button1), setting_custom_theme);
	}
	else {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(button1), g_get_home_dir());
	}
	gtk_box_pack_start(GTK_BOX(hbox), button1, TRUE, TRUE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(button1), "file_set",
	                 GTK_SIGNAL_FUNC(set_custom_theme),
	                 (gpointer) button1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* knob dir: label + file chooser button */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Knob Directory:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	button1 = gtk_file_chooser_button_new("Select a knob image directory",
	                                      GTK_FILE_CHOOSER_ACTION_OPEN);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_file_chooser_button_set_width_chars(GTK_FILE_CHOOSER_BUTTON(button1), 11);
#endif

#if GTK_CHECK_VERSION(2, 4, 0)
	gtk_file_chooser_set_action(GTK_FILE_CHOOSER(button1), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
#endif

	if (setting_knob_dir != NULL) {
		if (pixmap_dir != NULL) {
			free(pixmap_dir);
		}
		tmp_dir = strdup(setting_knob_dir);
		pixmap_dir = dirname(tmp_dir);
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(button1), pixmap_dir);
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(button1), setting_knob_dir);
		free(tmp_dir);
		tmp_dir = NULL;
		pixmap_dir = NULL;
	}
	else {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(button1), g_get_home_dir());
	}
	gtk_box_pack_start(GTK_BOX(hbox), button1, TRUE, TRUE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(button1), "file_set",
	                 GTK_SIGNAL_FUNC(set_knob_dir),
	                 (gpointer) button1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* detent knob dir: label + file chooser button */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Detent Knob Directory:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	button1 = gtk_file_chooser_button_new("Select a knob image directory",
	                                      GTK_FILE_CHOOSER_ACTION_OPEN);
#if GTK_CHECK_VERSION(2, 6, 0)
	gtk_file_chooser_button_set_width_chars(GTK_FILE_CHOOSER_BUTTON(button1), 11);
#endif

#if GTK_CHECK_VERSION(2, 4, 0)
	gtk_file_chooser_set_action(GTK_FILE_CHOOSER(button1), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
#endif

	if (setting_detent_knob_dir != NULL) {
		if (pixmap_dir != NULL) {
			free(pixmap_dir);
		}
		tmp_dir = strdup(setting_detent_knob_dir);
		pixmap_dir = dirname(tmp_dir);
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(button1), pixmap_dir);
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(button1), setting_detent_knob_dir);
		free(tmp_dir);
		tmp_dir = NULL;
		pixmap_dir = NULL;
	}
	else {
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(button1), g_get_home_dir());
	}
	gtk_box_pack_start(GTK_BOX(hbox), button1, TRUE, TRUE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(button1), "file_set",
	                 GTK_SIGNAL_FUNC(set_detent_knob_dir),
	                 (gpointer) button1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* setting preview text for the dialog seems to be impossible when */
	/* realized by a GtkFontButton... oh well... */

	/* font selection */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Font:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	button1 = gtk_font_button_new_with_font(setting_font);
	gtk_font_button_set_title(GTK_FONT_BUTTON(button1), "Select a Font");
	gtk_font_button_set_use_font(GTK_FONT_BUTTON(button1), TRUE);
	gtk_font_button_set_use_size(GTK_FONT_BUTTON(button1), 11);
	//gtk_font_selection_set_preview_text (GTK_FONT_SELECTION (GTK_FONT_BUTTON (button1)),
	//                   "ABCDEFG hijk LMNOPRS tuvwxyz 0123456789");
	gtk_box_pack_start(GTK_BOX(hbox), button1, TRUE, TRUE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(button1), "font_set",
	                 GTK_SIGNAL_FUNC(set_font),
	                 (gpointer) button1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* numeric font selection */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Title Font:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	button1 = gtk_font_button_new_with_font(setting_title_font);
	gtk_font_button_set_title(GTK_FONT_BUTTON(button1), "Select a Title Font");
	gtk_font_button_set_use_font(GTK_FONT_BUTTON(button1), TRUE);
	gtk_font_button_set_use_size(GTK_FONT_BUTTON(button1), 11);
	//gtk_font_selection_set_preview_text (GTK_FONT_SELECTION (button1),
	//                   "1123581321345589144233377610987");
	gtk_box_pack_start(GTK_BOX(hbox), button1, TRUE, TRUE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(button1), "font_set",
	                 GTK_SIGNAL_FUNC(set_title_font),
	                 (gpointer) button1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	/* numeric font selection */
	hbox = gtk_hbox_new(TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 4);

	label = gtk_label_new("Numeric Font:");
	widget_set_custom_font(label, phasex_font_desc);
	gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);

	button1 = gtk_font_button_new_with_font(setting_numeric_font);
	gtk_font_button_set_title(GTK_FONT_BUTTON(button1), "Select a Numeric Font");
	gtk_font_button_set_use_font(GTK_FONT_BUTTON(button1), TRUE);
	gtk_font_button_set_use_size(GTK_FONT_BUTTON(button1), 11);
	//gtk_font_selection_set_preview_text (GTK_FONT_SELECTION (button1),
	//                   "1123581321345589144233377610987");
	gtk_box_pack_start(GTK_BOX(hbox), button1, TRUE, TRUE, 4);

	/* connect signals */
	g_signal_connect(GTK_OBJECT(button1), "font_set",
	                 GTK_SIGNAL_FUNC(set_numeric_font),
	                 (gpointer) button1);

	/* separator */
	sep = gtk_hseparator_new();
	gtk_box_pack_end(GTK_BOX(vbox), sep, FALSE, FALSE, 3);

	gtk_widget_show(page_hbox);

	/* connect signals for notebook */
	page_num = config_page_num;
	g_signal_connect(G_OBJECT(config_notebook), "switch-page",
	                 GTK_SIGNAL_FUNC(on_config_notebook_page_switch),
	                 (gpointer)((long int) page_num));

	/* connect signals for dialog window */
	g_signal_connect(G_OBJECT(config_dialog), "response",
	                 GTK_SIGNAL_FUNC(close_config_dialog),
	                 (gpointer) 0);
	g_signal_connect(G_OBJECT(config_dialog), "destroy",
	                 GTK_SIGNAL_FUNC(close_config_dialog),
	                 (gpointer) - 1);

	/* open to page based on hint left before restart */
	config_page_num = page_num;
	gtk_notebook_set_current_page(GTK_NOTEBOOK(config_notebook), page_num);
}
#endif /* ENABLE_CONFIG_DIALOG */
