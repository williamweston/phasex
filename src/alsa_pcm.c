/*****************************************************************************
 *
 * alsa_pcm.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 2012 William Weston <whw@linuxmail.org>
 *
 * Shamelessly adapded from test/pcm.c in alsa-lib-1.0.24.
 *   Copyright (C) Free Software Foundation, Inc.
 *   (dates and author credits specific to this file unknown.)
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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <alsa/asoundlib.h>
#include <sys/mman.h>
#include "phasex.h"
#include "timekeeping.h"
#include "buffer.h"
#include "engine.h"
#include "midi_event.h"
#include "midi_process.h"
#include "alsa_pcm.h"
#include "settings.h"
#include "debug.h"
#include "driver.h"
#include "settings.h"


static snd_pcm_format_t     alsa_pcm_format                 = SND_PCM_FORMAT_S16;
#ifdef ENABLE_INPUTS
static unsigned int         alsa_pcm_capture_channels       = 2;
#endif
static unsigned int         alsa_pcm_playback_channels      = 2;
static unsigned int         alsa_pcm_resample               = 0;
snd_pcm_uframes_t           alsa_pcm_buffer_size;
snd_pcm_uframes_t           alsa_pcm_period_size;

unsigned int                alsa_pcm_format_bits;
unsigned int                alsa_pcm_max_sample_val;
sample_t                    f_alsa_pcm_max_sample_val;
unsigned int                alsa_pcm_bytes_per_sample;
unsigned int                alsa_pcm_phys_bytes_per_sample;
int                         alsa_pcm_big_endian;
int                         alsa_pcm_is_unsigned;
int                         alsa_pcm_is_float;

int                         alsa_pcm_can_mmap               = 0;
int                         alsa_pcm_enable_inputs          = 0;

ALSA_PCM_INFO               *alsa_pcm_info;

char                        *alsa_pcm_device                = NULL;

ALSA_PCM_HW_INFO            *alsa_pcm_capture_hw            = NULL;
ALSA_PCM_HW_INFO            *alsa_pcm_playback_hw           = NULL;

int                         alsa_pcm_hw_changed             = 0;


/*****************************************************************************
 * alsa_pcm_get_hw_list()
 *****************************************************************************/
ALSA_PCM_HW_INFO *
alsa_pcm_get_hw_list(snd_pcm_stream_t stream)
{
	char                name[32];
	ALSA_PCM_HW_INFO    *head               = NULL;
	ALSA_PCM_HW_INFO    *cur                = NULL;
	ALSA_PCM_HW_INFO    *hw_info;
	snd_ctl_t           *handle;
	snd_ctl_card_info_t *cardinfo;
	snd_pcm_info_t      *pcminfo;
	int                 card_num;
	int                 device_num;
	int                 subdevice_num;
	int                 err;
#ifdef ALSA_SCAN_SUBDEVICES
	unsigned int        num_subdevices;
#endif
	unsigned int        avail_subdevices;

	snd_ctl_card_info_alloca(&cardinfo);
	snd_pcm_info_alloca(&pcminfo);

	card_num = -1;
	while (snd_card_next(&card_num) >= 0) {
		if (card_num < 0) {
			break;
		}
		sprintf(name, "hw:%d", card_num);
		if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "snd_ctl_open (card %d): %s\n",
			             card_num, snd_strerror(err));
			continue;
		}
		if ((err = snd_ctl_card_info(handle, cardinfo)) < 0) {
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "snd_ctl_card_info (card %d): %s\n",
			             card_num, snd_strerror(err));
			snd_ctl_close(handle);
			continue;
		}
		device_num = -1;
		while (snd_ctl_pcm_next_device(handle, &device_num) >= 0) {
			if (device_num < 0) {
				break;
			}
			snd_pcm_info_set_device(pcminfo, (unsigned int) device_num);
			snd_pcm_info_set_subdevice(pcminfo, 0);
			snd_pcm_info_set_stream(pcminfo, stream);
			if (snd_ctl_pcm_info(handle, pcminfo) < 0) {
				continue;
			}
#ifdef ALSA_SCAN_SUBDEVICES
			num_subdevices = snd_pcm_info_get_subdevices_count(pcminfo);
#endif
			avail_subdevices = snd_pcm_info_get_subdevices_avail(pcminfo);
			if (avail_subdevices) {
				subdevice_num = 0;
				/* ?????: Is it correct behavior to scan subdevices or not?
				   Some cards list at least 32 subdevices, which seems a bit
				   excessive. */
#ifdef ALSA_SCAN_SUBDEVICES
				for (subdevice_num = 0; subdevice_num < (int) num_subdevices; subdevice_num++) {
#endif
					snd_pcm_info_set_subdevice(pcminfo,
					                           (unsigned int) subdevice_num);
					if (snd_ctl_pcm_info(handle, pcminfo) >= 0) {
						if ((hw_info = malloc(sizeof(ALSA_PCM_HW_INFO)))
						    == NULL) {
							phasex_shutdown("Out of memory!\n");
						}
						hw_info->card_num       = card_num;
						hw_info->device_num     = device_num;
						hw_info->subdevice_num  = subdevice_num;
						hw_info->card_id        = strdup(snd_ctl_card_info_get_id(cardinfo));
						hw_info->card_name      = strdup(snd_ctl_card_info_get_name(cardinfo));
						hw_info->device_id      = strdup(snd_pcm_info_get_id(pcminfo));
						hw_info->device_name    = strdup(snd_pcm_info_get_name(pcminfo));
						hw_info->subdevice_name = strdup(snd_pcm_info_get_subdevice_name(pcminfo));
						hw_info->connect_request    = 0;
						hw_info->disconnect_request = 0;
#ifdef ALSA_SCAN_SUBDEVICES
						if (num_subdevices > 1) {
							snprintf(hw_info->alsa_name,
							         sizeof(hw_info->alsa_name),
							         "hw:%d,%d,%d",
							         hw_info->card_num,
							         hw_info->device_num,
							         hw_info->subdevice_num);
						}
						else {
#endif
							snprintf(hw_info->alsa_name,
							         sizeof(hw_info->alsa_name),
							         "hw:%d,%d",
							         hw_info->card_num,
							         hw_info->device_num);
#ifdef ALSA_SCAN_SUBDEVICES
						}
#endif
						hw_info->next = NULL;
						if (cur == NULL) {
							cur = head = hw_info;
						}
						else {
							cur->next = hw_info;
							cur = cur->next;
						}
					}
#ifdef ALSA_SCAN_SUBDEVICES
				}
#endif
			}
		}
		snd_ctl_close(handle);
	}

	return head;
}


/*****************************************************************************
 * alsa_pcm_hw_list_free()
 *****************************************************************************/
void
alsa_pcm_hw_list_free(ALSA_PCM_HW_INFO *hw_list)
{
	ALSA_PCM_HW_INFO        *cur = hw_list;
	ALSA_PCM_HW_INFO        *next;

	while (cur != NULL) {
		next = cur->next;

		if (cur->card_id != NULL) {
			free(cur->card_id);
		}
		if (cur->card_name != NULL) {
			free(cur->card_name);
		}
		if (cur->device_id != NULL) {
			free(cur->device_id);
		}
		if (cur->device_name != NULL) {
			free(cur->device_name);
		}
		if (cur->subdevice_name != NULL) {
			free(cur->subdevice_name);
		}

		free(cur);
		cur = next;
	}
}


/*****************************************************************************
 * alsa_pcm_hw_list_compare()
 *****************************************************************************/
int
alsa_pcm_hw_list_compare(ALSA_PCM_HW_INFO *a, ALSA_PCM_HW_INFO *b)
{
	ALSA_PCM_HW_INFO    *cur_a = a;
	ALSA_PCM_HW_INFO    *cur_b = b;

	while (cur_a != NULL) {
		if ((cur_b == NULL) ||
		    (cur_a->card_num != cur_b->card_num) ||
		    (cur_a->device_num != cur_b->device_num) ||
		    (cur_a->subdevice_num != cur_b->subdevice_num) ||
		    (strcmp(cur_a->card_id, cur_b->card_id) != 0) ||
		    (strcmp(cur_a->card_name, cur_b->card_name) != 0) ||
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


/*****************************************************************************
 * alsa_pcm_set_hwparams()
 *****************************************************************************/
static int
alsa_pcm_set_hwparams(ALSA_PCM_INFO    *pcminfo,
                      snd_pcm_access_t access,
                      int              USED_FOR_INPUTS(capture))
{
	snd_pcm_t               *handle;
	snd_pcm_uframes_t       size;
	snd_pcm_format_mask_t   *mask;
	unsigned int            rrate;
	int                     err;
	int                     dir;
	unsigned int            alsa_pcm_buffer_time;
	unsigned int            alsa_pcm_period_time;
	int                     j;
	unsigned int            min_channels;
	unsigned int            max_channels;
	snd_pcm_hw_params_t     *params;
	char                    *type;

#ifdef ENABLE_INPUTS
	if (capture) {
		handle = pcminfo->capture_handle;
		params = pcminfo->capture_hw_params;
		type = "CAPTURE";
	}
	else
#endif
		{
			handle = pcminfo->playback_handle;
			params = pcminfo->playback_hw_params;
			type = "PLAYBACK";
		}

	/* choose all parameters */
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		PHASEX_ERROR("Broken configuration for %s: no configurations available: %s\n",
		             type, snd_strerror(err));
		return err;
	}
	/* set hardware resampling */
	err = snd_pcm_hw_params_set_rate_resample(handle, params, alsa_pcm_resample);
	if (err < 0) {
		PHASEX_ERROR("Resampling setup failed for %s: %s\n", type, snd_strerror(err));
		return err;
	}
	/* set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access(handle, params, access);
	if (err < 0) {
		PHASEX_ERROR("Access type not available for %s: %s\n", type, snd_strerror(err));
		return err;
	}
	/* examine available formats */
	snd_pcm_format_mask_alloca(&mask);
	snd_pcm_hw_params_get_format_mask(params, mask);
	if (debug_class & DEBUG_CLASS_AUDIO) {
		for (j = 0; j < 64; j++) {
			if (snd_pcm_format_mask_test(mask, j)) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Sample format %s supported by %s hardware.\n",
				             snd_pcm_format_name(j), type);
			}
		}
	}
	/* if force_16bit is enabled, disable all non-16-bit modes */
	if (setting_force_16bit) {
		for (j = 0; j <= SND_PCM_FORMAT_LAST; j++) {
			if (snd_pcm_format_mask_test(mask, j)) {
				if (snd_pcm_format_width(j) != 16) {
					PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
					             "Disabling sample format %s for %s (not 16-bit).\n",
					             snd_pcm_format_name(j), type);
					snd_pcm_format_mask_reset(mask, j);
				}
			}
		}
		snd_pcm_hw_params_set_format_mask(handle, params, mask);
		err = snd_pcm_hw_params_set_format_first(handle, params, &alsa_pcm_format);
		if (err < 0) {
			PHASEX_ERROR("Unable to determine default sample format for %s hardware: %s\n",
			             type, snd_strerror(err));
		}
	}
	/* if force_16bit is not enabled, test for common 32bit formats */
	else {
		if (snd_pcm_format_mask_test(mask, SND_PCM_FORMAT_U32)) {
			alsa_pcm_format = SND_PCM_FORMAT_U32;
		}
		if (snd_pcm_format_mask_test(mask, SND_PCM_FORMAT_S32)) {
			alsa_pcm_format = SND_PCM_FORMAT_S32;
		}
		if (snd_pcm_format_mask_test(mask, SND_PCM_FORMAT_FLOAT)) {
			alsa_pcm_format = SND_PCM_FORMAT_FLOAT;
		}
	}
	/* set the sample format */
	err = snd_pcm_hw_params_set_format(handle, params, alsa_pcm_format);
	if (err < 0) {
		PHASEX_ERROR("Sample format not available for %s: %s\n", type, snd_strerror(err));
		return err;
	}
	/* set variables needed in buffer mixdown */
	alsa_pcm_format_bits            = (unsigned int) snd_pcm_format_width(alsa_pcm_format);
	alsa_pcm_max_sample_val         = (unsigned int)((1 << (alsa_pcm_format_bits - 1)) - 1);
	f_alsa_pcm_max_sample_val       = (sample_t) alsa_pcm_max_sample_val;
	alsa_pcm_bytes_per_sample       = alsa_pcm_format_bits / 8;
	alsa_pcm_phys_bytes_per_sample  = (unsigned int)(snd_pcm_format_physical_width
	                                                 (alsa_pcm_format) / 8);
	alsa_pcm_big_endian             = snd_pcm_format_big_endian(alsa_pcm_format) == 1;
	alsa_pcm_is_unsigned            = snd_pcm_format_unsigned(alsa_pcm_format) == 1;
	alsa_pcm_is_float               = (alsa_pcm_format == SND_PCM_FORMAT_FLOAT_LE ||
	                                   alsa_pcm_format == SND_PCM_FORMAT_FLOAT_BE) ? 1 : 0;
	PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
	             "ALSA PCM %s:  Using sample format %s:\n"
	             "  bits=%u  max=%u  bps=%d  pbps=%d  be=%d  unsigned=%d  float=%d\n",
	             type,
	             snd_pcm_format_name(alsa_pcm_format),
	             alsa_pcm_format_bits,
	             alsa_pcm_max_sample_val,
	             alsa_pcm_bytes_per_sample,
	             alsa_pcm_phys_bytes_per_sample,
	             alsa_pcm_big_endian,
	             alsa_pcm_is_unsigned,
	             alsa_pcm_is_float);
	/* set the count of channels */
	err = snd_pcm_hw_params_get_channels_min(params, &min_channels);
	if (err < 0) {
		PHASEX_ERROR("Unable to determine number of %s channels: %s\n", type, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_channels_min(params, &max_channels);
	if (err < 0) {
		PHASEX_ERROR("Unable to determine number of %s channels: %s\n", type, snd_strerror(err));
		return err;
	}
	PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
	             "Available %s channels:  %d (min) through %d (max)\n",
	             type, min_channels, max_channels);
#ifdef ENABLE_INPUTS
	if (capture) {
		if ((alsa_pcm_capture_channels < min_channels) ||
		    (alsa_pcm_capture_channels > max_channels)) {
			alsa_pcm_capture_channels = min_channels;
		}
		err = snd_pcm_hw_params_set_channels(handle, params, alsa_pcm_capture_channels);
		if (err < 0) {
			PHASEX_ERROR("Channel count (%i) not available for capture: %s\n",
			             alsa_pcm_capture_channels, snd_strerror(err));
			return err;
		}
	}
	else
#endif
		{
			if ((alsa_pcm_playback_channels < min_channels) ||
			    (alsa_pcm_playback_channels > max_channels)) {
				alsa_pcm_playback_channels = min_channels;
			}
			err = snd_pcm_hw_params_set_channels(handle, params, alsa_pcm_playback_channels);
			if (err < 0) {
				PHASEX_ERROR("Channel count (%i) not available for playback: %s\n",
				             alsa_pcm_playback_channels, snd_strerror(err));
				return err;
			}
		}
	/* set the stream rate */
	rrate = (unsigned int) setting_sample_rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
	if (err < 0) {
		PHASEX_ERROR("Rate %iHz not available for %s: %s\n",
		             setting_sample_rate, type, snd_strerror(err));
		return err;
	}
	if (rrate != (unsigned int) setting_sample_rate) {
		switch (rrate) {
		case 22050:
		case 44100:
		case 48000:
		case 64000:
		case 88200:
		case 96000:
			setting_sample_rate = (int) rrate;
			PHASEX_WARN("%s rate does not match (requested %iHz, got %iHz)\n",
			            type, setting_sample_rate, err);
			break;
		default:
			PHASEX_ERROR("%s rate does not match (requested %iHz, got %iHz)\n",
			             type, setting_sample_rate, err);
			return -EINVAL;
		}
	}
	sample_rate = (int) rrate;
	/* scale sample rate depending on mode */
	switch (setting_sample_rate_mode) {
	case SAMPLE_RATE_UNDERSAMPLE:
		sample_rate /= 2;
		break;
	case SAMPLE_RATE_OVERSAMPLE:
		sample_rate *= 2;
		break;
	}
	/* calculate basic values based on sample rate */
	f_sample_rate = (sample_t) sample_rate;
	nyquist_freq  = (sample_t)(f_sample_rate / 2.0);
	wave_period   = (sample_t)(F_WAVEFORM_SIZE / f_sample_rate);
	/* set the buffer time */
	alsa_pcm_buffer_time = (unsigned int)
		((sample_t)(setting_buffer_period_size * 2 * 1000000) / f_sample_rate);
	alsa_pcm_period_time = (unsigned int)
		((sample_t)(setting_buffer_period_size * 1000000) / f_sample_rate);
	err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
	                                             &alsa_pcm_buffer_time, &dir);
	if (err < 0) {
		PHASEX_ERROR("Unable to set buffer time %i for %s: %s\n",
		             alsa_pcm_buffer_time, type, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_buffer_size(params, &size);
	if (err < 0) {
		PHASEX_ERROR("Unable to get buffer size for %s: %s\n", type, snd_strerror(err));
		return err;
	}
	alsa_pcm_buffer_size = size;
	/* set the period time */
	err = snd_pcm_hw_params_set_period_time_near(handle, params,
	                                             &alsa_pcm_period_time, &dir);
	if (err < 0) {
		PHASEX_ERROR("Unable to set period time %i for %s: %s\n",
		             alsa_pcm_period_time, type, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
	if (err < 0) {
		PHASEX_ERROR("Unable to get period size for %s: %s\n", type, snd_strerror(err));
		return err;
	}
	alsa_pcm_period_size = size;
	/* write the parameters to device */
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		PHASEX_ERROR("Unable to set hw params for %s: %s\n", type, snd_strerror(err));
		return err;
	}
	/* extract info from current config space */
	alsa_pcm_can_mmap = (setting_enable_mmap &&
	                     snd_pcm_hw_params_can_mmap_sample_resolution(params)) ? 1 : 0;

	return 0;
}


/*****************************************************************************
 * alsa_pcm_set_swparams()
 *****************************************************************************/
static int
alsa_pcm_set_swparams(ALSA_PCM_INFO    *pcminfo,
                      int              period_event,
                      int              USED_FOR_INPUTS(capture))
{
	snd_pcm_t           *handle;
	int                 err;
	snd_pcm_sw_params_t *params;
	char                *type;

#ifdef ENABLE_INPUTS
	if (capture) {
		handle = pcminfo->capture_handle;
		params = pcminfo->capture_sw_params;
		type = "CAPTURE";
	}
	else
#endif
		{
			handle = pcminfo->playback_handle;
			params = pcminfo->playback_sw_params;
			type = "PLAYBACK";
		}

	/* get the current swparams */
	err = snd_pcm_sw_params_current(handle, params);
	if (err < 0) {
		PHASEX_ERROR("Unable to determine current swparams for %s: %s\n",
		             type, snd_strerror(err));
		return err;
	}
	/* start the transfer when the buffer is almost full:
	   (buffer_size / avail_min) * avail_min */
	err = snd_pcm_sw_params_set_start_threshold(handle, params,
	                                            (alsa_pcm_buffer_size / alsa_pcm_period_size) * alsa_pcm_period_size);
	if (err < 0) {
		PHASEX_ERROR("Unable to set start threshold mode for %s: %s\n",
		             type, snd_strerror(err));
		return err;
	}

	/* allow the transfer when at least period_size samples can be
	   processed or disable this mechanism when period event is
	   enabled (aka interrupt like style processing) */
	err = snd_pcm_sw_params_set_avail_min(handle, params,
	                                      period_event ?
	                                      alsa_pcm_buffer_size : alsa_pcm_period_size);
	if (err < 0) {
		PHASEX_ERROR("Unable to set avail min for %s: %s\n", type, snd_strerror(err));
		return err;
	}
	/* enable period events when requested */
#if (SND_LIB_VERSION >= 65554)
	if (period_event) {
		err = snd_pcm_sw_params_set_period_event(handle, params, 1);
		if (err < 0) {
			PHASEX_ERROR("Unable to set period event for %s: %s\n", type, snd_strerror(err));
			return err;
		}
	}
#endif
	/* write the parameters to the playback device */
	err = snd_pcm_sw_params(handle, params);
	if (err < 0) {
		PHASEX_ERROR("Unable to set sw params for %s: %s\n", type, snd_strerror(err));
		return err;
	}
	return 0;
}


/*****************************************************************************
 * alsa_pcm_init()
 *****************************************************************************/
int
alsa_pcm_init(void)
{
	ALSA_PCM_HW_INFO        *hw_list;
#ifdef ENABLE_INPUTS
	snd_pcm_hw_params_t     *capture_hw_params;
	snd_pcm_sw_params_t     *capture_sw_params;
#endif
	snd_pcm_hw_params_t     *playback_hw_params;
	snd_pcm_sw_params_t     *playback_sw_params;
	ALSA_PCM_INFO           *new_pcm_info;
	static snd_output_t     *output                 = NULL;
	unsigned int            i;
	int                     err                     = 0;
#if (SND_LIB_VERSION >= 65554)
	int                     period_event            = 1;
#else
	int                     period_event            = 0;
#endif
	unsigned int            chn;
	snd_pcm_access_t        access                  = SND_PCM_ACCESS_MMAP_INTERLEAVED;
	snd_pcm_type_t          type;
	int                     mode                    = 0;

	if ((new_pcm_info = malloc(sizeof(ALSA_PCM_INFO))) == NULL) {
		phasex_shutdown("out of memory!\n");
	}
	memset(new_pcm_info, 0, sizeof(ALSA_PCM_INFO));

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "alsa_pcm_init():  setting_alsa_pcm_device = '%s'\n",
	             setting_alsa_pcm_device);

	if ((setting_alsa_pcm_device != NULL) && (*setting_alsa_pcm_device != '\0')) {
		if (alsa_pcm_device != NULL) {
			free(alsa_pcm_device);
		}
		alsa_pcm_device = strdup(setting_alsa_pcm_device);
	}

	alsa_pcm_enable_inputs = setting_enable_inputs;

	/* scan for ALSA PCM capture and playback devices */
#ifdef ENABLE_INPUTS
	if (alsa_pcm_enable_inputs) {
		if (alsa_pcm_capture_hw != NULL) {
			alsa_pcm_hw_list_free(alsa_pcm_capture_hw);
		}
		if ((alsa_pcm_capture_hw = alsa_pcm_get_hw_list(SND_PCM_STREAM_CAPTURE))  == NULL) {
			PHASEX_ERROR("Unable to get ALSA PCM CAPTURE hardware device list.\n");
			return -EIO;
		}
	}
#endif
	if (alsa_pcm_playback_hw != NULL) {
		alsa_pcm_hw_list_free(alsa_pcm_playback_hw);
	}
	if ((alsa_pcm_playback_hw = alsa_pcm_get_hw_list(SND_PCM_STREAM_PLAYBACK)) == NULL) {
		PHASEX_ERROR("Unable to get ALSA PCM PLAYBACK hardware device list.\n");
		return -EIO;
	}

#ifdef ENABLE_INPUTS
	if (alsa_pcm_enable_inputs) {
		/* output list of detected capture devices */
		if (debug_class & DEBUG_CLASS_AUDIO) {
			hw_list = alsa_pcm_capture_hw;
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Found ALSA PCM CAPTURE hardware devices:\n");
			while (hw_list != NULL) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "  [%s]  %s: %s\n",
				             hw_list->alsa_name,
				             hw_list->card_name,
				             hw_list->device_name);
				hw_list = hw_list->next;
			}
		}
	}
#endif
	/* output list of detected playback devices */
	if (debug_class & DEBUG_CLASS_AUDIO) {
		hw_list = alsa_pcm_playback_hw;
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Found ALSA PCM PLAYBACK hardware devices:\n");
		while (hw_list != NULL) {
			PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "  [%s]  %s: %s\n",
			             hw_list->alsa_name,
			             hw_list->card_name,
			             hw_list->device_name);
			hw_list = hw_list->next;
		}
	}

	/* if no ALSA PCM device is given, use the first playback
	   device on the list */
	if (alsa_pcm_device == NULL) {
#ifdef ENABLE_INPUTS
		if (alsa_pcm_enable_inputs) {
			alsa_pcm_device = strdup(alsa_pcm_capture_hw->alsa_name);
		}
		else
#endif
			{
				alsa_pcm_device = strdup(alsa_pcm_playback_hw->alsa_name);
			}
	}

	if (setting_enable_mmap) {
		mode = SND_PCM_NONBLOCK;
	}

	/* now open the device. */
	/* TODO: better handling on error */
	err = snd_pcm_open(& (new_pcm_info->playback_handle), alsa_pcm_device,
	                   SND_PCM_STREAM_PLAYBACK, mode);
	if (err < 0) {
		PHASEX_ERROR("Unable to open audio device %s in PLAYBACK mode:  %s\n",
		             alsa_pcm_device, snd_strerror(err));
		return err;
	}

	type = snd_pcm_type(new_pcm_info->playback_handle);
	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "opened audio device %s (snd_pcm_type = %s).\n",
	             alsa_pcm_device, snd_pcm_type_name(type));
	if (type == SND_PCM_TYPE_IOPLUG) {
		PHASEX_ERROR("SND_PCM_TYPE_IOPLUG not supported.  Please use a PLUG or HW device.\n");
		return -EINVAL;
	}

	err = snd_pcm_hw_params_malloc(&playback_hw_params);
	if (err < 0) {
		PHASEX_ERROR("cannot allocate hardware params for PLAYBACK: %s\n", snd_strerror(err));
		return err;
	}
	new_pcm_info->playback_hw_params = playback_hw_params;

	err = snd_pcm_sw_params_malloc(&playback_sw_params);
	if (err < 0) {
		PHASEX_ERROR("cannot allocate software params for PLAYBACK: %s\n", snd_strerror(err));
		return err;
	}
	new_pcm_info->playback_sw_params = playback_sw_params;

	if (setting_enable_mmap) {
		access = SND_PCM_ACCESS_MMAP_INTERLEAVED;
		period_event = 0;
	}
	else {
		access = SND_PCM_ACCESS_RW_INTERLEAVED;
		period_event = 0;
	}

	alsa_pcm_set_hwparams(new_pcm_info, access, 0);
	alsa_pcm_set_swparams(new_pcm_info, period_event, 0);

#ifdef ENABLE_INPUTS
	if (alsa_pcm_enable_inputs && alsa_pcm_can_mmap) {
		err = snd_pcm_open(& (new_pcm_info->capture_handle), alsa_pcm_device,
		                   SND_PCM_STREAM_CAPTURE, mode);
		if (err < 0) {
			PHASEX_ERROR("Unable to open audio device %s in CAPTURE mode:  %s\n",
			             alsa_pcm_device, snd_strerror(err));
			return err;
		}

		err = snd_pcm_hw_params_malloc(&capture_hw_params);
		if (err < 0) {
			PHASEX_ERROR("cannot allocate hardware params for CAPTURE: %s\n", snd_strerror(err));
			return err;
		}
		new_pcm_info->capture_hw_params = capture_hw_params;

		err = snd_pcm_sw_params_malloc(&capture_sw_params);
		if (err < 0) {
			PHASEX_ERROR("cannot allocate software params for CAPTURE: %s\n", snd_strerror(err));
			return err;
		}
		new_pcm_info->capture_sw_params = capture_sw_params;

		alsa_pcm_set_hwparams(new_pcm_info, access, 1);
		alsa_pcm_set_swparams(new_pcm_info, period_event, 1);
	}
#endif

	buffer_periods      = DEFAULT_BUFFER_PERIODS;
	buffer_size         = (unsigned int)(alsa_pcm_period_size * buffer_periods);
	buffer_size_mask    = buffer_size - 1;
	buffer_period_size  = (unsigned int) alsa_pcm_period_size;
	buffer_period_mask  = buffer_period_size - 1;
	buffer_latency      = setting_buffer_latency * buffer_period_size;

	for (i = 2; i < 24; i++) {
		if (buffer_size == (1U << i)) {
			buffer_size_bits = i;
		}
		if (buffer_period_size == (1U << i)) {
			buffer_period_size_bits = i;
		}
	}

	init_buffer_indices();
	g_atomic_int_set(&need_increment, 0);

	if (!alsa_pcm_can_mmap) {
#ifdef ENABLE_INPUTS
		if (alsa_pcm_enable_inputs) {
			new_pcm_info->capture_samples =
				malloc((alsa_pcm_period_size *
				        alsa_pcm_capture_channels * (unsigned int)
				        snd_pcm_format_physical_width(alsa_pcm_format)) / 8U);
			if (new_pcm_info->capture_samples == NULL) {
				PHASEX_ERROR("Not enough memory for ALSA PCM sample data.\n");
				return -ENOMEM;
			}

			new_pcm_info->capture_areas = calloc(alsa_pcm_capture_channels,
			                                     sizeof(snd_pcm_channel_area_t));
			if (new_pcm_info->capture_areas == NULL) {
				PHASEX_ERROR("Not enough memory for ALSA PCM audio buffer areas.\n");
				return -ENOMEM;
			}

			for (chn = 0; chn < alsa_pcm_capture_channels; chn++) {
				new_pcm_info->capture_areas[chn].addr  =
					new_pcm_info->capture_samples;

				new_pcm_info->capture_areas[chn].first =
					chn * (unsigned int)
					snd_pcm_format_physical_width(alsa_pcm_format);

				new_pcm_info->capture_areas[chn].step  =
					alsa_pcm_capture_channels * (unsigned int)
					snd_pcm_format_physical_width(alsa_pcm_format);
			}
		}
#endif
		new_pcm_info->playback_samples =
			malloc((alsa_pcm_period_size *
			        alsa_pcm_playback_channels * (unsigned int)
			        snd_pcm_format_physical_width(alsa_pcm_format)) / 8U);
		if (new_pcm_info->playback_samples == NULL) {
			PHASEX_ERROR("Not enough memory for ALSA PCM sample data.\n");
			return -ENOMEM;
		}

		new_pcm_info->playback_areas =
			calloc(alsa_pcm_playback_channels, sizeof(snd_pcm_channel_area_t));
		if (new_pcm_info->playback_areas == NULL) {
			PHASEX_ERROR("Not enough memory for ALSA PCM audio buffer areas.\n");
			return -ENOMEM;
		}

		for (chn = 0; chn < alsa_pcm_playback_channels; chn++) {
			new_pcm_info->playback_areas[chn].addr  = new_pcm_info->playback_samples;
			new_pcm_info->playback_areas[chn].first = chn *
				(unsigned int) snd_pcm_format_physical_width(alsa_pcm_format);
			new_pcm_info->playback_areas[chn].step  = alsa_pcm_playback_channels *
				(unsigned int) snd_pcm_format_physical_width(alsa_pcm_format);
		}
	}

#ifdef ENABLE_INPUTS
	if (alsa_pcm_enable_inputs && alsa_pcm_can_mmap) {
		err = snd_pcm_prepare(new_pcm_info->capture_handle);
		if (err < 0) {
			PHASEX_ERROR("Cannot prepare ALSA PCM audio CAPTURE interface for use: %s\n",
			             snd_strerror(err));
			return err;
		}
	}
#endif

	err = snd_pcm_prepare(new_pcm_info->playback_handle);
	if (err < 0) {
		PHASEX_ERROR("Cannot prepare ALSA PCM audio PLAYBACK interface for use: %s\n",
		             snd_strerror(err));
		return err;
	}

#ifdef ENABLE_INPUTS
	if (alsa_pcm_enable_inputs && alsa_pcm_can_mmap) {
		err = snd_pcm_link(new_pcm_info->capture_handle, new_pcm_info->playback_handle);
		if (err < 0) {
			PHASEX_ERROR("Unable to link CAPTURE and PLAYBACK streams: %s\n",
			             snd_strerror(err));
			return err;
		}
	}
#endif

	strncpy(new_pcm_info->pcm_name, alsa_pcm_device, sizeof(new_pcm_info->pcm_name));
	PHASEX_DEBUG(DEBUG_CLASS_INIT,
	             "ALSA PCM audio setup for [%s] complete!\n",
	             new_pcm_info->pcm_name);

	/* now that audio has opened, set setting_alsa_pcm_device */
	if ((setting_alsa_pcm_device == NULL) ||
	    (strcmp(setting_alsa_pcm_device, alsa_pcm_device) != 0)) {
		if (setting_alsa_pcm_device != NULL) {
			free(setting_alsa_pcm_device);
		}
		setting_alsa_pcm_device = strdup(alsa_pcm_device);
		config_changed = 1;
	}
	if (setting_audio_driver == AUDIO_DRIVER_NONE) {
		setting_audio_driver = AUDIO_DRIVER_ALSA_PCM;
		config_changed = 1;
	}

	if (debug_class & DEBUG_CLASS_AUDIO) {
		usleep(125000);
		if ((err = snd_output_stdio_attach(&output, stdout, 0)) < 0) {
			PHASEX_ERROR("Output failed: %s\n", snd_strerror(err));
			return 0;
		}
#ifdef ENABLE_INPUTS
		if (alsa_pcm_enable_inputs && alsa_pcm_can_mmap) {
			snd_pcm_dump(new_pcm_info->capture_handle, output);
		}
#endif
		snd_pcm_dump(new_pcm_info->playback_handle, output);
		usleep(125000);
	}

	alsa_pcm_info = new_pcm_info;

	return 0;
}


/*****************************************************************************
 * alsa_pcm_watchdog_cycle()
 *****************************************************************************/
void
alsa_pcm_watchdog_cycle(void)
{
	ALSA_PCM_HW_INFO    *cur;

	if ((audio_driver == AUDIO_DRIVER_ALSA_PCM) && (alsa_pcm_info != NULL)) {
		cur = alsa_pcm_playback_hw;
		while (cur != NULL) {
			if (cur->connect_request) {
				cur->connect_request    = 0;
				cur->disconnect_request = 0;
				if (setting_alsa_pcm_device != NULL) {
					free(setting_alsa_pcm_device);
				}
				setting_alsa_pcm_device = strdup(cur->alsa_name);
				stop_audio();
				wait_audio_stop();
				alsa_pcm_init();
				start_audio();
				wait_audio_start();
				break;
			}
			/* Ignore disconnect requests since they're controlled by radio
			   buttons.  All that is needed to switch devices properly here is
			   the connect request. */
			cur = cur->next;
		}
	}
}


/*****************************************************************************
 * alsa_pcm_mix_parts()
 *****************************************************************************/
int
alsa_pcm_mix_parts(ALSA_PCM_INFO                *pcminfo,
                   snd_pcm_uframes_t            offset,
                   unsigned int                 nframes,
                   const snd_pcm_channel_area_t *USED_FOR_INPUTS(capt_areas),
                   const snd_pcm_channel_area_t *play_areas)
{
	PART                            *part;
#ifdef ENABLE_INPUTS
	const snd_pcm_channel_area_t    *capture_areas = ((capt_areas == NULL) ?
	                                                  pcminfo->capture_areas : capt_areas);
	unsigned char                   *capture_samples[alsa_pcm_capture_channels];
	unsigned int                    capture_steps[alsa_pcm_capture_channels];
#endif
	const snd_pcm_channel_area_t    *playback_areas = ((play_areas == NULL) ?
	                                                   pcminfo->playback_areas : play_areas);
	unsigned char                   *playback_samples[alsa_pcm_playback_channels];
	unsigned int                    playback_steps[alsa_pcm_playback_channels];
	unsigned int                    chn;
	unsigned int                    a_index;
	unsigned int                    part_num;
	unsigned int                    i;
	unsigned int                    j;
	union {
		float           f;
		int             i;
	}                               fval[16];
	union {
		int             i;
		unsigned int    u;
		unsigned char   c[4];
	}                               ival[16];

	set_midi_cycle_time();

	if (check_active_sensing_timeout() > 0) {
		broadcast_notes_off();
	}

	/* verify and prepare the contents of areas */
#ifdef ENABLE_INPUTS
	if (alsa_pcm_enable_inputs) {
		for (chn = 0; chn < alsa_pcm_capture_channels; chn++) {
			if ((capture_areas[chn].first % 8) != 0) {
				PHASEX_ERROR("capture_areas[%i].first == %i, aborting...\n",
				             chn, capture_areas[chn].first);
				return -EIO;
			}
			capture_samples[chn] = (((unsigned char *) capture_areas[chn].addr) +
			                        (capture_areas[chn].first / 8));
			if ((capture_areas[chn].step % 16) != 0) {
				PHASEX_ERROR("capture_areas[%i].step == %i, aborting...\n",
				             chn, capture_areas[chn].step);
				return -EIO;
			}
			capture_steps[chn]    = capture_areas[chn].step / 8U;
			capture_samples[chn] += offset * capture_steps[chn];
		}
	}
#endif
	for (chn = 0; chn < alsa_pcm_playback_channels; chn++) {
		if ((playback_areas[chn].first % 8) != 0) {
			PHASEX_ERROR("playback_areas[%i].first == %i, aborting...\n",
			             chn, playback_areas[chn].first);
			return -EIO;
		}
		playback_samples[chn] = (((unsigned char *) playback_areas[chn].addr) +
		                         (playback_areas[chn].first / 8));
		if ((playback_areas[chn].step % 16) != 0) {
			PHASEX_ERROR("playback_areas[%i].step == %i, aborting...\n",
			             chn, playback_areas[chn].step);
			return -EIO;
		}
		playback_steps[chn]    = playback_areas[chn].step / 8U;
		playback_samples[chn] += offset * playback_steps[chn];
	}

	memset(output_buffer1, 0, sizeof(sample_t) * nframes);
	memset(output_buffer2, 0, sizeof(sample_t) * nframes);

	a_index = get_audio_index();

	/* mix parts generated in engine threads */
	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		part = get_part(part_num);

		for (j = 0; j < nframes; j++) {
			output_buffer1[j] += part->output_buffer1[a_index + j];
			output_buffer2[j] += part->output_buffer2[a_index + j];
		}
	}

	/* fill the output channel areas from output buffers. */
	for (j = 0; j < nframes; j++) {
		if (alsa_pcm_is_float) {
			fval[0].f = (float) output_buffer1[j];
			ival[0].i = fval[0].i;
			fval[1].f = (float) output_buffer2[j];
			ival[1].i = fval[1].i;
		}
		else {
			if (alsa_pcm_is_unsigned) {
				ival[0].u ^= 1U << (alsa_pcm_format_bits - 1U);
				ival[1].u ^= 1U << (alsa_pcm_format_bits - 1U);
			}
			ival[0].i = (int)(output_buffer1[j] * f_alsa_pcm_max_sample_val);
			ival[1].i = (int)(output_buffer2[j] * f_alsa_pcm_max_sample_val);
		}

		/* TODO: handle output channel mapping and > 2 output channels. */
		for (chn = 0; chn < alsa_pcm_playback_channels; chn++) {
			if (alsa_pcm_big_endian) {
				for (i = 0; i < alsa_pcm_bytes_per_sample; i++) {
					* (playback_samples[chn] +
					   alsa_pcm_phys_bytes_per_sample - 1U - i) =
						(ival[chn].u >> i * 8U) & 0xFF;
				}
			}
			else {
				for (i = 0; i < alsa_pcm_bytes_per_sample; i++) {
					* (playback_samples[chn] + i) =
						(ival[chn].u >> i * 8U) & 0xFF;
				}
			}
			playback_samples[chn] += playback_steps[chn];
		}
	}

#ifdef ENABLE_INPUTS
	if (alsa_pcm_enable_inputs) {
		/* fill the input buffers from the input channel areas. */
		for (j = 0; j < nframes; j++) {
			/* TODO: handle input channel mapping and > 2 input channels. */
			for (chn = 0; chn < 2; chn++) {
				if (alsa_pcm_big_endian) {
					for (i = 0; i < alsa_pcm_bytes_per_sample; i++) {
						ival[chn].c[i] = (* (capture_samples[chn % alsa_pcm_capture_channels] +
						                     alsa_pcm_phys_bytes_per_sample - 1 - i)) & 0xFF;
					}
				}
				else {
					for (i = 0; i < alsa_pcm_bytes_per_sample; i++) {
						ival[chn].c[i] = (* (capture_samples[chn % alsa_pcm_capture_channels] +
						                     i)) & 0xFF;
					}
				}
				capture_samples[chn] += capture_steps[chn];
			}

			if (alsa_pcm_is_float) {
				fval[0].i                  = ival[0].i;
				input_buffer1[a_index + j] = (sample_t) fval[0].f;
				fval[1].i                  = ival[1].i;
				input_buffer2[a_index + j] = (sample_t) fval[1].f;
			}
			else {
				if (alsa_pcm_is_unsigned) {
					ival[0].u ^= 1U << (alsa_pcm_format_bits - 1U);
					ival[1].u ^= 1U << (alsa_pcm_format_bits - 1U);
				}
				input_buffer1[a_index + j] = (sample_t) ival[0].i / f_alsa_pcm_max_sample_val;
				input_buffer2[a_index + j] = (sample_t) ival[1].i / f_alsa_pcm_max_sample_val;
			}
		}
	}
#endif

	/* Done using the audio index until next ALSA PCM period. */
	inc_audio_index(nframes);

	return 0;
}


/*****************************************************************************
 * alsa_pcm_xrun_recovery()
 *****************************************************************************/
static int
alsa_pcm_xrun_recovery(snd_pcm_t *handle, int err)
{
	if (err == -EPIPE) {            /* under-run */
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "ALSA xrun stream recovery (-EPIPE)...\n");
		if ((err = snd_pcm_prepare(handle)) < 0) {
			PHASEX_ERROR("Can't recover from underrun, prepare failed: %s\n",
			             snd_strerror(err));
		}
		return 0;
	}
	else if (err == -ESTRPIPE) {
		PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "ALSA xrun stream recovery (-ESTRIPE)...\n");
		while ((err = snd_pcm_resume(handle)) == -EAGAIN) {
			usleep(100);    /* wait until the suspend flag is released */
		}
		if (err < 0) {
			if ((err = snd_pcm_prepare(handle)) < 0) {
				PHASEX_ERROR("Can't recover from suspend, prepare failed: %s\n",
				             snd_strerror(err));
			}
		}
		return 0;
	}
	return err;
}


/*****************************************************************************
 * alsa_pcm_wait_for_poll()
 *****************************************************************************/
static int
alsa_pcm_wait_for_poll(snd_pcm_t *handle, struct pollfd *ufds, unsigned int count, int capture)
{
	unsigned short  revents;

	while (1) {
		poll(ufds, count, -1);
		snd_pcm_poll_descriptors_revents(handle, ufds, count, &revents);
		if (revents & POLLERR) {
			return -EIO;
		}
		if (!capture && (revents & POLLOUT)) {
			return 0;
		}
		if (capture && (revents & POLLIN)) {
			return 0;
		}
	}
}


/*****************************************************************************
 * alsa_pcm_write_and_poll_loop()
 *****************************************************************************/
static int
alsa_pcm_write_and_poll_loop(ALSA_PCM_INFO *pcminfo)
{
	struct pollfd   *playback_ufds;
	int             playback_count;
	unsigned char   *ptr;
	int             result;
	int             nframes;
	int             init;
	unsigned int    part_num;
#ifdef ENABLE_INPUTS__
	struct pollfd   *capture_ufds;
	int             capture_count;

	if ((capture_count = snd_pcm_poll_descriptors_count(pcminfo->capture_handle)) <= 0) {
		PHASEX_ERROR("Invalid poll descriptors count\n");
		return capture_count;
	}

	if ((capture_ufds = malloc(sizeof(struct pollfd) *
	                           (size_t)(capture_count + 1))) == NULL) {
		PHASEX_ERROR("Out of memory\n");
		return -ENOMEM;
	}
	if ((result = snd_pcm_poll_descriptors(pcminfo->capture_handle,
	                                       capture_ufds,
	                                       (unsigned int) capture_count)) < 0) {
		PHASEX_ERROR("Unable to obtain poll descriptors for capture: %s\n",
		             snd_strerror(result));
		free(capture_ufds);
		return result;
	}
#endif

	if ((playback_count = snd_pcm_poll_descriptors_count(pcminfo->playback_handle)) <= 0) {
		PHASEX_ERROR("Invalid poll descriptors count\n");
		return playback_count;
	}

	if ((playback_ufds = malloc(sizeof(struct pollfd) *
	                            (size_t)(playback_count + 1))) == NULL) {
		PHASEX_ERROR("Out of memory\n");
		return -ENOMEM;
	}
	if ((result = snd_pcm_poll_descriptors(pcminfo->playback_handle,
	                                       playback_ufds,
	                                       (unsigned int) playback_count)) < 0) {
		PHASEX_ERROR("Unable to obtain poll descriptors for playback: %s\n",
		             snd_strerror(result));
		free(playback_ufds);
#ifdef ENABLE_INPUTS__
		free(capture_ufds);
#endif
		return result;
	}

	memset(pcminfo->playback_samples, 0, buffer_period_size * sizeof(signed short));

	init = 1;
	while (!audio_stopped && !pending_shutdown) {
#ifdef ENABLE_INPUTS__
		if (!init) {
			if ((result = alsa_pcm_wait_for_poll(pcminfo->capture_handle, capture_ufds,
			                                     (unsigned int) capture_count, 1)) < 0) {
				if ((snd_pcm_state(pcminfo->capture_handle) == SND_PCM_STATE_XRUN) ||
				    (snd_pcm_state(pcminfo->capture_handle) == SND_PCM_STATE_SUSPENDED)) {
					PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "xxx1xxx ");
					result = (snd_pcm_state(pcminfo->capture_handle) ==
					          SND_PCM_STATE_XRUN) ? -EPIPE : -ESTRPIPE;
					if (alsa_pcm_xrun_recovery(pcminfo->capture_handle, result) < 0) {
						PHASEX_ERROR("ALSA PCM read error 1: %s\n", snd_strerror(result));
						select_audio_driver(NULL, AUDIO_DRIVER_NONE);
						free(playback_ufds);
						free(capture_ufds);
						return result;
					}
					init = 1;
				}
				else {
					PHASEX_ERROR("ALSA PCM wait for poll (read) failed.\n");
					select_audio_driver(NULL, AUDIO_DRIVER_NONE);
					free(playback_ufds);
					free(capture_ufds);
					return result;
				}
			}
		}
		else {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
			             DEBUG_COLOR_MAGENTA "*" DEBUG_COLOR_DEFAULT);
		}

		ptr     = pcminfo->capture_samples;
		nframes = (int) buffer_period_size;
		while (nframes > 0) {
			if ((result = (int) snd_pcm_readi(pcminfo->capture_handle,
			                                  ptr, (unsigned int) nframes)) < 0) {
				PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "xxx2xxx ");
				if (alsa_pcm_xrun_recovery(pcminfo->capture_handle, result) < 0) {
					PHASEX_ERROR("ALSA PCM read error 2: (%d) %s\n",
					             result, snd_strerror(result));
					select_audio_driver(NULL, AUDIO_DRIVER_NONE);
					free(playback_ufds);
					free(capture_ufds);
					return result;
				}
				init = 1;
				/* Decrement the need_increment counter, since this period is
				   being skipped.  The xrun recovery is called elsewhere
				   within the audio cycle, so make sure we catch this after an
				   xrun when we know that a cycle is being skipped. */
				g_atomic_int_add(&need_increment, -1);
				init_buffer_indices();
				for (part_num = 0; part_num < MAX_PARTS; part_num++) {
					need_index_resync[part_num] = 1;
				}
				break;  /* skip one period */
			}
			if (snd_pcm_state(pcminfo->capture_handle) == SND_PCM_STATE_RUNNING) {
				init = 0;
			}
			ptr     += (unsigned int)(result) * alsa_pcm_capture_channels;
			nframes -= result;
			if (nframes == 0) {
				break;
			}
			/* It is possible, that the initial buffer cannot store all data
			   from the last period, so wait awhile. */
			if ((result = alsa_pcm_wait_for_poll(pcminfo->capture_handle, capture_ufds,
			                                     (unsigned int) capture_count, 1)) < 0) {
				if ((snd_pcm_state(pcminfo->capture_handle) == SND_PCM_STATE_XRUN) ||
				    (snd_pcm_state(pcminfo->capture_handle) == SND_PCM_STATE_SUSPENDED)) {
					PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "xxx3xxx ");
					result = (snd_pcm_state(pcminfo->capture_handle) ==
					          SND_PCM_STATE_XRUN) ? -EPIPE : -ESTRPIPE;
					if (alsa_pcm_xrun_recovery(pcminfo->capture_handle, result) < 0) {
						PHASEX_ERROR("ALSA PCM read error 3: %s\n", snd_strerror(result));
						select_audio_driver(NULL, AUDIO_DRIVER_NONE);
						free(playback_ufds);
						free(capture_ufds);
						return result;
					}
					init = 1;
				}
				else {
					PHASEX_ERROR("ALSA PCM wait for poll (read) failed.\n");
					select_audio_driver(NULL, AUDIO_DRIVER_NONE);
					free(playback_ufds);
					free(capture_ufds);
					return result;
				}
			}
		}

		if (init) {
			continue;
		}
#endif
		if (!init) {
			if ((result = alsa_pcm_wait_for_poll(pcminfo->playback_handle, playback_ufds,
			                                     (unsigned int) playback_count, 0)) < 0) {
				if ((snd_pcm_state(pcminfo->playback_handle) == SND_PCM_STATE_XRUN) ||
				    (snd_pcm_state(pcminfo->playback_handle) == SND_PCM_STATE_SUSPENDED)) {
					PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "xxx4xxx ");
					result = (snd_pcm_state(pcminfo->playback_handle) ==
					          SND_PCM_STATE_XRUN) ? -EPIPE : -ESTRPIPE;
					if (alsa_pcm_xrun_recovery(pcminfo->playback_handle, result) < 0) {
						PHASEX_ERROR("ALSA PCM write error 1: %s\n", snd_strerror(result));
						select_audio_driver(NULL, AUDIO_DRIVER_NONE);
						free(playback_ufds);
#ifdef ENABLE_INPUTS__
						free(capture_ufds);
#endif
						return result;
					}
					init = 1;
				}
				else {
					PHASEX_ERROR("ALSA PCM wait for poll (write) failed.\n");
					select_audio_driver(NULL, AUDIO_DRIVER_NONE);
					free(playback_ufds);
#ifdef ENABLE_INPUTS__
					free(capture_ufds);
#endif
					return result;
				}
			}
			/* Mix phasex parts into the area buffers.  This must happen after
			   the read and before the write! */
			if ((result = alsa_pcm_mix_parts(pcminfo, 0, buffer_period_size, NULL, NULL)) < 0) {
				free(playback_ufds);
#ifdef ENABLE_INPUTS__
				free(capture_ufds);
#endif
				return result;
			}

		}
		else {
			PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING,
			             DEBUG_COLOR_YELLOW "*" DEBUG_COLOR_DEFAULT);
		}

		ptr     = pcminfo->playback_samples;
		nframes = (int) buffer_period_size;
		while (nframes > 0) {
			if ((result = (int) snd_pcm_writei(pcminfo->playback_handle,
			                                   ptr, (unsigned int) nframes)) < 0) {
				PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "xxx5xxx ");
				if (alsa_pcm_xrun_recovery(pcminfo->playback_handle, result) < 0) {
					PHASEX_ERROR("ALSA PCM write error 2: %s\n", snd_strerror(result));
					select_audio_driver(NULL, AUDIO_DRIVER_NONE);
					free(playback_ufds);
#ifdef ENABLE_INPUTS__
					free(capture_ufds);
#endif
					return result;
				}
				/* Decrement the need_increment counter, since this period is
				   being skipped.  The xrun recovery is called elsewhere
				   within the audio cycle, so make sure we catch this after an
				   xrun when we know that a cycle is being skipped. */
				g_atomic_int_add(&need_increment, -1);
				init_buffer_indices();
				for (part_num = 0; part_num < MAX_PARTS; part_num++) {
					need_index_resync[part_num] = 1;
				}
				break;  /* skip one period */
			}
			if (snd_pcm_state(pcminfo->playback_handle) == SND_PCM_STATE_RUNNING) {
				init = 0;
			}
			ptr     += ((unsigned int)(result) * alsa_pcm_playback_channels);
			nframes -= result;
			if (nframes == 0) {
				break;
			}
			/* It is possible, that the initial buffer cannot store all data
			   from the last period, so wait awhile. */
			if ((result = alsa_pcm_wait_for_poll(pcminfo->playback_handle, playback_ufds,
			                                     (unsigned int) playback_count, 0)) < 0) {
				if ((snd_pcm_state(pcminfo->playback_handle) == SND_PCM_STATE_XRUN) ||
				    (snd_pcm_state(pcminfo->playback_handle) == SND_PCM_STATE_SUSPENDED)) {
					PHASEX_DEBUG(DEBUG_CLASS_MIDI_TIMING, "xxx6xxx ");
					result = (snd_pcm_state(pcminfo->playback_handle) ==
					          SND_PCM_STATE_XRUN) ? -EPIPE : -ESTRPIPE;
					if (alsa_pcm_xrun_recovery(pcminfo->playback_handle, result) < 0) {
						PHASEX_ERROR("ALSA PCM write error 3: %s\n", snd_strerror(result));
						select_audio_driver(NULL, AUDIO_DRIVER_NONE);
						free(playback_ufds);
#ifdef ENABLE_INPUTS__
						free(capture_ufds);
#endif
						return result;
					}
					init = 1;
				}
				else {
					PHASEX_ERROR("ALSA PCM wait for poll (write) failed.\n");
					select_audio_driver(NULL, AUDIO_DRIVER_NONE);
					free(playback_ufds);
#ifdef ENABLE_INPUTS__
					free(capture_ufds);
#endif
					return result;
				}
			}
		}
	}

	free(playback_ufds);
#ifdef ENABLE_INPUTS__
	free(capture_ufds);
#endif

	return 0;
}


/*****************************************************************************
 * alsa_pcm_mmap_loop()
 *****************************************************************************/
static int
alsa_pcm_mmap_loop(ALSA_PCM_INFO *pcminfo)
{
	const snd_pcm_channel_area_t    *capture_areas      = NULL;
	const snd_pcm_channel_area_t    *playback_areas;
#ifdef ENABLE_INPUTS
	snd_pcm_uframes_t               capture_offset;
	snd_pcm_uframes_t               capture_frames;
#endif
	snd_pcm_uframes_t               playback_offset;
	snd_pcm_uframes_t               playback_frames;
	snd_pcm_uframes_t               size;
	snd_pcm_sframes_t               avail;
	snd_pcm_sframes_t               commitres;
	snd_pcm_state_t                 state;
	int                             err;
	int                             first               = 1;

	while (!audio_stopped && !pending_shutdown) {
		state = snd_pcm_state(pcminfo->playback_handle);
		if (state == SND_PCM_STATE_XRUN) {
			err = alsa_pcm_xrun_recovery(pcminfo->playback_handle, -EPIPE);
			if (err < 0) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "XRUN recovery failed: %s\n",
				             snd_strerror(err));
				return err;
			}
			first = 1;
		}
		else if (state == SND_PCM_STATE_SUSPENDED) {
			err = alsa_pcm_xrun_recovery(pcminfo->playback_handle, -ESTRPIPE);
			if (err < 0) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "SUSPEND recovery failed: %s\n",
				             snd_strerror(err));
				return err;
			}
		}
		avail = snd_pcm_avail_update(pcminfo->playback_handle);
		if (avail < 0) {
			err = alsa_pcm_xrun_recovery(pcminfo->playback_handle, (int) avail);
			if (err < 0) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "avail update failed: %s\n",
				             snd_strerror(err));
				return err;
			}
			first = 1;
			continue;
		}
		if ((unsigned int) avail < buffer_period_size) {
			if (first) {
				first = 0;
				err = snd_pcm_start(pcminfo->playback_handle);
				if (err < 0) {
					PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Start error: %s\n", snd_strerror(err));
					return err;
				}
			}
			else {
#ifdef ENABLE_INPUTS
				if (alsa_pcm_enable_inputs) {
					err = snd_pcm_wait(pcminfo->capture_handle, -1);
					if (err < 0) {
						if ((err = alsa_pcm_xrun_recovery(pcminfo->capture_handle, err)) < 0) {
							PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "snd_pcm_wait CAPTURE error: %s\n",
							             snd_strerror(err));
							return err;
						}
						first = 1;
					}
				}
#endif
				err = snd_pcm_wait(pcminfo->playback_handle, -1);
				if (err < 0) {
					if ((err = alsa_pcm_xrun_recovery(pcminfo->playback_handle, err)) < 0) {
						PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "snd_pcm_wait PLAYBACK error: %s\n",
						             snd_strerror(err));
						return err;
					}
					first = 1;
				}
			}
			continue;
		}

		size = buffer_period_size;
		while (size > 0) {
			playback_frames = size;

#ifdef ENABLE_INPUTS
			if (alsa_pcm_enable_inputs) {
				capture_frames = size;

				err = snd_pcm_mmap_begin(pcminfo->capture_handle,
				                         &capture_areas,
				                         &capture_offset,
				                         &capture_frames);
				if (err < 0) {
					if ((err = alsa_pcm_xrun_recovery(pcminfo->capture_handle, err)) < 0) {
						PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "MMAP begin avail error: %s\n",
						             snd_strerror(err));
						return err;
					}
					first = 1;
				}
			}
#endif
			err = snd_pcm_mmap_begin(pcminfo->playback_handle,
			                         &playback_areas,
			                         &playback_offset,
			                         &playback_frames);
			if (err < 0) {
				if ((err = alsa_pcm_xrun_recovery(pcminfo->playback_handle, err)) < 0) {
					PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "MMAP begin avail error: %s\n",
					             snd_strerror(err));
					return err;
				}
				first = 1;
			}

#ifdef ENABLE_INPUTS__
			if (alsa_pcm_enable_inputs && (capture_offset != playback_offset)) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
				             "*** capture_offset=%lu  playback_offset=%lu ***\n",
				             capture_offset, playback_offset);
			}
#endif

			/* Mix phasex parts into the area buffers.  This must happen after
			   the read and before the write! */
			if ((err = alsa_pcm_mix_parts(pcminfo,
			                              playback_offset,
			                              buffer_period_size,
			                              capture_areas,
			                              playback_areas)) < 0) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "MMAP access error.\n");
				return err;
			}

			commitres = snd_pcm_mmap_commit(pcminfo->playback_handle,
			                                playback_offset,
			                                playback_frames);
			if (commitres < 0 || (snd_pcm_uframes_t) commitres != playback_frames) {
				if ((err = alsa_pcm_xrun_recovery(pcminfo->playback_handle,
				                                  (int)(commitres >= 0 ?
				                                        -EPIPE : commitres))) < 0) {
					PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "MMAP commit error: %s\n",
					             snd_strerror(err));
					return err;
				}
				first = 1;
			}
#ifdef ENABLE_INPUTS
			if (alsa_pcm_enable_inputs) {
				commitres = snd_pcm_mmap_commit(pcminfo->capture_handle,
				                                capture_offset,
				                                capture_frames);
				if ((commitres < 0) || ((snd_pcm_uframes_t) commitres != capture_frames)) {
					if ((err = alsa_pcm_xrun_recovery(pcminfo->capture_handle,
					                                  (int)(commitres >= 0 ?
					                                        -EPIPE : commitres))) < 0) {
						PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "MMAP commit error: %s\n",
						             snd_strerror(err));
						return err;
					}
					first = 1;
				}
			}
#endif

#ifdef ENABLE_INPUTS__
			if (alsa_pcm_enable_inputs && (capture_frames != playback_frames)) {
				PHASEX_DEBUG(DEBUG_CLASS_AUDIO,
				             "** capture_frames=%lu  playback_frames=%lu ***\n",
				             capture_frames, playback_frames);
			}
#endif

			size -= playback_frames;
		}
	}

	return 0;
}


/*****************************************************************************
 * alsa_pcm_cleanup()
 *
 * Cleanup handler for ALSA PCM thread.
 * Closes ALSA devices and frees dynamic mem.
 *****************************************************************************/
void
alsa_pcm_cleanup(void *UNUSED(arg))
{
	int                 part_num;

	if (alsa_pcm_info != NULL) {
#ifdef ENABLE_INPUTS
		if (alsa_pcm_info->capture_handle != NULL) {
			snd_pcm_close(alsa_pcm_info->capture_handle);
		}
		if (alsa_pcm_info->capture_hw_params != NULL) {
			snd_pcm_hw_params_free(alsa_pcm_info->capture_hw_params);
		}
		if (alsa_pcm_info->capture_sw_params != NULL) {
			snd_pcm_sw_params_free(alsa_pcm_info->capture_sw_params);
		}
		if (!alsa_pcm_can_mmap) {
			if (alsa_pcm_info->capture_areas != NULL) {
				free(alsa_pcm_info->capture_areas);
			}
			if (alsa_pcm_info->capture_samples != NULL) {
				free(alsa_pcm_info->capture_samples);
			}
		}
#endif
		if (alsa_pcm_info->playback_handle != NULL) {
			if (!alsa_pcm_can_mmap) {
				snd_pcm_drain(alsa_pcm_info->playback_handle);
			}
			snd_pcm_close(alsa_pcm_info->playback_handle);
		}
		if (alsa_pcm_info->playback_hw_params != NULL) {
			snd_pcm_hw_params_free(alsa_pcm_info->playback_hw_params);
		}
		if (alsa_pcm_info->playback_sw_params != NULL) {
			snd_pcm_sw_params_free(alsa_pcm_info->playback_sw_params);
		}
		if (!alsa_pcm_can_mmap) {
			if (alsa_pcm_info->playback_areas != NULL) {
				free(alsa_pcm_info->playback_areas);
			}
			if (alsa_pcm_info->playback_samples != NULL) {
				free(alsa_pcm_info->playback_samples);
			}
		}
		free(alsa_pcm_info);
		alsa_pcm_info = NULL;
		snd_config_update_free_global();
	}

	init_engine_buffers();

	init_buffer_indices();
	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		need_index_resync[part_num] = 1;
	}

	audio_stopped  = 1;
	audio_thread_p = 0;
}


/*****************************************************************************
 * alsa_pcm_thread()
 *****************************************************************************/
void *
alsa_pcm_thread(void *UNUSED(arg))
{
	struct sched_param  schedparam;
	pthread_t           thread_id;
	int                 part_num;

	/* set realtime scheduling and priority */
	thread_id = pthread_self();
	memset(&schedparam, 0, sizeof(struct sched_param));
	schedparam.sched_priority = setting_audio_priority;
	pthread_setschedparam(thread_id, setting_sched_policy, &schedparam);

	/* setup thread cleanup handler */
	pthread_cleanup_push(&alsa_pcm_cleanup, NULL);

	PHASEX_DEBUG(DEBUG_CLASS_AUDIO, "Starting ALSA PCM thread...\n");

	/* broadcast the audio ready condition */
	pthread_mutex_lock(&audio_ready_mutex);
	audio_ready = 1;
	pthread_cond_broadcast(&audio_ready_cond);
	pthread_mutex_unlock(&audio_ready_mutex);

	/* initialize buffer indices and set reference clock. */
	init_buffer_indices();
	for (part_num = 0; part_num < MAX_PARTS; part_num++) {
		need_index_resync[part_num] = 1;
	}
	start_midi_clock();

	/* MAIN LOOP: poll for audio and copy buffers */
	if (alsa_pcm_can_mmap) {
		alsa_pcm_mmap_loop(alsa_pcm_info);
	}
	else {
		alsa_pcm_write_and_poll_loop(alsa_pcm_info);
	}

	/* execute cleanup handler and remove it */
	pthread_cleanup_pop(1);

	/* end of MIDI thread */
	pthread_exit(NULL);
	return NULL;
}
