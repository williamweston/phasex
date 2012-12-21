/*****************************************************************************
 *
 * settings.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2012 William Weston <whw@linuxmail.org>
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
#ifndef _PHASEX_SETTINGS_H_
#define _PHASEX_SETTINGS_H_

#include <gtk/gtk.h>
#include "phasex.h"
#include "timekeeping.h"


#define MAXIMIZE_OFF        0
#define MAXIMIZE_ON         1

#define FULLSCREEN_OFF      0
#define FULLSCREEN_ON       1

#define LAYOUT_ONE_PAGE     0
#define LAYOUT_NOTEBOOK     1
#define LAYOUT_WIDESCREEN   2

#define PHASEX_THEME_DARK   0
#define PHASEX_THEME_LIGHT  1
#define PHASEX_THEME_SYSTEM 2
#define PHASEX_THEME_CUSTOM 3

#define KNOB_SIZE_SMALL     0
#define KNOB_SIZE_MEDIUM    1
#define KNOB_SIZE_LARGE     2

#define KNOB_SIZE_16x16     0
#define KNOB_SIZE_20x20     1
#define KNOB_SIZE_24x24     2
#define KNOB_SIZE_28x28     3
#define KNOB_SIZE_32x32     4
#define KNOB_SIZE_36x36     5
#define KNOB_SIZE_40x40     6
#define KNOB_SIZE_44x44     7
#define KNOB_SIZE_48x48     8
#define KNOB_SIZE_52x52     9
#define KNOB_SIZE_56x56     10
#define KNOB_SIZE_60x60     11
#define KNOB_SIZE_64x64     12

#define VIEW_800x600        0
#define VIEW_960x600        1
#define VIEW_1024x768       2
#define VIEW_1280x960       3
#define VIEW_1440x900       4
#define VIEW_1600x1200      5
#define VIEW_1680x1050      6
#define VIEW_1920x1080      7


/* Current (most recently loaded) config file */
extern char                         *config_file;

/* Status flag variables */
extern int                          config_is_open;
extern int                          config_page_num;
extern int                          no_config;
extern int                          config_changed;
extern int                          sample_rate_changed;

/* MIDI settings */
extern int                          setting_midi_driver;
extern char                         *setting_oss_midi_device;
extern char                         *setting_generic_midi_device;
extern char                         *setting_alsa_raw_midi_device;
extern char                         *setting_alsa_seq_port;
extern int                          setting_ignore_midi_program_change;
extern timecalc_t                   setting_audio_phase_lock;
extern timecalc_t                   setting_clock_constant;

/* Audio settings */
extern int                          setting_audio_driver;
extern unsigned int                 setting_buffer_latency;

/* ALSA PCM settings */
extern char                         *setting_alsa_pcm_device;
extern int                          setting_sample_rate;
extern unsigned int                 setting_buffer_period_size;
extern int                          setting_force_16bit;
extern int                          setting_enable_mmap;
extern int                          setting_enable_inputs;

/* JACK settings */
extern int                          setting_jack_multi_out;
extern int                          setting_jack_autoconnect;
extern int                          setting_jack_transport_mode;

/* Synth settings */
#if (ARCH_BITS == 32)
extern float                        setting_tuning_freq;
#endif
#if (ARCH_BITS == 64)
extern double                       setting_tuning_freq;
#endif
#ifdef NONSTANDARD_HARMONICS
extern double                       setting_harmonic_base;
extern double                       setting_harmonic_steps;
#endif
extern int                          setting_polyphony;
extern int                          setting_sample_rate_mode;
extern int                          setting_bank_mem_mode;

/* Interface settings */
extern int                          setting_fullscreen;
extern int                          setting_backing_store;
extern int                          setting_window_layout;
extern int                          setting_knob_size;
extern int                          setting_refresh_interval;

/* System settings */
extern int                          setting_audio_priority;
extern int                          setting_midi_priority;
extern int                          setting_engine_priority;
extern int                          setting_sched_policy;

/* Theme settings */
extern int                          setting_theme;
extern char                         *setting_custom_theme;
extern char                         *setting_knob_dir;
extern char                         *setting_detent_knob_dir;
extern char                         *setting_font;
extern char                         *setting_title_font;
extern char                         *setting_numeric_font;

/* Hidden settings */
extern int                          setting_maximize;
extern char                         *setting_midimap_file;

/* Dialog widgets that we need to keep track of */
extern GtkWidget                    *config_dialog;
extern GtkWidget                    *custom_theme_button;
extern GtkWidget                    *config_notebook;
extern GtkWidget                    *bank_autosave_button;
extern GtkWidget                    *bank_warn_button;
extern GtkWidget                    *bank_protect_button;
extern GtkWidget                    *jack_midi_button;
extern GtkWidget                    *enable_input_button;
extern GtkWidget                    *audio_status_label;
extern GtkObject                    *audio_phase_lock_adj;

/* Internal font descriptions, based on settings */
extern PangoFontDescription         *phasex_font_desc;
extern PangoFontDescription         *title_font_desc;
extern PangoFontDescription         *numeric_font_desc;

/* Status message buffer, filled in by driver code */
extern char                         audio_driver_status_msg[256];

/* Strings for gui components */
extern char                         *sample_rate_mode_names[];
extern char                         *bank_mem_mode_names[];
extern char                         *layout_names[];
extern char                         *theme_names[];


int read_settings(char *filename);
int save_settings(char *filename);

/* MIDI settings */
void set_midi_driver(GtkWidget *widget, gpointer data);
void set_oss_midi_device(GtkWidget *UNUSED(widget), gpointer UNUSED(data));
void set_generic_midi_device(GtkWidget *UNUSED(widget), gpointer UNUSED(data));
void set_alsa_raw_midi_device(GtkWidget *UNUSED(widget), gpointer UNUSED(data));
void set_alsa_seq_port(GtkWidget *UNUSED(widget), gpointer UNUSED(data));
void set_midi_audio_phase_lock(GtkWidget *UNUSED(widget), gpointer data);
void on_restart_midi(GtkWidget *widget, gpointer UNUSED(data));

/* Audio settings */
void set_audio_driver(GtkWidget *widget, gpointer data);
void set_alsa_pcm_device(GtkWidget *UNUSED(widget), gpointer UNUSED(data));
void set_sample_rate(GtkWidget *widget, gpointer data);
void set_buffer_latency(GtkWidget *UNUSED(widget), gpointer data);
void set_force_16bit(GtkWidget *UNUSED(widget), gpointer data);
void set_enable_mmap(GtkWidget *UNUSED(widget), gpointer data);
void set_enable_inputs(GtkWidget *UNUSED(widget), gpointer data);
void on_restart_audio(GtkWidget *widget, gpointer UNUSED(data));

/* Synth settings */
void set_tuning_freq(GtkWidget *UNUSED(widget), gpointer data);
void set_polyphony(GtkWidget *UNUSED(widget), gpointer data);
#ifdef NONSTANDARD_HARMONICS
void set_harmonic_base(GtkWidget *UNUSED(widget), gpointer data);
void set_harmonic_steps(GtkWidget *UNUSED(widget), gpointer data);
#endif
void set_sample_rate_mode(GtkWidget *widget, gpointer data);
void set_bank_mem_mode(GtkWidget *widget, gpointer data, GtkWidget *UNUSED(widget2));

/* System settings */
void set_jack_transport_mode(GtkWidget *widget, gpointer data);
void set_jack_autoconnect(GtkWidget *widget, gpointer data, GtkWidget *widget2);
void set_jack_multi_out(GtkWidget *widget, gpointer data, GtkWidget *widget2);
void set_buffer_period_size(GtkWidget *widget, gpointer data);
void set_audio_priority(GtkWidget *UNUSED(widget), gpointer data);
void set_midi_priority(GtkWidget *UNUSED(widget), gpointer data);
void set_engine_priority(GtkWidget *UNUSED(widget), gpointer data);
void set_sched_policy(GtkWidget *widget, gpointer data);
void set_midi_channel(GtkWidget *widget, gpointer data, GtkWidget *UNUSED(widget2));

/* Interface settings */
void set_fullscreen_mode(GtkWidget *widget, gpointer UNUSED(data1), gpointer data2);
void set_maximize_mode(GtkWidget *UNUSED(widget), gpointer data);
#ifdef SHOW_BACKING_STORE_SETTING
void set_backing_store(GtkWidget *UNUSED(widget), gpointer data);
#endif
void set_window_layout(GtkWidget *widget, gpointer data, GtkWidget *widget2);
void set_desktop_view(GtkWidget *UNUSED(widget), gpointer data, GtkWidget *UNUSED(widget2));
void set_knob_size(GtkWidget *widget, gpointer data);
void set_refresh_interval(GtkWidget *UNUSED(widget), gpointer data);

/* Theme settings */
void set_theme(GtkWidget *widget, gpointer data);
void set_custom_theme(GtkWidget *widget, gpointer UNUSED(data));
void set_theme_env(void);
void set_knob_dir(GtkWidget *widget, gpointer UNUSED(data));
void set_detent_knob_dir(GtkWidget *widget, gpointer UNUSED(data));
void set_font(GtkFontButton *button, GtkWidget *UNUSED(widget));
void set_title_font(GtkFontButton *button, GtkWidget *UNUSED(widget));
void set_numeric_font(GtkFontButton *button, GtkWidget *UNUSED(widget));

#ifdef ENABLE_CONFIG_DIALOG
void close_config_dialog(GtkWidget *widget, gpointer data);
void on_config_notebook_page_switch(GtkNotebook      *UNUSED(notebook),
                                    GtkNotebookPage  *UNUSED(page),
                                    gint              page_num,
                                    gpointer          UNUSED(user_data));
void show_config_dialog(void);
void create_config_dialog(void);
#endif


#endif /* _PHASEX_SETTINGS_H_ */
