/*****************************************************************************
 *
 * gui_layout.h
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
 *
 *****************************************************************************/
#ifndef _PHASEX_GUI_LAYOUT_H_
#define _PHASEX_GUI_LAYOUT_H_

#include <gtk/gtk.h>
#include "phasex.h"


/* Update NUM_PARAM_GROUPS after changing groups in param.c */
#define NUM_PARAM_GROUPS        20

/* Update NUM_PARAM_PAGES after changing paes in param.c */
#define NUM_PARAM_PAGES         2


/* Parameter group, with array of parameter IDs */
typedef struct param_group {
	gchar           *name;          /* Name to use for naming frame widget  */
	gchar           *label;         /* Frame label for parameter group      */
	int             full_x;         /* Table column in fullscreen layout    */
	int             group_order;    /* Layout specific ordering             */
	int             notebook_page;  /* Tab number, starting at 0            */
	int             notebook_x;     /* Table column in notebook layout      */
	int             wide_x;         /* Table column in widescreen layout    */
	int             param_list[16]; /* List of up to 15 parameters          */
	GtkWidget       *event;
	GtkWidget       *frame;
	GtkWidget       *frame_event;
	GtkWidget       *table;
} PARAM_GROUP;

/* Parameter page definition */
typedef struct param_page {
	gchar           *label;         /* Label for GUI page                   */
} PARAM_PAGE;


/* Parameter groups and pages reference paramenters */
extern PARAM_GROUP  param_group[NUM_PARAM_GROUPS];
extern PARAM_PAGE   param_page[NUM_PARAM_PAGES];

extern int          notebook_order[NUM_PARAM_GROUPS];
extern int          one_page_order[NUM_PARAM_GROUPS];
extern int          widescreen_order[NUM_PARAM_GROUPS];
extern int          param_order_by_group[NUM_PARAM_GROUPS];
extern int          param_group_by_order[NUM_PARAM_GROUPS];


void init_param_groups(void);
void init_param_pages(void);
void create_param_group(GtkWidget *main_window, GtkWidget *parent_vbox,
                        PARAM_GROUP *param_group, int page_num);
void create_param_notebook_page(GtkWidget *main_window, GtkWidget *notebook,
                                PARAM_PAGE *param_page, int page_num);
void create_param_notebook(GtkWidget *main_window, GtkWidget *box,
                           PARAM_PAGE *param_page);
void create_param_one_page(GtkWidget *main_window, GtkWidget *box,
                           PARAM_PAGE *UNUSED(param_page));
void create_param_widescreen(GtkWidget *main_window, GtkWidget *box,
                             PARAM_PAGE *UNUSED(param_page));


#endif /* _PHASEX_GUI_LAYOUT_H_ */
