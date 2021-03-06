/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-2000 Peter Mattis and Spencer Kimball
 *
 * gimpplugin_pdb.c
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* NOTE: This file is autogenerated by pdbgen.pl */

#include "plugin_main.h"

/**
 * gimp_progress_init:
 * @message: Message to use in the progress dialog.

 *
 * Initializes the progress bar for the current plug-in.
 *
 * Initializes the progress bar for the current plug-in. It is only
 * valid to call this procedure from a plug-in.
 *
 * Returns: TRUE on success.
 */
gboolean
gimp_progress_init (gchar *message)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean success = TRUE;

  return_vals = gimp_run_procedure ("gimp_progress_init",
				    &nreturn_vals,
				    GIMP_PDB_STRING, message,
#if 0
				    GIMP_PDB_INT32, gimp_default_display (),
#endif
				    GIMP_PDB_END);

  success = return_vals[0].data.d_status == STATUS_SUCCESS;

  gimp_destroy_params (return_vals, nreturn_vals);

  return success;
}

/**
 * gimp_progress_update:
 * @percentage: Percentage of progress completed.
 *
 * Updates the progress bar for the current plug-in.
 *
 * Updates the progress bar for the current plug-in. It is only valid
 * to call this procedure from a plug-in.
 *
 * Returns: TRUE on success.
 */
gboolean
gimp_progress_update (gdouble percentage)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean success = TRUE;

  return_vals = gimp_run_procedure ("gimp_progress_update",
				    &nreturn_vals,
				    GIMP_PDB_FLOAT, percentage,
				    GIMP_PDB_END);

  success = return_vals[0].data.d_status == STATUS_SUCCESS;

  gimp_destroy_params (return_vals, nreturn_vals);

  return success;
}

/**
 * gimp_temp_PDB_name:
 *
 * Generates a unique temporary PDB name.
 *
 * This procedure generates a temporary PDB entry name that is
 * guaranteed to be unique. It is many used by the interactive popup
 * dialogs to generate a PDB entry name.
 *
 * Returns: A unique temporary name for a temporary PDB entry.
 */
gchar *
gimp_temp_PDB_name (void)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gchar *temp_name = NULL;

  return_vals = gimp_run_procedure ("gimp_temp_PDB_name",
				    &nreturn_vals,
				    GIMP_PDB_END);

  if (return_vals[0].data.d_status == STATUS_SUCCESS)
    temp_name = g_strdup (return_vals[1].data.d_string);

  gimp_destroy_params (return_vals, nreturn_vals);

  return temp_name;
}

/**
 * gimp_plugin_domain_register:
 * @domain_name: The name of the textdomain (must be unique).
 * @domain_path: The absolute path to the compiled message catalog (may be NULL).
 *
 * Registers a textdomain for localisation.
 *
 * This procedure adds a textdomain to the list of domains Gimp
 * searches for strings when translating its menu entries. There is no
 * need to call this function for plug-ins that have their strings
 * included in the gimp-std-plugins domain as that is used by default.
 * If the compiled message catalog is not in the standard location, you
 * may specify an absolute path to another location. This procedure can
 * only be called in the query function of a plug-in and it has to be
 * called before any procedure is installed.
 *
 * Returns: TRUE on success.
 */
gboolean
gimp_plugin_domain_register (gchar *domain_name,
			     gchar *domain_path)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean success = TRUE;

  return_vals = gimp_run_procedure ("gimp_plugin_domain_register",
				    &nreturn_vals,
				    GIMP_PDB_STRING, domain_name,
				    GIMP_PDB_STRING, domain_path,
				    GIMP_PDB_END);

  success = return_vals[0].data.d_status == STATUS_SUCCESS;

  gimp_destroy_params (return_vals, nreturn_vals);

  return success;
}

/**
 * gimp_plugin_help_register:
 * @help_path: The rootdir of the plug-in's help pages.
 *
 * Register a help path for a plug-in.
 *
 * This procedure changes the help rootdir for the plug-in which calls
 * it. All subsequent calls of gimp_help from this plug-in will be
 * interpreted relative to this rootdir. This procedure can only be
 * called in the query function of a plug-in and it has to be called
 * before any procedure is installed.
 *
 * Returns: TRUE on success.
 */
gboolean
gimp_plugin_help_register (gchar *help_path)
{
  GimpParam *return_vals;
  gint nreturn_vals;
  gboolean success = TRUE;

  return_vals = gimp_run_procedure ("gimp_plugin_help_register",
				    &nreturn_vals,
				    GIMP_PDB_STRING, help_path,
				    GIMP_PDB_END);

  success = return_vals[0].data.d_status == STATUS_SUCCESS;

  gimp_destroy_params (return_vals, nreturn_vals);

  return success;
}
