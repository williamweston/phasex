/*****************************************************************************
 *
 * rawmidi.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2001-2004,2012 William Weston <whw@linuxmail.org>
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
#include <ctype.h>
#include <pthread.h>
#include <asoundlib.h>
#include "phasex.h"
#include "timekeeping.h"
#include "settings.h"
#include "rawmidi.h"
#include "buffer.h"
#include "mididefs.h"
#include "midi_event.h"
#include "midi_process.h"
#include "engine.h"
#include "driver.h"
#include "debug.h"


RAWMIDI_INFO            *rawmidi_info;

#ifdef HAVE_CLOCK_NANOSLEEP
struct timespec         rawmidi_sleep_time       = { 0, 100000 };
#else
int                     rawmidi_sleep_time       = 100;
#endif

unsigned char           midi_realtime_type[32];
int                     realtime_event_count     = 0;

#ifdef ENABLE_RAWMIDI_ALSA_RAW
ALSA_RAWMIDI_HW_INFO    *alsa_rawmidi_hw         = NULL;

int                     alsa_rawmidi_hw_changed  = 0;


/******************************************************************************
 * alsa_rawmidi_hw_info_free()
 ******************************************************************************/
void
alsa_rawmidi_hw_info_free(ALSA_RAWMIDI_HW_INFO *hwinfo)
{
	ALSA_RAWMIDI_HW_INFO    *cur = hwinfo;
	ALSA_RAWMIDI_HW_INFO    *next;

	while (cur != NULL) {
		if (cur != NULL) {
			if (cur->device_id != NULL) {
				free(cur->device_id);
			}
			if (cur->device_name != NULL) {
				free(cur->device_name);
			}
			if (cur->subdevice_name != NULL) {
				free(cur->subdevice_name);
			}
		}
		next = cur->next;
		free(cur);
		cur = next;
	}
}


/******************************************************************************
 * alsa_rawmidi_hw_list_compare()
 ******************************************************************************/
int
alsa_rawmidi_hw_list_compare(ALSA_RAWMIDI_HW_INFO *a, ALSA_RAWMIDI_HW_INFO *b)
{
	ALSA_RAWMIDI_HW_INFO    *cur_a = a;
	ALSA_RAWMIDI_HW_INFO    *cur_b = b;

	while (cur_a != NULL) {
		if ((cur_b == NULL) ||
		    (cur_a->card_num != cur_b->card_num) ||
		    (cur_a->device_num != cur_b->device_num) ||
		    (cur_a->subdevice_num != cur_b->subdevice_num) ||
		    (strcmp(cur_a->device_id, cur_b->device_id) != 0) ||
		    (strcmp(cur_a->device_name, cur_b->device_name) != 0) ||
		    (strcmp(cur_a->subdevice_name, cur_b->subdevice_name) != 0)) {
			return 1;
		}
		cur_a = cur_a->next;
		cur_b = cur_b->next;
	}
	if (cur_b != NULL) {
		return 1;
	}

	return 0;
}


/******************************************************************************
 * alsa_rawmidi_get_hw_list()
 ******************************************************************************/
ALSA_RAWMIDI_HW_INFO *
alsa_rawmidi_get_hw_list(void)
{
	ALSA_RAWMIDI_HW_INFO    *head = NULL;
	ALSA_RAWMIDI_HW_INFO    *cur  = NULL;
	ALSA_RAWMIDI_HW_INFO    *hwinfo;
	snd_ctl_t               *handle;
	snd_rawmidi_info_t      *alsaraw_info;
	char                    alsa_name[32];
	int                     card_num;
	int                     device_num;
	unsigned int            subdevice_num;
	unsigned int            num_subdevices;
	int                     err;

	card_num = -1;
	while (snd_card_next(&card_num) >= 0) {
		if (card_num < 0) {
			break;
		}
		sprintf(alsa_name, "hw:%d", card_num);
		if ((err = snd_ctl_open(&handle, alsa_name, 0)) < 0) {
			PHASEX_DEBUG(DEBUG_CLASS_RAW_MIDI,
			             "Unable to open ALSA control for card %d: %s\n",
			             card_num, snd_strerror(err));
			continue;
		}
		device_num = -1;
		while (snd_ctl_rawmidi_next_device(handle, &device_num) >= 0) {
			if (device_num < 0) {
				break;
			}
			snd_rawmidi_info_alloca(&alsaraw_info);
			memset(alsaraw_info, 0, snd_rawmidi_info_sizeof());

			snd_rawmidi_info_set_device(alsaraw_info, (unsigned int) device_num);
			snd_rawmidi_info_set_subdevice(alsaraw_info, 0);
			snd_rawmidi_info_set_stream(alsaraw_info, SND_RAWMIDI_STREAM_INPUT);

#ifdef ALSA_SCAN_SUBDEVICES
			num_subdevices = snd_rawmidi_info_get_subdevices_count(alsaraw_info);
#else
			num_subdevices = 1;
#endif

			for (subdevice_num = 0; subdevice_num < num_subdevices; subdevice_num++) {
				snd_rawmidi_info_set_subdevice(alsaraw_info, subdevice_num);
				if (snd_ctl_rawmidi_info(handle, alsaraw_info) < 0) {
					PHASEX_DEBUG(DEBUG_CLASS_RAW_MIDI,
					             "snd_ctl_rawmidi_info (hw:%d,%d,%d) failed:  %s\n",
					             card_num, device_num, subdevice_num, snd_strerror(err));
					continue;
				}
				if (subdevice_num == 0) {
					num_subdevices = snd_rawmidi_info_get_subdevices_count(alsaraw_info);
				}
				if ((hwinfo = malloc(sizeof(ALSA_RAWMIDI_HW_INFO))) == NULL) {
					phasex_shutdown("Out of Memory!\n");
				}
				hwinfo->card_num       = card_num;
				hwinfo->device_num     = device_num;
				hwinfo->subdevice_num  = (int) subdevice_num;
				hwinfo->device_id      = strdup(snd_rawmidi_info_get_id(alsaraw_info));
				hwinfo->device_name    = strdup(snd_rawmidi_info_get_name(alsaraw_info));
				hwinfo->subdevice_name = strdup(snd_rawmidi_info_get_subdevice_name(alsaraw_info));
				if (num_subdevices > 1) {
					snprintf(hwinfo->alsa_name,
					         sizeof(hwinfo->alsa_name),
					         "hw:%d,%d,%d",
					         card_num,
					         device_num,
					         subdevice_num);
				}
				else {
					snprintf(hwinfo->alsa_name,
					         sizeof(hwinfo->alsa_name),
					         "hw:%d,%d",
					         card_num,
					         device_num);
				}
				hwinfo->connect_request    = 0;
				hwinfo->disconnect_request = 0;
				hwinfo->next = NULL;
				if (head == NULL) {
					head = cur = hwinfo;
				}
				else {
					cur->next = hwinfo;
					cur = cur->next;
				}
			}
		}
		snd_ctl_close(handle);
	}

	return head;
}
#endif /* ENABLE_RAWMIDI_ALSA_RAW */


/******************************************************************************
 * rawmidi_open()
 *  char        *device
 *
 * Opens raw MIDI device specified by <device> with semantics for OSS, ALSA,
 * and generic raw MIDI according to the currently selected MIDI driver.
 ******************************************************************************/
RAWMIDI_INFO *
rawmidi_open(char *device)
{
	RAWMIDI_INFO        *rawmidi;
	int                 flags;
#ifdef ENABLE_RAWMIDI_GENERIC
	int                 flush_input    = 1;
#endif
#ifdef ENABLE_RAWMIDI_ALSA_RAW
	int                 err;
#endif

	/* allocate mem */
	if ((rawmidi = malloc(sizeof(RAWMIDI_INFO))) == NULL) {
		PHASEX_ERROR("Unable to malloc() -- %s\n", strerror(errno));
		return NULL;
	}
	/* memset here causes rawmidi thread to hang.... */
#if defined(ENABLE_RAWMIDI_OSS) || defined(ENABLE_RAWMIDI_GENERIC)
	rawmidi->fd     = -1;
#endif
#if defined(ENABLE_RAWMIDI_ALSA_RAW) || defined(ENABLE_RAWMIDI_GENERIC)
	rawmidi->handle = NULL;
	rawmidi->pfds   = NULL;
	rawmidi->npfds  = 0;
#endif
	rawmidi->device = NULL;

	/* use default device appropriate for driver type if none given */
	if ((device == NULL) || (device[0] == '\0') || (strcmp(device, "auto") == 0)) {
		switch (midi_driver) {
#ifdef ENABLE_RAWMIDI_OSS
		case MIDI_DRIVER_RAW_OSS:
			rawmidi->device = strdup(RAWMIDI_OSS_DEVICE);
			break;
#endif
#ifdef ENABLE_RAWMIDI_ALSA_RAW
		case MIDI_DRIVER_RAW_ALSA:
			if (alsa_rawmidi_hw != NULL) {
				rawmidi->device = strdup(alsa_rawmidi_hw->alsa_name);
			}
			else {
				rawmidi->device = strdup(RAWMIDI_ALSA_DEVICE);
			}
			break;
#endif
#ifdef ENABLE_RAWMIDI_GENERIC
		case MIDI_DRIVER_RAW_GENERIC:
			rawmidi->device = strdup(RAWMIDI_RAW_DEVICE);
			break;
#endif
		}
	}
	else {
		rawmidi->device = strdup(device);
	}

	PHASEX_DEBUG(DEBUG_CLASS_INIT, "Opening Raw MIDI device '%s'...\n", rawmidi->device);

	/* separate device open methods for OSS/RAW and ALSA insterfaces */
	switch (midi_driver) {
#ifdef ENABLE_RAWMIDI_OSS
	case MIDI_DRIVER_RAW_OSS:
#endif
#ifdef ENABLE_RAWMIDI_GENERIC
	case MIDI_DRIVER_RAW_GENERIC:
#endif
#if defined(ENABLE_RAWMIDI_OSS) || defined(ENABLE_RAWMIDI_GENERIC)
		/* open and close nonblocking first to flush */
		flags = O_RDONLY | O_NONBLOCK;
		if ((rawmidi->fd = open(rawmidi->device, flags)) < 0) {
			PHASEX_WARN("Unable to open raw MIDI device '%s' in nonblocking mode -- %s\n",
			            rawmidi->device, strerror(errno));
			flush_input = 0;
		}
		/* flush anything currently in the buffer */
		if (flush_input) {
			rawmidi_flush(rawmidi);
			close(rawmidi->fd);
		}
#  if !defined(RAWMIDI_GENERIC_NONBLOCK) && !defined(RAWMIDI_USE_POLL)
		flags = O_RDONLY;
#  endif
		/* open device readonly for blocking io */
		if ((rawmidi->fd = open(rawmidi->device, flags)) < 0) {
			PHASEX_ERROR("Unable to open raw MIDI device '%s' for read -- %s\n",
			             rawmidi->device, strerror(errno));
			rawmidi_free(rawmidi);
			return NULL;
		}
		break;
#endif /* ENABLE_RAWMIDI_OSS || ENABLE_RAWMIDI_GENERIC */

#ifdef ENABLE_RAWMIDI_ALSA_RAW
	case MIDI_DRIVER_RAW_ALSA:
		if (rawmidi->device != NULL) {
			if ((err = snd_rawmidi_open(& (rawmidi->handle),
			                            NULL,
			                            rawmidi->device,
			                            O_RDONLY
# if defined(RAWMIDI_ALSA_NONBLOCK)
			                            | SND_RAWMIDI_NONBLOCK
# endif
			                            ))) {
				PHASEX_ERROR("snd_rawmidi_open %s failed: %d\n",
				             rawmidi->device, err);
				rawmidi->handle = NULL;
				rawmidi_free(rawmidi);
				return NULL;
			}
# if defined(RAWMIDI_ALSA_NONBLOCK)
			if (snd_rawmidi_nonblock(rawmidi->handle, 1) != 0) {
				PHASEX_ERROR("Unable to set nonblock mode for ALSA rawmidi.\n");
				rawmidi_free(rawmidi);
				return NULL;
			}
# endif
# if defined(RAWMIDI_ALSA_NONBLOCK) || defined(RAWMIDI_USE_POLL)
			/* in case we opened nonblocking, we need our poll descriptors */
			if ((rawmidi->npfds = snd_rawmidi_poll_descriptors_count(rawmidi->handle)) > 0) {
				if ((rawmidi->pfds = malloc((size_t)(rawmidi->npfds *
				                                     (int) sizeof(struct pollfd)))) == NULL) {
					phasex_shutdown("Out of memory!\n");
				}
				if (snd_rawmidi_poll_descriptors(rawmidi->handle, rawmidi->pfds,
				                                 (unsigned int) rawmidi->npfds) <= 0) {
					PHASEX_ERROR("No ALSA rawmidi descriptors to poll.\n");
					rawmidi_free(rawmidi);
					return NULL;
				}
			}
# endif /* RAWMIDI_ALSA_NONBLOCK || RAWMIDI_USE_POLL */

		}
		PHASEX_DEBUG(DEBUG_CLASS_INIT, "Opened Raw MIDI device '%s'.\n", rawmidi->device);
		break;
#endif /* ENABLE_RAWMIDI_ALSA_RAW */
	default:
		break;
	}

	switch (midi_driver) {
#ifdef ENABLE_RAWMIDI_ALSA_RAW
	case MIDI_DRIVER_RAW_ALSA:
		/* now that midi has opened, set setting_alsa_raw_midi_device */
		if ((setting_alsa_raw_midi_device == NULL) ||
		    (strcmp(setting_alsa_raw_midi_device, rawmidi->device) != 0)) {
			if (setting_alsa_raw_midi_device != NULL) {
				free(setting_alsa_raw_midi_device);
			}
			setting_alsa_raw_midi_device = strdup(rawmidi->device);
			config_changed = 1;
		}
		break;
#endif
#ifdef ENABLE_RAWMIDI_GENERIC
	case MIDI_DRIVER_RAW_GENERIC:
		break;
#endif
#ifdef ENABLE_RAWMIDI_OSS
	case MIDI_DRIVER_RAW_OSS:
		break;
#endif
	}

	/* Return the abstracted rawmidi device */
	return rawmidi;
}


/******************************************************************************
 * rawmidi_close()
 *  RAWMIDI_INFO *rawmidi
 *
 * Closes the rawmidi interfaces and upon success clears the file
 * descriptors held in the internal structure.  Returns zero on
 * success, or -1 on error.
 ******************************************************************************/
int
rawmidi_close(RAWMIDI_INFO *rawmidi)
{
	int     retval  = -1;

	/* separate device open methods for OSS and ALSA insterfaces */
	switch (midi_driver) {
#ifdef ENABLE_RAWMIDI_OSS
	case MIDI_DRIVER_RAW_OSS:
#endif
#ifdef ENABLE_RAWMIDI_GENERIC
	case MIDI_DRIVER_RAW_GENERIC:
#endif
#if defined(ENABLE_RAWMIDI_OSS) || defined(ENABLE_RAWMIDI_GENERIC)
		/* sanity checks */
		if (rawmidi == NULL) {
			return -1;
		}
		if (rawmidi->fd < 0) {
			return -1;
		}

		/* close device */
		if ((retval = close(rawmidi->fd)) == 0) {
			rawmidi->fd = -1;
		}
		if (retval == 0) {
			return 0;
		}

		return -1;
#endif /* ENABLE_RAWMIDI_OSS || ENABLE_RAWMIDI_GENERIC */
#ifdef ENABLE_RAWMIDI_ALSA_RAW
	case MIDI_DRIVER_RAW_ALSA:
		/* sanity checks */
		if (rawmidi == NULL) {
			return -1;
		}
		if (rawmidi->handle == NULL) {
			return -1;
		}

		/* close device */
		if ((retval = snd_rawmidi_close(rawmidi->handle)) == 0) {
			rawmidi->handle = NULL;
		}
		if (retval == 0) {
			return 0;
		}

		return -1;
#endif /* ENABLE_RAWMIDI_ALSA_RAW */
	default:
		break;
	}

	return retval;
}


/******************************************************************************
 * rawmidi_free()
 *  RAWMIDI_INFO *rawmidi
 *
 * Frees all memory and descriptors associated with a rawmidi
 * structure.  Returns zero on success, or -1 on error.
 ******************************************************************************/
int
rawmidi_free(RAWMIDI_INFO *rawmidi)
{
	/* close descriptors */
	rawmidi_close(rawmidi);

	/* free allocated strings */
	if (rawmidi->device != NULL) {
		free(rawmidi->device);
	}

	if (rawmidi->pfds != NULL) {
		free(rawmidi->pfds);
	}

	/* free allocated structure */
	free(rawmidi);

	return 0;
}


/******************************************************************************
 * rawmidi_read()
 *  RAWMIDI_INFO    *rawmidi
 *  char            *buf
 *  int             len
 *
 * Reads <len> bytes from the rawmidi input device and places the
 * contents in <buf>.  Much like the read() (2) system call, <buf>
 * must be of sufficient size for the requested length.  Return value
 * is the number of raw midi bytes read, or -1 on error.
 ******************************************************************************/
int
rawmidi_read(RAWMIDI_INFO *rawmidi, unsigned char *buf, int len)
{
#ifdef ENABLE_RAWMIDI_OSS
	unsigned char           ibuf[4];
#endif /* ENABLE_RAWMIDI_OSS */
	int                     bytes_read      = 0;
#if defined(ENABLE_RAWMIDI_ALSA_RAW) && defined(RAWMIDI_DEBUG)
	static unsigned char    read_buf[256];
	static int              buf_index       = 0;
	static ssize_t          buf_available   = 0;
	int                     output_index    = 0;
#endif /* ENABLE_RAWMIDI_ALSA_RAW && RAWMIDI_ALSA_MULTI_BYTE_IO */

	switch (midi_driver) {

		/* for OSS, filter event type and extract raw data from event quads */
#ifdef ENABLE_RAWMIDI_OSS
	case MIDI_DRIVER_RAW_OSS:
		/* strip raw midi message out of larger OSS message */
		while (bytes_read < len) {
			/* read one event quad at a time */
			if (read(rawmidi->fd, ibuf, 4) != 4) {
				PHASEX_ERROR("Unable to read from OSS MIDI device '%s' -- %s!\n",
				             rawmidi->device, strerror(errno));
				break;
			}
			/* only interested in raw midi data type events */
			if (ibuf[0] != SEQ_MIDIPUTC) {
				continue;
			}
			/* only take the second of every four bytes */
			buf[bytes_read++] = ibuf[1];
		}
		break;
#endif /* ENABLE_RAWMIDI_OSS */

		/* for ALSA, single byte IO seems more dependable */
#ifdef ENABLE_RAWMIDI_ALSA_RAW
	case MIDI_DRIVER_RAW_ALSA:
# ifdef RAWMIDI_ALSA_MULTI_BYTE_IO
		if (buf_available > 0) {
			while ((buf_index < buf_available) && (output_index < len)) {
				buf[output_index++] = read_buf[buf_index++];
			}
		}
		if (output_index < len) {
#  if defined(RAWMIDI_ALSA_NONBLOCK) || defined(RAWMIDI_USE_POLL)
			if (poll(rawmidi_info->pfds, (nfds_t) rawmidi_info->npfds, 10) > 0)
#  endif
				{
					if ((buf_available = snd_rawmidi_read(rawmidi->handle, read_buf, 256)) < 1) {
						PHASEX_ERROR("Unable to read from ALSA MIDI device '%s'!\n",
						             rawmidi->device);
					}
					buf_index = 0;
					while ((buf_index < buf_available) && (output_index < len)) {
						buf[output_index++] = read_buf[buf_index++];
					}
				}
		}
#  ifdef RAWMIDI_DEBUG
		for (bytes_read = 0; bytes_read < buf_available; bytes_read++) {
			PHASEX_DEBUG(DEBUG_CLASS_RAW_MIDI, "%02X ", read_buf[bytes_read]);
		}
#  endif /* RAWMIDI_DEBUG */
		bytes_read = output_index;
# else /* !RAWMIDI_ALSA_MULTI_BYTE_IO */
#  if defined(RAWMIDI_ALSA_NONBLOCK) || defined(RAWMIDI_USE_POLL)
		if (poll(rawmidi_info->pfds, (nfds_t) rawmidi_info->npfds, 10) > 0)
#  endif
			{
				for (bytes_read = 0; bytes_read < len; bytes_read++) {
					if (snd_rawmidi_read(rawmidi->handle, & (buf[bytes_read]), 1) != 1) {
						PHASEX_ERROR("Unable to read from ALSA MIDI device '%s'!\n", rawmidi->device);
						break;
					}
#  ifdef RAWMIDI_DEBUG
					PHASEX_DEBUG(DEBUG_CLASS_RAW_MIDI, "%02X ", buf[bytes_read]);
#  endif /* RAWMIDI_DEBUG */
				}
			}
# endif /* RAWMIDI_ALSA_MULTI_BYTE_IO */
		break;
#endif /* ENABLE_RAWMIDI_ALSA_RAW */

		/* for raw interfaces, read one byte at a time */
#ifdef ENABLE_RAWMIDI_GENERIC
	case MIDI_DRIVER_RAW_GENERIC:
		/* read one byte at a time to make some interfaces happy */
		for (bytes_read = 0; bytes_read < len; bytes_read++) {
# ifdef RAWMIDI_GENERIC_NONBLOCK
			while (!pending_shutdown && (read(rawmidi->fd, &buf[bytes_read], 1) != 1)) {
#  ifdef HAVE_CLOCK_NANOSLEEP
				clock_nanosleep(CLOCK_MONOTONIC, 0, &rawmidi_sleep_time, NULL);
#  else
				usleep(rawmidi_sleep_time);
#  endif
			}
# else /* !RAWMIDI_GENERIC_NONBLOCK */
			if (!pending_shutdown && (read(rawmidi->fd, &buf[bytes_read], 1) != 1)) {
				PHASEX_ERROR("Unable to read from Raw MIDI device '%s' -- %s!\n",
				             rawmidi->device, strerror(errno));
				break;
			}
# endif /* !RAWMIDI_GENERIC_NONBLOCK */
# ifdef RAWMIDI_DEBUG
			PHASEX_DEBUG(DEBUG_CLASS_RAW_MIDI, "%02X ", buf[bytes_read]);
# endif /* RAWMIDI_DEBUG */
		}
		break;
#endif /* ENABLE_RAWMIDI_GENERIC */
	}

	/* return number of midi message bytes read */
	return bytes_read;
}


/******************************************************************************
 * rawmidi_flush()
 *  RAWMIDI_INFO *rawmidi
 *
 * Flushes any data waiting to be read from the rawmidi device and
 * simply throws it away.  Nonblocking IO is used to avoid select() or
 * poll() calls, which may not be dependable for some drivers.  Return
 * value is zero on success, or -1 on error.
 ******************************************************************************/
int
rawmidi_flush(RAWMIDI_INFO *rawmidi)
{
#if defined(ENABLE_RAWMIDI_OSS) || defined(ENABLE_RAWMIDI_GENERIC)
	long        flags;
	char        buf[2];
#endif /* ENABLE_RAWMIDI_OSS || ENABLE_RAWMIDI_GENERIC */

	switch (midi_driver) {
#ifdef ENABLE_RAWMIDI_OSS
	case MIDI_DRIVER_RAW_OSS:
#endif
#ifdef ENABLE_RAWMIDI_GENERIC
	case MIDI_DRIVER_RAW_GENERIC:
#endif
#if defined(ENABLE_RAWMIDI_OSS) || defined(ENABLE_RAWMIDI_GENERIC)
		/* play tricks with nonblocking io and sync to flush the buffer with
		   standard read(). */

		/* get flags from raw midi descriptor and set to nonblock */
		flags = fcntl(rawmidi->fd, F_GETFL, 0);
# ifdef RAWMIDI_FLUSH_NONBLOCK
		if (fcntl(rawmidi->fd, F_SETFL, flags | O_NONBLOCK) != 0) {
			PHASEX_WARN("Unable to set nonblocking IO for raw MIDI -- %s\n",
			            strerror(errno));
			return -1;
		}
# endif /* RAWMIDI_FLUSH_NONBLOCK */

		/* For some interfaces, a call to fdatasync() may help ensure that the
		   entire buffer gets flushed.  This has not been known to cause error
		   with interfaces that do not require it.  The same cannot be said
		   for fsync(), which attempts to flush metadata which may not be
		   implemented by the driver. */
		fdatasync(rawmidi->fd);

		/* read raw midi device until there's no data left */
		while (read(rawmidi->fd, buf, 1) == 1) {
			usleep(100);
		}

# ifdef RAWMIDI_FLUSH_NONBLOCK
		/* set original flags for raw midi input descriptor */
		if (fcntl(rawmidi->fd, F_SETFL, flags) != 0) {
			PHASEX_WARN("Unable to set blocking IO for raw MIDI -- %s\n",
			            strerror(errno));
			return -1;
		}
# endif /* RAWMIDI_FLUSH_NONBLOCK */

		return 0;
#endif /* ENABLE_RAWMIDI_OSS || ENABLE_RAWMIDI_GENERIC */
#ifdef ENABLE_RAWMIDI_ALSA_RAW
	case MIDI_DRIVER_RAW_ALSA:
# ifdef RAWMIDI_ALSA_NONBLOCK
		while (snd_rawmidi_read(rawmidi->handle, buf, 1) == 1) {
			usleep(100);
		}
		//              snd_rawmidi_drop(rawmidi->handle);
# endif /* RAWMIDI_ALSA_NONBLOCK */
		return 0;
#endif /* ENABLE_RAWMIDI_ALSA_RAW */
	}

	return 0;
}


/*****************************************************************************
 * rawmidi_watchdog_cycle()
 *****************************************************************************/
void
rawmidi_watchdog_cycle(void)
{
	ALSA_RAWMIDI_HW_INFO    *cur;
	ALSA_RAWMIDI_HW_INFO    *old_rawmidi_hw;
	ALSA_RAWMIDI_HW_INFO    *new_rawmidi_hw;

	//    PHASEX_DEBUG (DEBUG_CLASS_MIDI, "--- rawmidi watchdog ---\n");

	if ((midi_driver == MIDI_DRIVER_RAW_ALSA) && (rawmidi_info != NULL)) {
		old_rawmidi_hw = alsa_rawmidi_hw;
		cur = alsa_rawmidi_hw;
		while (cur != NULL) {
			if (cur->connect_request) {
				cur->connect_request    = 0;
				cur->disconnect_request = 0;
				stop_midi();
				wait_midi_stop();
				if (setting_alsa_raw_midi_device != NULL) {
					free(setting_alsa_raw_midi_device);
				}
				setting_alsa_raw_midi_device = strdup(cur->alsa_name);
				rawmidi_init();
				start_midi();
				wait_midi_start();
				break;

			}
			/* ignore disconnect requests, since we can only open 1 at a time. */
			cur = cur->next;
			if (alsa_rawmidi_hw == NULL) {
				break;
			}
		}
		new_rawmidi_hw = alsa_rawmidi_get_hw_list();
		if (alsa_rawmidi_hw_list_compare(old_rawmidi_hw, new_rawmidi_hw) == 0) {
			alsa_rawmidi_hw_info_free(new_rawmidi_hw);
		}
		else {
			alsa_rawmidi_hw = new_rawmidi_hw;
			alsa_rawmidi_hw_info_free(old_rawmidi_hw);
			alsa_rawmidi_hw_changed = 1;
		}
	}
}


/*****************************************************************************
 * rawmidi_init()
 *
 * Open MIDI device and leave in a ready state for the MIDI thread
 * to start reading events.
 *****************************************************************************/
int
rawmidi_init(void)
{
#ifdef ENABLE_RAWMIDI_ALSA_RAW
	ALSA_RAWMIDI_HW_INFO    *cur;
#endif
	char                    *device;

	switch (midi_driver) {
#ifdef ENABLE_RAWMIDI_ALSA_RAW
	case MIDI_DRIVER_RAW_ALSA:
		device = setting_alsa_raw_midi_device;
		break;
#endif
#ifdef ENABLE_RAWMIDI_GENERIC
	case MIDI_DRIVER_RAW_GENERIC:
		device = setting_generic_midi_device;
		break;
#endif
#ifdef ENABLE_RAWMIDI_OSS
	case MIDI_DRIVER_RAW_OSS:
		device = setting_oss_midi_device;
		break;
#endif
	default:
		device = NULL;
	}

#ifdef ENABLE_RAWMIDI_ALSA_RAW
	if (midi_driver == MIDI_DRIVER_RAW_ALSA) {
		if (alsa_rawmidi_hw != NULL) {
			alsa_rawmidi_hw_info_free(alsa_rawmidi_hw);
		}
		alsa_rawmidi_hw = alsa_rawmidi_get_hw_list();
		if ((alsa_rawmidi_hw != NULL) && (debug_class & DEBUG_CLASS_RAW_MIDI)) {
			PHASEX_DEBUG(DEBUG_CLASS_RAW_MIDI, "Found ALSA Raw MIDI hardware devices:\n");
			cur = alsa_rawmidi_hw;
			while (cur != NULL) {
				PHASEX_DEBUG(DEBUG_CLASS_RAW_MIDI,
				             "  [%s] %s: %s: %s\n",
				             cur->alsa_name,
				             cur->device_id,
				             cur->device_name,
				             cur->subdevice_name);
				cur = cur->next;
			}
		}
	}
#endif

	if ((rawmidi_info = rawmidi_open(device)) == NULL) {
		if (device != NULL) {
			if ((rawmidi_info = rawmidi_open(NULL)) == NULL) {
				return -1;
			}
		}
	}

	return 0;
}


/*****************************************************************************
 * rawmidi_cleanup()
 *  void *      arg
 *
 * Cleanup handler for RAWMIDI thread.
 * Closes RAWMIDI ports.
 *****************************************************************************/
void
rawmidi_cleanup(void *UNUSED(arg))
{
	if (rawmidi_info != NULL) {
		rawmidi_close(rawmidi_info);
		rawmidi_free(rawmidi_info);
		rawmidi_info = NULL;
	}

	/* Add some guard time, in case MIDI hardware is re-initialized soon. */
	usleep(125000);

	midi_thread_p = 0;
}


/*****************************************************************************
 * rawmidi_read_sysex ()
 *****************************************************************************/
void
rawmidi_read_sysex(void)
{
	unsigned char   midi_byte = 0xF0;
	do {
		if (rawmidi_read(rawmidi_info, (unsigned char *) &midi_byte, 1) == 1) {
			if ((midi_byte > 0xF0) && (midi_byte != 0xF7)) {
				PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "---<0x%x>--- ", midi_byte);
				midi_realtime_type[realtime_event_count++] = midi_byte;
			}
		}
		else {
			PHASEX_ERROR("*** MIDI Read Error! *** ");
		}
	}
	while (midi_byte != 0xF7);
}


/*****************************************************************************
 * rawmidi_read_byte ()
 *****************************************************************************/
unsigned char
rawmidi_read_byte(void)
{
	unsigned char   midi_byte;

	do {
		if (rawmidi_read(rawmidi_info, (unsigned char *) &midi_byte, 1) == 1) {
			if (midi_byte == 0xF0) {
				rawmidi_read_sysex();
				midi_byte = 0xF7;
			}
			else if (midi_byte > 0xF0) {
				PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "---<0x%x>--- ", midi_byte);
				midi_realtime_type[realtime_event_count++] = midi_byte;
			}
		}
		else {
			PHASEX_ERROR("*** MIDI Read Error! *** ");
		}
	}
	while (midi_byte >= 0xF0);

	return midi_byte;
}


/*****************************************************************************
 * rawmidi_thread()
 *
 * Raw MIDI input thread function.  Queues incoming MIDI events read from a
 * raw MIDI device.  Since this is raw (not event driven) MIDI input, this
 * thread will properly handle interleaved MIDI realtime events.
 *****************************************************************************/
void *
rawmidi_thread(void *UNUSED(arg))
{
	PART                *part;
	MIDI_EVENT          midi_event;
	MIDI_EVENT          *out_event      = &midi_event;
	timecalc_t          delta_nsec;
	struct timespec     now;
	struct sched_param  schedparam;
	pthread_t           thread_id;
	unsigned int        part_num;
	int                 running_status;
	unsigned int        cycle_frame     = 0;
	unsigned char       type            = MIDI_EVENT_NO_EVENT;
	unsigned char       channel         = 0x7F;
	unsigned char       midi_byte;
	int                 q;
	unsigned int        index           = 0;

	out_event->state = EVENT_STATE_ALLOCATED;

	/* set realtime scheduling and priority */
	thread_id = pthread_self();
	memset(&schedparam, 0, sizeof(struct sched_param));
	schedparam.sched_priority = setting_midi_priority;
	pthread_setschedparam(thread_id, setting_sched_policy, &schedparam);

	/* setup thread cleanup handler */
	pthread_cleanup_push(&rawmidi_cleanup, NULL);

	PHASEX_DEBUG(DEBUG_CLASS_RAW_MIDI, "Starting Raw MIDI thread...\n");

	/* flush MIDI input */
#ifdef RAWMIDI_FLUSH_ON_START
	rawmidi_flush(rawmidi_info);
#endif

	/* broadcast the midi ready condition */
	pthread_mutex_lock(&midi_ready_mutex);
	midi_ready = 1;
	pthread_cond_broadcast(&midi_ready_cond);
	pthread_mutex_unlock(&midi_ready_mutex);

	/* MAIN LOOP: read raw midi device and queue events */
	while (!midi_stopped && !pending_shutdown) {

		/* set thread cancelation point */
		pthread_testcancel();

		/* poll for new MIDI input */
#if defined(RAWMIDI_ALSA_NONBLOCK) || defined(RAWMIDI_USE_POLL)
		if ((midi_driver != MIDI_DRIVER_RAW_ALSA) ||
		    (poll(rawmidi_info->pfds, (nfds_t) rawmidi_info->npfds, 100) > 0))
#endif
			{
				/* start with first byte */
				if (rawmidi_read(rawmidi_info, (unsigned char *) &midi_byte, 1) == 1) {

					/* To determine message type, assuming no running status until
					   we learn otherwise. */
					running_status = 0;

					/* No status byte.  Use running status.  Set runing_status
					   flag so we don't try to read the second byte again. */
					if (midi_byte < 0x80) {
						running_status = 1;
					}
					/* status byte was found so keep track of message type and
					   channel. */
					else {
						type    = midi_byte & MIDI_TYPE_MASK;     // & 0xF0
						channel = midi_byte & MIDI_CHANNEL_MASK;  // & 0x0F
					}
					/* handle channel events (with or without status byte). */
					if (type < 0xF0) {
						out_event->type    = type;
						out_event->channel = channel;
						/* read second byte if we haven't already */
						if (running_status) {
							out_event->byte2 = midi_byte;
						}
						else {
							out_event->byte2 = rawmidi_read_byte();
						}
						/* all channel specific messages except program change and
						   polypressure have 2 bytes following status byte */
						if ((type == MIDI_EVENT_PROGRAM_CHANGE) ||
						    (type == MIDI_EVENT_POLYPRESSURE)) {
							out_event->byte3 = 0x00;
						}
						else {
							out_event->byte3 = rawmidi_read_byte();
						}
						/* get timestamp and determine index / frame position of
						   this event. */
						delta_nsec  = get_time_delta(&now);
						cycle_frame = get_midi_cycle_frame(delta_nsec);
						index = get_midi_index();
						PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
						             DEBUG_COLOR_CYAN "[%d] " DEBUG_COLOR_DEFAULT,
						             (index / buffer_period_size));
						/* queue for all parts that want it. */
						for (part_num = 0; part_num < MAX_PARTS; part_num++) {
							part = get_part(part_num);
							if ((channel == part->midi_channel) || (part->midi_channel == 16)) {
								queue_midi_event(part_num, out_event, cycle_frame, index);
							}
						}
					}
					/* handle system and realtime messages */
					else {
						PHASEX_DEBUG(DEBUG_CLASS_RAW_MIDI, "---<Sys Msg 0x%x>--- ", midi_byte);
						/* clear running status (by setting type) when we see
						   system (but not realtime) messages. */
						/* clock tick and realtime msgs (0xF8 and above) do not
						   clear running status. */
						if (midi_byte < 0xF8) {
							type           = midi_byte;
							channel        = 0x7F;
						}
						/* switch on midi_byte instead of type, since type could
						   have been masked for channel messages */
						switch (midi_byte) {
							/* variable length system messages */
						case MIDI_EVENT_SYSEX:          // 0xF0
							/* ignore sysex messages for now */
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "---<sysex>--- ");
							rawmidi_read_sysex();
							break;
							/* 3 byte system messages */
						case MIDI_EVENT_SONGPOS:        // 0xF2
							/* read 2 more bytes (watching out for interleaved
							   realtime msgs). */
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "---<0x%x>--- ", midi_byte);
							out_event->byte2 = rawmidi_read_byte();
							out_event->byte3 = rawmidi_read_byte();
							break;
							/* 2 byte system messages */
						case MIDI_EVENT_MTC_QFRAME:     // 0xF1
						case MIDI_EVENT_SONG_SELECT:    // 0xF3
							/* read 1 more byte (watching out for interleaved
							   realtime msgs). */
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "---<0x%x>--- ", midi_byte);
							out_event->byte2 = rawmidi_read_byte();
							out_event->byte3 = 0;
							break;
							/* 1 byte realtime messages */
						case MIDI_EVENT_STOP:           // 0xFC
						case MIDI_EVENT_SYSTEM_RESET:   // 0xFF
							/* send stop and reset events to all queues */
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "---<0x%x>--- ", midi_byte);
							/* get timestamp and determine index / frame position
							   of this event. */
							delta_nsec  = get_time_delta(&now);
							cycle_frame = get_midi_cycle_frame(delta_nsec);
							index = get_midi_index();
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
							             DEBUG_COLOR_CYAN "[%d] "
							             DEBUG_COLOR_DEFAULT,
							             (index / buffer_period_size));
							queue_midi_realtime_event(ALL_PARTS, midi_byte, cycle_frame, index);
							break;
						case MIDI_EVENT_ACTIVE_SENSING: // 0xFE
							set_active_sensing_timeout();
							break;
							/* ignored 1-byte system and realtime messages */
						case MIDI_EVENT_BUS_SELECT:     // 0xF5
						case MIDI_EVENT_TUNE_REQUEST:   // 0xF6
						case MIDI_EVENT_END_SYSEX:      // 0xF7
						case MIDI_EVENT_TICK:           // 0xF8
						case MIDI_EVENT_START:          // 0xFA
						case MIDI_EVENT_CONTINUE:       // 0xFB
							PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "---<0x%x>--- ", midi_byte);
							break;
						default:
							break;
						}
					}

					/* Interleaved realtime events can only be queued _after_ the
					   event that the realtime event was interleaved in.  Queuing
					   interleaved events for the same cycle frame as the initial
					   event alleviates this problem completely. */
					for (q = 0; q < realtime_event_count; q++) {
						queue_midi_realtime_event(ALL_PARTS, midi_realtime_type[q],
						                          cycle_frame, index);
						midi_realtime_type[q] = 0;
					}
					realtime_event_count = 0;
				}
			}
	}

	/* execute cleanup handler and remove it */
	pthread_cleanup_pop(1);

	/* end of RAWMIDI thread */
	pthread_exit(NULL);
	return NULL;
}
