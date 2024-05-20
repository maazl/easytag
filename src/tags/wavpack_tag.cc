/* EasyTAG - tag editor for audio files
 * Copyright (C) 2014-2015 David King <amigadave@amigadave.com>
 * Copyright (C) 2007 Maarten Maathuis (madman2003@gmail.com)
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
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#ifdef ENABLE_WAVPACK

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>
#include <wavpack/wavpack.h>

#include "picture.h"
#include "charset.h"
#include "misc.h"
#include "wavpack_private.h"
#include "wavpack_tag.h"
#include "file.h"
#include "file_description.h"

#define MAXLEN 1024


// registration
const struct Wavpack_Description : ET_File_Description
{	Wavpack_Description()
	{	Extension = ".wv";
		FileType = _("Wavpack File");
		TagType = _("Wavpack Tag");
		read_file = wavpack_read_file;
		write_file_tag = wavpack_tag_write_file_tag;
		display_file_info_to_ui = et_wavpack_header_display_file_info_to_ui;
	}
}
WV_Description;

/*
 * For the APEv2 tags, the following field names are officially supported and
 * recommended by WavPack (although there are no restrictions on what field names
 * may be used):
 * 
 *     Artist
 *     Title
 *     Album
 *     Track
 *     Year
 *     Genre
 *     Comment
 *     Cuesheet (note: may include replay gain info as remarks)
 *     Replay_Track_Gain
 *     Replay_Track_Peak
 *     Replay_Album_Gain
 *     Replay_Album_Peak
 *     Cover Art (Front)
 *     Cover Art (Back)
 */

/*
 * Read tag data from a Wavpack file.
 */
gboolean wavpack_read_file (GFile *file, ET_File *ETFile, GError **error)
{
    WavpackStreamReader reader = { wavpack_read_bytes, wavpack_get_pos,
                                   wavpack_set_pos_abs, wavpack_set_pos_rel,
                                   wavpack_push_back_byte, wavpack_get_length,
                                   wavpack_can_seek, NULL /* No writing. */ };
    EtWavpackState state;
    WavpackContext *wpc;
    gchar message[80];
    gchar field[MAXLEN] = { 0, };
    const int open_flags = OPEN_TAGS;

    g_return_val_if_fail (file != NULL && ETFile != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    state.error = NULL;
    state.istream = g_file_read (file, NULL, &state.error);

    if (!state.istream)
    {
        g_propagate_error (error, state.error);
        return FALSE;
    }

    state.seekable = G_SEEKABLE (state.istream);

    /* NULL for the WavPack correction file. */
    wpc = WavpackOpenFileInputEx (&reader, &state, NULL, message, open_flags,
                                  0);

    if (wpc == NULL)
    {
        if (state.error)
        {
            g_propagate_error (error, state.error);
        }
        else
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                         message);
        }

        g_object_unref (state.istream);
        return FALSE;
    }

    ET_File_Info* ETFileInfo = &ETFile->ETFileInfo;
    ETFileInfo->version     = WavpackGetVersion(wpc);
    /* .wvc correction file not counted */
    ETFileInfo->bitrate     = WavpackGetAverageBitrate(wpc, 0);
    ETFileInfo->samplerate  = WavpackGetSampleRate(wpc);
    ETFileInfo->mode        = WavpackGetNumChannels(wpc);
    ETFileInfo->layer       = WavpackGetChannelMask (wpc);
    ETFileInfo->duration    = (double)WavpackGetNumSamples(wpc) / ETFileInfo->samplerate;

    File_Tag* FileTag = ETFile->FileTag->data;

    auto set_field = [wpc, &field](const char* tag, gchar*& target)
    {   int length = WavpackGetTagItem(wpc, tag, field, MAXLEN);
        if (length > 0 && target == NULL)
            target = Try_To_Validate_Utf8_String(field);
    };

    set_field("title", FileTag->title);
    set_field("version", FileTag->version);
    set_field("subtitle", FileTag->subtitle);
    set_field("artist", FileTag->artist);

    set_field("album artist", FileTag->album_artist);
    set_field("album", FileTag->album);
    set_field("discsubtitle", FileTag->disc_subtitle);

    /*
     * Discnumber + Disctotal.
     */
    if (FileTag->disc_number == NULL)
    {   gchar* value = nullptr;
        set_field("part", value);
        FileTag->disc_and_total(value);
        g_free(value);
    }

    set_field("year", FileTag->year);
    set_field("release year", FileTag->release_year);

    /*
     * Tracknumber + tracktotal
     */
    if (FileTag->track == NULL)
    {   gchar* value = nullptr;
        set_field("track", value);
        FileTag->track_and_total(value);
        g_free(value);
    }

    set_field("genre", FileTag->genre);
    set_field("comment", FileTag->comment);
    set_field("description", FileTag->description);
    set_field("composer", FileTag->composer);
    set_field("original artist", FileTag->orig_artist);
    set_field("original year", FileTag->orig_year);

    set_field("copyright", FileTag->copyright);
    set_field("copyright url", FileTag->url);
    set_field("encoded by", FileTag->encoded_by);

    auto set_float = [wpc, &field, FileTag](const char*tag, void (File_Tag::*func)(const char*))
    {   if (WavpackGetTagItem(wpc, tag, field, MAXLEN) > 0)
            (FileTag->*func)(field);
    };

    set_float("replaygain_track_gain", &File_Tag::track_gain_str);
    set_float("replaygain_track_peak", &File_Tag::track_peak_str);
    set_float("replaygain_album_gain", &File_Tag::album_gain_str);
    set_float("replaygain_album_peak", &File_Tag::album_peak_str);

    WavpackCloseFile(wpc);

    g_object_unref (state.istream);

    return TRUE;
}

/*
 * et_wavpack_append_or_delete_tag_item:
 * @wpc: the #WavpackContext of which to modify tags
 * @tag: the tag item name
 * @value: the tag value to write, or %NULL to delete
 *
 * Appends @value to the @tag item of @wpc, or removes the tag item if @value
 * is %NULL.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
static gboolean
et_wavpack_append_or_delete_tag_item (WavpackContext *wpc,
                                      const gchar *tag,
                                      const gchar *value)
{
    if (!et_str_empty(value))
    {
        return WavpackAppendTagItem (wpc, tag, value, strlen (value));
    }
    else
    {
        WavpackDeleteTagItem (wpc, tag);

        /* It is not an error if there was no tag item to delete. */
        return TRUE;
    }
}

gboolean
wavpack_tag_write_file_tag (const ET_File *ETFile,
                            GError **error)
{
    WavpackStreamReader writer = { wavpack_read_bytes, wavpack_get_pos,
                                   wavpack_set_pos_abs, wavpack_set_pos_rel,
                                   wavpack_push_back_byte, wavpack_get_length,
                                   wavpack_can_seek, wavpack_write_bytes };
    GFile *file;
    EtWavpackWriteState state;
    const File_Tag *FileTag;
    WavpackContext *wpc;
    gchar message[80];

    g_return_val_if_fail (ETFile != NULL && ETFile->FileTag != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    FileTag = (File_Tag *)ETFile->FileTag->data;

    file = g_file_new_for_path(ETFile->FileNameCur->data->value());
    state.error = NULL;
    state.iostream = g_file_open_readwrite (file, NULL, &state.error);
    g_object_unref (file);

    if (!state.iostream)
    {
        g_propagate_error (error, state.error);
        return FALSE;
    }

    state.istream = G_FILE_INPUT_STREAM (g_io_stream_get_input_stream (G_IO_STREAM (state.iostream)));
    state.ostream = G_FILE_OUTPUT_STREAM (g_io_stream_get_output_stream (G_IO_STREAM (state.iostream)));
    state.seekable = G_SEEKABLE (state.iostream);

    /* NULL for the WavPack correction file. */
    wpc = WavpackOpenFileInputEx (&writer, &state, NULL, message,
                                  OPEN_EDIT_TAGS, 0);

    if (wpc == NULL)
    {
        if (state.error)
        {
            g_propagate_error (error, state.error);
        }
        else
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                         message);
        }

        g_object_unref (state.iostream);
        return FALSE;
    }

    if (!et_wavpack_append_or_delete_tag_item (wpc, "title", FileTag->title))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "version", FileTag->version))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "subtitle", FileTag->subtitle))
        goto err;

    if (!et_wavpack_append_or_delete_tag_item (wpc, "artist", FileTag->artist))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "album artist", FileTag->album_artist))
        goto err;

    if (!et_wavpack_append_or_delete_tag_item (wpc, "album", FileTag->album))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "discsubtitle", FileTag->disc_subtitle))
        goto err;

    if (!et_wavpack_append_or_delete_tag_item (wpc, "part", FileTag->disc_and_total().c_str()))
        goto err;

    if (!et_wavpack_append_or_delete_tag_item (wpc, "year", FileTag->year))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "release year", FileTag->release_year))
        goto err;

    if (!et_wavpack_append_or_delete_tag_item (wpc, "track", FileTag->track_and_total().c_str()))
        goto err;

    if (!et_wavpack_append_or_delete_tag_item (wpc, "genre", FileTag->genre))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "comment", FileTag->comment))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "description", FileTag->description))
        goto err;

    if (!et_wavpack_append_or_delete_tag_item (wpc, "composer", FileTag->composer))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "original artist", FileTag->orig_artist))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "original year", FileTag->orig_year))
        goto err;

    if (!et_wavpack_append_or_delete_tag_item (wpc, "copyright", FileTag->copyright))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "copyright url", FileTag->url))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "encoded by", FileTag->encoded_by))
        goto err;

    if (!et_wavpack_append_or_delete_tag_item (wpc, "replaygain_track_gain", FileTag->track_gain_str().c_str()))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "replaygain_track_peak", FileTag->track_peak_str().c_str()))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "replaygain_album_gain", FileTag->album_gain_str().c_str()))
        goto err;
    if (!et_wavpack_append_or_delete_tag_item (wpc, "replaygain_album_peak", FileTag->album_peak_str().c_str()))
        goto err;

    if (WavpackWriteTag (wpc) == 0)
        goto err;

    WavpackCloseFile (wpc);

    g_object_unref (state.iostream);

    // validate date fields
    ETFile->check_dates(3, true); // From field 3 arbitrary strings are allowed

    return TRUE;

err:
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                 WavpackGetErrorMessage (wpc));
    WavpackCloseFile (wpc);
    return FALSE;
}

/*
 * et_wavpack_channel_mask_to_string:
 * @channels: total number of channels
 * @mask: Microsoft channel mask
 *
 * Formats a number of channels and a channel mask into a string, suitable for
 * display to the user.
 *
 * Returns: the formatted channel information
 */
static std::string
et_wavpack_channel_mask_to_string (gint channels,
                                   gint mask)
{
    gboolean lfe;

    /* Low frequency effects channel is bit 3. */
    lfe = mask & (1 << 3);

    if (lfe)
    {
        return strprintf("%d.1", channels - 1);
    }
    else
    {
        return strprintf("%d", channels);
    }
}

void
et_wavpack_header_display_file_info_to_ui (EtFileHeaderFields *fields, const ET_File *ETFile)
{
    const ET_File_Info *info = &ETFile->ETFileInfo;

    /* Encoder version */
    fields->version_label = _("Encoder:");
    fields->version = strprintf("%d", info->version);

    /* Mode */
    fields->mode_label = _("Channels:");
    fields->mode = et_wavpack_channel_mask_to_string(info->mode, info->layer);
}

#endif /* ENABLE_WAVPACK */
