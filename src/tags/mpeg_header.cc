/* EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
 * Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
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

#if defined ENABLE_MP3 && defined ENABLE_ID3LIB

#include <glib/gi18n.h>
#include <errno.h>

#include "id3_tag.h"
#include "mpeg_header.h"
#include "misc.h"

#include <memory>

#include <id3/tag.h>


/****************
 * Declarations *
 ****************/
static const gchar *layer_names[3] =
{
    "I",    /* Layer 1 */
    "II",   /* Layer 2 */
    "III"   /* Layer 3 */
};

static const gchar *
channel_mode_name (int mode)
{
    static const gchar * const channel_mode[] =
    {
        N_("Stereo"),
        N_("Joint stereo"),
        N_("Dual channel"),
        N_("Single channel")
    };

    if (mode < 0 || mode > 3)
    {
        return "";
    }

    return _(channel_mode[mode]);
}

/*
 * Read infos into header of first frame
 */
gboolean
et_mpeg_header_read_file_info (GFile *file,
                               ET_File_Info *ETFileInfo,
                               GError **error)
{
    /*
     * With id3lib, the header frame couldn't be read if the file contains an ID3v2 tag with an APIC frame
     */
    const Mp3_Headerinfo* headerInfo = NULL;

    g_return_val_if_fail (file != NULL || ETFileInfo != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    /* Check if the file is corrupt. */
    if (!et_id3tag_check_if_file_is_valid (file, error))
    {
        return FALSE;
    }

    /* Get size of file */
    auto info = make_unique(g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, error), g_object_unref);
    if (!info)
        return FALSE;

    ETFileInfo->size = g_file_info_get_size(info.get());

  try
  {
    /* Get data from tag */
    ID3_Tag id3_tag;

    /* Link the file to the tag (uses ID3TT_ID3V2 to get header if APIC is present in Tag) */
    gString filename(g_file_get_path(file));
#ifdef G_OS_WIN32
    /* On Windows, id3lib expects filenames to be in the system codepage. */
    {
        gString locale_filename(g_win32_locale_filename_from_utf8(filename));

        if (!locale_filename)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s",
                         g_strerror (EINVAL));
            return FALSE;
        }

        id3_tag.Link(locale_filename, ID3TT_ID3V2);
    }
#else
    id3_tag.Link(filename, ID3TT_ID3V2);
#endif

    if ( (headerInfo = id3_tag.GetMp3HeaderInfo()) )
    {
        switch (headerInfo->version)
        {
            case MPEGVERSION_1:
                ETFileInfo->version = 1;
                ETFileInfo->mpeg25 = FALSE;
                break;
            case MPEGVERSION_2:
                ETFileInfo->version = 2;
                ETFileInfo->mpeg25 = FALSE;
                break;
            case MPEGVERSION_2_5:
                ETFileInfo->version = 2;
                ETFileInfo->mpeg25 = TRUE;
                break;
            case MPEGVERSION_FALSE:
            case MPEGVERSION_Reserved:
            default:
                break;
        }

        switch (headerInfo->layer)
        {
            case MPEGLAYER_I:
                ETFileInfo->layer = 1;
                break;
            case MPEGLAYER_II:
                ETFileInfo->layer = 2;
                break;
            case MPEGLAYER_III:
                ETFileInfo->layer = 3;
                break;
            case MPEGLAYER_FALSE:
            case MPEGLAYER_UNDEFINED:
            default:
                break;
        }

        // Samplerate
        ETFileInfo->samplerate = headerInfo->frequency;

        // Mode -> Seems to be detected but incorrect?!
        switch (headerInfo->modeext)
        {
            case MP3CHANNELMODE_STEREO:
                ETFileInfo->mode = 0;
                break;
            case MP3CHANNELMODE_JOINT_STEREO:
                ETFileInfo->mode = 1;
                break;
            case MP3CHANNELMODE_DUAL_CHANNEL:
                ETFileInfo->mode = 2;
                break;
            case MP3CHANNELMODE_SINGLE_CHANNEL:
                ETFileInfo->mode = 3;
                break;
            case MP3CHANNELMODE_FALSE:
            default:
                break;
        }

        // Bitrate
        if (headerInfo->vbr_bitrate <= 0)
        {
            ETFileInfo->variable_bitrate = FALSE;
            ETFileInfo->bitrate = headerInfo->bitrate/1000;
        }else
        {
            ETFileInfo->variable_bitrate = TRUE;
            ETFileInfo->bitrate = headerInfo->vbr_bitrate/1000;
        }

        // Duration
        ETFileInfo->duration = headerInfo->time;
    }
  } catch (...)
  { // The C API of ID3lib always handles exceptions with "on error resume next".
    // Let's improve the situation slightly...
    return FALSE;
  }

    return TRUE;
}

/* For displaying header information in the main window. */
void
et_mpeg_header_display_file_info_to_ui (EtFileHeaderFields *fields, const ET_File *ETFile)
{
    const ET_File_Info *info = &ETFile->ETFileInfo;
    gsize ln_num = G_N_ELEMENTS (layer_names);

    if (ETFile->ETFileDescription->FileType == MP3_FILE)
        fields->description = _("MP3 File");
    else if (ETFile->ETFileDescription->FileType == MP2_FILE)
        fields->description = _("MP2 File");
    else
        g_assert_not_reached ();

    /* MPEG, Layer versions */
    fields->version_label = _("MPEG");

    if (info->mpeg25)
    {
        fields->version = strprintf("2.5, Layer %s",
            info->layer >= 1 && info->layer <= ln_num ? layer_names[info->layer - 1] : "?");
    }
    else
    {
        fields->version = strprintf("%d, Layer %s", info->version,
            info->layer >= 1 && info->layer <= ln_num ? layer_names[info->layer - 1] : "?");
    }

    /* Mode */
    fields->mode_label = _("Mode:");
    fields->mode = _(channel_mode_name (info->mode));
}

#endif /* defined ENABLE_MP3 && defined ENABLE_ID3LIB */
