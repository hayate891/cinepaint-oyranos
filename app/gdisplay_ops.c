/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "../lib/version.h"
#include "libgimp/gimpintl.h"
#include "appenv.h"
#include "actionarea.h"
#include "colormaps.h"
#include "cursorutil.h"
#include "fileops.h"
#include "gdisplay_ops.h"
#include "general.h"
#include "gimage.h"
#include "gximage.h"
#include "interface.h"
#include "menus.h"
#include "scale.h"
#include "rc.h"
#include "minimize.h"
#include "layout.h"
#include "base_frame_manager.h"

#ifdef GDK_WINDOWING_X11
#include "X11/Xcm/Xcm.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#endif

static void gdisplay_close_warning_callback (GtkWidget *, gpointer);
static void gdisplay_cancel_warning_callback (GtkWidget *, gpointer);
static void gdisplay_close_warning_dialog   (char *, GDisplay *);

static GtkWidget *warning_dialog = NULL;

/*
 *  This file is for operations on the gdisplay object
 */

gulong
gdisplay_white_pixel (GDisplay *gdisp)
{
  return g_white_pixel;
}

gulong
gdisplay_gray_pixel (GDisplay *gdisp)
{
  return g_gray_pixel;
}

gulong
gdisplay_black_pixel (GDisplay *gdisp)
{
  return g_black_pixel;
}

gulong
gdisplay_color_pixel (GDisplay *gdisp)
{
  return g_color_pixel;
}


void
gdisplay_new_view (GDisplay *gdisp)
{
  GDisplay *new_gdisp;

  /* make sure the image has been fully loaded... */
  if (gdisp->gimage)
    {
      new_gdisp = gdisplay_new (gdisp->gimage, gdisp->scale);
      new_gdisp->scale = gdisp->scale;
      new_gdisp->offset_x = 0; //gdisp->offset_x;
      new_gdisp->offset_y = 0; //gdisp->offset_y;
    }
}


void
gdisplay_close_window (GDisplay *gdisp,
		       int       kill_it)
{
  /*  If the image has been modified, give the user a chance to save
   *  it before nuking it--this only applies if its the last view
   *  to an image canvas.  (a gimage with ref_count = 1)
   */
  if (!kill_it && (gdisp->gimage->ref_count == 1) &&
      (gdisp->gimage->dirty != 0) && confirm_on_close )
    gdisplay_close_warning_dialog (prune_filename (gimage_filename (gdisp->gimage)), gdisp);
  else
    {
      /* If POPUP_SHELL references this shell, then reset it. */
      if (popup_shell == gdisp->shell)
	popup_shell = NULL;
      /* If the Frame manager shell is open to this display, then destroy it. - twh - 13July2004 */
      if(gdisp->bfm && gdisp->bfm->sfm->shell)
	gtk_widget_destroy (GTK_WIDGET(gdisp->bfm->sfm->shell));
      gtk_widget_destroy (gdisp->shell);
    }
}


void
gdisplay_shrink_wrap (GDisplay *gdisp)
{
  gint x, y;
  gint disp_width, disp_height;
  gint width, height;
  gint shell_x, shell_y;
  gint shell_width, shell_height;
  gint max_auto_width, max_auto_height;
  gint border_x, border_y;
  int s_width, s_height;

  s_width = gdk_screen_width ();
  s_height = gdk_screen_height ();

  width = SCALE (gdisp, gdisp->gimage->width);
  height = SCALE (gdisp, gdisp->gimage->height);

  disp_width = gdisp->disp_width;
  disp_height = gdisp->disp_height;

  shell_width = gdisp->shell->allocation.width;
  shell_height = gdisp->shell->allocation.height;

  border_x = shell_width - disp_width;
  border_y = shell_height - disp_height;

  max_auto_width = (s_width - border_x) * 0.75;
  max_auto_height = (s_height - border_y) * 0.75;

  if(max_auto_width < disp_width)
    max_auto_width = disp_width;
  if(max_auto_height < disp_height)
    max_auto_height = disp_height;

  /*  If 1) the projected width & height are smaller than screen size, &
   *     2) the current display size isn't already the desired size, expand
   */
  if (((width + border_x) < s_width || (height + border_y) < s_height) &&
      (width != disp_width || height != disp_height))
    {
      gint mw = gdisp->menubar->requisition.width;
      width = ((width + border_x) < s_width) ? width : max_auto_width;
      height = ((height + border_y) < s_height) ? height : max_auto_height;

#if GTK_MAJOR_VERSION > 1
      gtk_window_resize ( GTK_WINDOW(gdisp->shell),
			    MAX(width, mw) + border_x,
                            height + border_y);
#endif
      gtk_widget_set_usize (gdisp->canvas,
			    width, height);

      gtk_widget_show (gdisp->canvas);

      gdk_window_get_origin (gdisp->shell->window, &shell_x, &shell_y);

      shell_width = width + border_x;
      shell_height = height + border_y;

      x = HIGHPASS (shell_x, BOUNDS (s_width - shell_width, border_x, s_width));
      y = HIGHPASS (shell_y, BOUNDS (s_height - shell_height, border_y, s_height));

      if( (x != shell_x || y != shell_y) &&
          0 < x && x < disp_width && 0 < y && y < disp_height )
	gdk_window_move (gdisp->shell->window, x, y);

      /*  Set the new disp_width and disp_height values  */
      gdisp->disp_width = width;
      gdisp->disp_height = height;
    }
  /*  If the projected width is greater than current, but less than
   *  3/4 of the screen size, expand automagically
   */
  else if ((width > disp_width || height > disp_height) &&
	   (disp_width < max_auto_width || disp_height < max_auto_height))
    {
      max_auto_width = MINIMUM (max_auto_width, width);
      max_auto_height = MINIMUM (max_auto_height, height);
      
      gtk_widget_set_usize (gdisp->canvas,
			    max_auto_width, max_auto_height);

      gtk_widget_show (gdisp->canvas);

      gdk_window_get_origin (gdisp->shell->window, &shell_x, &shell_y);

      shell_width = width + border_x;
      shell_height = height + border_y;

      x = HIGHPASS (shell_x, BOUNDS (s_width - shell_width, border_x, s_width));
      y = HIGHPASS (shell_y, BOUNDS (s_height - shell_height, border_y, s_height));

      if( (x != shell_x || y != shell_y) &&
          0 < x && x < disp_width && 0 < y && y < disp_height )
	gdk_window_move (gdisp->shell->window, x, y);

      /*  Set the new disp_width and disp_height values  */
      gdisp->disp_width = max_auto_width;
      gdisp->disp_height = max_auto_height;
    }
  /*  Otherwise, reexpose by hand to reflect changes  */
  else
    gdisplay_expose_full (gdisp);

  /*  If the width or height of the display has changed, recalculate
   *  the display offsets...
   */
  if (disp_width != gdisp->disp_width ||
      disp_height != gdisp->disp_height)
    {
      gdisp->offset_x += (disp_width - gdisp->disp_width) / 2;
      gdisp->offset_y += (disp_height - gdisp->disp_height) / 2;
      bounds_checking (gdisp);
    }
}


int
gdisplay_resize_image (GDisplay *gdisp)
{
  int sx, sy;
  int width, height;

  /*  Calculate the width and height of the new canvas  */
  sx = SCALE (gdisp, gdisp->gimage->width);
  sy = SCALE (gdisp, gdisp->gimage->height);
  width = HIGHPASS (sx, gdisp->disp_width);
  height = HIGHPASS (sy, gdisp->disp_height);

  /* if the new dimensions of the ximage are different than the old...resize */
  if (width != gdisp->disp_width || height != gdisp->disp_height)
    {
      /*  adjust the gdisplay offsets -- we need to set them so that the
       *  center of our viewport is at the center of the image.
       */
      gdisp->offset_x = (sx / 2) - (width / 2);
      gdisp->offset_y = (sy / 2) - (height / 2);

      gdisp->disp_width = width;
      gdisp->disp_height = height;

      if (GTK_WIDGET_VISIBLE (gdisp->canvas))
	gtk_widget_hide (gdisp->canvas);

      gtk_widget_set_usize (gdisp->canvas,
			    gdisp->disp_width,
			    gdisp->disp_height);

      gtk_widget_show (gdisp->canvas);
    }

  return 1;
}


/********************************************************
 *   Routines to query before closing a dirty image     *
 ********************************************************/

static void
gdisplay_close_warning_callback (GtkWidget *w,
				 gpointer   client_data)
{
  GDisplay *gdisp;
  GtkWidget *mbox;

  menus_set_sensitive ("<Image>/File/Close", TRUE);
  mbox = (GtkWidget *) client_data;
  gdisp = (GDisplay *) gtk_object_get_user_data (GTK_OBJECT (mbox));

  /* If POPUP_SHELL references this shell, then reset it. */
  if (popup_shell == gdisp->shell)
    popup_shell = NULL;
  
  gtk_widget_destroy (gdisp->shell);
  gtk_widget_destroy (mbox);
}


static void
gdisplay_cancel_warning_callback (GtkWidget *w,
				  gpointer   client_data)
{
  GtkWidget *mbox;

  menus_set_sensitive ("<Image>/File/Close", TRUE);
  mbox = (GtkWidget *) client_data;
  gtk_widget_destroy (mbox);
}

static gint
gdisplay_delete_warning_callback (GtkWidget *widget,
				  GdkEvent  *event,
				  gpointer  client_data)
{
  menus_set_sensitive ("<Image>/File/Close", TRUE);

  return FALSE;
}

static void
gdisplay_destroy_warning_callback (GtkWidget *widget,
				  gpointer client_data)
{
  warning_dialog = NULL;
}

static void
gdisplay_close_warning_dialog (char     *image_name,
			       GDisplay *gdisp)
{
  static ActionAreaItem mbox_action_items[2] =
  {
    { "Close", gdisplay_close_warning_callback, NULL, NULL },
    { "Cancel", gdisplay_cancel_warning_callback, NULL, NULL }
  };
  GtkWidget *mbox;
  GtkWidget *vbox;
  GtkWidget *label;
  char *warning_buf, *tmp;

  { int i;
    for(i = 0; i < 2; ++i)
      mbox_action_items[i].label = _(mbox_action_items[i].label);
  }
  /* FIXUP this will raise any prexsisting close dialogs, which can be a
     a bit confusing if you tried to close a new window because you had
     forgotten the old dialog was still around */
  /* If a warning dialog already exists raise the window and get out */
  if (warning_dialog != NULL)
    {
      gdk_window_raise (warning_dialog->window);
      return;
    }

  menus_set_sensitive ("<Image>/File/Close", FALSE);

  warning_dialog = mbox = gtk_dialog_new ();
  /* should this be image_window or the actual image name??? */
  gtk_window_set_wmclass (GTK_WINDOW (mbox), "really_close", PROGRAM_NAME);
  gtk_window_set_title (GTK_WINDOW (mbox), image_name);
  gtk_widget_set_uposition(mbox, generic_window_x, generic_window_y);
  layout_connect_window_position(mbox, &generic_window_x, &generic_window_y, FALSE);
  minimize_register(mbox);
  gtk_object_set_user_data (GTK_OBJECT (mbox), gdisp);

  gtk_signal_connect (GTK_OBJECT (mbox), "delete_event",
		      GTK_SIGNAL_FUNC (gdisplay_delete_warning_callback),
		      mbox);

  gtk_signal_connect (GTK_OBJECT (mbox), "destroy",
		      GTK_SIGNAL_FUNC (gdisplay_destroy_warning_callback),
		      mbox);

  vbox = gtk_vbox_new (FALSE, 1);
  gtk_container_border_width (GTK_CONTAINER (vbox), 1);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (mbox)->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  tmp = _("Changes made to %s.  Close anyway?");
  warning_buf = (char *) g_malloc (strlen (image_name) + strlen (tmp) + 1);
  sprintf (warning_buf, tmp, image_name);
  label = gtk_label_new (warning_buf);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);
  gtk_widget_show (label);
  g_free (warning_buf);

  mbox_action_items[0].user_data = mbox;
  mbox_action_items[1].user_data = mbox;
  build_action_area (GTK_DIALOG (mbox), mbox_action_items, 2, 0);

  gtk_widget_show (mbox);
}

#define OY_DBG_FORMAT_ "%s:%d %s() "
#define OY_DBG_ARGS_ __FILE__,__LINE__,strrchr(__func__,'/')?strrchr(__func__,'/')+1:__func__

void gdisplay_set_colour_region(GDisplay * gdisp)
{
#ifdef GDK_WINDOWING_X11
  gint       i,j;

  if(!gdk_window_is_visible(gdisp->canvas->window))
    return;

  if( gdisp->old_disp_geometry[0] != gdisp->disp_xoffset ||
      gdisp->old_disp_geometry[1] != gdisp->disp_yoffset ||
      gdisp->old_disp_geometry[2] != gdisp->disp_width ||
      gdisp->old_disp_geometry[3] != gdisp->disp_height )
  {
    GdkDisplay *display = gdk_display_get_default ();
    GdkWindow * event_box = gtk_widget_get_window(gdisp->canvas);
    GdkWindow * top_window = gdk_window_get_toplevel(event_box);
    Window w = GDK_WINDOW_XID(top_window);
    Display    *xdisplay;
    GdkScreen  *screen;
    int offx = 0, offy = 0, offx2 = 0, offy2 = 0;
    gdk_window_get_origin( event_box, &offx, &offy );
    gdk_window_get_origin( top_window, &offx2, &offy2 );
    xdisplay = gdk_x11_display_get_xdisplay (display);
    screen  = gdk_screen_get_default ();

    {
      XRectangle rec[2] = { { 0,0,0,0 }, { 0,0,0,0 } },
               * rect = 0;
      int nRect = 0;
      XserverRegion reg = 0;
      XcolorRegion region, *old_regions = 0;
      unsigned long old_regions_n = 0;
      int pos = -1;
      const char * display_string = DisplayString(xdisplay);
      int dim_corr_x, dim_corr_y,
          inner_dis_x, inner_dist_y;
      int error;
      Atom netColorTarget;

      inner_dis_x = offx - offx2;
      inner_dist_y = offy - offy2;
      rec[0].x = gdisp->disp_xoffset + inner_dis_x;
      rec[0].y = gdisp->disp_yoffset + inner_dist_y;
      dim_corr_x = 2 * rec[0].x - 2 * inner_dis_x;
      dim_corr_y = 2 * rec[0].y - 2 * inner_dist_y;
      rec[0].width = gdisp->disp_width - dim_corr_x;
      rec[0].height = gdisp->disp_height - dim_corr_y;

      reg = XFixesCreateRegion( xdisplay, rec, 1);
      rect = XFixesFetchRegion( xdisplay, reg, &nRect );
      if(!nRect)
      {
        printf( OY_DBG_FORMAT_
                 "Display: %s Window id: %d  Could not load Xregion:%d\n",
                 OY_DBG_ARGS_,
                 display_string, (int)w, (int)reg );

      } else if(rect[0].x != rec[0].x ||
                rect[0].y != rec[0].y )
      {
        printf( OY_DBG_FORMAT_
                 "Display: %s Window id: %d  Xregion:%d has wrong position %d,%d\n",
                 OY_DBG_ARGS_,
                 display_string, (int)w, (int)reg, rect[0].x, rect[0].y );
      } else
        printf( OY_DBG_FORMAT_
                 "Display: %s Window id: %d  Xregion:%d uploaded %dx%d+%d+%d"
                 "  %d:%d %d:%d\n",
                 OY_DBG_ARGS_,
                 display_string, (int)w, (int)reg,
                 rect[0].width, rect[0].height, rect[0].x, rect[0].y,
                 offx, offx2, offy, offy2 );

      region.region = htonl(reg);
      memset( region.md5, 0, 16 );

      /* look for old regions */
      old_regions = XcolorRegionFetch( xdisplay, w, &old_regions_n );
     /* remove our own old region */
      for(i = 0; i < old_regions_n; ++i)
      {

        if(!old_regions[i].region || pos >= 0)
          break;

        rect = XFixesFetchRegion( xdisplay, ntohl(old_regions[i].region),
                                  &nRect );

        for(j = 0; j < nRect; ++j)
        {
          int * old_window_rectangle = gdisp->old_disp_geometry;

          printf( OY_DBG_FORMAT_
                 "reg[%d]: %dx%d+%d+%d %dx%d+%d+%d\n",
                 OY_DBG_ARGS_, i,
                 old_window_rectangle[2], old_window_rectangle[3],
                 old_window_rectangle[0], old_window_rectangle[1],
                 rect[j].width, rect[j].height, rect[j].x, rect[j].y
                );
          if(old_window_rectangle[0] == rect[j].x &&
             old_window_rectangle[1] == rect[j].y &&
             old_window_rectangle[2] == rect[j].width &&
             old_window_rectangle[3] == rect[j].height )
          {
            pos = i;
            break;
          }
        }
      }
      if(pos >= 0)
      {
        int undeleted_n = old_regions_n;
        XcolorRegionDelete( xdisplay, w, pos, 1 );
        old_regions = XcolorRegionFetch( xdisplay, w, &old_regions_n );
        if(undeleted_n - old_regions_n != 1)
          printf(  OY_DBG_FORMAT_"removed %d; have still %d\n", OY_DBG_ARGS_,
                   pos, (int)old_regions_n );
      }

      /* upload the new or changed region to the X server */
      error = XcolorRegionInsert( xdisplay, w, 0, &region, 1 );
      if(error)
        printf( OY_DBG_FORMAT_
                 "XcolorRegionInsert failed\n",
                 OY_DBG_ARGS_ );
      netColorTarget = XInternAtom( xdisplay, "_ICC_COLOR_TARGET", True );
      if(!netColorTarget)
      {
        printf( OY_DBG_FORMAT_
                 "XInternAtom(..\"_ICC_COLOR_TARGET\"..) failed\n",
                 OY_DBG_ARGS_ );
        error = 1;
      }
      if(!error)
      XChangeProperty( xdisplay, w, netColorTarget, XA_STRING, 8,
                       PropModeReplace,
                       (unsigned char*) display_string, strlen(display_string));

      XFlush(xdisplay);

      /* remember the old rectangle */
        gdisp->old_disp_geometry[0] = gdisp->disp_xoffset + offx - offx2;
        gdisp->old_disp_geometry[1] = gdisp->disp_yoffset + offy - offy2;
        gdisp->old_disp_geometry[2] = gdisp->disp_width - dim_corr_x;
        gdisp->old_disp_geometry[3] = gdisp->disp_height - dim_corr_y;
    }
  }
#endif
}

