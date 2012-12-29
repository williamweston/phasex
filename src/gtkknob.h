/*****************************************************************************
 *
 * gtkknob.h
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Original GtkKnob from gAlan 0.2.0 Copyright (C) 1999 Tony Garnock-Jones
 *
 * Modifications by Sean Bolton:     Copyright (C) 2004, 2008-2010
 * Modifications by Pete Shorthose:  Copyright (C) 2007
 * Modifications by William Weston:  Copyright (C) 2007-2009, 2012
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
#ifndef __GTK_KNOB_H__
#define __GTK_KNOB_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif


#define GTK_KNOB(obj)           GTK_CHECK_CAST(obj, gtk_knob_get_type(), GtkKnob)
#define GTK_KNOB_CLASS(klass)   GTK_CHECK_CLASS_CAST(klass, gtk_knob_get_type(), GtkKnobClass)
#define GTK_IS_KNOB(obj)        GTK_CHECK_TYPE(obj, gtk_knob_get_type())


	typedef struct _GtkKnob         GtkKnob;
	typedef struct _GtkKnobClass    GtkKnobClass;
	typedef struct _GtkKnobAnim     GtkKnobAnim;


	/* better to make this an object and let widgets ref/deref it perhaps */
	struct _GtkKnobAnim {
		GdkPixbuf       *pixbuf;
		gint            width;          /* derived from image width   */
		gint            height;         /* derived from image height. */
		gint            frame_width;    /* derived from pixbuf (width / height)
		                                   or provided override for rectangular frames */
	};

	struct _GtkKnob {
		GtkWidget       widget;

		/* update policy (GTK_UPDATE_[CONTINUOUS/DELAYED/DISCONTINUOUS]) */
		guint           policy : 2;

		/* State of widget (to do with user interaction) */
		guint8          state;
		gint            saved_x;
		gint            saved_y;

		/* ID of update timer, or 0 if none */
		guint32         timer;

		/* knob animation */
		GtkKnobAnim     *anim;
		gint            width;
		gint            height;
		gint            frame_offset;

		GdkBitmap       *mask;
		GdkGC           *mask_gc;

		/* Old values from adjustment stored so we know when something changes */
		gfloat          old_value;
		gfloat          old_lower;
		gfloat          old_upper;

		/* The adjustment object that stores the data for this knob */
		GtkAdjustment   *adjustment;
	};

	struct _GtkKnobClass {
		GtkWidgetClass  parent_class;
	};


	extern int      knob_width[14];
	extern int      knob_height[14];


	GtkWidget *gtk_knob_new(GtkAdjustment *adjustment, GtkKnobAnim *anim);
	GType gtk_knob_get_type(void);
	GtkAdjustment *gtk_knob_get_adjustment(GtkKnob *knob);
	void gtk_knob_set_update_policy(GtkKnob *knob, GtkUpdateType  policy);
	void gtk_knob_set_adjustment(GtkKnob *knob, GtkAdjustment *adjustment);

	GtkKnobAnim *gtk_knob_animation_new_from_file_full(gchar *filename,
	                                                   gint frame_width,
	                                                   gint width,
	                                                   gint height);

	GtkKnobAnim *gtk_knob_animation_new_from_file(gchar *filename);
	void gtk_knob_set_animation(GtkKnob *knob, GtkKnobAnim *anim);
	void gtk_knob_animation_destroy(GtkKnobAnim *anim);


#ifdef __cplusplus
}
#endif


#endif /* __GTK_KNOB_H__ */
