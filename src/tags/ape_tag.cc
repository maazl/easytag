/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2014-2015  David King <amigadave@amigadave.com>
 * Copyright (C) 2001-2003  Jerome Couderc <easytag@gmail.com>
 * Copyright (C) 2002-2003  Artur Polaczyñski <artii@o2.pl>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "ape_tag.h"
#include "et_core.h"
#include "misc.h"
#include "setting.h"
#include "charset.h"
#include "libapetag/apetaglib.h"

/*************
 * Functions *
 *************/

/*
 * set_string_field:
 * @field: (inout): pointer to a location in which to store the field value
 * @string: (transfer none): the string to copy and store in @field
 *
 * Copy @string and store it in @field, first validating that the string is
 * valid UTF-8.
 */
static void
set_string_field (gchar **field,
                  gchar *string)
{
    if (!et_str_empty (string))
    {
        if (g_utf8_validate (string, -1, NULL))
        {
            *field = g_strdup (string);
        }
        else
        {
            /* Unnecessarily validates the field again, but this should not be
             * the common case. */
            *field = Try_To_Validate_Utf8_String (string);
        }
    }
}

/*
 * Note:
 *  - if field is found but contains no info (strlen(str)==0), we don't read it
 */
gboolean
ape_tag_read_file_tag (GFile *file,
                       File_Tag *FileTag,
                       GError **error)
{
    FILE *fp;
    gchar *filename;
    gchar *string = NULL;
    gchar *string1 = NULL;
    apetag *ape_cnt;

    g_return_val_if_fail (file != NULL && FileTag != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    filename = g_file_get_path (file);

    if ((fp = g_fopen (filename, "rb")) == NULL)
    {
        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                     _("Error while opening file: %s"),
                     g_strerror (errno));
        g_free (filename);
        return FALSE;
    }

    ape_cnt = apetag_init();
    apetag_read_fp (ape_cnt, fp, filename, 0); /* read all tags ape,id3v[12]*/

    g_free (filename);

    /* Title */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_TITLE);
    set_string_field (&FileTag->title, string);

    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_SUBTITLE);
    set_string_field (&FileTag->subtitle, string);

    string = apefrm_getstr (ape_cnt, "Version");
    set_string_field (&FileTag->version, string);

    /* Artist */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_ARTIST);
    set_string_field (&FileTag->artist, string);

    /* Album artist. */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_ALBUMARTIST);
    set_string_field (&FileTag->album_artist, string);

    /* Album */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_ALBUM);
    set_string_field (&FileTag->album, string);

    string = apefrm_getstr (ape_cnt, "DiscSubtitle");
    set_string_field (&FileTag->disc_subtitle, string);

    /* Disc Number and Disc Total */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_PART);

    if (string)
    {
        string = Try_To_Validate_Utf8_String (string);

        string1 = strchr (string, '/');

        if (string1)
        {
            FileTag->disc_total = et_disc_number_to_string(string1 + 1);
            *string1 = '\0';
        }

        FileTag->disc_number = et_disc_number_to_string(string);

        g_free (string);
    }
    else
    {
        FileTag->disc_number = FileTag->disc_total = NULL;
    }

    /* Year */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_YEAR);
    set_string_field (&FileTag->year, string);

    /* Release year */
    string = apefrm_getstr (ape_cnt, "Release Year");
    set_string_field (&FileTag->release_year, string);

    /* Track and Total Track */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_TRACK);

    if (string)
    {
        string = Try_To_Validate_Utf8_String(string);

        string1 = strchr (string, '/');

        if (string1)
        {
            FileTag->track_total = et_track_number_to_string(string1 + 1);
            *string1 = '\0';
        }
        FileTag->track = et_track_number_to_string(string);

        g_free(string);
    } else
    {
        FileTag->track = FileTag->track_total = NULL;
    }

    /* Genre */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_GENRE);
    set_string_field (&FileTag->genre, string);

    /* Comment */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_COMMENT);
    set_string_field (&FileTag->comment, string);

    /* Composer */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_COMPOSER);
    set_string_field (&FileTag->composer, string);

    /* Original artist */
    string = apefrm_getstr (ape_cnt, "Original Artist");
    set_string_field (&FileTag->orig_artist, string);

    /* Original year */
    string = apefrm_getstr (ape_cnt, "Original Year");
    set_string_field (&FileTag->orig_year, string);

    /* Copyright */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_COPYRIGHT);
    set_string_field (&FileTag->copyright, string);

    /* URL */
    string = apefrm_getstr (ape_cnt, APE_TAG_FIELD_RELATED_URL);
    set_string_field (&FileTag->url, string);

    /* Encoded by */
    string = apefrm_getstr (ape_cnt, "Encoded By");
    set_string_field (&FileTag->encoded_by, string);

    apetag_free (ape_cnt);
    fclose (fp);

    return TRUE;
}

gboolean
ape_tag_write_file_tag (const ET_File *ETFile,
                        GError **error)
{
    const File_Tag *FileTag;
    const gchar *filename_in;
    apetag   *ape_mem;

    g_return_val_if_fail (ETFile != NULL && ETFile->FileTag != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    FileTag     = (File_Tag *)ETFile->FileTag->data;
    filename_in = ((File_Name *)ETFile->FileNameCur->data)->value;

    ape_mem = apetag_init ();

    /* TODO: Pointless, as g_set_error() will try to malloc. */
    if (!ape_mem)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM, "%s",
                     g_strerror (ENOMEM));
        return FALSE;
    }

    auto ape_set = [ape_mem](const char* fieldname, const char* value)
    {   if (!et_str_empty(value))
            apefrm_add(ape_mem, 0, fieldname, value);
        else
            apefrm_remove(ape_mem, fieldname);
    };

    ape_set(APE_TAG_FIELD_TITLE, FileTag->title);
    ape_set("Version", FileTag->version);
    ape_set(APE_TAG_FIELD_SUBTITLE, FileTag->subtitle);

    ape_set(APE_TAG_FIELD_ARTIST, FileTag->artist);
    ape_set(APE_TAG_FIELD_ALBUMARTIST, FileTag->album_artist);

    ape_set(APE_TAG_FIELD_ALBUM, FileTag->album);
    ape_set("DiscSubtitle", FileTag->disc_subtitle);
    ape_set(APE_TAG_FIELD_PART, FileTag->disc_and_total().c_str());

    ape_set(APE_TAG_FIELD_YEAR, FileTag->year);
    ape_set("Release Year", FileTag->release_year);

    ape_set(APE_TAG_FIELD_TRACK, FileTag->track_and_total().c_str());

    ape_set(APE_TAG_FIELD_GENRE, FileTag->genre);
    ape_set(APE_TAG_FIELD_COMMENT, FileTag->comment);

    ape_set(APE_TAG_FIELD_COMPOSER, FileTag->composer);
    ape_set("Original Artist", FileTag->orig_artist);
    ape_set("Original Year", FileTag->orig_year);

    ape_set(APE_TAG_FIELD_COPYRIGHT, FileTag->copyright);
    ape_set(APE_TAG_FIELD_RELATED_URL, FileTag->url);

    ape_set("Encoded By", FileTag->encoded_by);

    /* reread all tag-type again  excl. changed frames by apefrm_remove() */
    if (apetag_save (filename_in, ape_mem, APE_TAG_V2 + SAVE_NEW_OLD_APE_TAG)
        != 0)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                     _("Failed to write APE tag"));
        apetag_free (ape_mem);
        return FALSE;
    }

    apetag_free(ape_mem);

    return TRUE;
}
