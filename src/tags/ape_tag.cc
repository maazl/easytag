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
#include "libapetag/info_mac.h"
#include "libapetag/info_mpc.h"

#include <cmath>
#include <limits>
using namespace std;

/*************
 * Functions *
 *************/

/*
 * Note:
 *  - if field is found but contains no info (strlen(str)==0), we don't read it
 */
static gboolean
ape_tag_read_file_tag (GFile *file,
                       File_Tag *FileTag,
                       GError **error)
{
    FILE *fp;
    gchar *filename;
    gchar *string = NULL;
    apetag *ape_cnt;

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

    auto fetch_tag = [ape_cnt](const char* fieldname) -> gchar*
    {	const char* s = apefrm_getstr(ape_cnt, fieldname);
    	if (et_str_empty(s))
    		return nullptr;
    	return Try_To_Validate_Utf8_String(s);
    };

    FileTag->title = fetch_tag(APE_TAG_FIELD_TITLE);
    FileTag->subtitle = fetch_tag(APE_TAG_FIELD_SUBTITLE);
    FileTag->version = fetch_tag("Version");
    FileTag->artist = fetch_tag(APE_TAG_FIELD_ARTIST);

    FileTag->album_artist = fetch_tag(APE_TAG_FIELD_ALBUMARTIST);
    FileTag->album = fetch_tag(APE_TAG_FIELD_ALBUM);
    FileTag->disc_subtitle = fetch_tag("DiscSubtitle");

    /* Disc Number and Disc Total */
    string = fetch_tag(APE_TAG_FIELD_PART);
    FileTag->disc_and_total(string);
    g_free (string);

    FileTag->year = fetch_tag(APE_TAG_FIELD_YEAR);
    FileTag->release_year = fetch_tag("Release Year");

    /* Track and Total Track */
    string = fetch_tag(APE_TAG_FIELD_TRACK);
    FileTag->track_and_total(string);
    g_free(string);

    FileTag->genre = fetch_tag(APE_TAG_FIELD_GENRE);
    FileTag->comment = fetch_tag(APE_TAG_FIELD_COMMENT);
    FileTag->description = fetch_tag("Description");

    FileTag->composer = fetch_tag(APE_TAG_FIELD_COMPOSER);
    FileTag->orig_artist = fetch_tag("Original Artist");
    FileTag->orig_year = fetch_tag("Original Year");

    FileTag->copyright = fetch_tag(APE_TAG_FIELD_COPYRIGHT);
    FileTag->url =fetch_tag(APE_TAG_FIELD_RELATED_URL);
    FileTag->encoded_by = fetch_tag("Encoded By");

    auto fetch_float = [ape_cnt](const char* fieldname) -> float
    {	const char* s = apefrm_getstr(ape_cnt, fieldname);
    	if (et_str_empty(s))
    		return numeric_limits<float>::quiet_NaN();
    	return File_Tag::parse_float(s);
    };

    FileTag->track_gain = fetch_float("REPLAYGAIN_TRACK_GAIN");
    FileTag->track_peak = fetch_float("REPLAYGAIN_TRACK_PEAK");
    FileTag->album_gain = fetch_float("REPLAYGAIN_ALBUM_GAIN");
    FileTag->album_peak = fetch_float("REPLAYGAIN_ALBUM_PEAK");

    apetag_free (ape_cnt);
    fclose (fp);

    return TRUE;
}

gboolean mac_read_file(GFile *file, ET_File *ETFile, GError **error)
{
	g_return_val_if_fail (file != NULL && ETFile != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return ape_tag_read_file_tag(file, ETFile->FileTag->data, error)
		&& info_mac_read(file, ETFile, error);
}

gboolean mpc_read_file(GFile *file, ET_File *ETFile, GError **error)
{
	g_return_val_if_fail (file != NULL && ETFile != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return ape_tag_read_file_tag(file, ETFile->FileTag->data, error)
		&& info_mpc_read (file, ETFile, error);
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
    ape_set("Description", FileTag->description);

    ape_set(APE_TAG_FIELD_COMPOSER, FileTag->composer);
    ape_set("Original Artist", FileTag->orig_artist);
    ape_set("Original Year", FileTag->orig_year);

    ape_set(APE_TAG_FIELD_COPYRIGHT, FileTag->copyright);
    ape_set(APE_TAG_FIELD_RELATED_URL, FileTag->url);
    ape_set("Encoded By", FileTag->encoded_by);

    ape_set("REPLAYGAIN_TRACK_GAIN", FileTag->track_gain_str().c_str());
    ape_set("REPLAYGAIN_TRACK_PEAK", FileTag->track_peak_str().c_str());
    ape_set("REPLAYGAIN_ALBUM_GAIN", FileTag->album_gain_str().c_str());
    ape_set("REPLAYGAIN_ALBUM_PEAK", FileTag->album_peak_str().c_str());

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

void
et_mac_header_display_file_info_to_ui (EtFileHeaderFields *fields, const ET_File *ETFile)
{
    const ET_File_Info *info = &ETFile->ETFileInfo;

    fields->description = _("Monkey's Audio File");

    /* Mode changed to profile name  */
    fields->mode_label = _("Profile:");
    if (info->mpc_profile)
        fields->mode = info->mpc_profile;

    /* Version changed to encoder version */
    fields->version_label = _("Encoder:");
    fields->version = strprintf("%i.%i", info->version / 1000, info->version % 1000);
}

void
et_mpc_header_display_file_info_to_ui (EtFileHeaderFields *fields, const ET_File *ETFile)
{
    const ET_File_Info *info = &ETFile->ETFileInfo;

    fields->description = _("MusePack File");

    /* Mode changed to profile name  */
    fields->mode_label = _("Profile:");
    if (info->mpc_profile)
    {   fields->mode_label = _("Profile:");
        fields->mode = strprintf("%s (SV%d)", info->mpc_profile, info->version);
    } else
    {   fields->mode_label = _("Version:");
        strprintf("%d", info->version);
    }

    /* Version changed to encoder version */
    fields->version_label = _("Encoder:");
    if (info->mpc_version)
        fields->version = info->mpc_version;
}
