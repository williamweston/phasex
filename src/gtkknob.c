/*****************************************************************************
 *
 * gtkknob.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Original GtkKnob from gAlan 0.2.0 Copyright (C) 1999 Tony Garnock-Jones
 *
 * Modifications by Sean Bolton:     Copyright (C) 2004, 2008-2010
 * Modifications by Pete Shorthose:  Copyright (C) 2007
 * Modifications by William Weston:  Copyright (C) 2007-2008, 2012-2013
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
#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>

#include "config.h"
#include "gtkknob.h"
#include "settings.h"
#include "debug.h"


#ifndef M_PI
# define M_PI       3.14159265358979323846  /* pi */
#endif
#ifndef M_1_PI
# define M_1_PI     0.31830988618379067154  /* 1/pi */
#endif


#define SCROLL_DELAY_LENGTH 250

#define STATE_IDLE          0
#define STATE_PRESSED       1
#define STATE_DRAGGING      2

static void gtk_knob_class_init(GtkKnobClass *class);
static void gtk_knob_init(GtkKnob *knob);
static void gtk_knob_destroy(GtkObject *object);
static void gtk_knob_realize(GtkWidget *widget);
static void gtk_knob_size_request(GtkWidget *widget, GtkRequisition *requisition);
static void gtk_knob_size_allocate(GtkWidget *widget, GtkAllocation *allocation);
static gint gtk_knob_expose(GtkWidget *widget, GdkEventExpose *event);
static gint gtk_knob_scroll(GtkWidget *widget, GdkEventScroll *event);
static gint gtk_knob_button_press(GtkWidget *widget, GdkEventButton *event);
static gint gtk_knob_button_release(GtkWidget *widget, GdkEventButton *event);
static gint gtk_knob_motion_notify(GtkWidget *widget, GdkEventMotion *event);
static gint gtk_knob_timer(GtkKnob *knob);

static void gtk_knob_update_mouse_update(GtkKnob *knob);
static void gtk_knob_update_mouse(GtkKnob *knob, gint x, gint y, gboolean absolute);
static void gtk_knob_update(GtkKnob *knob);
static void gtk_knob_adjustment_changed(GtkAdjustment *adjustment, gpointer data);
static void gtk_knob_adjustment_value_changed(GtkAdjustment *adjustment, gpointer data);

GError *gerror;

int knob_width[14]  = { 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, -1 };
int knob_height[14] = { 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, -1 };


/* Local data */

static GtkWidgetClass *knob_parent_class = NULL;


/*****************************************************************************
 * gtk_knob_get_type()
 *****************************************************************************/
GType
gtk_knob_get_type(void)
{
	static GType knob_type = 0;

	if (!knob_type) {
		static const GTypeInfo info = {
			sizeof(GtkKnobClass),
			NULL,
			NULL,
			(GClassInitFunc) gtk_knob_class_init,
			NULL,
			NULL,
			sizeof(GtkKnob),
			0,
			(GInstanceInitFunc) gtk_knob_init,
			NULL
		};

		knob_type = g_type_register_static(GTK_TYPE_WIDGET,
		                                   "GtkKnob",
		                                   &info,
		                                   0);
	}

	return knob_type;
}


/*****************************************************************************
 * gtk_knob_class_init()
 *****************************************************************************/
static void
gtk_knob_class_init(GtkKnobClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	knob_parent_class = gtk_type_class(gtk_widget_get_type());

	object_class->destroy = gtk_knob_destroy;

	widget_class->realize              = gtk_knob_realize;
	widget_class->expose_event         = gtk_knob_expose;
	widget_class->size_request         = gtk_knob_size_request;
	widget_class->size_allocate        = gtk_knob_size_allocate;
	widget_class->scroll_event         = gtk_knob_scroll;
	widget_class->button_press_event   = gtk_knob_button_press;
	widget_class->button_release_event = gtk_knob_button_release;
	widget_class->motion_notify_event  = gtk_knob_motion_notify;
}


/*****************************************************************************
 * gtk_knob_init()
 *****************************************************************************/
static void
gtk_knob_init(GtkKnob *knob)
{
	knob->policy     = GTK_UPDATE_CONTINUOUS;
	knob->state      = STATE_IDLE;
	knob->saved_x    = 0;
	knob->saved_y    = 0;
	knob->timer      = 0;
	knob->anim       = NULL;
	knob->mask       = NULL;
	knob->mask_gc    = NULL;
	knob->old_value  = 0.0;
	knob->old_lower  = 0.0;
	knob->old_upper  = 0.0;
	knob->adjustment = NULL;
}


/*****************************************************************************
 * gtk_knob_new()
 *****************************************************************************/
GtkWidget *
gtk_knob_new(GtkAdjustment *adjustment, GtkKnobAnim *anim)
{
	GtkKnob *knob;

	g_return_val_if_fail(anim != NULL, NULL);
	g_return_val_if_fail(GDK_IS_PIXBUF(anim->pixbuf), NULL);

	knob = gtk_type_new(gtk_knob_get_type());

	gtk_knob_set_animation(knob, anim);

	if (!adjustment) {
		adjustment = (GtkAdjustment *) gtk_adjustment_new(0.0, 0.0, 0.0,
		                                                  0.0, 0.0, 0.0);
	}

	gtk_knob_set_adjustment(knob, adjustment);

	return GTK_WIDGET(knob);
}


/*****************************************************************************
 * gtk_knob_destroy()
 *****************************************************************************/
static void
gtk_knob_destroy(GtkObject *object)
{
	GtkKnob *knob;

	g_return_if_fail(object != NULL);
	g_return_if_fail(GTK_IS_KNOB(object));

	knob = GTK_KNOB(object);

	if (knob->adjustment != NULL) {
		g_signal_handlers_disconnect_by_func(knob->adjustment,
		                                     gtk_knob_adjustment_changed,
		                                     knob);
		g_signal_handlers_disconnect_by_func(knob->adjustment,
		                                     gtk_knob_adjustment_value_changed,
		                                     knob);
		gtk_object_unref(GTK_OBJECT(knob->adjustment));
		knob->adjustment = NULL;
	}
	if ((knob->anim != NULL) && (knob->anim->pixbuf) != NULL) {
		g_object_unref(G_OBJECT(knob->anim->pixbuf));
		knob->anim = NULL;
	}
	if (knob->mask != NULL) {
		gdk_bitmap_unref(knob->mask);
		knob->mask = NULL;
	}
	if (knob->mask_gc != NULL) {
		gdk_gc_unref(knob->mask_gc);
		knob->mask_gc = NULL;
	}
	if (knob->timer) {
		g_source_remove(knob->timer);
		knob->timer = 0;
	}

	if (GTK_OBJECT_CLASS(knob_parent_class)->destroy) {
		(*GTK_OBJECT_CLASS(knob_parent_class)->destroy)(object);
	}
}


/*****************************************************************************
 * gtk_knob_get_adjustment()
 *****************************************************************************/
GtkAdjustment *
gtk_knob_get_adjustment(GtkKnob *knob)
{
	g_return_val_if_fail((knob != NULL), NULL);
	g_return_val_if_fail(GTK_IS_KNOB(knob), NULL);

	return knob->adjustment;
}


/*****************************************************************************
 * gtk_knob_set_update_policy()
 *****************************************************************************/
void
gtk_knob_set_update_policy(GtkKnob *knob, GtkUpdateType policy)
{
	g_return_if_fail(knob != NULL);
	g_return_if_fail(GTK_IS_KNOB(knob));

	knob->policy = policy;
}


/*****************************************************************************
 * gtk_knob_set_frame_offset()
 *****************************************************************************/
gint
gtk_knob_set_frame_offset(GtkKnob *knob, gfloat value)
{
	g_return_val_if_fail((knob != NULL), 0);
	g_return_val_if_fail(GTK_IS_KNOB(knob), 0);

	if ((knob->anim != NULL) && (knob->adjustment != NULL)) {
		knob->frame_offset = (int)((((gfloat)(knob->anim->width) /
		                             (gfloat)(knob->anim->frame_width)) - 1) *
		                           (value - knob->adjustment->lower) *
		                           (1.0 / ((gfloat)(knob->adjustment->upper) -
		                                   (gfloat)(knob->adjustment->lower)))
		                           ) * knob->width;
	}
	return knob->frame_offset;
}


/*****************************************************************************
 * gtk_knob_set_adjustment()
 *
 * Establishes supplied adjustment as the knob's internal adjustment.
 *****************************************************************************/
void
gtk_knob_set_adjustment(GtkKnob *knob, GtkAdjustment *adjustment)
{
	g_return_if_fail(knob != NULL);
	g_return_if_fail(GTK_IS_KNOB(knob));

	if (knob->adjustment) {
		g_signal_handlers_disconnect_by_func(knob->adjustment,
		                                     gtk_knob_adjustment_changed,
		                                     knob);
		g_signal_handlers_disconnect_by_func(knob->adjustment,
		                                     gtk_knob_adjustment_value_changed,
		                                     knob);
		gtk_object_unref(GTK_OBJECT(knob->adjustment));
	}

	knob->adjustment = adjustment;
	gtk_object_ref(GTK_OBJECT(knob->adjustment));
	gtk_object_sink(GTK_OBJECT(knob->adjustment));

	gtk_signal_connect(GTK_OBJECT(adjustment), "changed",
	                   GTK_SIGNAL_FUNC(gtk_knob_adjustment_changed),
	                   (gpointer) knob);
	gtk_signal_connect(GTK_OBJECT(adjustment), "value_changed",
	                   GTK_SIGNAL_FUNC(gtk_knob_adjustment_value_changed),
	                   (gpointer) knob);

	knob->old_value = (gfloat) adjustment->value;
	knob->old_lower = (gfloat) adjustment->lower;
	knob->old_upper = (gfloat) adjustment->upper;

	gtk_knob_set_frame_offset(knob, adjustment->value);
	gtk_knob_update(knob);
}


/*****************************************************************************
 * gtk_knob_realize()
 *****************************************************************************/
static void
gtk_knob_realize(GtkWidget *widget)
{
	GtkKnob         *knob;
	GdkWindowAttr   attributes;
	gint            attributes_mask;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_KNOB(widget));

	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
	knob = GTK_KNOB(widget);

	attributes.x           = widget->allocation.x;
	attributes.y           = widget->allocation.y;
	attributes.width       = widget->allocation.width;
	attributes.height      = widget->allocation.height;
	attributes.wclass      = GDK_INPUT_OUTPUT;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.event_mask  =
		gtk_widget_get_events(widget) |
		GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
		GDK_POINTER_MOTION_HINT_MASK;
	attributes.visual   = gtk_widget_get_visual(widget);
	attributes.colormap = gtk_widget_get_colormap(widget);

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new(widget->parent->window, &attributes, attributes_mask);
	widget->style  = gtk_style_attach(widget->style, widget->window);

	gdk_window_set_user_data(widget->window, widget);
	gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

	knob->mask_gc = gdk_gc_new(widget->window);
	gdk_gc_copy(knob->mask_gc, widget->style->bg_gc[GTK_STATE_NORMAL]);
	gdk_gc_set_clip_mask(knob->mask_gc, knob->mask);
}


/*****************************************************************************
 * gtk_knob_size_request()
 *****************************************************************************/
static void
gtk_knob_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_KNOB(widget));

	requisition->width  = GTK_KNOB(widget)->width;
	requisition->height = GTK_KNOB(widget)->height;
}


/*****************************************************************************
 * gtk_knob_size_allocate()
 *****************************************************************************/
static void
gtk_knob_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	g_return_if_fail(widget != NULL);
	g_return_if_fail(GTK_IS_KNOB(widget));
	g_return_if_fail(allocation != NULL);

	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED(widget)) {
		gdk_window_move_resize(widget->window,
		                       allocation->x, allocation->y,
		                       allocation->width, allocation->height);
	}
}


/*****************************************************************************
 * gtk_knob_expose()
 *****************************************************************************/
static gint
gtk_knob_expose(GtkWidget *widget, GdkEventExpose *event)
{
	GtkKnob *knob;
	cairo_t *cairo_context;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(GTK_IS_KNOB(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	if (event->count > 0) {
		return FALSE;
	}

	knob = GTK_KNOB(widget);

#if GTK_CHECK_VERSION(2, 8, 0)
	cairo_context = gdk_cairo_create(GDK_DRAWABLE(widget->window));
	gdk_cairo_set_source_pixbuf(cairo_context, knob->anim->pixbuf, (0 - knob->frame_offset), 0);
	cairo_paint(cairo_context);
	cairo_destroy(cairo_context);
#else
	gdk_draw_pixbuf(widget->window, knob->mask_gc, knob->anim->pixbuf,
	                knob->frame_offset, 0, 0, 0, knob->width, knob->height,
	                GDK_RGB_DITHER_NONE, 0, 0);
#endif

	return FALSE;
}


/*****************************************************************************
 * gtk_knob_scroll()
 *****************************************************************************/
static gint
gtk_knob_scroll(GtkWidget *widget, GdkEventScroll *event)
{
	GtkKnob *knob;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(GTK_IS_KNOB(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	knob = GTK_KNOB(widget);

	switch (event->direction) {
	case GDK_SCROLL_UP:
		knob->adjustment->value += knob->adjustment->step_increment;
		gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment),
		                        "value_changed");
		break;
	case GDK_SCROLL_DOWN:
		knob->adjustment->value -= knob->adjustment->step_increment;
		gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment),
		                        "value_changed");
		break;
	default:
		break;
	}

	return FALSE;
}


/*****************************************************************************
 * gtk_knob_button_press()
 *****************************************************************************/
static gint
gtk_knob_button_press(GtkWidget *widget, GdkEventButton *event)
{
	GtkKnob *knob;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(GTK_IS_KNOB(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	knob = GTK_KNOB(widget);

	switch (knob->state) {
	case STATE_IDLE:
		switch (event->button) {
		case 1:
		case 3:
			gtk_grab_add(widget);
			knob->state   = STATE_PRESSED;
			knob->saved_x = (gint) event->x;
			knob->saved_y = (gint) event->y;
			break;
		case 2:
			knob->adjustment->value = floor((knob->adjustment->lower +
			                                 knob->adjustment->upper + 1.0)
			                                * 0.5);
			gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment),
			                        "value_changed");
			break;
		}
		break;
	}

	return FALSE;
}


/*****************************************************************************
 * gtk_knob_button_release()
 *****************************************************************************/
static gint
gtk_knob_button_release(GtkWidget *widget, GdkEventButton *event)
{
	GtkKnob *knob;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(GTK_IS_KNOB(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	knob = GTK_KNOB(widget);

	switch (knob->state) {

	case STATE_PRESSED:
		gtk_grab_remove(widget);
		knob->state = STATE_IDLE;

		switch (event->button) {
		case 1:
			knob->adjustment->value -= knob->adjustment->page_increment;
			gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");
			break;
		case 3:
			knob->adjustment->value += knob->adjustment->page_increment;
			gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");
			break;
		}
		break;

	case STATE_DRAGGING:
		gtk_grab_remove(widget);
		knob->state = STATE_IDLE;

		switch (event->button) {
		case 1:
		case 3:
			if (knob->policy != GTK_UPDATE_CONTINUOUS
			    && knob->old_value != knob->adjustment->value) {
				gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");
			}
			break;
		}

		break;
	}

	return FALSE;
}


/*****************************************************************************
 * gtk_knob_motion_notify()
 *****************************************************************************/
static gint
gtk_knob_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
	GtkKnob         *knob;
	GdkModifierType mods;
	gint            x;
	gint            y;

	g_return_val_if_fail(widget != NULL, FALSE);
	g_return_val_if_fail(GTK_IS_KNOB(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);

	knob = GTK_KNOB(widget);

	x = (gint) event->x;
	y = (gint) event->y;

	if (event->is_hint || (event->window != widget->window)) {
		gdk_window_get_pointer(widget->window, &x, &y, &mods);
	}

	switch (knob->state) {

	case STATE_PRESSED:
		knob->state = STATE_DRAGGING;
		/* fall through */

	case STATE_DRAGGING:
		if (mods & GDK_BUTTON1_MASK) {
			gtk_knob_update_mouse(knob, x, y, TRUE);
			return TRUE;
		}
		else if (mods & GDK_BUTTON3_MASK) {
			gtk_knob_update_mouse(knob, x, y, FALSE);
			return TRUE;
		}
		break;
	}

	return FALSE;
}


/*****************************************************************************
 * gtk_knob_timer()
 *****************************************************************************/
static gint
gtk_knob_timer(GtkKnob *knob)
{
	g_return_val_if_fail(knob != NULL, FALSE);
	g_return_val_if_fail(GTK_IS_KNOB(knob), FALSE);

	if (knob->policy == GTK_UPDATE_DELAYED) {
		gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");
	}

	/* don't keep running this timer */
	return FALSE;
}


/*****************************************************************************
 * gtk_knob_update_mouse_update()
 *
 * Called only from gtk_knob_update_mouse(), and only when it is determined
 * that the widget be visibly updated.
 *****************************************************************************/
static void
gtk_knob_update_mouse_update(GtkKnob *knob)
{
	if (knob->policy == GTK_UPDATE_CONTINUOUS) {
		gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");
	}
	else {
		gtk_knob_update(knob);

		if (knob->policy == GTK_UPDATE_DELAYED) {
			if (knob->timer) {
				gtk_timeout_remove(knob->timer);
			}
			knob->timer = gtk_timeout_add(SCROLL_DELAY_LENGTH,
			                              (GtkFunction) gtk_knob_timer,
			                              (gpointer) knob);
		}
	}
}


/*****************************************************************************
 * gtk_knob_update_mouse()
 *
 * Process mouse motion (currently click-drag) events to calculate new
 * knob value.
 *****************************************************************************/
static void
gtk_knob_update_mouse(GtkKnob *knob, gint x, gint y, gboolean absolute)
{
	gfloat  old_value;
	gfloat  new_value;
	gfloat  dv;
	gfloat  dh;
	gfloat  angle;
	gfloat  range;
	gfloat  scale;

	g_return_if_fail(knob != NULL);
	g_return_if_fail(GTK_IS_KNOB(knob));

	old_value = (gfloat) knob->adjustment->value;

	range = (gfloat)(knob->adjustment->upper - knob->adjustment->lower + 1.0);
	scale = range * range / 16384.0;

	angle = atan2f((float)(-y + (knob->height >> 1) + 2), (float)(x - (knob->width >> 1) - 1));

	if (absolute) {
		/* map [1.25pi, -0.25pi] onto [0, 1] */
		angle *= M_1_PI;
		if (angle < -0.5) {
			angle += 2.0;
		}
		new_value = 0.66666666666666666666 * (1.25 - angle);
		new_value *= (gfloat)(knob->adjustment->upper - knob->adjustment->lower);
		new_value += (gfloat) knob->adjustment->lower;
	}
	else {
		dv = (gfloat)(knob->saved_y - y);
		dh = (gfloat)(x - knob->saved_x);

		new_value = (gfloat)(knob->adjustment->value +
		                     ((dv + dh) * scale * knob->adjustment->step_increment));
	}

	new_value = (gfloat) MAX(MIN(new_value, knob->adjustment->upper),
	                         knob->adjustment->lower);

	knob->adjustment->value = new_value;

	if (floorf((float) knob->adjustment->value + 0.5) != floorf(old_value + 0.5)) {
		gtk_knob_update_mouse_update(knob);
		knob->saved_x = x;
		knob->saved_y = y;
	}
}


/*****************************************************************************
 * gtk_knob_update()
 *****************************************************************************/
static void
gtk_knob_update(GtkKnob *knob)
{
	gfloat new_value;

	g_return_if_fail(knob != NULL);
	g_return_if_fail(GTK_IS_KNOB(knob));

	if (knob->adjustment->step_increment == 1) {
		new_value = floorf((float) knob->adjustment->value + 0.5);
	}
	else {
		new_value = (gfloat) knob->adjustment->value;
	}

	if (new_value < knob->adjustment->lower) {
		new_value = (gfloat) knob->adjustment->lower;
	}
	else if (new_value > knob->adjustment->upper) {
		new_value = (gfloat) knob->adjustment->upper;
	}

	gtk_knob_set_frame_offset(knob, new_value);

	if (new_value != knob->adjustment->value) {
		knob->adjustment->value = new_value;
		gtk_signal_emit_by_name(GTK_OBJECT(knob->adjustment), "value_changed");
	}

	gtk_widget_draw(GTK_WIDGET(knob), NULL);
}


/*****************************************************************************
 * gtk_knob_adjustment_changed()
 *****************************************************************************/
static void
gtk_knob_adjustment_changed(GtkAdjustment *adjustment, gpointer data)
{
	GtkKnob *knob;

	g_return_if_fail(adjustment != NULL);
	g_return_if_fail(data != NULL);

	knob = GTK_KNOB(data);

	if ((knob->old_value != adjustment->value) ||
	    (knob->old_lower != adjustment->lower) ||
	    (knob->old_upper != adjustment->upper)) {
		gtk_knob_update(knob);

		knob->old_value = (gfloat) adjustment->value;
		knob->old_lower = (gfloat) adjustment->lower;
		knob->old_upper = (gfloat) adjustment->upper;
	}
}


/*****************************************************************************
 * gtk_knob_adjustment_value_changed()
 *****************************************************************************/
static void
gtk_knob_adjustment_value_changed(GtkAdjustment *adjustment, gpointer data)
{
	GtkKnob *knob;

	g_return_if_fail(adjustment != NULL);
	g_return_if_fail(data != NULL);

	knob = GTK_KNOB(data);

	if (adjustment->value > adjustment->upper) {
		adjustment->value = adjustment->upper;
	}
	else if (adjustment->value < adjustment->lower) {
		adjustment->value = adjustment->lower;
	}
	if (knob->old_value != adjustment->value) {
		gtk_knob_update(knob);
		knob->old_value = (gfloat) adjustment->value;
	}
}


/*****************************************************************************
 * gtk_knob_set_animation()
 *****************************************************************************/
void
gtk_knob_set_animation(GtkKnob *knob, GtkKnobAnim *anim)
{
	g_return_if_fail(knob != NULL);
	g_return_if_fail(anim != NULL);
	g_return_if_fail(GTK_IS_KNOB(knob));
	g_return_if_fail(GDK_IS_PIXBUF(anim->pixbuf));

	if ((knob->anim != NULL) && (knob->anim->pixbuf != NULL)) {
		g_object_unref(G_OBJECT(knob->anim->pixbuf));
	}
	knob->anim   = (GtkKnobAnim *) anim;
	knob->width  = anim->frame_width;
	knob->height = anim->height;
	gtk_knob_set_frame_offset(knob, (knob->adjustment == NULL) ?
	                          0.0 : knob->adjustment->value);
	g_object_ref(G_OBJECT(knob->anim->pixbuf));

	if (GTK_WIDGET_REALIZED(knob)) {
		gtk_widget_queue_resize(GTK_WIDGET(knob));
	}
}


/*****************************************************************************
 * gtk_knob_animation_new_from_file()
 *****************************************************************************/
GtkKnobAnim *
gtk_knob_animation_new_from_file(gchar *filename)
{
	GtkKnobAnim *anim;

	anim = gtk_knob_animation_new_from_file_full(filename,
	                                             knob_width[setting_knob_size],
	                                             -1,
	                                             knob_height[setting_knob_size]);
	return anim;
}


/*****************************************************************************
 * gtk_knob_animation_new_from_file_full()
 *
 * frame_width: overrides the frame width (to make rectangular frames)
 * but doesn't affect the image size.  Width and height cause optional
 * scaling if not set to -1 when they are derived from the native
 * image size.
 *
 * FIXME: account for any problems where (width % frame_width != 0)
 *****************************************************************************/
GtkKnobAnim *
gtk_knob_animation_new_from_file_full(gchar *filename, gint frame_width,
                                      gint width, gint height)
{
	GtkKnobAnim *anim = g_new0(GtkKnobAnim, 1);

	g_return_val_if_fail((filename != NULL), NULL);

#if GTK_CHECK_VERSION(2, 10, 0)
	if (!(anim->pixbuf = gdk_pixbuf_new_from_file_at_size(filename, width,
	                                                      height, &gerror))) {
		return NULL;
	}
#else
	if (!(anim->pixbuf = gdk_pixbuf_new_from_file(filename, &gerror))) {
		return NULL;
	}
#endif
	else {
		g_object_ref(G_OBJECT(anim->pixbuf));
		anim->height      = gdk_pixbuf_get_height(anim->pixbuf);
		anim->width       = gdk_pixbuf_get_width(anim->pixbuf);
		anim->frame_width = (frame_width != -1) ? frame_width : anim->height;
	}

	return anim;
}


/*****************************************************************************
 * gtk_knob_animation_destroy()
 *
 * TODO:  turn GtkKnobAnim into a proper GObject initialized with a
 * floating reference.
 *****************************************************************************/
void
gtk_knob_animation_destroy(GtkKnobAnim *anim)
{
	g_return_if_fail(anim != NULL);
	if (anim->pixbuf != NULL) {
		g_object_unref(G_OBJECT(anim->pixbuf));
	}
	g_free(anim);
}
