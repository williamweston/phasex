/*****************************************************************************
 *
 * alsa_pcm.h
 *
 * PHASEX:  [P]hase [H]armonic [A]uroral [S]ynthesis [EX]periment
 *
 * Copyright 1999-2012 William Weston <whw@linuxmail.org>
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
#ifndef _ALSA_PCM_H_
#define _ALSA_PCM_H_

#include <alsa/asoundlib.h>


typedef struct alsa_pcm_hw_info {
	int                     card_num;
	int                     device_num;
	int                     subdevice_num;
	short                   connect_request;
	short                   disconnect_request;
	char                    *card_id;
	char                    *card_name;
	char                    *device_id;
	char                    *device_name;
	char                    *subdevice_name;
	char                    alsa_name[32];
	struct alsa_pcm_hw_info *next;
} ALSA_PCM_HW_INFO;

typedef struct {
#ifdef ENABLE_INPUTS
	snd_pcm_t               *capture_handle;
	snd_pcm_hw_params_t     *capture_hw_params;
	snd_pcm_sw_params_t     *capture_sw_params;
	snd_pcm_channel_area_t  *capture_areas;
	unsigned char           *capture_samples;
#endif
	snd_pcm_t               *playback_handle;
	snd_pcm_hw_params_t     *playback_hw_params;
	snd_pcm_sw_params_t     *playback_sw_params;
	snd_pcm_channel_area_t  *playback_areas;
	unsigned char           *playback_samples;
	//ALSA_PCM_HW_INFO        *capture_hw;
	//ALSA_PCM_HW_INFO        *playback_hw;
	snd_pcm_sframes_t       period_size;
	snd_pcm_sframes_t       buffer_size;
	unsigned int            rate;
	//short                   capture_channels;
	//short                   playback_channels;
	char                    pcm_name[32];
} ALSA_PCM_INFO;


extern ALSA_PCM_INFO            *alsa_pcm_info;

extern char                     *alsa_pcm_device;

extern ALSA_PCM_HW_INFO         *alsa_pcm_capture_hw;
extern ALSA_PCM_HW_INFO         *alsa_pcm_playback_hw;

extern int                      alsa_pcm_hw_changed;

extern snd_pcm_uframes_t        alsa_pcm_buffer_size;
extern snd_pcm_uframes_t        alsa_pcm_period_size;

extern unsigned int             alsa_pcm_format_bits;


void alsa_pcm_hw_list_free(ALSA_PCM_HW_INFO *hw_list);
ALSA_PCM_HW_INFO *alsa_pcm_get_hw_list(snd_pcm_stream_t stream);
int alsa_pcm_init(void);
void alsa_pcm_watchdog_cycle(void);
void alsa_pcm_cleanup(void *UNUSED(arg));
void *alsa_pcm_thread(void *UNUSED(arg));


#endif /* _ALSA_PCM_H_ */
