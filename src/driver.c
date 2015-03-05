/*****************************************************************************
 *
 * driver.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012-2015 Willaim Weston <william.h.weston@gmail.com>
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
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "phasex.h"
#include "driver.h"
#include "config.h"
#include "timekeeping.h"
#include "debug.h"
#include "buffer.h"
#include "settings.h"
#include "filter.h"
#include "midi_process.h"
#include "rawmidi.h"
#include "alsa_seq.h"
#include "alsa_pcm.h"
#include "jack.h"
#include "engine.h"
#include "patch.h"
#include "param.h"
#include "midimap.h"
#include "gui_main.h"

#ifndef WITHOUT_LASH
# include "lash.h"
#endif


int                 audio_driver                = AUDIO_DRIVER_NONE;
int                 midi_driver                 = MIDI_DRIVER_NONE;

int                 sample_rate_mode_changed    = 0;

char                *audio_driver_name          = "none";
char                *midi_driver_name           = "none";

DRIVER_FUNC         audio_init_func;
DRIVER_FUNC         audio_start_func;
DRIVER_FUNC         audio_stop_func;
DRIVER_VOID_FUNC    audio_restart_func;
THREAD_FUNC         audio_thread_func;
DRIVER_VOID_FUNC    audio_watchdog_func;

DRIVER_FUNC         midi_init_func;
DRIVER_FUNC         midi_start_func;
DRIVER_FUNC         midi_stop_func;
DRIVER_VOID_FUNC    midi_restart_func;
THREAD_FUNC         midi_thread_func;
DRIVER_VOID_FUNC    midi_watchdog_func;

pthread_mutex_t     audio_ready_mutex;
pthread_cond_t      audio_ready_cond            = PTHREAD_COND_INITIALIZER;
int                 audio_ready                 = 0;

pthread_mutex_t     midi_ready_mutex;
pthread_cond_t      midi_ready_cond             = PTHREAD_COND_INITIALIZER;
int                 midi_ready                  = 0;

int                 audio_stopped               = 0;
int                 midi_stopped                = 0;
int                 engine_stopped              = 0;


char *audio_driver_names[] = {
	"none",
	"alsa",
	"jack",
	NULL
};

char *midi_driver_names[] = {
	"none",
	"jack",
	"alsa-seq",
	"alsa-raw",
#ifdef ENABLE_RAWMIDI_GENERIC
	"generic",
#endif
#ifdef ENABLE_RAWMIDI_OSS
	"oss",
#endif
	NULL
};


/*****************************************************************************
 * select_audio_driver()
 *****************************************************************************/
void
select_audio_driver(char *driver_name, int driver_id)
{
	if (driver_name == NULL) {
		driver_name = "";
	}
	if ((driver_id == AUDIO_DRIVER_ALSA_PCM) ||
	    (strcmp(driver_name, "alsa") == 0)) {
		audio_driver_name     = "alsa";
		audio_driver          = AUDIO_DRIVER_ALSA_PCM;
		audio_init_func       = &alsa_pcm_init;
		audio_start_func      = NULL;
		audio_stop_func       = NULL;
		audio_restart_func    = NULL;
		audio_thread_func     = &alsa_pcm_thread;
		audio_watchdog_func   = &alsa_pcm_watchdog_cycle;
	}
	else if ((driver_id == AUDIO_DRIVER_JACK) ||
	         (strcmp(driver_name, "jack") == 0)) {
		audio_driver_name     = "jack";
		audio_driver          = AUDIO_DRIVER_JACK;
		audio_init_func       = &jack_audio_init;
		audio_start_func      = &jack_start;
		audio_stop_func       = &jack_stop;
		audio_restart_func    = &jack_restart;
		audio_thread_func     = NULL;
		audio_watchdog_func   = &jack_watchdog_cycle;
	}
	else if ((driver_id == AUDIO_DRIVER_NONE) ||
	         (strcmp(driver_name, "none") == 0)) {
		audio_driver_name     = "none";
		audio_driver          = AUDIO_DRIVER_NONE;
		audio_init_func       = NULL;
		audio_start_func      = NULL;
		audio_stop_func       = NULL;
		audio_restart_func    = NULL;
		audio_thread_func     = NULL;
		audio_watchdog_func   = NULL;
	}
}


/*****************************************************************************
 * select_midi_driver()
 *****************************************************************************/
void
select_midi_driver(char *driver_name, int driver_id)
{
	if (driver_name == NULL) {
		driver_name = "";
	}
	if ((driver_id == MIDI_DRIVER_JACK) ||
	    (strcmp(driver_name, "jack") == 0)) {
		midi_driver_name   = "jack";
		midi_driver        = MIDI_DRIVER_JACK;
		midi_init_func     = NULL;
		midi_start_func    = NULL;
		midi_stop_func     = NULL;
		midi_restart_func  = NULL;
		midi_thread_func   = NULL;
		midi_watchdog_func = NULL;
	}
	else if ((driver_id == MIDI_DRIVER_ALSA_SEQ) ||
	         (strcmp(driver_name, "alsa") == 0) ||
	         (strcmp(driver_name, "alsa-seq") == 0)) {
		midi_driver_name   = "alsa-seq";
		midi_driver        = MIDI_DRIVER_ALSA_SEQ;
		midi_init_func     = &alsa_seq_init;
		midi_start_func    = NULL;
		midi_stop_func     = NULL;
		midi_restart_func  = NULL;
		midi_thread_func   = &alsa_seq_thread;
		midi_watchdog_func = &alsa_seq_watchdog_cycle;
	}
	else if ((driver_id == MIDI_DRIVER_RAW_ALSA) ||
	         (strcmp(driver_name, "raw") == 0) ||
	         (strcmp(driver_name, "rawmidi") == 0) ||
	         (strcmp(driver_name, "raw-alsa") == 0) ||
	         (strcmp(driver_name, "alsa-raw") == 0)) {
		midi_driver_name   = "alsa-raw";
		midi_driver        = MIDI_DRIVER_RAW_ALSA;
		midi_init_func     = &rawmidi_init;
		midi_start_func    = NULL;
		midi_stop_func     = NULL;
		midi_restart_func  = NULL;
		midi_thread_func   = &rawmidi_thread;
		midi_watchdog_func = &rawmidi_watchdog_cycle;
	}
#ifdef ENABLE_RAWMIDI_GENERIC
	else if ((driver_id == MIDI_DRIVER_RAW_GENERIC) ||
	         (strcmp(driver_name, "generic") == 0)) {
		midi_driver_name   = "generic";
		midi_driver        = MIDI_DRIVER_RAW_GENERIC;
		midi_init_func     = &rawmidi_init;
		midi_start_func    = NULL;
		midi_stop_func     = NULL;
		midi_restart_func  = NULL;
		midi_thread_func   = &rawmidi_thread;
		midi_watchdog_func = NULL;
	}
#endif
#ifdef ENABLE_RAWMIDI_OSS
	else if ((driver_id == MIDI_DRIVER_RAW_OSS) ||
	         (strcmp(driver_name, "oss") == 0)) {
		midi_driver_name   = "oss";
		midi_driver        = MIDI_DRIVER_RAW_OSS;
		midi_init_func     = &rawmidi_init;
		midi_start_func    = NULL;
		midi_stop_func     = NULL;
		midi_restart_func  = NULL;
		midi_thread_func   = &rawmidi_thread;
		midi_watchdog_func = NULL;
	}
#endif
	else if ((driver_id == MIDI_DRIVER_NONE) ||
	         (strcmp(driver_name, "none") == 0)) {
		midi_driver_name   = "none";
		midi_driver        = MIDI_DRIVER_NONE;
		midi_init_func     = NULL;
		midi_start_func    = NULL;
		midi_stop_func     = NULL;
		midi_restart_func  = NULL;
		midi_thread_func   = NULL;
		midi_watchdog_func = NULL;
	}
}


/*****************************************************************************
 * init_audio()
 *****************************************************************************/
void
init_audio(void)
{
	int     j;
	int     no_driver_selected = 0;

	if (audio_driver == AUDIO_DRIVER_NONE) {
		no_driver_selected = 1;
		select_audio_driver(NULL, DEFAULT_AUDIO_DRIVER);
	}

	switch (audio_driver) {
	case AUDIO_DRIVER_ALSA_PCM:
		if (alsa_pcm_init() == 0) {
			setting_audio_driver = audio_driver;
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Successfully opened ALSA PCM.\n");
		}
		else {
			PHASEX_WARN("Unable to open ALSA PCM...\n");
			select_audio_driver(NULL, AUDIO_DRIVER_NONE);
		}
		break;
	case AUDIO_DRIVER_JACK:
		/* connect to jack server, retrying for up to 3 seconds */
		for (j = 0; j < 3; j++) {
			if (jack_audio_init() == 0) {
				setting_audio_driver = audio_driver;
				break;
			}
			else {
				PHASEX_WARN("Waiting for JACK server to start...\n");
				sleep(1);
			}
		}
		/* give up if jack server was not found in 15 seconds */
		if (j == 3) {
			if (no_driver_selected) {
				select_audio_driver(NULL, AUDIO_DRIVER_ALSA_PCM);
				if (alsa_pcm_init() == 0) {
					setting_audio_driver = audio_driver;
					PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Successfully opened ALSA PCM.\n");
				}
				else {
					PHASEX_WARN("Unable to open ALSA PCM...\n");
					select_audio_driver(NULL, AUDIO_DRIVER_NONE);
				}
			}
			else {
				PHASEX_WARN("Unable to conect to JACK server.  Is JACK running?\n");
				select_audio_driver(NULL, AUDIO_DRIVER_NONE);
			}
		}
		break;
	}
}


/*****************************************************************************
 * init_midi()
 *****************************************************************************/
void
init_midi(void)
{
	/* build midi controller matrix after init_params() and before midi_thread() */
	build_ccmatrix();

	if (midi_driver == MIDI_DRIVER_NONE) {
		select_midi_driver(NULL, DEFAULT_MIDI_DRIVER);
	}

	if (midi_init_func != NULL) {
		if (midi_init_func() != 0) {
			PHASEX_WARN("Unable to initialize MIDI input driver '%s'.\n",
			            midi_driver_name);
			select_midi_driver(NULL, MIDI_DRIVER_NONE);
		}
	}
}


/*****************************************************************************
 * start_audio()
 *****************************************************************************/
void
start_audio(void)
{
	int     saved_errno;
	int     ret;

	switch (audio_driver) {
	case AUDIO_DRIVER_ALSA_PCM:
		if (audio_thread_func != NULL) {
			init_rt_mutex(&audio_ready_mutex, 1);
			if ((ret = pthread_create(&audio_thread_p, NULL, audio_thread_func, NULL)) != 0) {
				saved_errno = errno;
				PHASEX_DEBUG(DEBUG_CLASS_INIT,
				             "Unable to start AUDIO thread:  "
				             "error %d (%s).\n  errno=%d (%s)\n",
				             ret,
				             (ret == EAGAIN) ? "EAGAIN" :
				             (ret == EINVAL) ? "EINVAL" :
				             (ret == EPERM)  ? "EPERM"  : "",
				             saved_errno,
				             strerror(saved_errno));
				PHASEX_WARN("Unable to start ALSA PCM audio thread...\n");
				select_audio_driver(NULL, AUDIO_DRIVER_NONE);
			}
		}
		else if (audio_start_func != NULL) {
			if (audio_start_func() >= 0) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Main: Started ALSA PCM with 2 channels.\n");
			}
			else {
				PHASEX_WARN("Unable to start ALSA PCM...\n");
				select_audio_driver(NULL, AUDIO_DRIVER_NONE);
			}
		}
		break;
	case AUDIO_DRIVER_JACK:
		if (audio_thread_func != NULL) {
			init_rt_mutex(&audio_ready_mutex, 1);
			if ((ret = pthread_create(&audio_thread_p, NULL, audio_thread_func, NULL)) != 0) {
				saved_errno = errno;
				PHASEX_DEBUG(DEBUG_CLASS_INIT,
				             "Unable to start JACK AUDIO thread:  "
				             "error %d (%s).\n  errno=%d (%s)\n",
				             ret,
				             (ret == EAGAIN) ? "EAGAIN" :
				             (ret == EINVAL) ? "EINVAL" :
				             (ret == EPERM)  ? "EPERM"  : "",
				             saved_errno,
				             strerror(saved_errno));
				PHASEX_WARN("Unable to start JACK AUDIO thread...\n");
				select_audio_driver(NULL, AUDIO_DRIVER_NONE);
			}
		}
		/* ready for jack to start running our process callback */
		else if (jack_start() == 0) {
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Main: Started JACK with client threads:  0x%lx\n",
			             jack_thread_p);
		}
		else {
			PHASEX_WARN("Unable to start JACK client.\n");
			select_audio_driver(NULL, AUDIO_DRIVER_NONE);
		}
		break;
	case AUDIO_DRIVER_NONE:
	default:
		break;
	}
}


/*****************************************************************************
 * start_midi()
 *****************************************************************************/
void
start_midi(void)
{
	int     saved_errno;
	int     ret;

	set_midi_cycle_time();

	if (midi_thread_func == NULL) {
		if (midi_start_func != NULL) {
			midi_start_func();
		}
	}
	else {
		init_rt_mutex(&midi_ready_mutex, 1);
		if ((ret = pthread_create(&midi_thread_p, NULL, midi_thread_func, NULL)) != 0) {
			saved_errno = errno;
			PHASEX_DEBUG(DEBUG_CLASS_INIT,
			             "Unable to start MIDI thread:  error %d (%s).\n  errno=%d (%s)\n",
			             ret,
			             (ret == EAGAIN) ? "EAGAIN" :
			             (ret == EINVAL) ? "EINVAL" :
			             (ret == EPERM)  ? "EPERM"  : "",
			             saved_errno,
			             strerror(saved_errno));
			select_midi_driver(NULL, MIDI_DRIVER_NONE);
		}
	}
}


/*****************************************************************************
 * wait_audio_start()
 *****************************************************************************/
void
wait_audio_start(void)
{
	if (audio_thread_func != NULL) {
		pthread_mutex_lock(&audio_ready_mutex);
		if (!audio_ready) {
			pthread_cond_wait(&audio_ready_cond, &audio_ready_mutex);
		}
		pthread_mutex_unlock(&audio_ready_mutex);
	}
}


/*****************************************************************************
 * wait_midi_start()
 *****************************************************************************/
void
wait_midi_start(void)
{
	if (midi_thread_func != NULL) {
		pthread_mutex_lock(&midi_ready_mutex);
		if (!midi_ready) {
			pthread_cond_wait(&midi_ready_cond, &midi_ready_mutex);
		}
		pthread_mutex_unlock(&midi_ready_mutex);
	}
}


/*****************************************************************************
 * wait_audio_stop()
 *****************************************************************************/
void
wait_audio_stop(void)
{
	switch (audio_driver) {
	case AUDIO_DRIVER_ALSA_PCM:
		if (audio_thread_p != 0) {
			pthread_join(audio_thread_p,  NULL);
			usleep(125000);
		}
		break;
	case AUDIO_DRIVER_JACK:
		while (jack_thread_p != 0) {
			usleep(125000);
		}
		break;
	}
}


/*****************************************************************************
 * wait_midi_stop()
 *****************************************************************************/
void
wait_midi_stop(void)
{
	if (midi_thread_p != 0) {
		pthread_join(midi_thread_p,  NULL);
		usleep(125000);
	}
}


/*****************************************************************************
 * wait_engine_stop()
 *****************************************************************************/
void
wait_engine_stop(void)
{
	int     i;

	for (i = 0; i < MAX_PARTS; i++) {
		if (engine_thread_p[i] != 0) {
			pthread_join(engine_thread_p[i],  NULL);
		}
	}
	usleep(125000);
}


/*****************************************************************************
 * wait_engine_start()
 *****************************************************************************/
void
wait_engine_start(void)
{
	int     i;

	for (i = 0; i < MAX_PARTS; i++) {
		//while (g_atomic_int_get(&engine_ready[i]) == 0) {
		//	usleep(125000);
		//}
		/* wait until gtkui thread is ready */
		pthread_mutex_lock(&engine_ready_mutex[i]);
		if (!engine_ready[i]) {
			pthread_cond_wait(&engine_ready_cond[i], &engine_ready_mutex[i]);
		}
		pthread_mutex_unlock(&engine_ready_mutex[i]);
	}
}


/*****************************************************************************
 * stop_audio()
 *****************************************************************************/
void
stop_audio(void)
{
	audio_stopped  = 1;
	if (audio_stop_func != NULL) {
		audio_stop_func();
	}
	init_engine_buffers();
}


/*****************************************************************************
 * stop_midi()
 *****************************************************************************/
void
stop_midi(void)
{
	midi_stopped = 1;
	if (midi_stop_func != NULL) {
		midi_stop_func();
	}
}


/*****************************************************************************
 * restart_audio()
 *****************************************************************************/
void
restart_audio(void)
{
	if (audio_restart_func == NULL) {
		stop_audio();
		start_audio();
	}
	else {
		audio_restart_func();
	}
}


/*****************************************************************************
 * restart_midi()
 *****************************************************************************/
void
restart_midi(void)
{
	if (midi_restart_func == NULL) {
		stop_midi();
		start_midi();
	}
	else {
		midi_restart_func();
	}
	start_midi_clock();
}


/*****************************************************************************
 * phasex_watchdog()
 *****************************************************************************/
void
phasex_watchdog(void)
{
	unsigned int    part_num;
	PATCH           *patch;

	while (!pending_shutdown) {
		if (audio_watchdog_func != NULL) {
			audio_watchdog_func();
		}
		if (midi_watchdog_func != NULL) {
			midi_watchdog_func();
		}

#ifndef WITHOUT_LASH
		if (!lash_disabled && !pending_shutdown) {
			lash_poll_event();
		}
#endif
		if (sample_rate_mode_changed) {
			sample_rate_mode_changed = 0;
			sample_rate_changed = 1;
			stop_engine();
			stop_midi();
			stop_audio();
		}
		if (engine_stopped && !pending_shutdown) {
			wait_engine_stop();
		}
		if (midi_stopped && !pending_shutdown) {
			wait_midi_stop();
			if (midi_driver != setting_midi_driver) {
				if ((midi_driver == MIDI_DRIVER_JACK) ||
				    (setting_midi_driver == MIDI_DRIVER_JACK)) {
					stop_audio();
				}
				select_midi_driver(NULL, setting_midi_driver);
			}
		}
		if (audio_stopped && !pending_shutdown) {
			wait_audio_stop();
			if (audio_driver != setting_audio_driver) {
				select_audio_driver(NULL, setting_audio_driver);
			}
			init_audio();
			if (sample_rate_changed) {
				sample_rate_changed = 0;
				build_filter_tables();
				build_env_tables();
				init_engine_internals();
				init_engine_parameters();
				for (part_num = 0; part_num < MAX_PARTS; part_num++) {
					patch = get_active_patch(part_num);
					init_patch_state(patch);
				}
			}
		}
		if (engine_stopped && !pending_shutdown) {
			engine_stopped = 0;
			start_engine_threads();
		}
		if (audio_stopped && !pending_shutdown) {
			audio_stopped = 0;
			start_audio();
			wait_audio_start();
			if (gtkui_ready && (config_dialog != NULL) && (audio_status_label != NULL)) {
				query_audio_driver_status(audio_driver_status_msg);
				gtk_label_set_text(GTK_LABEL(audio_status_label), audio_driver_status_msg);
			}
		}
		if (midi_stopped && !pending_shutdown) {
			midi_stopped = 0;
			init_midi();
			start_midi();
			wait_midi_start();
		}
		if (config_changed && !engine_stopped && !audio_stopped && !midi_stopped &&
		    (audio_driver != AUDIO_DRIVER_NONE) && (midi_driver != MIDI_DRIVER_NONE)) {
			config_changed = 0;
			save_settings(NULL);
		}

		usleep(125000);
	}
}


/*****************************************************************************
 * scan_audio_and_midi()
 *****************************************************************************/
void
scan_audio_and_midi(void)
{
	ALSA_PCM_HW_INFO        *pcm_hw_list;
	ALSA_SEQ_INFO           *alsa_seq_info;
	ALSA_SEQ_PORT           *seq_port_list;
	ALSA_RAWMIDI_HW_INFO    *rawmidi_hw_list;

	/* scan for ALSA PCM capture and playback devices */
#ifdef ENABLE_INPUTS
	if (alsa_pcm_capture_hw != NULL) {
		alsa_pcm_hw_list_free(alsa_pcm_capture_hw);
	}
	if ((alsa_pcm_capture_hw = alsa_pcm_get_hw_list(SND_PCM_STREAM_CAPTURE)) == NULL) {
		printf("Unable to get ALSA PCM CAPTURE hardware device list.\n");
		return;
	}
#endif
	if (alsa_pcm_playback_hw != NULL) {
		alsa_pcm_hw_list_free(alsa_pcm_playback_hw);
	}
	if ((alsa_pcm_playback_hw = alsa_pcm_get_hw_list(SND_PCM_STREAM_PLAYBACK)) == NULL) {
		printf("Unable to get ALSA PCM PLAYBACK hardware device list.\n");
		return;
	}

#ifdef ENABLE_INPUTS
	/* output list of detected capture devices */
	pcm_hw_list = alsa_pcm_capture_hw;
	printf("Found ALSA PCM CAPTURE hardware devices:\n");
	while (pcm_hw_list != NULL) {
		printf("    [%s]\t%s: %s\n",
		       pcm_hw_list->alsa_name,
		       pcm_hw_list->card_name,
		       pcm_hw_list->device_name);
		pcm_hw_list = pcm_hw_list->next;
	}
	printf("\n");
#endif
	/* output list of detected playback devices */
	pcm_hw_list = alsa_pcm_playback_hw;
	printf("Found ALSA PCM PLAYBACK hardware devices:\n");
	while (pcm_hw_list != NULL) {
		printf("    [%s]\t%s: %s\n",
		       pcm_hw_list->alsa_name,
		       pcm_hw_list->card_name,
		       pcm_hw_list->device_name);
		pcm_hw_list = pcm_hw_list->next;
	}
	printf("\n");

	if ((alsa_seq_info = malloc(sizeof(ALSA_SEQ_INFO))) == NULL) {
		phasex_shutdown("Out of memory!\n");
	}
	memset(alsa_seq_info, 0, sizeof(ALSA_SEQ_INFO));
	alsa_seq_info->seq = NULL;

	/* open the sequencer */
	if (snd_seq_open(& (alsa_seq_info->seq), "default",
	                 SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) < 0) {
		printf("Unable to open ALSA sequencer.\n");
		return;
	}

	/* get list of all capture ports  */
	if ((alsa_seq_info->capture_ports =
	     alsa_seq_get_port_list(alsa_seq_info,
	                            (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ),
	                            alsa_seq_info->capture_ports)) == NULL) {
		printf("Unable to get ALSA sequencer port list.\n");
	}
	seq_port_list = alsa_seq_info->capture_ports;
	if (seq_port_list != NULL) {
		printf("Found ALSA sequencer capture ports:\n");
	}
	while (seq_port_list != NULL) {
		printf("    [%s]\t%s: %s\n",
		       seq_port_list->alsa_name,
		       seq_port_list->client_name,
		       seq_port_list->port_name);
		seq_port_list = seq_port_list->next;
	}

	alsa_seq_port_free(alsa_seq_info->capture_ports);
	snd_seq_close(alsa_seq_info->seq);
	free(alsa_seq_info);

#ifdef ENABLE_RAWMIDI_ALSA_RAW
	if (alsa_rawmidi_hw != NULL) {
		alsa_rawmidi_hw_info_free(alsa_rawmidi_hw);
	}
	alsa_rawmidi_hw = alsa_rawmidi_get_hw_list();
	if (alsa_rawmidi_hw != NULL) {
		printf("\nFound ALSA Raw MIDI hardware devices:\n");
		rawmidi_hw_list = alsa_rawmidi_hw;
		while (rawmidi_hw_list != NULL) {
			printf("    [%s]\t%s: %s: %s\n",
			       rawmidi_hw_list->alsa_name,
			       rawmidi_hw_list->device_id,
			       rawmidi_hw_list->device_name,
			       rawmidi_hw_list->subdevice_name);
			rawmidi_hw_list = rawmidi_hw_list->next;
		}
	}
#endif
}


/*****************************************************************************
 * audio_driver_running()
 *****************************************************************************/
int
audio_driver_running(void)
{
	switch (audio_driver) {
	case AUDIO_DRIVER_ALSA_PCM:
		return audio_ready;
	case AUDIO_DRIVER_JACK:
		return audio_ready;
	}
	return 0;
}


/*****************************************************************************
 * query_audio_driver_status()
 *****************************************************************************/
void
query_audio_driver_status(char *buf)
{
	snprintf(buf, 256,
	         "Audio Driver:  %s\n"
	         "Status:  %s\n"
	         "Sample Rate:  %d\n"
	         "Sample Size:  %u bits\n"
	         "Period Size:  %d samples\n",
	         audio_driver_names[audio_driver],
	         (audio_driver_running()) ? "OK / Running" : "Not Running",
	         sample_rate,
	         ((audio_driver == AUDIO_DRIVER_JACK) ?
	          (unsigned int)(sizeof(jack_default_audio_sample_t) * 8) :
	          (unsigned int) alsa_pcm_format_bits),
	         buffer_period_size
	         );
}
