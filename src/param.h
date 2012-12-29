/*****************************************************************************
 *
 * param.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2009 William Weston <whw@linuxmail.org>
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
#ifndef _PHASEX_PARAM_H_
#define _PHASEX_PARAM_H_

#include <gtk/gtk.h>
#include "phasex.h"
#include "gtkknob.h"


/* Special value to serve as an ignore flag. */
#define PARAM_VAL_IGNORE            -131073

/* Parameter IDs */
#define PARAM_BPM                   0
#define PARAM_PATCH_TUNE            1
#define PARAM_VOLUME                2
#define PARAM_KEYMODE               3
#define PARAM_KEYFOLLOW_VOL         4
#define PARAM_TRANSPOSE             5
#define PARAM_PORTAMENTO            6
#define PARAM_INPUT_BOOST           7
#define PARAM_INPUT_FOLLOW          8
#define PARAM_PAN                   9
#define PARAM_STEREO_WIDTH          10

#define PARAM_AMP_VELOCITY          11
#define PARAM_AMP_ATTACK            12
#define PARAM_AMP_DECAY             13
#define PARAM_AMP_SUSTAIN           14
#define PARAM_AMP_RELEASE           15

#define PARAM_FILTER_CUTOFF         16
#define PARAM_FILTER_RESONANCE      17
#define PARAM_FILTER_SMOOTHING      18
#define PARAM_FILTER_KEYFOLLOW      19
#define PARAM_FILTER_MODE           20
#define PARAM_FILTER_TYPE           21
#define PARAM_FILTER_GAIN           22
#define PARAM_FILTER_ENV_AMOUNT     23
#define PARAM_FILTER_ENV_SIGN       24
#define PARAM_FILTER_LFO            25
#define PARAM_FILTER_LFO_CUTOFF     26
#define PARAM_FILTER_LFO_RESONANCE  27
#define PARAM_FILTER_ATTACK         28
#define PARAM_FILTER_DECAY          29
#define PARAM_FILTER_SUSTAIN        30
#define PARAM_FILTER_RELEASE        31

#define PARAM_CHORUS_MIX            32
#define PARAM_CHORUS_AMOUNT         33
#define PARAM_CHORUS_TIME           34
#define PARAM_CHORUS_FEED           35
#define PARAM_CHORUS_CROSSOVER      36
#define PARAM_CHORUS_LFO_WAVE       37
#define PARAM_CHORUS_LFO_RATE       38
#define PARAM_CHORUS_PHASE_RATE     39
#define PARAM_CHORUS_PHASE_BALANCE  40

#define PARAM_DELAY_MIX             41
#define PARAM_DELAY_FEED            42
#define PARAM_DELAY_TIME            43
#define PARAM_DELAY_CROSSOVER       44
#define PARAM_DELAY_LFO             45

#define PARAM_OSC1_MODULATION       46
#define PARAM_OSC1_POLARITY         47
#define PARAM_OSC1_FREQ_BASE        48
#define PARAM_OSC1_WAVE             49
#define PARAM_OSC1_RATE             50
#define PARAM_OSC1_INIT_PHASE       51
#define PARAM_OSC1_TRANSPOSE        52
#define PARAM_OSC1_FINE_TUNE        53
#define PARAM_OSC1_PITCHBEND        54
#define PARAM_OSC1_AM_LFO           55
#define PARAM_OSC1_AM_LFO_AMOUNT    56
#define PARAM_OSC1_FREQ_LFO         57
#define PARAM_OSC1_FREQ_LFO_AMOUNT  58
#define PARAM_OSC1_FREQ_LFO_FINE    59
#define PARAM_OSC1_PHASE_LFO        60
#define PARAM_OSC1_PHASE_LFO_AMOUNT 61
#define PARAM_OSC1_WAVE_LFO         62
#define PARAM_OSC1_WAVE_LFO_AMOUNT  63

#define PARAM_OSC2_MODULATION       64
#define PARAM_OSC2_POLARITY         65
#define PARAM_OSC2_FREQ_BASE        66
#define PARAM_OSC2_WAVE             67
#define PARAM_OSC2_RATE             68
#define PARAM_OSC2_INIT_PHASE       69
#define PARAM_OSC2_TRANSPOSE        70
#define PARAM_OSC2_FINE_TUNE        71
#define PARAM_OSC2_PITCHBEND        72
#define PARAM_OSC2_AM_LFO           73
#define PARAM_OSC2_AM_LFO_AMOUNT    74
#define PARAM_OSC2_FREQ_LFO         75
#define PARAM_OSC2_FREQ_LFO_AMOUNT  76
#define PARAM_OSC2_FREQ_LFO_FINE    77
#define PARAM_OSC2_PHASE_LFO        78
#define PARAM_OSC2_PHASE_LFO_AMOUNT 79
#define PARAM_OSC2_WAVE_LFO         80
#define PARAM_OSC2_WAVE_LFO_AMOUNT  81

#define PARAM_OSC3_MODULATION       82
#define PARAM_OSC3_POLARITY         83
#define PARAM_OSC3_FREQ_BASE        84
#define PARAM_OSC3_WAVE             85
#define PARAM_OSC3_RATE             86
#define PARAM_OSC3_INIT_PHASE       87
#define PARAM_OSC3_TRANSPOSE        88
#define PARAM_OSC3_FINE_TUNE        89
#define PARAM_OSC3_PITCHBEND        90
#define PARAM_OSC3_AM_LFO           91
#define PARAM_OSC3_AM_LFO_AMOUNT    92
#define PARAM_OSC3_FREQ_LFO         93
#define PARAM_OSC3_FREQ_LFO_AMOUNT  94
#define PARAM_OSC3_FREQ_LFO_FINE    95
#define PARAM_OSC3_PHASE_LFO        96
#define PARAM_OSC3_PHASE_LFO_AMOUNT 97
#define PARAM_OSC3_WAVE_LFO         98
#define PARAM_OSC3_WAVE_LFO_AMOUNT  99

#define PARAM_OSC4_MODULATION       100
#define PARAM_OSC4_POLARITY         101
#define PARAM_OSC4_FREQ_BASE        102
#define PARAM_OSC4_WAVE             103
#define PARAM_OSC4_RATE             104
#define PARAM_OSC4_INIT_PHASE       105
#define PARAM_OSC4_TRANSPOSE        106
#define PARAM_OSC4_FINE_TUNE        107
#define PARAM_OSC4_PITCHBEND        108
#define PARAM_OSC4_AM_LFO           109
#define PARAM_OSC4_AM_LFO_AMOUNT    110
#define PARAM_OSC4_FREQ_LFO         111
#define PARAM_OSC4_FREQ_LFO_AMOUNT  112
#define PARAM_OSC4_FREQ_LFO_FINE    113
#define PARAM_OSC4_PHASE_LFO        114
#define PARAM_OSC4_PHASE_LFO_AMOUNT 115
#define PARAM_OSC4_WAVE_LFO         116
#define PARAM_OSC4_WAVE_LFO_AMOUNT  117

#define PARAM_LFO1_POLARITY         118
#define PARAM_LFO1_FREQ_BASE        119
#define PARAM_LFO1_WAVE             120
#define PARAM_LFO1_RATE             121
#define PARAM_LFO1_INIT_PHASE       122
#define PARAM_LFO1_TRANSPOSE        123
#define PARAM_LFO1_PITCHBEND        124
#define PARAM_LFO1_VOICE_AM         125

#define PARAM_LFO2_POLARITY         126
#define PARAM_LFO2_FREQ_BASE        127
#define PARAM_LFO2_WAVE             128
#define PARAM_LFO2_RATE             129
#define PARAM_LFO2_INIT_PHASE       130
#define PARAM_LFO2_TRANSPOSE        131
#define PARAM_LFO2_PITCHBEND        132
#define PARAM_LFO2_LFO1_FM          133

#define PARAM_LFO3_POLARITY         134
#define PARAM_LFO3_FREQ_BASE        135
#define PARAM_LFO3_WAVE             136
#define PARAM_LFO3_RATE             137
#define PARAM_LFO3_INIT_PHASE       138
#define PARAM_LFO3_TRANSPOSE        139
#define PARAM_LFO3_PITCHBEND        140
#define PARAM_LFO3_CUTOFF           141

#define PARAM_LFO4_POLARITY         142
#define PARAM_LFO4_FREQ_BASE        143
#define PARAM_LFO4_WAVE             144
#define PARAM_LFO4_RATE             145
#define PARAM_LFO4_INIT_PHASE       146
#define PARAM_LFO4_TRANSPOSE        147
#define PARAM_LFO4_PITCHBEND        148
#define PARAM_LFO4_LFO3_FM          149

/* Update NUM_PARAMS after adding or removing parameters */
#define NUM_PARAMS                  150

/* The following only behave like parameters for the help system */
#define PARAM_MIDI_CHANNEL          150
#define PARAM_PART_NUMBER           151
#define PARAM_PROGRAM_NUMBER        152
#define PARAM_PATCH_NAME            153
#define PARAM_SESSION_NUMBER        154
#define PARAM_SESSION_NAME          155

/* Main help for PHASEX */
#define PARAM_PHASEX_HELP           156

/* Update MAX_PARAMS after adding or removing parameters */
#define MAX_PARAMS                  157
#define NUM_HELP_PARAMS             MAX_PARAMS

/* Parameter types */
#define PARAM_TYPE_HELP             0
#define PARAM_TYPE_INT              1
#define PARAM_TYPE_REAL             2
#define PARAM_TYPE_LIST             3
#define PARAM_TYPE_BOOL             4
#define PARAM_TYPE_RATE             5
#define PARAM_TYPE_BBOX             6
#define PARAM_TYPE_DTNT             7


struct param_info;

typedef struct param_nav_list {
	GtkWidget               *widget;
	int                     page_num;
	struct param_info       *param_info;
	struct param_nav_list   *prev;
	struct param_nav_list   *next;
} PARAM_NAV_LIST;


/* Parameter struct for handling conversion and value passing */
typedef struct param_info {
	const char      *name;          /* Parameter name for patches       */
	const char      *label_text;    /* Parameter label for GUI          */
	char            **strval_list;  /* List of string values for file saves */
	unsigned int    id;             /* Param unique ID.  See defs above.    */
	unsigned int    type;           /* Integer, real, or list values    */
	unsigned int    index;          /* Index for array (lfo or osc) params  */
	int             cc_num;         /* MIDI controller number           */
	int             cc_limit;       /* Upper bound for MIDI controller  */
	int             leap;           /* leap (moderately large step) size    */
	int             cc_offset;      /* CC to int val offset             */
	int             cc_default;     /* Default MIDI ctlr value          */
	int             locked;         /* Allow only user-explicit updates */
	int             prelight;       /* Prelight or hover state active   */
	int             focused;        /* Currently focused or selected    */
	int             sensitive;      /* Sensitivity state tracking       */
	int             sr_dep;         /* Sample rate dependency flag  ?????   */
	PARAM_NAV_LIST  *param_nav;     /* This param's entry in nav queue  */
	GtkKnob         *knob;          /* Widget that holds param value    */
	GtkObject       *adj;           /* Widget that holds param value    */
	GtkWidget       *spin;          /* Widget that holds param value    */
	GtkWidget       *combo;         /* Widget that holds param value    */
	GtkWidget       *text;          /* Widget that holds param value    */
	GtkWidget       *label;         /* Widget that holds param value    */
	GtkWidget       *event;         /* Event box (usually parent)       */
	GtkWidget       *table;         /* Param group table                */
	GtkWidget       *frame;         /* Param group frame                */
	GtkWidget       *button[12];    /* Widgets that hold param value    */
	GtkWidget       *button_event[12][2]; /* Widgets that send signals  */
	GtkWidget       *button_label[12]; /* For sensitivity management    */
	GSList          *button_group;
	const gchar     **list_labels;  /* List for list based parameters   */
	char            _padding[16];
} PARAM_INFO;


typedef struct param_value {
	int         cc_prev;    /* Previous MIDI ctlr value     */
	int         cc_val;     /* Current MIDI ctlr value      */
	int         int_val;    /* Current integer param value  */
	int         _padding;
} PARAM_VAL;


/* forward declaration */
struct patch;

typedef struct phasex_param {
	PARAM_INFO      *info;
	PARAM_VAL       value;
	struct patch    *patch;     /* container for this param     */
	int             updated;    /* dirty flag for gui           */
	char            _padding[24];
} PARAM;


typedef void (*PARAM_CB)(PARAM *param);
//typedef void (*PHASEX_GUI_CB) (GtkWidget *widget, gpointer *data);
//typedef int (*PARSE_CB) (char *token, char *filename, int line);


typedef struct param_cb_info {
	PARAM_CB        update_patch_state; /* Callback for updating engine state   */
	//PARSE_CB        parse;              /* Get cc_val from strval               */
	//STRVAL_CB       get_strval;         /* Get parseable strval from param val  */
} PARAM_CB_INFO;


extern PARAM_INFO       phasex_param_info[NUM_HELP_PARAMS];
extern PARAM_CB_INFO    cb_info[NUM_HELP_PARAMS];

extern PARAM_NAV_LIST   *param_nav_head;
extern PARAM_NAV_LIST   *param_nav_tail;


PARAM *get_param(unsigned int part_num, unsigned int param_id);
PARAM *get_param_by_name(struct patch *patch, char *param_name);
int get_param_id_by_name(char *param_name);
PARAM_INFO *get_param_info_by_id(unsigned int param_id);
PARAM_CB_INFO *get_param_cb_info_by_id(unsigned int param_id);
void run_param_callbacks(int init);
void init_param_info(unsigned int id, const char *name, const char *label, unsigned int type,
                     int cc, int lim, int ccv, int ofst, unsigned int ix, int leap, int sr_dep,
                     PARAM_CB callback, const char *label_list[], char *strval_list[]);
void init_params(void);


#endif /* _PHASEX_PARAM_H_ */
