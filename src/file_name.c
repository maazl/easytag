/* EasyTAG - tag editor for audio files
 * Copyright (C) 2015  David King <amigadave@amigadave.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "file_name.h"

#include "charset.h"
#include "misc.h"

#include <string.h>

/*
 * Create a new File_Name structure
 */
File_Name *
et_file_name_new (void)
{
    File_Name *file_name;

    file_name = g_slice_new (File_Name);
    file_name->key = et_undo_key_new ();
    file_name->saved = FALSE;
    file_name->value = NULL;
    file_name->value_utf8 = NULL;
    file_name->rel_value = NULL;
    file_name->rel_value_utf8 = NULL;
    file_name->path_value_ck = NULL;
    file_name->file_value_ck = NULL;

    return file_name;
}

static void
et_file_name_free_value(File_Name *file_name)
{
    g_free (file_name->value);
    if (file_name->value_utf8 != file_name->value)
        g_free (file_name->value_utf8);
    g_free (file_name->path_value_ck);
}

/*
 * Frees a File_Name item.
 */
void
et_file_name_free (File_Name *file_name)
{
    g_return_if_fail (file_name != NULL);
    et_file_name_free_value(file_name);
    g_slice_free (File_Name, file_name);
}

static const gchar*
et_file_name_skip_root(const gchar* value, const gchar* root, const gchar* root_rel)
{	if (!root || !*root)
		return value;
	size_t len = root_rel - root;
	if (len && strncmp(value, root, len) == 0)
	{
		value += len;
		value += G_IS_DIR_SEPARATOR(*value);
	}
	return value;
}

/*
 * Fill content of a FileName item according to the filename passed in argument (UTF-8 filename or not)
 * Calculate also the collate key.
 * It treats numbers intelligently so that "file1" "file10" "file5" is sorted as "file1" "file5" "file10"
 */
void
ET_Set_Filename_File_Name_Item (File_Name *FileName,
                                const File_Name *root,
                                const gchar *filename_utf8,
                                const gchar *filename)
{
    const gchar *root_value, *root_value_utf8, *root_value_ck;

    g_return_if_fail (FileName != NULL);
    et_file_name_free_value(FileName);

    /* be aware of aliasing root == FileName */
    if (root)
    {
        root_value = root->value;
        root_value_utf8 = root->value_utf8;
    }

    if (filename_utf8 && filename)
    {
        FileName->value = g_strdup (filename);
        if (strcmp(filename_utf8, FileName->value) == 0)
            FileName->value_utf8 = g_strdup (filename_utf8);
        else
            FileName->value_utf8 = FileName->value;
    }
    else if (filename_utf8)
    {
        FileName->value_utf8 = g_strdup (filename_utf8);
        FileName->value = filename_from_display (filename_utf8);

        if (strcmp(FileName->value_utf8, FileName->value) == 0)
        {
            g_free(FileName->value_utf8);
            FileName->value_utf8 = FileName->value;
        }
    }
    else if (filename)
    {
        FileName->value_utf8 = g_filename_display_name (filename);
        FileName->value = g_strdup (filename);

        if (strcmp(FileName->value_utf8, FileName->value) == 0)
        {
            g_free(FileName->value_utf8);
            FileName->value_utf8 = FileName->value;
        }
    }

    if (root)
    {
        FileName->rel_value = et_file_name_skip_root(FileName->value, root_value, root->rel_value);
        FileName->rel_value_utf8 = et_file_name_skip_root(FileName->value_utf8, root_value_utf8, root->rel_value_utf8);
    }
    else
    {
        FileName->rel_value = FileName->value + strlen(FileName->value);
        FileName->rel_value_utf8 = FileName->value_utf8 + strlen(FileName->value_utf8);
    }

    gchar* mod = g_strdup(FileName->rel_value_utf8);
    gchar* file_ck = NULL;
    // Replace dir separator by dot for more reasonable sort order
    gchar* cp = mod;
    for (;;)
    {
        gchar* cp2 = strchr(cp, G_DIR_SEPARATOR);
        if (!cp2)
        {
            file_ck = g_utf8_collate_key_for_filename(cp, -1);
            break;
        }
        *cp2 = '.';
        cp = cp2 + 1;
    }
    FileName->file_value_ck = FileName->path_value_ck = g_utf8_collate_key_for_filename(mod, -1);
    if (file_ck)
    {
        FileName->file_value_ck += strlen(FileName->path_value_ck) - strlen(file_ck);
        ((gchar*)FileName->file_value_ck)[-1] = '\0'; // Terminate path before filename
    }
    g_free(file_ck);
    g_free(mod);
}

gboolean
et_file_name_set_from_components (File_Name *file_name,
                                  const File_Name *root,
                                  const gchar *new_name,
                                  const gchar *dir_name,
                                  gboolean replace_illegal)
{
    /* Check if new filename seems to be correct. */
    if (new_name)
    {
        gchar *filename_new;
        gchar *path_new;

        filename_new = g_strdup (new_name);

        /* Convert the illegal characters. */
        et_filename_prepare (filename_new, replace_illegal);

        /* Set the new filename (in file system encoding). */
        path_new = g_build_filename (dir_name, filename_new, NULL);
        ET_Set_Filename_File_Name_Item (file_name, root, NULL, path_new);

        g_free (path_new);
        g_free (filename_new);
        return TRUE;
    }
    else
    {
        et_file_name_free_value(file_name);
        file_name->value = NULL;
        file_name->value_utf8 = NULL;
        file_name->path_value_ck = NULL;

        return FALSE;
    }
}

/*
 * Compares two File_Name items :
 *  - returns TRUE if there aren't the same
 *  - else returns FALSE
 */
gboolean
et_file_name_detect_difference (const File_Name *a,
                                const File_Name *b)
{
    g_return_val_if_fail (a && b, FALSE);

    if (a && !b) return TRUE;
    if (!a && b) return TRUE;

    /* Both a and b are != NULL. */
    if (!a->value && !b->value) return FALSE;
    if (a->value && !b->value) return TRUE;
    if (!a->value && b->value) return TRUE;

    /* Compare collate keys (with FileName->value converted to UTF-8 as it
     * contains raw data). */
    /* Filename changed ? (we check path + file). */
    return strcmp (a->path_value_ck, b->path_value_ck) != 0;
}
