/* The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 * PNM reading and writing code Copyright (C) 1996 Erik Nygren
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
 * Especially in the case of DICOM medical images, it MAY NOT BE USED
 * AS THE BASIS OF ANY MEDICAL DIAGNOSIS.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: dicom.c,v 1.1 2004/12/04 23:41:52 xiaoleilu Exp $ */

/*
 * The dicom reading and writing code was written from scratch by Dov Grobgeld.
 * (dov@imagic.weizman.ac.il).
 */

/*
 * The dicom plugin was adapted for Cinepaint by Dat Huynh.
 * (dathuynh@fastmail.fm)  
 */

#define _(s) s
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* Why this #define in glib does not work is beyond me*/
#define G_N_ELEMENTS(arr)               (sizeof (arr) / sizeof ((arr)[0]))

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GIMP_VERSION
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#else
#include "lib/plugin_main.h"
#include "lib/ui.h"
#include "lib/intl.h"
#include "lib/dialog.h"
#endif

#ifdef G_OS_WIN32
#include <io.h>
#endif

#ifndef _O_BINARY
#define _O_BINARY 0
#endif


#define GUESS_ENDIAN 1

static void guess_and_set_endian2(guint16 *buf16,
                                  int length);
static void toggle_endian2(guint16 *buf16,
                           int length);
static void add_tag_pointer(GByteArray *group_stream,
			    gint group,
			    gint element,
			    gchar *value_rep,
			    guint8 *data,
			    gint length);
static void add_tag_string(GByteArray *group_stream,
			   gint group,
			   gint element,
			   char *value_rep,
			   gchar *s
			   );
static void add_tag_int(GByteArray *group_stream,
			gint group,
			gint element,
			char *value_rep,
			int value
			);
static int write_group_to_file(FILE *DICOM,
			       gint group,
			       GByteArray *group_stream);

/* Declare local data types
 */

typedef struct _DicomScanner
{
  gint    fd;		      /* The file descriptor of the file being read */
  gchar   cur;		      /* The current character in the input stream */
  gint    eof;		      /* Have we reached end of file? */
  gchar  *inbuf;	      /* Input buffer - initially 0 */
  gint    inbufsize;	      /* Size of input buffer */
  gint    inbufvalidsize;     /* Size of input buffer with valid data */
  gint    inbufpos;           /* Position in input buffer */
} DicomScanner;

typedef struct _DicomInfo
{
  gint       width, height;	/* The size of the image */
  gint       maxval;		/* For 16 and 24 bit image files, the max value
				 * which we need to normalize to */
  gint       samples_per_pixel;  /* Number of image planes (0 for pbm) */
  gint       bpp;
  jmp_buf    jmpbuf;		/* Where to jump to on an error loading */
  /* Routine to use to load the dicom body */
  void    (* loader) (DicomScanner *, struct _DicomInfo *, GimpPixelRgn *);
} DicomInfo;

#define BUFLEN 512		/* The input buffer size for data returned
				 * from the scanner.  Note that lines
				 * aren't allowed to be over 256 characters
				 * by the spec anyways so this shouldn't
				 * be an issue. */

#define SAVE_COMMENT_STRING "# CREATOR: The GIMP's Dicom Filter Version 1.0\n"

/* Declare some local functions.
 */
static void   query      (void);
static void   run        (const gchar      *name,
                          gint              nparams,
                          const GimpParam  *param,
                          gint             *nreturn_vals,
                          GimpParam       **return_vals);
static gint32 load_image (gchar  *filename);
static gint   save_image (const gchar  *filename,
                          gint32        image_ID,
                          gint32        drawable_ID);

static void   dicom_loader             (guint8       *pix_buf,
					DicomInfo    *info,
					GimpPixelRgn  *pixel_rgn);


GimpPlugInInfo PLUG_IN_INFO =
{
  NULL,  /* init_proc  */
  NULL,  /* quit_proc  */
  query, /* query_proc */
  run,   /* run_proc   */
};

MAIN ()

static void
query (void)
{
  static GimpParamDef load_args[] =
  {
    { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
    { GIMP_PDB_STRING, "filename", "The name of the file to load" },
    { GIMP_PDB_STRING, "raw_filename", "The name of the file to load" }
  };
  static GimpParamDef load_return_vals[] =
  {
    { GIMP_PDB_IMAGE, "image", "Output image" }
  };
  static gint nload_args = sizeof (load_args) / sizeof (load_args[0]);
  static gint nload_return_vals = (sizeof (load_return_vals) /
				   sizeof (load_return_vals[0]));

  static GimpParamDef save_args[] =
  {
    { GIMP_PDB_INT32,    "run_mode",     "Interactive, non-interactive" },
    { GIMP_PDB_IMAGE,    "image",        "Input image" },
    { GIMP_PDB_DRAWABLE, "drawable",     "Drawable to save" },
    { GIMP_PDB_STRING,   "filename",     "The name of the file to save the image in" },
    { GIMP_PDB_STRING,   "raw_filename", "The name of the file to save the image in" },
  };
  
  gimp_install_procedure ("file_dicom_save",
                          "save file in the DIOCOM file format",
                          "Save an image in the medical standard DICOM image formats.",
                          "Dov Grobgeld",
                          "Dov Grobgeld <dov@imagic.weizmann.ac.il>",
                          "2003",
                          "<Save>/DICOM",
                          "RGB, GRAY",
                          GIMP_PLUGIN,
                          G_N_ELEMENTS (save_args), 0,
                          save_args, NULL);
  


  gimp_install_procedure ("file_dicom_load",
                          "loads files of the dicom file format",
                          "Load a file in the DICOM standard format",
                          "Dov Grobgeld",
                          "Dov Grobgeld <dov@imagic.weizmann.ac.il>",
                          "2003",
                          "<Load>/DICOM",
			  NULL,
                          GIMP_PLUGIN,
                          nload_args, nload_return_vals,
                          load_args, load_return_vals);

  gimp_register_magic_load_handler ("file_dicom_load",
				    "dcm,DCM,dicom,DICOM",
				    "",
				    "128,string,DICM"
				    );
  gimp_register_save_handler       ("file_dicom_save",
				    "dicom,dcm,DCM,DICOM",
				    "");
}

static void
run (const gchar*     name,
     gint             nparams,
     const GimpParam* param,
     gint*            nreturn_vals,
     GimpParam**      return_vals)
{
  static GimpParam values[2];
/* I fixed this */
  GimpRunModeType  run_mode;
  GimpPDBStatusType   status = GIMP_PDB_SUCCESS;
  gint32        image_ID, drawable_ID;
#ifdef GIMP_VERSION 
  GimpExportReturnType  export = GIMP_EXPORT_CANCEL;
#endif 


  run_mode = param[0].data.d_int32;

  *nreturn_vals = 1;
  *return_vals  = values;
  values[0].type          = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;


  if (strcmp (name, "file_dicom_load") == 0)
    {
      image_ID = load_image (param[1].data.d_string);

      if (image_ID != -1)
	{
	  *nreturn_vals = 2;
	  values[1].type         = GIMP_PDB_IMAGE;
	  values[1].data.d_image = image_ID;
	}
      else
	{
	  status = GIMP_PDB_EXECUTION_ERROR;
	}
    }
  else if (strcmp (name, "file_dicom_save") == 0)
    {
      image_ID = param[1].data.d_int32;
      drawable_ID = param[2].data.d_int32;

      /*  eventually export the image */
      image_ID = param[1].data.d_int32;
      drawable_ID = param[2].data.d_int32;

      /*  eventually export the image */
      switch (run_mode)
	{
	case GIMP_RUN_INTERACTIVE:
	case GIMP_RUN_WITH_LAST_VALS:
	  gimp_ui_init ("dicom", FALSE);
#ifdef GIMP_VERSION 
	  export = gimp_export_image (&image_ID, &drawable_ID, "DICOM", (GIMP_EXPORT_CAN_HANDLE_RGB | GIMP_EXPORT_CAN_HANDLE_GRAY ));
	if (export == GIMP_EXPORT_CANCEL)
	  {
	    values[0].data.d_status = GIMP_PDB_CANCEL;
	    return;
	  }
#endif
	break;
      default:
	break;
      }

      switch (run_mode)
        {
        case GIMP_RUN_INTERACTIVE:
          break;

        case GIMP_RUN_NONINTERACTIVE:
          /*  Make sure all the arguments are there!  */
          if (nparams != 5)
            status = GIMP_PDB_CALLING_ERROR;
          break;

        case GIMP_RUN_WITH_LAST_VALS:
          break;

        default:
          break;
        }

      if (status == GIMP_PDB_SUCCESS)
	{
	  if (! save_image (param[3].data.d_string, image_ID, drawable_ID))
	    status = GIMP_PDB_EXECUTION_ERROR;
	}
#ifdef GIMP_VERSION
	if (export == GIMP_EXPORT_EXPORT)
	  gimp_image_delete (image_ID);
#endif 
    }
  else
    {
      status = GIMP_PDB_CALLING_ERROR;
    }

  values[0].data.d_status = status;
}

static gint32
load_image (gchar *filename)
{
  GimpPixelRgn pixel_rgn;
  gint32 volatile image_ID = -1;
  gint32 layer_ID;
  GimpDrawable *drawable;
  int fd;			/* File descriptor */
  char *temp;
  char buf[BUFLEN];		/* buffer for random things like scanning */
  DicomInfo *dicominfo;
  int height, width, samples_per_pixel, bpp;
  guint8 *pix_buf;
  gboolean do_toggle_endian = FALSE;

  temp = g_strdup_printf (_("Loading %s:"), filename);
  gimp_progress_init (temp);
  g_free (temp);

  /* open the file */
  fd = open (filename, O_RDONLY | _O_BINARY);

  if (fd == -1)
    {
      temp = g_strdup_printf(_("Dicom: Can't open file %s."), filename);
      g_message ("%s", temp);
      g_free(temp);
      return -1;
    }

  /* allocate the necessary structures */
  dicominfo = (DicomInfo *) g_malloc (sizeof (DicomInfo));

  /* Parse the file */
  read(fd, buf, 128); /* skip past buffer */

  /* Check for unsupported formats */
  if (g_strncasecmp(buf, "PAPYRUS", 7) == 0)
    {
      temp = g_strdup_printf(_("Dicom: %s is a PAPYRUS DICOM file which this plug-in does not support yet."), filename);
      g_message ("%s", temp);
      g_free(temp);
      return -1;
    }

  read(fd, buf, 4); /* This should be dicom */
  if (g_strncasecmp(buf,"DICM",4) != 0)
    {
      temp = g_strdup_printf(_("Dicom: %s is not a DICOM file."), filename);
      g_message ("%s", temp);
      g_free(temp);
      return -1;
    }

  while(1)
    {
      guint16 group_word;
      guint16 element_word;
      char value_rep[3];
      guint32 element_length;
      guint32 ctx_ul;
      guint16 ctx_us;
      guint8* value;

      /* groupword = unpack('v', "next two bytes") */
      if (read(fd, &group_word, 2) == 0)
	break;
      group_word = g_ntohs(GUINT16_SWAP_LE_BE(group_word));

      read(fd, &element_word, 2);
      element_word = g_ntohs(GUINT16_SWAP_LE_BE(element_word));

      read(fd, value_rep, 2);
      value_rep[2] = 0;

      /* Implicit encoding */
      if (value_rep[0] < 'A' || value_rep[0] > 'Z'
          || value_rep[1] < 'A' || value_rep[1] > 'Z') {
          /* Look up type from the dictionary. At the time we dont
	     support this option... */
          char element_length_chars[4];

          /* Store the bytes that were read */
          element_length_chars[0] = value_rep[0];
          element_length_chars[1] = value_rep[1];

	  strcpy(value_rep, "??");

          /* For implicit value_values the length is always four bytes,
             so we need to read another two. */
          read(fd, &element_length_chars[2], 2);

          /* Now cast to integer and insert into element_length */
          element_length = g_ntohl(GUINT32_SWAP_LE_BE(*((int*)element_length_chars)));
      }
      /* Binary value reps are OB, OW, SQ or UN */
      else if (strncmp(value_rep, "OB", 2) == 0
	  || strncmp(value_rep, "OW", 2) == 0
	  || strncmp(value_rep, "SQ", 2) == 0
	  || strncmp(value_rep, "UN", 2) == 0)
	{
	  read(fd, &element_length, 2); /* skip two bytes */
	  read(fd, &element_length, 4);
	  element_length = g_ntohl(GUINT32_SWAP_LE_BE(element_length));
	}
      else
	{
	  guint16 el16;
	  read(fd, &el16, 2);
	  element_length = g_ntohs(GUINT16_SWAP_LE_BE(el16));
	}

      /* Read contents. Allocate a bit more to make room for casts to int
       below. */
      value = (guint8*)g_new0(guint8, element_length+4);
      read(fd, value, element_length);

      /* Some special casts that are used below */
      ctx_ul = *(guint32*)value;
      ctx_us = *(guint16*)value;

      /* Recognize some critical tags */
      if (group_word == 0x0028)
	{
	  switch (element_word)
	    {
	    case 0x0002:  /* samples per pixel */
	      samples_per_pixel = ctx_us;
	      break;
	    case 0x0010:  /* rows */
	      height = ctx_us;
	      break;
	    case 0x0011:  /* columns */
	      width = ctx_us;
	      break;
	    case 0x0100:  /* bits_allocated */
	      bpp = ctx_us;
	      break;
	    case 0x0103:  /* pixel representation */
	      do_toggle_endian = ctx_us;
	      break;

	    }
	  
	}

      /* Pixel data */
      if (group_word == 0x7fe0 && element_word == 0x0010)
	{
	  pix_buf = value;
	}
      else
	g_free(value);
    }

  /* debug */
  dicominfo->width = width;
  dicominfo->height = height;
  dicominfo->bpp = bpp;
  dicominfo->samples_per_pixel = samples_per_pixel;
  dicominfo->maxval = -1;   /* External normalization factor - not used
			       yet . */
  
  /* Create a new image of the proper size and associate the filename with it.
   */
  image_ID = gimp_image_new (dicominfo->width, dicominfo->height,
			     (dicominfo->samples_per_pixel >= 3) ? GIMP_RGB : GIMP_GRAY);
  gimp_image_set_filename (image_ID, filename);

  layer_ID = gimp_layer_new (image_ID, _("Background"),
			     dicominfo->width, dicominfo->height,
			     (dicominfo->samples_per_pixel >= 3) ? GIMP_RGB_IMAGE : GIMP_GRAY_IMAGE,
			     100, GIMP_NORMAL_MODE);
  gimp_image_add_layer (image_ID, layer_ID, 0);

  drawable = gimp_drawable_get (layer_ID);
  gimp_pixel_rgn_init (&pixel_rgn, drawable,
		       0, 0, drawable->width, drawable->height, TRUE, FALSE);

#if GUESS_ENDIAN
  guess_and_set_endian2((guint16*)pix_buf, width*height);
#endif
  
  dicom_loader(pix_buf, dicominfo, &pixel_rgn);

  /* free the structures */
  g_free(pix_buf);
  g_free (dicominfo);

  /* close the file */
  close (fd);

  /* Tell the GIMP to display the image.
   */
  gimp_drawable_flush (drawable);

  return image_ID;
}

static void
dicom_loader (guint8        *pix_buffer,
	      DicomInfo     *info,
	      GimpPixelRgn  *pixel_rgn)
{
  unsigned char *data, *d;
  int            row_idx, i;
  int            start, end, scanlines;
  int width = info->width;
  int height = info->height;
  int samples_per_pixel = info->samples_per_pixel;
  guint16 *buf16 = (guint16*)pix_buffer;
  int pix_idx;
  guint16 max;

  if (info->bpp == 16)
    {
      /* Reorder the buffer and look for max */
      max = 0;
      for (pix_idx=0; pix_idx < width * height * samples_per_pixel; pix_idx++)
	{
	  guint pix_gl = g_ntohs(GUINT16_SWAP_LE_BE(buf16[pix_idx]));
	  if (pix_gl > max)
	    max = pix_gl;
	  buf16[pix_idx] = pix_gl;
	}
    }
      
  data = g_malloc (gimp_tile_height () * width * samples_per_pixel);
  
  for (row_idx = 0; row_idx < height; )
    {
      start = row_idx;
      end = row_idx + gimp_tile_height ();
      end = MIN (end, height);
      scanlines = end - start;
      d = data;
      
      for (i = 0; i < scanlines; i++)
	{
	  if (info->bpp == 16)
	    {
	      int col_idx;
	      
	      guint16 *row_start = buf16 + (row_idx + i) * width * samples_per_pixel;
	      for (col_idx = 0; col_idx < width * samples_per_pixel; col_idx++)
		d[col_idx] = (guint8)(255.0*(double)(row_start[col_idx]) / max);
	    }
	  else if (info->bpp == 8)
	    {
	      int col_idx;
	      guint8 *row_start = pix_buffer + (row_idx + i) * width * samples_per_pixel;
	      for (col_idx=0; col_idx < width * samples_per_pixel; col_idx++)
		d[col_idx] = row_start[col_idx];
	    }
	  
	  d += width * samples_per_pixel;
	}

      gimp_progress_update ((double) row_idx / (double) height);
      gimp_pixel_rgn_set_rect (pixel_rgn, data, 0, row_idx, width, scanlines);
      row_idx += scanlines;
    }

  g_free (data);
}


/*======================================================================
//   Guess and set endian. Guesses the endian of a buffer by
//   checking the maximum value of the first and the last byte
//   in the words of the buffer. It assumes that the least
//   significant byte
//   wil have a larger maximum than hte most significant byte.
//----------------------------------------------------------------------
*/
static void guess_and_set_endian2(guint16 *buf16,
                                  int length)
{
  guint16 *p = buf16;
  
  int max_first=-1, max_second=-1;

  while(p<buf16+length)
    {
      if (*(guint8*)p > max_first)
        max_first = *(guint8*)p;
      if (((guint8*)p)[1] > max_second)
        max_second = ((guint8*)p)[1];
      p++;
    }

  if (   ((max_second > max_first) && (G_BYTE_ORDER == G_LITTLE_ENDIAN))
      || ((max_second < max_first) && (G_BYTE_ORDER == G_BIG_ENDIAN)))
    toggle_endian2(buf16, length);
}

/*======================================================================
//  toggle_endian2 toggles the endian for a 16 bit entity.
//----------------------------------------------------------------------
*/
static void toggle_endian2(guint16 *buf16,
                           int length)
{
  guint16 *p = buf16;
  while (p<buf16+length)
    {
      *p = ((*p & 0xff) << 8) | (*p >> 8);
      p++;
    }
}

static gint   save_image (const gchar  *filename,
                          gint32        image_ID,
                          gint32        drawable_ID)
{
  FILE *DICOM;
  GimpImageType drawable_type;
  GimpDrawable *drawable;
  GimpPixelRgn pixel_rgn;
  GByteArray *group_stream;
  int group;
  GDate *date;
  gchar today_string[16];
  gchar *photometric_interp;
  int samples_per_pixel;
  int retval = TRUE;

  drawable_type = gimp_drawable_type (drawable_ID);
  drawable = gimp_drawable_get (drawable_ID);

  /*  Make sure we're not saving an image with an alpha channel  */
  if (gimp_drawable_has_alpha (drawable_ID))
    {
      g_message (_("DICOM save cannot handle images with alpha channels"));
      return FALSE;
    }

  switch (drawable_type)
    {
    case GIMP_GRAY_IMAGE:
      samples_per_pixel = 1;
      photometric_interp = "MONOCHROME2";
      break;
    case GIMP_RGB_IMAGE:
      samples_per_pixel = 3;
      photometric_interp = "RGB";
      break;
    default:
      g_message (_("Cannot operate on unknown image types"));
      return (FALSE);
      break;
    }

  date = g_date_new();
  g_date_set_time (date, time (NULL));
  sprintf(today_string, "%04d%02d%02d", date->year, date->month, date->day);
  g_date_free(date);

  /* Open the output file. */
  DICOM = fopen(filename, "wb");
  
  /* Print dicom header */
  {
    guint8 val = 0;
    int i;
    
    for (i=0; i<0x80; i++)
      fwrite(&val, 1, 1, DICOM);
  } 
  fprintf(DICOM, "DICM");
  
  group_stream = g_byte_array_new();
  
  group = 0x0002;
  add_tag_pointer(group_stream, group, 0x0001, "OB", "\0\1", 2);
  add_tag_string (group_stream, group, 0x0010, "UI", "1.2.840.10008.1.2.1");
  add_tag_string (group_stream, group, 0x0013, "SH", "Gimp Dicom Plugin 1.0");
  write_group_to_file(DICOM, group, group_stream);

  group = 0x0008;
  add_tag_string(group_stream, group, 0x0020, "DA", today_string);
  add_tag_string(group_stream, group, 0x0021, "DA", today_string);
  add_tag_string(group_stream, group, 0x0022, "DA", today_string);
  add_tag_string(group_stream, group, 0x0023, "DA", today_string);
  add_tag_string(group_stream, group, 0x0060, "CS", "MR");
  write_group_to_file(DICOM, group, group_stream);

  group = 0x0010;
  add_tag_string(group_stream, group, 0x0010, "PN", "Gimpy Gimp");
  add_tag_string(group_stream, group, 0x0020, "CS", "314159265");
  add_tag_string(group_stream, group, 0x0030, "DA", today_string);
  add_tag_string(group_stream, group, 0x0040, "CS", "?");
  write_group_to_file(DICOM, group, group_stream);
  
  group = 0x0020;
  add_tag_string(group_stream, group, 0x0013, "IS", "1");
  write_group_to_file(DICOM, group, group_stream);

  group = 0x0028;
  add_tag_int   (group_stream, group, 0x0002, "US", samples_per_pixel);
  add_tag_string(group_stream, group, 0x0004, "CS", photometric_interp);
  if (samples_per_pixel == 3)
    add_tag_int(group_stream, group, 0x0006, "US", 0); /* Planar configuration */
  add_tag_int   (group_stream, group, 0x0010, "US", drawable->height);
  add_tag_int   (group_stream, group, 0x0011, "US", drawable->width);
  add_tag_int   (group_stream, group, 0x0100, "US", 8);
  add_tag_int   (group_stream, group, 0x0101, "US", 8);
  add_tag_int   (group_stream, group, 0x0102, "US", 7);
  add_tag_int   (group_stream, group, 0x0103, "US", 0);
  write_group_to_file(DICOM, group, group_stream);

  /* Pixel data */
  group = 0x7fe0;
  {
    guchar *src;

    src = g_new (guchar, drawable->height * drawable->width * samples_per_pixel);
    
    gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, drawable->width, drawable->height, FALSE, FALSE);
    gimp_pixel_rgn_get_rect (&pixel_rgn, src, 0, 0, drawable->width, drawable->height);
    add_tag_pointer(group_stream, group, 0x0010, "OW", (guint8*)src, drawable->width * drawable->height * samples_per_pixel);
    g_free(src);
  }
  if (write_group_to_file(DICOM, group, group_stream) != 0)
    retval = FALSE;
  
  fclose(DICOM);
  
  g_byte_array_free(group_stream, TRUE);
  gimp_drawable_detach (drawable);
  
  return retval;
}

static void add_tag_pointer(GByteArray *group_stream,
			    gint group,
			    gint element,
			    gchar *value_rep,
			    guint8 *data,
			    gint length)
{
  gboolean is_long =(strstr("OB|OW|SQ|UN", value_rep) != NULL)
    || length > 65535;
  guint16 swapped16;
  guint32 swapped32;
  
  swapped16 = g_ntohs(GUINT16_SWAP_LE_BE(group));
  g_byte_array_append(group_stream, (char*)&swapped16, 2);

  swapped16 = g_ntohs(GUINT16_SWAP_LE_BE(element));
  g_byte_array_append(group_stream, (char*)&swapped16, 2);
      
  g_byte_array_append(group_stream, value_rep, 2);
  if (is_long)
    {
      
      g_byte_array_append(group_stream, "\0\0", 2);
      
      swapped32 = g_ntohl(GUINT32_SWAP_LE_BE(length));
      g_byte_array_append(group_stream, (char*)&swapped32, 4);
    }
  else
    {
      swapped16 = g_ntohs(GUINT16_SWAP_LE_BE(length));
      g_byte_array_append(group_stream, (char*)&swapped16, 2);
    }
  
  g_byte_array_append(group_stream, data, length);
}

static void add_tag_string(GByteArray *group_stream,
			   gint group,
			   gint element,
			   gchar *value_rep,
			   gchar *s
			   )
{
  add_tag_pointer(group_stream, group, element, value_rep, (guint8*)s, strlen(s));
}

static void add_tag_int(GByteArray *group_stream,
			gint group,
			gint element,
			char *value_rep,
			int value
			)
{
  int len;

  if (strcmp(value_rep, "US") == 0)
    len = 2;
  else
    len = 4;
  
  add_tag_pointer(group_stream, group, element, value_rep, (guint8*)&value, len);
}

static int write_group_to_file(FILE *DICOM,
			       gint group,
			       GByteArray *group_stream)
{
  int ret = 0;
  
  guint16 swapped16;
  guint32 swapped32;
  
  /* Add header to the group and output it */
  swapped16 = g_ntohs(GUINT16_SWAP_LE_BE(group));
  fwrite((char*)&swapped16,1,2,DICOM);
  fputc(0, DICOM);
  fputc(0, DICOM);
  fputc('U', DICOM);
  fputc('L', DICOM);
  swapped16 = g_ntohs(GUINT16_SWAP_LE_BE(4));
  fwrite((char*)&swapped16,1,2,DICOM);
  swapped32 = g_ntohl(GUINT32_SWAP_LE_BE(group_stream->len));
  fwrite((char*)&swapped32,1,4,DICOM);
  
  if (fwrite(group_stream->data, 1, group_stream->len, DICOM) != group_stream->len)
    ret = -1;
  g_byte_array_set_size(group_stream, 0);

  return ret;
}
