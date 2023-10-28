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

#include "et_core.h"
#include "picture.h"
#include "charset.h"
#include "misc.h"
#include "wavpack_private.h"
#include "wavpack_tag.h"

#define MAXLEN 1024

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
gboolean
wavpack_tag_read_file_tag (GFile *file,
                           File_Tag *FileTag,
                           GError **error)
{
    WavpackStreamReader reader = { wavpack_read_bytes, wavpack_get_pos,
                                   wavpack_set_pos_abs, wavpack_set_pos_rel,
                                   wavpack_push_back_byte, wavpack_get_length,
                                   wavpack_can_seek, NULL /* No writing. */ };
    EtWavpackState state;
    WavpackContext *wpc;
    gchar message[80];
    gchar field[MAXLEN] = { 0, };
    gchar *field2;
    guint length;
    const int open_flags = OPEN_TAGS;

    g_return_val_if_fail (file != NULL && FileTag != NULL, FALSE);
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

    /*
     * Title
     */
    length = WavpackGetTagItem(wpc, "title", field, MAXLEN);

    if ( length > 0 && FileTag->title == NULL ) {
        FileTag->title = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    length = WavpackGetTagItem(wpc, "version", field, MAXLEN);

    if ( length > 0 && FileTag->version == NULL ) {
        FileTag->version = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    length = WavpackGetTagItem(wpc, "subtitle", field, MAXLEN);

    if ( length > 0 && FileTag->subtitle == NULL ) {
        FileTag->subtitle = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Artist
     */
    length = WavpackGetTagItem(wpc, "artist", field, MAXLEN);

    if ( length > 0 && FileTag->artist == NULL) {
        FileTag->artist = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /* Album artist. */
    length = WavpackGetTagItem (wpc, "album artist", field, MAXLEN);

    if (length > 0 && FileTag->album_artist == NULL)
    {
        FileTag->album_artist = Try_To_Validate_Utf8_String (field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Album
     */
    length = WavpackGetTagItem(wpc, "album", field, MAXLEN);

    if ( length > 0 && FileTag->album == NULL ) {
        FileTag->album = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    length = WavpackGetTagItem(wpc, "discsubtitle", field, MAXLEN);

    if ( length > 0 && FileTag->disc_subtitle == NULL ) {
        FileTag->disc_subtitle = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Discnumber + Disctotal.
     */
    length = WavpackGetTagItem (wpc, "part", field, MAXLEN);
    field2 = strchr (field, '/');

    /* Need to cut off the total tracks if present */
    if (field2)
    {
        *field2 = 0;
        field2++;
    }

    if (field2 && FileTag->disc_total == NULL)
    {
        gchar *tmp;

        tmp = Try_To_Validate_Utf8_String (field2);
        FileTag->disc_total = et_disc_number_to_string (tmp);
        g_free (tmp);
    }

    if (length > 0 && FileTag->disc_number == NULL)
    {
        gchar *tmp;

        tmp = Try_To_Validate_Utf8_String (field);
        FileTag->disc_number = et_disc_number_to_string (tmp);
        g_free (tmp);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Year
     */
    length = WavpackGetTagItem(wpc, "year", field, MAXLEN);

    if ( length > 0 && FileTag->year == NULL ) {
        FileTag->year = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Release year
     */
    length = WavpackGetTagItem(wpc, "release year", field, MAXLEN);

    if ( length > 0 && FileTag->release_year == NULL ) {
        FileTag->release_year = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Tracknumber + tracktotal
     */
    length = WavpackGetTagItem(wpc, "track", field, MAXLEN);
    field2 = strchr (field, '/');

    /* Need to cut off the total tracks if present */
    if (field2) {
        *field2 = 0;
        field2++;
    }

    if (field2 && FileTag->track_total == NULL)
    {
        gchar *tmp;

        tmp = Try_To_Validate_Utf8_String (field2);
        FileTag->track_total = et_track_number_to_string (tmp);
        g_free (tmp);
    }

    if (length > 0 && FileTag->track == NULL)
    {
        gchar *tmp;

        tmp = Try_To_Validate_Utf8_String (field);
        FileTag->track = et_track_number_to_string (tmp);
        g_free (tmp);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Genre
     */
    length = WavpackGetTagItem(wpc, "genre", field, MAXLEN);

    if ( length > 0 && FileTag->genre == NULL ) {
        FileTag->genre = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Comment
     */
    length = WavpackGetTagItem(wpc, "comment", field, MAXLEN);

    if ( length > 0 && FileTag->comment == NULL ) {
        FileTag->comment = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Description
     */
    length = WavpackGetTagItem(wpc, "description", field, MAXLEN);

    if ( length > 0 && FileTag->description == NULL ) {
        FileTag->description = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Composer
     */
    length = WavpackGetTagItem(wpc, "composer", field, MAXLEN);

    if ( length > 0 && FileTag->composer == NULL ) {
        FileTag->composer = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Original artist
     */
    length = WavpackGetTagItem(wpc, "original artist", field, MAXLEN);

    if ( length > 0 && FileTag->orig_artist == NULL ) {
        FileTag->orig_artist  = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Original year
     */
    length = WavpackGetTagItem(wpc, "original year", field, MAXLEN);

    if ( length > 0 && FileTag->orig_year == NULL ) {
        FileTag->orig_year = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Copyright
     */
    length = WavpackGetTagItem(wpc, "copyright", field, MAXLEN);

    if ( length > 0 && FileTag->copyright == NULL ) {
        FileTag->copyright = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * URL
     */
    length = WavpackGetTagItem(wpc, "copyright url", field, MAXLEN);

    if ( length > 0 && FileTag->url == NULL ) {
        FileTag->url = Try_To_Validate_Utf8_String(field);
    }

    memset (field, '\0', MAXLEN);

    /*
     * Encoded by
     */
    length = WavpackGetTagItem(wpc, "encoded by", field, MAXLEN);

    if ( length > 0 && FileTag->encoded_by == NULL ) {
        FileTag->encoded_by = Try_To_Validate_Utf8_String(field);
    }

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
    const gchar *filename;
    const File_Tag *FileTag;
    WavpackContext *wpc;
    gchar message[80];

    g_return_val_if_fail (ETFile != NULL && ETFile->FileTag != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    filename = ((File_Name *)((GList *)ETFile->FileNameCur)->data)->value;
    FileTag = (File_Tag *)ETFile->FileTag->data;

    file = g_file_new_for_path (filename);
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

    if (WavpackWriteTag (wpc) == 0)
        goto err;

    WavpackCloseFile (wpc);

    g_object_unref (state.iostream);

    return TRUE;

err:
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                 WavpackGetErrorMessage (wpc));
    WavpackCloseFile (wpc);
    return FALSE;
}

#endif /* ENABLE_WAVPACK */
