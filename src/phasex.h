/*****************************************************************************
 *
 * phasex.h
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
 *****************************************************************************/
#ifndef _PHASEX_H_
#define _PHASEX_H_

#include <pthread.h>
#include <sys/param.h>
#include "../config.h"
#include "driver.h"


/*****************************************************************************
 *
 * Tunable Compile Time Constants
 *
 *****************************************************************************/

/* Enable debug output (test thoroughly if disabling!!!).  For now,
   all error / warn / debug output is handled by debug thread. */
#define ENABLE_DEBUG

/* Set default cpu power level.  For most builds, this should be set with
     '../configure --enable-cpu-power=X' where X is:
   1 for the oldest, painfully slow dinosaurs.
   2 for older hardware still capable of running current linux distros.
   3 for the newer multi-core and other incredibly fast chips.
   4 for switching almost entirely over to 64-bit math. */
#if !defined(PHASEX_CPU_POWER)
# define PHASEX_CPU_POWER               2
#endif

/* Type to use for (almost) all floating point math. */
#if (PHASEX_CPU_POWER == 4)
# if (ARCH_BITS == 64))
typedef double sample_t;
# else
#  warn ***** Only 64-bit builds are supported with PHASEX_CPU_POWER == 4
#  warn ***** Reverting to PHASEX_CPU_POWER == 3
#  undef PHASEX_CPU_POWER
#  define PHASEX_CPU_POWER 3
typedef float sample_t;
# endif
#else
typedef float sample_t;
#endif

/* Default realtime thread priorities. */
/* These can be changed at runtime in the preferences. */
#define MIDI_THREAD_PRIORITY            68
#define ENGINE_THREAD_PRIORITY          67
#define AUDIO_THREAD_PRIORITY           66

/* Default realtime scheduling policy for midi and engine threads. */
#define PHASEX_SCHED_POLICY             SCHED_FIFO

/* Base tuning frequency, in Hz. */
/* Use the -t command line flag to override at runtime. */
#define A4FREQ                          440.0

/* Number of micro-tuning steps between halfsteps. */
/* A value of 120 seems to provide good harmonics. */
#define TUNING_RESOLUTION               120
#define F_TUNING_RESOLUTION             120.0

/* Default timer interval for visual parameter refresh (in msec). */
#if (PHASEX_CPU_POWER == 1)
# define DEFAULT_REFRESH_INTERVAL       120
#endif
#if (PHASEX_CPU_POWER == 2)
# define DEFAULT_REFRESH_INTERVAL       75
#endif
#if (PHASEX_CPU_POWER == 3)
# define DEFAULT_REFRESH_INTERVAL       50
#endif
#if (PHASEX_CPU_POWER == 4)
# define DEFAULT_REFRESH_INTERVAL       40
#endif

/* Factor by which the filter is oversampled.  Increase for richer
   harmonics and more stability at high resonance.  Decrease to save
   CPU cycles or for thinner harmonics.  6x oversampling seems to
   provide good harmonics at reasonable cost.  */
#if (PHASEX_CPU_POWER == 1)
# define FILTER_OVERSAMPLE              2
# define F_FILTER_OVERSAMPLE            2.0
#endif
#if (PHASEX_CPU_POWER == 2)
# define FILTER_OVERSAMPLE              4
# define F_FILTER_OVERSAMPLE            4.0
#endif
#if (PHASEX_CPU_POWER == 3)
# define FILTER_OVERSAMPLE              6
# define F_FILTER_OVERSAMPLE            6.0
#endif
#if (PHASEX_CPU_POWER == 4)
# define FILTER_OVERSAMPLE              6
# define F_FILTER_OVERSAMPLE            6.0
#endif

/* Number of samples for a single wave period in the osc wave table.
   Larger values sound cleaner, but burn more memory.  Smaller values
   sound grittier, but burn less memory. On some CPUs, performance
   degrades with larger lookup tables.  Values with more small prime
   factors provide better harmonics than large powers of 2.  Try these
   values first: */
/* 24576 28800 32400 44100 48000 49152 57600 64800 75600 73728 86400
   88200 96000 97200 98304 115200 129600 132300 144000 162000
   176400 */
#if (PHASEX_CPU_POWER == 1)
# define WAVEFORM_SIZE                  32400
# define F_WAVEFORM_SIZE                32400.0
#endif
#if (PHASEX_CPU_POWER == 2)
# define WAVEFORM_SIZE                  32400
# define F_WAVEFORM_SIZE                32400.0
#endif
#if (PHASEX_CPU_POWER == 3)
# define WAVEFORM_SIZE                  50820
# define F_WAVEFORM_SIZE                50820.0
#endif
#if (PHASEX_CPU_POWER == 4)
# define WAVEFORM_SIZE                  50820
# define F_WAVEFORM_SIZE                50820.0
#endif

/* Size of the lookup table for logarithmic envelope curves.  Envelope
   curve table size shouldn't drop much below 14400. */
#if (PHASEX_CPU_POWER == 1)
# define ENV_CURVE_SIZE                 28800
# define F_ENV_CURVE_SIZE               28800.0
#endif
#if (PHASEX_CPU_POWER == 2)
# define ENV_CURVE_SIZE                 32400
# define F_ENV_CURVE_SIZE               32400.0
#endif
#if (PHASEX_CPU_POWER == 3)
# define ENV_CURVE_SIZE                 57600
# define F_ENV_CURVE_SIZE               57600.0
#endif
#if (PHASEX_CPU_POWER == 4)
# define ENV_CURVE_SIZE                 57600
# define F_ENV_CURVE_SIZE               57600.0
#endif

/* Total polyphony: As a general rule of thumb, this should be
   somewhat lower than one voice per 100 MHz CPU power.  YMMV. */
#if (PHASEX_CPU_POWER == 1)
# define DEFAULT_POLYPHONY              6
#endif
#if (PHASEX_CPU_POWER == 2)
# define DEFAULT_POLYPHONY              8
#endif
#if (PHASEX_CPU_POWER == 3)
# define DEFAULT_POLYPHONY              12
#endif
#if (PHASEX_CPU_POWER == 4)
# define DEFAULT_POLYPHONY              12
#endif

/* Max concurrent parts/patches, set by ../configure --enable-parts=N
   This determines number of parts and thus number of engine
   threads. */
#define MAX_PARTS                       NUM_PARTS
#ifdef ALL_PARTS
# undef ALL_PARTS
#endif
#define ALL_PARTS                       MAX_PARTS

/* Turn on experimental hermite interpolation on chorus buffer reads
   and wavetable lookups.  This is somewhat expensive, so don't enable
   for extremely slow CPUs. */
#if PHASEX_CPU_POWER >= 2
# define INTERPOLATE_CHORUS
# define INTERPOLATE_WAVETABLE_LOOKUPS
#endif

/* Smallest useful gain value.  For now, assume we can't hear anything
   below 20 leading zero bits (anything < -120dB). */
#define MINIMUM_GAIN                    (1.0 / (sample_t) (1 << 20) )
#define D_MINIMUM_GAIN                  (1.0 / (double) (1 << 20) )

/* User files and directories for configs, patches, midimaps, etc.
   Since these are not full paths, any new code should reference the
   globals provided at the bottom of this file instead. */
#define USER_DATA_DIR                   ".phasex"
#define USER_PATCH_DIR                  "patches"
#define OLD_USER_PATCH_DIR              "user-patches"
#define USER_MIDIMAP_DIR                "midimaps"
#define USER_SESSION_DIR                "sessions"
#define USER_BANK_FILE                  "patchbank"
#define USER_SESSION_BANK_FILE          "sessionbank"
#define USER_PATCHDUMP_FILE             "patchdump"
#define USER_MIDIMAP_DUMP_FILE          "midimapdump"
#define USER_SESSION_DUMP_DIR           "_current_"
#define USER_CONFIG_FILE                "phasex.cfg"
#define SYS_DEFAULT_PATCH               "phasex-default.phx"

/* Default knob image directories. */
#define DEFAULT_LIGHT_KNOB_DIR          "Light"
#define DEFAULT_LIGHT_DETENT_KNOB_DIR   "Light"
#define DEFAULT_DARK_KNOB_DIR           "Dark"
#define DEFAULT_DARK_DETENT_KNOB_DIR    "Dark"

/* Default fonts (overridden with font settings). */
#define DEFAULT_ALPHA_FONT              "Sans 7"
#define DEFAULT_TITLE_FONT              "Sans 7"
#define DEFAULT_NUMERIC_FONT            "Monospace 7"

/* Default GTK theme engine (change this in misc/gtkenginerc as well). */
#define PHASEX_GTK_ENGINE               "nodoka"


/* Misc options */

/* apply user selectable fonts to menus too? */
#define CUSTOM_FONTS_IN_MENUS


/* Audio options */

/* Load sampled waveforms (juno*, vox*, and analog_sq) on startup. */
#define ENABLE_SAMPLE_LOADING

/* Per part DC rejection filter */
//define ENABLE_DC_REJECTION_FILTER

/* Audio output defaults, mostly for first startup w/ no config file.
   These options now can be set in config file and/or command line. */
//define DEFAULT_AUDIO_DRIVER            AUDIO_DRIVER_ALSA_PCM
#define DEFAULT_AUDIO_DRIVER            AUDIO_DRIVER_JACK
#define DEFAULT_MIDI_DRIVER             MIDI_DRIVER_ALSA_SEQ

/* Build with audio input code enabled (ALSA and JACK). */
#define ENABLE_INPUTS

/* Define ALSA_SCAN_SUBDEVICES to scan individual audio / MIDI
   subdevices.  In practice, almost everyone ignores the subdevices,
   this should not be necessary. */
//define ALSA_SCAN_SUBDEVICES

/* Phase of MIDI period for synchronizing audio buffer processing period starts. */
#define DEFAULT_AUDIO_PHASE_LOCK        0.9375

/* max number of samples to use in the 8 period ringbuffer. */
/* must be a power of 2, and must handle at least 8 periods of >= 1024 */
#define PHASEX_MAX_BUFSIZE              16384   /* up to 8 periods of 2048 */
#define DEFAULT_BUFFER_SIZE             (DEFAULT_BUFFER_PERIOD_SIZE * DEFAULT_BUFFER_PERIODS)
#define DEFAULT_BUFFER_PERIOD_SIZE      256
#define DEFAULT_BUFFER_PERIODS          8
#define DEFAULT_LATENCY_PERIODS         2
#define DEFAULT_SAMPLE_RATE             48000


/* Raw MIDI options */

/* Generic Raw MIDI and ALSA Raw MIDI are stable.
   These should be enabled for most builds. */
#define ENABLE_RAWMIDI_ALSA_RAW
#define ENABLE_RAWMIDI_GENERIC

/* OSS code is old and completely untested in its current form. */
//define ENABLE_RAWMIDI_OSS

#define RAWMIDI_DEBUG
#define RAWMIDI_ALSA_MULTI_BYTE_IO
#define RAWMIDI_ALSA_NONBLOCK
#define RAWMIDI_GENERIC_NONBLOCK
#define RAWMIDI_USE_POLL
#define RAWMIDI_FLUSH_ON_START
#define RAWMIDI_FLUSH_NONBLOCK

/* Default raw MIDI device to use when none is specified. */
#define RAWMIDI_ALSA_DEVICE             "hw:2,0"
#define RAWMIDI_RAW_DEVICE              "/dev/midi"
#define RAWMIDI_OSS_DEVICE              "/dev/sequencer"


/* Most builds should keep this enabled.  This is provided for
   situations when the config dialog is unneeded and unwanted.
   Config file and command line options are always supported! */
#define ENABLE_CONFIG_DIALOG


/*****************************************************************************
 *
 * Leave disabled unless actively working on affected code:
 *
 *****************************************************************************/

/* Incomplete features */
//define NONSTANDARD_HARMONICS
//define ENABLE_JACK_LATENCY_CALLBACK
//define ENABLE_JACK_GRAPH_ORDER_CALLBACK

/* Depracated features */

/* On all but ancient X servers, enabling backing store */
/* can actually hurt GUI performance.  The backing store */
/* code will be removed soon unless anyone speaks up. */
//define SHOW_BACKING_STORE_SETTING
//define ENABLE_BACKING_STORE


/*****************************************************************************
 *
 * Non-Tunable Constants
 *
 *****************************************************************************/

/* Number of per-voice oscs and per-part lfos */
#define NUM_OSCS                        4
#define NUM_LFOS                        4

/* The Off/Velocity LFOs and Oscilators get their own slot */
#define LFO_OFF                         NUM_LFOS
#define MOD_OFF                         NUM_OSCS
#define LFO_VELOCITY                    NUM_LFOS
#define MOD_VELOCITY                    NUM_OSCS

/* modulator types */
#define MOD_TYPE_OSC                    0
#define MOD_TYPE_OSC_LATCH              1
#define MOD_TYPE_LFO                    2
#define MOD_TYPE_VELOCITY               3

/* Hard polyphony limit (per voice) */
#define MAX_VOICES                      16

/* three options for sample rate: 1, 1/2, or 2 */
#define SAMPLE_RATE_NORMAL              0
#define SAMPLE_RATE_UNDERSAMPLE         1
#define SAMPLE_RATE_OVERSAMPLE          2

/* Update NUM_WAVEFORMS after adding new waveforms */
#define NUM_WAVEFORMS                   28

/* Maximum delay times, in samples, must be powers of 2, and large
   enough to function at all sample rates. */
#define DELAY_MAX                       524288
#define CHORUS_MAX                      8192
#define CHORUS_MASK                     (CHORUS_MAX - 1)

/* Even-multiple octaves work best. */
/* Must be able to handle patch transpose + part transpose + pitchbend + fm */
#define FREQ_SHIFT_HALFSTEPS            384
#define F_FREQ_SHIFT_HALFSTEPS          384.0
#define FREQ_SHIFT_TABLE_SIZE           (FREQ_SHIFT_HALFSTEPS * TUNING_RESOLUTION)
#define F_FREQ_SHIFT_TABLE_SIZE         (F_FREQ_SHIFT_HALFSTEPS * F_TUNING_RESOLUTION)
#define FREQ_SHIFT_ZERO_OFFSET          (F_FREQ_SHIFT_TABLE_SIZE * 0.5)


/*****************************************************************************
 *
 * Preprocessor macros used throughout the codebase
 *
 *****************************************************************************/

/* Insist that funtion arguments as USED, or UNUSED, if necessary */
#if defined(USED)
#elif defined(__GNUC__)
# define USED(x) x
#else
# define USED(x) x
#endif

#if defined(UNUSED)
#elif defined(__GNUC__)
# define UNUSED(x) x __attribute__ ((unused))
#else
# define UNUSED(x) x
#endif

#if !defined(ENABLE_INPUTS) && defined(__GNUC__)
# define USED_FOR_INPUTS(x) x __attribute__ ((unused))
#else
# define USED_FOR_INPUTS(x) x
#endif

/* Number of elements in an array */
#define NELEM(a)            ( sizeof(a) / sizeof((a)[0]) )


/*****************************************************************************
 *
 * Globals and prototypes from phasex.c
 *
 *****************************************************************************/

extern int debug;
extern int debug_level;
extern int phasex_instance;
extern int pending_shutdown;

extern pthread_t debug_thread_p;
extern pthread_t audio_thread_p;
extern pthread_t midi_thread_p;
extern pthread_t gtkui_thread_p;
extern pthread_t jack_thread_p;
extern pthread_t engine_thread_p[MAX_PARTS];

extern char *audio_input_ports;
extern char *audio_output_ports;

extern char *midi_port_name;
extern char *audio_device_name;

extern int use_gui;
extern int lash_disabled;

extern char user_data_dir[PATH_MAX];
extern char user_patch_dir[PATH_MAX];
extern char user_midimap_dir[PATH_MAX];
extern char user_session_dir[PATH_MAX];
extern char user_bank_file[PATH_MAX];
extern char user_session_bank_file[PATH_MAX];
extern char user_patchdump_file[MAX_PARTS][PATH_MAX];
extern char user_session_dump_dir[PATH_MAX];
extern char user_midimap_dump_file[PATH_MAX];
extern char user_config_file[PATH_MAX];
extern char user_default_patch[PATH_MAX];
extern char sys_default_patch[PATH_MAX];
extern char sys_bank_file[PATH_MAX];


void phasex_shutdown(const char *msg);
void init_rt_mutex(pthread_mutex_t *mutex, int rt);


#endif /* _PHASEX_H_ */
