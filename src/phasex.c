/*****************************************************************************
 *
 * phasex.c
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
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <asoundlib.h>
#include "phasex.h"
#include "config.h"
#include "driver.h"
#include "alsa_seq.h"
#include "jack.h"
#include "buffer.h"
#include "engine.h"
#include "wave.h"
#include "filter.h"
#include "patch.h"
#include "param.h"
#include "bpm.h"
#include "gui_main.h"
#include "gui_layout.h"
#include "gui_patch.h"
#include "bank.h"
#include "session.h"
#include "midimap.h"
#include "settings.h"
#include "help.h"
#include "debug.h"


#ifndef WITHOUT_LASH
# include "lash.h"
#endif


/* command line options */
#define HAS_ARG     1
#define NUM_OPTS    (27 + 1)
static struct option long_opts[] = {
	{ "config-file",     HAS_ARG, NULL, 'c' },
	{ "audio-driver",    HAS_ARG, NULL, 'A' },
	{ "audio-device",    HAS_ARG, NULL, 'a' },
	{ "latency-periods", HAS_ARG, NULL, 'p' },
	{ "period-size",     HAS_ARG, NULL, 's' },
	{ "sample-rate",     HAS_ARG, NULL, 'r' },
	{ "midi-driver",     HAS_ARG, NULL, 'M' },
	{ "midi-port",       HAS_ARG, NULL, 'm' },
	{ "input",           HAS_ARG, NULL, 'i' },
	{ "output",          HAS_ARG, NULL, 'o' },
	{ "bpm",             HAS_ARG, NULL, 'b' },
	{ "tuning",          HAS_ARG, NULL, 't' },
	{ "debug",           HAS_ARG, NULL, 'd' },
	{ "session-dir",     HAS_ARG, NULL, 'D' },
	{ "uuid",            HAS_ARG, NULL, 'u' },
	{ "undersample",     0,       NULL, 'U' },
	{ "oversample",      0,       NULL, 'O' },
	{ "fullscreen",      0,       NULL, 'f' },
	{ "maximize",        0,       NULL, 'x' },
	{ "no-gui",          0,       NULL, 'G' },
	{ "list",            0,       NULL, 'l' },
	{ "help",            0,       NULL, 'h' },
	{ "version",         0,       NULL, 'v' },
	{ "disable-lash",    0,       NULL, 'L' },
	{ "lash-project",    HAS_ARG, NULL, 'P' },
	{ "lash-server",     HAS_ARG, NULL, 'S' },
	{ "lash-id",         HAS_ARG, NULL, 'I' },
	{ 0,                 0,       NULL, 0 }
};


int         pending_shutdown              = 0;

pthread_t   debug_thread_p                = 0;
pthread_t   audio_thread_p                = 0;
pthread_t   midi_thread_p                 = 0;
pthread_t   gtkui_thread_p                = 0;
pthread_t   jack_thread_p                 = 0;
pthread_t   engine_thread_p[MAX_PARTS];

char        *audio_input_ports            = NULL;
char        *audio_output_ports           = NULL;

char        *midi_port_name               = NULL;
char        *audio_device_name            = NULL;

int         use_gui                       = 1;
int         lash_disabled                 = 0;

char        user_data_dir[PATH_MAX];
char        user_patch_dir[PATH_MAX];
char        user_session_dir[PATH_MAX];
char        user_midimap_dir[PATH_MAX];
char        user_bank_file[PATH_MAX];
char        user_session_bank_file[PATH_MAX];
char        user_session_dump_dir[PATH_MAX];
char        user_patchdump_file[MAX_PARTS][PATH_MAX];
char        user_midimap_dump_file[PATH_MAX];
char        user_config_file[PATH_MAX];
char        user_default_patch[PATH_MAX];
char        sys_default_patch[PATH_MAX];
char        sys_bank_file[PATH_MAX];


/*****************************************************************************
 * showusage()
 *****************************************************************************/
void
showusage(char *argvzero)
{
	printf("Usage:  %s [options] [patch-name]\n\n", argvzero);
	printf("PHASEX Options:\n");
	printf("  -c, --config-file=     Alternate config (takes precedence over default\n");
	printf("                             config file and command line options.\n");
	printf("  -A, --audio-driver=    Audio driver:  alsa or jack.\n");
	printf("  -a, --audio-device=    ALSA device name (hw:x,y format).\n");
	printf("  -r, --sample-rate=     Audio sample rate (ALSA only).\n");
	printf("  -p, --latency-periods= Number of buffer latency periods (ALSA only).\n");
	printf("  -s, --period-size=     Buffer period size (power of 2, ALSA only).\n");
	printf("  -M, --midi-driver=     MIDI driver:  jack, alsa-seq, alsa-raw, generic.\n");
	printf("  -m, --midi-port=       MIDI input port or device name (driver specific).\n");
	printf("  -f, --fullscreen       Start GUI in fullscreen mode.\n");
	printf("  -x, --maximize         Start GUI with main window maximized.\n");
	printf("  -b, --bpm=             Override BPM in patch bank and lock BPM parameter.\n");
	printf("  -t, --tuning=          Base tuning frequency for A4 (default 440 Hz).\n");
	printf("  -i, --input=           Comma separated pair of audio input matches.\n");
	printf("  -o, --output=          Comma separated pair of audio output matches.\n");
	printf("  -O, --oversample       Use double the sample rate for internal math.\n");
	printf("  -U, --undersample      Use half the sample rate for internal math.\n");
	printf("  -G, --no-gui           Run PHASEX without starting the GUI.\n");
	printf("  -D, --session-dir=     Set directory for loading initial session.\n");
	printf("  -u, --uuid=            Set UUID for JACK Session handling.\n");
	printf("  -d, --debug=           Debug class (Can be repeated. See debug.c).\n");
	printf("  -l, --list             Scan and list audio and MIDI devices.\n");
	printf("  -v, --version          Display version and exit.\n");
	printf("  -h, --help             Display this help message and probe ALSA hardware.\n\n");
#ifndef WITHOUT_LASH
	printf("LASH Options:\n");
	printf("  -P, --lash-project=    LASH project name.\n");
	printf("  -S, --lash-server=     LASH server address.\n");
	printf("  -I, --lash-id=         LASH client ID.\n");
	printf("  -L, --disable-lash     Disable LASH completely for the current session.\n\n");
#endif
	printf("[P]hase [H]armonic [A]dvanced [S]ynthesis [EX]permient ver. %s\n", PACKAGE_VERSION);
	printf("  (C) 1999-2015 Willaim Weston <william.h.weston@gmail.com>,\n");
	printf("With contributions (C) 2010 Anton Kormakov <assault64@gmail.com>, and\n");
	printf("  (C) 2007 Peter Shorthose <zenadsl6252@zen.co.uk>.\n");
	printf("GtkKnob code (C) 1999 Tony Garnock-Jones, (C) 2004 Sean Bolton,\n");
	printf("  (C) 2007 Peter Shorthose, and (C) 2007-2015 Willaim Weston.\n");
	printf("Distributed under the terms of the GNU GENERAL Public License, Version 3.\n");
	printf("  (See AUTHORS, LICENSE, and GPL-3.0.txt for details.)\n");
}


/*****************************************************************************
 * check_other_phasex_instances()
 *****************************************************************************/
int
check_other_phasex_instances(void)
{
	char           buf[1024];
	char           filename[PATH_MAX];
	FILE           *cmdfile;
	DIR            *slashproc;
	struct dirent  *procdir;
	pid_t          self_pid;
	int            other_instance       = 0;
	char           *p;
	char           *q;

	self_pid = getpid();

	if ((slashproc = opendir("/proc")) == NULL) {
		fprintf(stderr, "Unable to read /proc filesystem!\n");
		exit(1);
	}
	while ((procdir = readdir(slashproc)) != NULL) {
		if (procdir->d_type != DT_DIR) {
			continue;
		}
		if (atoi(procdir->d_name) == self_pid) {
			continue;
		}
		snprintf(filename, PATH_MAX, "/proc/%s/cmdline", procdir->d_name);
		p = q = (char *)(procdir->d_name);
		while (isdigit(*q) && ((q - p) < 8)) {
			q++;
		}
		if (*q != '\0') {
			continue;
		}
		if ((cmdfile = fopen(filename, "rt")) == NULL) {
			continue;
		}
		if (fread(buf, sizeof(char), sizeof(buf), cmdfile) <= 0) {
			fclose(cmdfile);
			continue;
		}
		fclose(cmdfile);
		if (strncmp(buf, "phasex", 6) != 0) {
			continue;
		}
		other_instance = 1;
		break;
	}
	closedir(slashproc);

	return other_instance;
}


/*****************************************************************************
 * check_user_data_dirs()
 *
 * Build all pathnames based on user's home dir, and build directory
 * structure in user's home directory, if needed.
 *****************************************************************************/
void
check_user_data_dirs(void)
{
	DIR         *dir;
	DIR         *old_dir;
	char        *home;
	char        cmd[1024];
	char        old_patch_dir[PATH_MAX];
	int         i;
	int         found_old_patches           = 0;

	/* look at environment to determine home directory */
	home = getenv("HOME");
	if (home == NULL) {
		phasex_shutdown("HOME is not set.  Unable to find user data.\n");
	}

	/* set up user data dir */
	snprintf(user_data_dir, PATH_MAX, "%s/%s", home, USER_DATA_DIR);
	if ((dir = opendir(user_data_dir)) == NULL) {
		if (errno == ENOENT) {
			if (mkdir(user_data_dir, 0755) != 0) {
				fprintf(stderr, "Unable to create user data directory '%s'.\nError %d: %s\n",
				        user_data_dir, errno, strerror(errno));
				phasex_shutdown(NULL);
			}
			/* copy in system patchbank only when creating new user dir */
			snprintf(cmd, sizeof(cmd), "/bin/cp %s/patchbank %s/%s",
			         PHASEX_DIR, user_data_dir, USER_BANK_FILE);
			if (system(cmd) == -1) {
				fprintf(stderr, "Unable to copy '%s/patchbank' to user data directory.\n",
				        PHASEX_DIR);
			}
		}
		else {
			fprintf(stderr, "Unable to open user data directory '%s'.\nError %d: %s\n",
			        user_data_dir, errno, strerror(errno));
			phasex_shutdown(NULL);
		}
	}
	else {
		closedir(dir);
	}

	/* check for old user-patches dir. */
	snprintf(old_patch_dir, PATH_MAX, "%s/%s", user_data_dir, OLD_USER_PATCH_DIR);
	if ((old_dir = opendir(old_patch_dir)) != NULL) {
		closedir(old_dir);
		found_old_patches = 1;
	}

	/* check user patch dir, creating if necessary */
	snprintf(user_patch_dir, PATH_MAX, "%s/%s", user_data_dir, USER_PATCH_DIR);
	if ((dir = opendir(user_patch_dir)) == NULL) {
		if (errno == ENOENT) {
			if (mkdir(user_patch_dir, 0755) != 0) {
				fprintf(stderr, "Unable to create user patch directory '%s'.\nError %d: %s\n",
				        user_patch_dir, errno, strerror(errno));
				phasex_shutdown(NULL);
			}
		}
		else {
			fprintf(stderr, "Unable to open user patch directory '%s'.\nError %d: %s\n",
			        user_patch_dir, errno, strerror(errno));
			phasex_shutdown(NULL);
		}

		/* convert patches in old location, save to new location. */
		if (found_old_patches) {
			snprintf(cmd, sizeof(cmd), "%s/bin/phasex-convert-patch %s %s",
			         CONFIG_PREFIX, old_patch_dir, user_patch_dir);
			fprintf(stderr, "Found patch dir for PHASEX <= v0.12.x.  Converting:\n    %s\n", cmd);
			if (system(cmd) == -1) {
				fprintf(stderr, "%s/bin/phasex-convert-patch failed.\n", PHASEX_DIR);
			}
		}
	}
	else {
		closedir(dir);
	}

	/* check user session dir, creating if necessary */
	snprintf(user_session_dir, PATH_MAX, "%s/%s", user_data_dir, USER_SESSION_DIR);
	if ((dir = opendir(user_session_dir)) == NULL) {
		if (errno == ENOENT) {
			if (mkdir(user_session_dir, 0755) != 0) {
				fprintf(stderr, "Unable to create user session directory '%s'.\nError %d: %s\n",
				        user_session_dir, errno, strerror(errno));
				phasex_shutdown(NULL);
			}
		}
		else {
			fprintf(stderr, "Unable to open user session directory '%s'.\nError %d: %s\n",
			        user_session_dir, errno, strerror(errno));
			phasex_shutdown(NULL);
		}
	}
	else {
		closedir(dir);
	}

	/* check user midimap dir, creating if necessary */
	snprintf(user_midimap_dir, PATH_MAX, "%s/%s", user_data_dir, USER_MIDIMAP_DIR);
	if ((dir = opendir(user_midimap_dir)) == NULL) {
		if (errno == ENOENT) {
			if (mkdir(user_midimap_dir, 0755) != 0) {
				fprintf(stderr, "Unable to create user midimap directory '%s'.\nError %d: %s\n",
				        user_midimap_dir, errno, strerror(errno));
				phasex_shutdown(NULL);
			}
		}
		else {
			fprintf(stderr, "Unable to open user midimap directory '%s'.\nError %d: %s\n",
			        user_midimap_dir, errno, strerror(errno));
			phasex_shutdown(NULL);
		}
	}
	else {
		closedir(dir);
	}

	snprintf(user_midimap_dump_file, PATH_MAX, "%s/%s", user_data_dir,  USER_MIDIMAP_DUMP_FILE);
	snprintf(user_config_file,       PATH_MAX, "%s/%s", user_data_dir,  USER_CONFIG_FILE);
	snprintf(user_default_patch,     PATH_MAX, "%s/%s", user_patch_dir, SYS_DEFAULT_PATCH);
	snprintf(sys_default_patch,      PATH_MAX, "%s/%s", PATCH_DIR,      SYS_DEFAULT_PATCH);
	snprintf(sys_bank_file,          PATH_MAX, "%s/%s", PHASEX_DIR,     USER_BANK_FILE);
	snprintf(user_bank_file,         PATH_MAX, "%s/%s", user_data_dir,  USER_BANK_FILE);
	snprintf(user_session_bank_file, PATH_MAX, "%s/%s", user_data_dir,  USER_SESSION_BANK_FILE);
	snprintf(user_session_dump_dir,  PATH_MAX, "%s/%s/%s",
	         user_data_dir,  USER_SESSION_DIR, USER_SESSION_DUMP_DIR);

	for (i = 0; i < MAX_PARTS; i++) {
		snprintf(user_patchdump_file[i], PATH_MAX, "%s/phasex-%02d.phx",
		         user_session_dump_dir, (i + 1));
	}
}


/*****************************************************************************
 * phasex_shutdown()
 *
 * Main shutdown function.  Can be called from anywhere to cleanly shutdown.
 *****************************************************************************/
void
phasex_shutdown(const char *msg)
{
	/* output message from caller */
	if (msg != NULL) {
		fprintf(stderr, "%s", msg);
	}

	/* keep current midi port settings. */
	if (midi_port_name != NULL) {
		switch (midi_driver) {
		case MIDI_DRIVER_ALSA_SEQ:
			setting_alsa_seq_port = midi_port_name;
			break;
		case MIDI_DRIVER_RAW_ALSA:
			setting_alsa_raw_midi_device = midi_port_name;
			break;
#ifdef ENABLE_RAWMIDI_GENERIC
		case MIDI_DRIVER_RAW_GENERIC:
			setting_generic_midi_device = midi_port_name;
			break;
#endif
#ifdef ENABLE_RAWMIDI_OSS
		case MIDI_DRIVER_RAW_OSS:
			setting_oss_midi_device = midi_port_name;
			break;
#endif
		case MIDI_DRIVER_JACK:
		default:
			break;
		}
		setting_midi_driver = midi_driver;
	}

	/* TODO: be more thorough about gathering settings */
	save_settings(NULL);

	/* set the global shutdown flag */
	pending_shutdown = 1;

	/* make sure engine threads can exit. */
	inc_audio_index(2);
}


/*****************************************************************************
 * init_rt_mutex()
 *****************************************************************************/
void
init_rt_mutex(pthread_mutex_t *mutex, int rt)
{
	pthread_mutexattr_t     attr;

	/* set attrs for realtime mutexes */
	pthread_mutexattr_init(&attr);
#ifdef HAVE_PTHREAD_MUTEXATTR_SETPROTOCOL
	if (rt) {
		pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
	}
#endif

	/* init mutex with attrs */
	pthread_mutex_init(mutex, &attr);
}


/*****************************************************************************
 * main()
 *
 * Parse command line, load patch, build lookup tables,
 * start engine and jack threads, and wait.
 *****************************************************************************/
int
main(int argc, char **argv)
{
	char            opts[NUM_OPTS * 2 + 1];
	char            filename[PATH_MAX];
	struct option   *op;
	char            *cp;
	char            *p;
	char            *patch_list             = NULL;
	char            *init_session_dir       = NULL;
	int             c;
	unsigned int    i;
	int             j;
	int             ret                     = 0;
	unsigned int    bpm_override            = 0;
	int             saved_errno;

	setlocale(LC_ALL, "C");

	if (check_other_phasex_instances()) {
		fprintf(stderr, "Unable to start:  Another instance of phasex is already running.\n");
		exit(1);
	}

	/* Start debug thread.  debug_class is not set until arguemnts are parsed,
	   so use fprintf() until then. */
	if ((ret = pthread_create(&debug_thread_p, NULL, &phasex_debug_thread, NULL)) != 0) {
		fprintf(stderr, "***** ERROR:  Unable to start debug thread.\n");
	}

	/* lock down memory (rt hates page faults) */
	if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
		saved_errno = errno;
		fprintf(stderr, "Unable to unlock memory:  errno=%d (%s)\n",
		        saved_errno, strerror(saved_errno));
	}

	/* init lash client */
#ifndef WITHOUT_LASH
	for (j = 0; j < argc; j++) {
		if ((strcmp(argv[j], "-L") == 0) || (strcmp(argv[j], "--disable-lash") == 0) ||
		    (strcmp(argv[j], "-h") == 0) || (strcmp(argv[j], "--help") == 0) ||
		    (strcmp(argv[j], "-l") == 0) || (strcmp(argv[j], "--list") == 0) ||
		    (strcmp(argv[j], "-v") == 0) || (strcmp(argv[j], "--version") == 0) ||
		    (strcmp(argv[j], "-D") == 0) || (strcmp(argv[j], "--session-dir") == 0) ||
		    (strcmp(argv[j], "-u") == 0) || (strcmp(argv[j], "--uuid") == 0)) {
			lash_disabled = 1;
			break;
		}
	}
	if (!lash_disabled) {
		if (lash_client_init(&argc, &argv) == 0) {
			lash_poll_event();
		}
	}
#endif

	/* make sure user data dirs exist. */
	check_user_data_dirs();

	/* If lash hasn't read a config, read user default config
	   before processing cli args. */
	if (config_file == NULL) {
		read_settings(user_config_file);
	}
	else {
		fprintf(stderr, "Using LASH project config '%s'\n", config_file);
	}

	/* utilize some settings for startup */
	select_audio_driver(NULL, setting_audio_driver);
	select_midi_driver(NULL, setting_midi_driver);

	/* build the short option string */
	cp = opts;
	for (op = long_opts; op < &long_opts[NUM_OPTS]; op++) {
		*cp++ = (char) op->val;
		if (op->has_arg) {
			*cp++ = ':';
		}
	}

	/* handle options */
	for (;;) {
		c = getopt_long(argc, argv, opts, long_opts, NULL);
		if (c == -1) {
			break;
		}

		switch (c) {
		case 'c':   /* config file */
			if (config_file != NULL) {
				free(config_file);
			}
			config_file = strdup(optarg);
			break;
		case 'A':   /* audio driver */
			p = strdup(optarg);
			select_audio_driver(p, -1);
			free(p);
			break;
		case 'a':   /* audio device */
			audio_device_name = strdup(optarg);
			break;
		case 'r':   /* audio sample rate (ALSA only) */
			setting_sample_rate = atoi(optarg);
			if ((setting_sample_rate < 8000) || (setting_sample_rate > 192000)) {
				setting_sample_rate = DEFAULT_SAMPLE_RATE;
			}
			break;
		case 'p':   /* audio buffer latency periods */
			setting_buffer_latency = (unsigned int) atoi(optarg);
			if ((setting_buffer_latency < 1) || (setting_buffer_latency > 3)) {
				setting_buffer_latency = DEFAULT_LATENCY_PERIODS;
			}
			break;
		case 's':   /* audio buffer period size (ALSA only) */
			setting_buffer_period_size = (unsigned int) atoi(optarg);
			i = 1;
			for (j = 1; j < 16; j++) {
				i *= 2;
				if (setting_buffer_period_size == i) {
					break;
				}
			}
			if (j == 16) {
				setting_buffer_period_size = DEFAULT_BUFFER_PERIOD_SIZE;
			}
			break;
		case 'M':   /* midi driver */
			select_midi_driver(optarg, -1);
			break;
		case 'm':   /* MIDI port / device */
			midi_port_name = strdup(optarg);
			break;
		case 'U':   /* undersample */
			if (setting_sample_rate_mode == SAMPLE_RATE_OVERSAMPLE) {
				setting_sample_rate_mode = SAMPLE_RATE_NORMAL;
			}
			else {
				setting_sample_rate_mode = SAMPLE_RATE_UNDERSAMPLE;
			}
			break;
		case 'O':   /* oversample */
			if (setting_sample_rate_mode == SAMPLE_RATE_UNDERSAMPLE) {
				setting_sample_rate_mode = SAMPLE_RATE_NORMAL;
			}
			else {
				setting_sample_rate_mode = SAMPLE_RATE_OVERSAMPLE;
			}
			break;
		case 'b':   /* bpm (tempo) */
			bpm_override = (unsigned int) atoi(optarg);
			if ((bpm_override < 64) || (bpm_override > 191)) {
				bpm_override = 0;
			}
			break;
		case 't':   /* tuning frequency */
			a4freq = atof(optarg);
			setting_tuning_freq = (sample_t) a4freq;
			if ((a4freq < 220.0) || (a4freq > 880.0)) {
				a4freq = A4FREQ;
			}
			break;
		case 'f':   /* fullscreen */
			setting_fullscreen = 1;
			break;
		case 'x':   /* maximize */
			setting_maximize = 1;
			setting_fullscreen = 0;
			break;
		case 'd':   /* debug */
			debug = 1;
			for (j = 0; debug_class_list[j].name != NULL; j++) {
				if (strcmp(debug_class_list[j].name, optarg) == 0) {
					debug_class |= debug_class_list[j].id;
				}
			}
			break;
		case 'i':   /* audio input ports */
			audio_input_ports = strdup(optarg);
			setting_jack_autoconnect = 0;
			break;
		case 'o':   /* audio output ports */
			audio_output_ports = strdup(optarg);
			setting_jack_autoconnect = 0;
			break;
		case 'G':   /* no-gui */
			use_gui = 0;
			break;
		case 'v':   /* version */
			printf("phasex-%s\n", PACKAGE_VERSION);
			return 0;
		case 'L':   /* disable lash */
			lash_disabled = 1;
			break;
		case 'l':   /* list audio / midi devices */
			scan_audio_and_midi();
			return 0;
		case 'D':   /* session directory */
			init_session_dir = strdup(optarg);
			if (config_file != NULL) {
				free(config_file);
			}
			snprintf(filename, PATH_MAX, "%s/%s", init_session_dir, USER_CONFIG_FILE);
			config_file = strdup(filename);
			break;
		case 'u':   /* jack session uuid */
			jack_session_uuid = strdup(optarg);
			break;
		case '?':
		case 'h':   /* help */
		default:
			showusage(argv[0]);
			return -1;
		}
	}

	/* grab patch name from end of command line */
	if (argv[optind] != NULL) {
		if ((patch_list = strdup(argv[optind])) == NULL) {
			phasex_shutdown("Out of memory!\n");
		}
	}

	/* If no alternate config file is given, use cli options for
	   audio/midi settings. */
	if ((config_file == NULL) || (strcmp(config_file, user_config_file) == 0)) {
		setting_audio_driver = audio_driver;
		setting_midi_driver  = midi_driver;
		if (audio_device_name != NULL) {
			if (setting_alsa_pcm_device != NULL) {
				free(setting_alsa_pcm_device);
			}
			setting_alsa_pcm_device = audio_device_name;
			audio_device_name = NULL;
		}
		if (midi_port_name != NULL) {
			switch (midi_driver) {
			case MIDI_DRIVER_ALSA_SEQ:
				if (setting_alsa_seq_port != NULL) {
					free(setting_alsa_seq_port);
				}
				setting_alsa_seq_port        = midi_port_name;
				break;
			case MIDI_DRIVER_RAW_ALSA:
				if (setting_alsa_raw_midi_device != NULL) {
					free(setting_alsa_raw_midi_device);
				}
				setting_alsa_raw_midi_device = midi_port_name;
				break;
			case MIDI_DRIVER_RAW_GENERIC:
				if (setting_generic_midi_device != NULL) {
					free(setting_generic_midi_device);
				}
				setting_generic_midi_device  = midi_port_name;
				break;
#ifdef ENABLE_RAWMIDI_OSS
			case MIDI_DRIVER_RAW_OSS:
				if (setting_oss_midi_device != NULL) {
					free(setting_oss_midi_device);
				}
				setting_oss_midi_device      = midi_port_name;
				break;
#endif
			}
			midi_port_name = NULL;
		}
	}
	/* read alternate config, if specified. */
	else {
		read_settings(config_file);
		audio_driver = setting_audio_driver;
		midi_driver  = setting_midi_driver;
	}

	/* start gtkui thread (in splash mode) */
	if (use_gui) {
		init_rt_mutex(&gtkui_ready_mutex, 1);
		if ((ret = pthread_create(&gtkui_thread_p, NULL, &gtkui_thread, NULL)) != 0) {
			phasex_shutdown("Unable to start gtkui thread.\n");
		}
	}

	/* build the lookup tables */
	build_freq_table();
	build_freq_shift_table();
	build_waveform_tables();
	build_mix_table();
	build_pan_table();
	build_gain_table();
	build_velocity_gain_table();
	build_keyfollow_table();

	/* initialize parameter lists */
	init_params();
	init_param_groups();
	init_param_pages();

	/* initialize realtime mutexes */
	init_rt_mutex(&sample_rate_mutex, 1);

	/* initialize audio system based on selected driver */
	//init_buffer_indices(1);
	//start_midi_clock();
	init_audio();
	while (sample_rate == 0) {
		usleep(125000);
		init_audio();
	}

	/* init MIDI system based on selected driver */
	init_midi();

	/* now that sample rate is known, build filter and envelope tables. */
	build_filter_tables();
	build_env_tables();

	PHASEX_DEBUG(DEBUG_CLASS_INIT, "audio_driver = %d (%s)  midi_driver = %d (%s)\n",
	             audio_driver, audio_driver_name, midi_driver, midi_driver_name);

	/* initialize the internal engine data structures before loading the
	   patch bank. */
	init_engine_internals();

	/* Initialize and load patch bank */
	init_patch_param_data();
	if (init_session_dir == NULL) {
		init_patch_bank(NULL);
		init_session_bank(NULL);
	}
	else {
		snprintf(filename, PATH_MAX, "%s/%s", init_session_dir, USER_BANK_FILE);
		init_patch_bank(filename);
		snprintf(filename, PATH_MAX, "%s/%s", init_session_dir, USER_BANK_FILE);
		init_session_bank(filename);
	}

	/* initialize help system (only after patch data is fully initialized) */
	init_help();

	/* handle initial patch(es) from command line */
	load_patch_list(patch_list);

	/* Now that sample rate is known _and_ we have a good patch structure
	   initialize engine parameters so parameters callbacks can start.  The
	   first round of param callbacks below is needed to finish putting the
	   engine into a consistent state.  init_engine_parameters(); */

	/* override bpm from command line */
	override_bpm(bpm_override);

	/* use midimap from settings, if given */
	if (setting_midimap_file != NULL) {
		read_midimap(setting_midimap_file);
	}

	/* start the gui */
	if (use_gui) {
		start_gui = 1;

		/* wait until gtkui thread is ready */
		pthread_mutex_lock(&gtkui_ready_mutex);
		if (!gtkui_ready) {
			pthread_cond_wait(&gtkui_ready_cond, &gtkui_ready_mutex);
		}
		pthread_mutex_unlock(&gtkui_ready_mutex);
	}

	/* Load JACK session, if necessary. */
	if (init_session_dir != NULL) {
		load_session(init_session_dir, 0, 1);
		p = jack_get_session_name_from_directory(init_session_dir);
		PHASEX_DEBUG(DEBUG_CLASS_INIT, "Loaded initial JACK Session '%s'\n", p)
	}

#ifndef WITHOUT_LASH
	/* Load LASH session, if necessary. */
	else if (lash_project_dir != NULL) {
		load_session(lash_project_dir, 0, 1);
		p = lash_set_phasex_session_name(NULL);
		PHASEX_DEBUG(DEBUG_CLASS_INIT, "Loaded initial LASH Session '%s'\n", p);
	}
#endif

	/* run the callbacks for all the parameters */
	run_param_callbacks(1);


	/* start the audio system, based on selected driver */
	start_audio();

	/* wait for audio to start before starting midi. */
	wait_audio_start();

	/* start MIDI system based on selected driver */
	start_midi();

	/* wait until midi thread is ready */
	wait_midi_start();

	/* start engine threads */
	start_engine_threads();

	/* wait until engine threads are ready */
	wait_engine_start();

	/* start midi clock now that everything is running and ready to go */
	init_buffer_indices(1);
	start_midi_clock();

	/* Phasex watchdog handles restarting threads on config changes and
	   runs driver supplied watchdog loop iterations. */
	phasex_watchdog();

	/* Save patch and session bank state for next time. */
	save_patch_bank(NULL);
	save_session_bank(NULL);

	/* Wait for threads created directly by PHASEX to terminate. */
	if (use_gui) {
		pthread_join(gtkui_thread_p,  NULL);
	}
	for (i = 0; i < MAX_PARTS; i++) {
		pthread_join(engine_thread_p[i], NULL);
	}
	if (audio_thread_p != 0) {
		pthread_join(audio_thread_p,  NULL);
	}
	if (midi_thread_p != 0) {
		pthread_join(midi_thread_p,  NULL);
	}
	pthread_join(debug_thread_p, NULL);

	return 0;
}
