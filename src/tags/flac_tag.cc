/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2014-2015  David King <amigadave@amigadave.com>
 * Copyright (C) 2001-2003  Jerome Couderc <easytag@gmail.com>
 * Copyright (C) 2003       Pavel Minayev <thalion@front.ru>
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

#ifdef ENABLE_FLAC

#include <glib/gi18n.h>
#include <errno.h>

#include "flac_private.h"
#include "flac_tag.h"
#include "ogg_tag.h"
#include "vcedit.h"
#include "et_core.h"
#include "id3_tag.h"
#include "misc.h"
#include "setting.h"
#include "picture.h"
#include "charset.h"

using namespace std;

/*
 * Read tag data from a FLAC file using the level 2 flac interface,
 * Note:
 *  - if field is found but contains no info (strlen(str)==0), we don't read it
 */
gboolean
flac_tag_read_file_tag (GFile *file,
                        File_Tag *FileTag,
                        GError **error)
{
    FLAC__Metadata_Chain *chain;
    EtFlacReadState state;
    FLAC__IOCallbacks callbacks = { et_flac_read_func,
                                    NULL, /* Do not set a write callback. */
                                    et_flac_seek_func, et_flac_tell_func,
                                    et_flac_eof_func,
                                    et_flac_read_close_func };
    FLAC__Metadata_Iterator *iter;

    EtPicture *prev_pic = NULL;

    g_return_val_if_fail (file != NULL && FileTag != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    chain = FLAC__metadata_chain_new ();

    if (chain == NULL)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM, "%s",
                     g_strerror (ENOMEM));
        return FALSE;
    }

    state.error = NULL;
    state.istream = g_file_read (file, NULL, &state.error);
    state.seekable = G_SEEKABLE (state.istream);

    if (!FLAC__metadata_chain_read_with_callbacks (chain, &state, callbacks))
    {
        /* TODO: Provide a dedicated error enum corresponding to status. */
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                     _("Error opening FLAC file"));
        et_flac_read_close_func (&state);

        return FALSE;
    }

    iter = FLAC__metadata_iterator_new ();

    if (iter == NULL)
    {
        et_flac_read_close_func (&state);
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM, "%s",
                     g_strerror (ENOMEM));
        return FALSE;
    }

    FLAC__metadata_iterator_init (iter, chain);

    while (FLAC__metadata_iterator_next (iter))
    {
        FLAC__StreamMetadata *block;

        block = FLAC__metadata_iterator_get_block (iter);

        if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
        {
            tags_hash tags;
            const FLAC__StreamMetadata_VorbisComment* vc = &block->data.vorbis_comment;

            /* Get comments from block. */
            for (int i = 0; i < vc->num_comments; i++)
            {
                const FLAC__StreamMetadata_VorbisComment_Entry comment = vc->comments[i];
                tags.add_tag((const char*)comment.entry, comment.length);
            }

            tags.to_file_tags(FileTag);

            /* Save unsupported fields. */
            tags.to_other_tags(FileTag);
        }
        else if (block->type == FLAC__METADATA_TYPE_PICTURE)
        {
            /* Picture. */
            const FLAC__StreamMetadata_Picture *p;
            GBytes *bytes;
            EtPicture *pic;
        
            /* Get picture data from block. */
            p = &block->data.picture;

            bytes = g_bytes_new (p->data, p->data_length);
        
            pic = et_picture_new ((EtPictureType)p->type,
                                  (const gchar *)p->description, 0, 0, bytes);
            g_bytes_unref (bytes);

            if (!prev_pic)
            {
                FileTag->picture = pic;
            }
            else
            {
                prev_pic->next = pic;
            }

            prev_pic = pic;
        }
    }

    FLAC__metadata_iterator_delete (iter);
    FLAC__metadata_chain_delete (chain);
    et_flac_read_close_func (&state);

#ifdef ENABLE_MP3
    /* If no FLAC vorbis tag found : we try to get the ID3 tag if it exists
     * (but it will be deleted when rewriting the tag) */
    if ( FileTag->title       == NULL
      && FileTag->version     == NULL
      && FileTag->subtitle    == NULL
      && FileTag->artist      == NULL
      && FileTag->album_artist == NULL
      && FileTag->album       == NULL
      && FileTag->disc_subtitle == NULL
      && FileTag->disc_number == NULL
      && FileTag->disc_total  == NULL
      && FileTag->year        == NULL
      && FileTag->release_year == NULL
      && FileTag->track       == NULL
      && FileTag->track_total == NULL
      && FileTag->genre       == NULL
      && FileTag->comment     == NULL
      && FileTag->composer    == NULL
      && FileTag->orig_artist == NULL
      && FileTag->orig_year   == NULL
      && FileTag->copyright   == NULL
      && FileTag->url         == NULL
      && FileTag->encoded_by  == NULL
      && FileTag->picture     == NULL)
    {
        id3tag_read_file_tag (file, FileTag, NULL);

        // If an ID3 tag has been found (and no FLAC tag), we mark the file as
        // unsaved to rewrite a flac tag.
        if ( FileTag->title       != NULL
          || FileTag->version     != NULL
          || FileTag->subtitle    != NULL
          || FileTag->artist      != NULL
          || FileTag->album_artist != NULL
          || FileTag->album       != NULL
          || FileTag->disc_subtitle != NULL
          || FileTag->disc_number != NULL
          || FileTag->disc_total  != NULL
          || FileTag->year        != NULL
          || FileTag->release_year != NULL
          || FileTag->track       != NULL
          || FileTag->track_total != NULL
          || FileTag->genre       != NULL
          || FileTag->comment     != NULL
          || FileTag->composer    != NULL
          || FileTag->orig_artist != NULL
          || FileTag->orig_year   != NULL
          || FileTag->copyright   != NULL
          || FileTag->url         != NULL
          || FileTag->encoded_by  != NULL
          || FileTag->picture     != NULL)
        {
            FileTag->saved = FALSE;
        }
    }
#endif

    return TRUE;
}

/*
 * vc_block_append_other_tag:
 * @vc_block: the Vorbis comment in which to add the tag
 * @tag: the name and value of the tag
 *
 * Append the unsupported (not shown in the UI) @tag to @vc_block.
 */
static void
vc_block_append_other_tag (FLAC__StreamMetadata *vc_block,
                           const gchar *tag)
{
    FLAC__StreamMetadata_VorbisComment_Entry field;

    field.entry = (FLAC__byte *)tag;
    field.length = strlen (tag);

    /* Safe to pass const data, if the last argument (copy) is true, according
     * to the FLAC API reference. */
    if (!FLAC__metadata_object_vorbiscomment_append_comment (vc_block, field,
                                                             true))
    {
        g_critical ("Invalid Vorbis comment, or memory allocation failed, when writing other FLAC tag '%s'",
                    tag);
    }
}

/*
 * vc_block_append_single_tag:
 * @vc_block: the Vorbis comment in which to add the tag
 * @tag_name: the name of the tag
 * @value: the value of the tag
 *
 * Save field value in a single tag.
 */
static void
vc_block_append_single_tag (FLAC__StreamMetadata *vc_block,
                            const gchar *tag_name,
                            const gchar *value)
{
    FLAC__StreamMetadata_VorbisComment_Entry field;

    if (!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair (&field,
                                                                         tag_name,
                                                                         value))
    {
        g_critical ("Invalid Vorbis comment, or memory allocation failed, when creating FLAC entry from tag name '%s' and value '%s'",
                    tag_name, value);
        return;
    }

    if (!FLAC__metadata_object_vorbiscomment_append_comment (vc_block, field,
                                                             false))
    {
        g_critical ("Invalid Vorbis comment, or memory allocation failed, when writing FLAC tag '%s' with value '%s'",
                    tag_name, value);
    }
}

/*
 * Write Flac tag, using the level 2 flac interface
 */
gboolean
flac_tag_write_file_tag (const ET_File *ETFile,
                         GError **error)
{
    const File_Tag *FileTag;
    GFile *file;
    GFileIOStream *iostream;
    EtFlacWriteState state;
    FLAC__IOCallbacks callbacks = { et_flac_read_func, et_flac_write_func,
                                    et_flac_seek_func, et_flac_tell_func,
                                    et_flac_eof_func,
                                    et_flac_write_close_func };
    const gchar *filename;
    const gchar *filename_utf8;
    const gchar *flac_error_msg;
    FLAC__Metadata_Chain *chain;
    FLAC__Metadata_Iterator *iter;
    FLAC__StreamMetadata_VorbisComment_Entry vce_field_vendor_string; // To save vendor string
    gboolean vce_field_vendor_string_found = FALSE;

    g_return_val_if_fail (ETFile != NULL && ETFile->FileTag != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    FileTag       = (File_Tag *)ETFile->FileTag->data;
    filename      = ((File_Name *)ETFile->FileNameCur->data)->value;
    filename_utf8 = ((File_Name *)ETFile->FileNameCur->data)->value_utf8;

    /* libFLAC is able to detect (and skip) ID3v2 tags by itself */
    
    /* Create a new chain instance to get all blocks in one time. */
    chain = FLAC__metadata_chain_new ();

    if (chain == NULL)
    {
        flac_error_msg = FLAC__Metadata_ChainStatusString[FLAC__METADATA_CHAIN_STATUS_MEMORY_ALLOCATION_ERROR];
        
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Error while opening file ‘%s’ as FLAC: %s"),
                     filename_utf8, flac_error_msg);
        return FALSE;
    }

    file = g_file_new_for_path (filename);

    state.file = file;
    state.error = NULL;
    /* TODO: Fallback to an in-memory copy of the file for non-local files,
     * where creation of the GFileIOStream may fail. */
    iostream = g_file_open_readwrite (file, NULL, &state.error);

    if (iostream == NULL)
    {
        FLAC__metadata_chain_delete (chain);
        g_propagate_error (error, state.error);
        g_object_unref (file);
        return FALSE;
    }

    state.istream = G_FILE_INPUT_STREAM (g_io_stream_get_input_stream (G_IO_STREAM (iostream)));
    state.ostream = G_FILE_OUTPUT_STREAM (g_io_stream_get_output_stream (G_IO_STREAM (iostream)));
    state.seekable = G_SEEKABLE (iostream);
    state.iostream = iostream;

    if (!FLAC__metadata_chain_read_with_callbacks (chain, &state, callbacks))
    {
        const FLAC__Metadata_ChainStatus status = FLAC__metadata_chain_status (chain);
        flac_error_msg = FLAC__Metadata_ChainStatusString[status];
        FLAC__metadata_chain_delete (chain);

        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Error while opening file ‘%s’ as FLAC: %s"),
                     filename_utf8, flac_error_msg);
        et_flac_write_close_func (&state);
        return FALSE;
    }
    
    /* Create a new iterator instance for the chain. */
    iter = FLAC__metadata_iterator_new ();

    if (iter == NULL)
    {
        flac_error_msg = FLAC__Metadata_ChainStatusString[FLAC__METADATA_CHAIN_STATUS_MEMORY_ALLOCATION_ERROR];

        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Error while opening file ‘%s’ as FLAC: %s"),
                     filename_utf8, flac_error_msg);
        FLAC__metadata_chain_delete (chain);
        et_flac_write_close_func (&state);
        return FALSE;
    }
    
    FLAC__metadata_iterator_init (iter, chain);
    
    while (FLAC__metadata_iterator_next (iter))
    {
        const FLAC__MetadataType block_type = FLAC__metadata_iterator_get_block_type (iter);

        /* TODO: Modify the blocks directly, rather than deleting and
         * recreating. */
        if (block_type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
        {
            // Delete the VORBIS_COMMENT block and convert to padding. But before, save the original vendor string.
            /* Get block data. */
            FLAC__StreamMetadata *block = FLAC__metadata_iterator_get_block(iter);
            FLAC__StreamMetadata_VorbisComment *vc = &block->data.vorbis_comment;
            
            if (vc->vendor_string.entry != NULL)
            {
                // Get initial vendor string, to don't alterate it by FLAC__VENDOR_STRING when saving file
                vce_field_vendor_string.entry = (FLAC__byte *)g_strdup ((gchar *)vc->vendor_string.entry);
                vce_field_vendor_string.length = strlen ((gchar *)vce_field_vendor_string.entry);
                vce_field_vendor_string_found = TRUE;
            }
            
            /* Free block data. */
            FLAC__metadata_iterator_delete_block (iter, true);
        }
        else if (block_type == FLAC__METADATA_TYPE_PICTURE)
        {
            /* Delete all the PICTURE blocks, and convert to padding. */
            FLAC__metadata_iterator_delete_block (iter, true);
        }
    }
    
    
    //
    // Create and insert a new VORBISCOMMENT block
    //
    {
        FLAC__StreamMetadata *vc_block; // For vorbis comments
        GList *l;
        
        // Allocate a block for Vorbis comments
        vc_block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);

        // Set the original vendor string, else will be use the version of library
        if (vce_field_vendor_string_found)
        {
            // must set 'copy' param to false, because the API will reuse the  pointer of an empty
            // string (yet still return 'true', indicating it was copied); the string is free'd during
            // metadata_chain_delete routine
            FLAC__metadata_object_vorbiscomment_set_vendor_string(vc_block, vce_field_vendor_string, /*copy=*/false);
        }

        gString delimiter;
        auto append_tag = [vc_block, &delimiter](const gchar *tag_name, const gchar *value, gint split = 0)
        {
            if (et_str_empty(value))
                return;

            if (split && (g_settings_get_flags(MainSettings, "ogg-split-fields") & abs(split)))
            {   const char* delim = "\n";
                if (split > 0)
                {   if (!delimiter)
                        delimiter.reset(g_settings_get_string(MainSettings, "split-delimiter"));
                    delim = delimiter;
                }

                gchar **strings = g_strsplit(value, delim, 255);
                for (int i = 0; strings[i] != NULL; i++)
                    vc_block_append_single_tag(vc_block, tag_name, strings[i]);
                g_strfreev (strings);
            }
            else
                vc_block_append_single_tag(vc_block, tag_name, value);
        };

        append_tag(ET_VORBIS_COMMENT_FIELD_TITLE, FileTag->title, ET_PROCESS_FIELD_TITLE);
        append_tag(ET_VORBIS_COMMENT_FIELD_VERSION, FileTag->version, ET_PROCESS_FIELD_VERSION);
        append_tag(ET_VORBIS_COMMENT_FIELD_SUBTITLE, FileTag->subtitle, ET_PROCESS_FIELD_SUBTITLE);

        append_tag(ET_VORBIS_COMMENT_FIELD_ARTIST, FileTag->artist, ET_PROCESS_FIELD_ARTIST);
        append_tag(ET_VORBIS_COMMENT_FIELD_ALBUM_ARTIST, FileTag->album_artist, ET_PROCESS_FIELD_ALBUM_ARTIST);

        append_tag(ET_VORBIS_COMMENT_FIELD_ALBUM, FileTag->album, ET_PROCESS_FIELD_ALBUM);
        append_tag(ET_VORBIS_COMMENT_FIELD_DISC_SUBTITLE, FileTag->disc_subtitle, ET_PROCESS_FIELD_DISC_SUBTITLE);

        append_tag(ET_VORBIS_COMMENT_FIELD_DISC_NUMBER, FileTag->disc_number);
        append_tag(ET_VORBIS_COMMENT_FIELD_DISC_TOTAL, FileTag->disc_total);

        append_tag(ET_VORBIS_COMMENT_FIELD_DATE, FileTag->year);
        append_tag(ET_VORBIS_COMMENT_FIELD_RELEASE_DATE, FileTag->release_year);

        append_tag(ET_VORBIS_COMMENT_FIELD_TRACK_NUMBER, FileTag->track);
        append_tag(ET_VORBIS_COMMENT_FIELD_TRACK_TOTAL, FileTag->track_total);

        append_tag(ET_VORBIS_COMMENT_FIELD_GENRE, FileTag->genre, ET_PROCESS_FIELD_GENRE);
        append_tag(ET_VORBIS_COMMENT_FIELD_COMMENT, FileTag->comment, ET_PROCESS_FIELD_COMMENT * (g_settings_get_boolean(MainSettings, "tag-multiline-comment") ? -1 : 1));
        append_tag(ET_VORBIS_COMMENT_FIELD_DESCRIPTION, FileTag->description, -ET_PROCESS_FIELD_DESCRIPTION);

        append_tag(ET_VORBIS_COMMENT_FIELD_COMPOSER, FileTag->composer, ET_PROCESS_FIELD_COMPOSER);
        append_tag(ET_VORBIS_COMMENT_FIELD_PERFORMER, FileTag->orig_artist, ET_PROCESS_FIELD_ORIGINAL_ARTIST);
        append_tag(ET_VORBIS_COMMENT_FIELD_ORIG_DATE, FileTag->orig_year);

        append_tag(ET_VORBIS_COMMENT_FIELD_COPYRIGHT, FileTag->copyright);
        append_tag(ET_VORBIS_COMMENT_FIELD_CONTACT, FileTag->url, ET_PROCESS_FIELD_URL);
        append_tag(ET_VORBIS_COMMENT_FIELD_ENCODED_BY, FileTag->encoded_by, ET_PROCESS_FIELD_ENCODED_BY);

        /**************************
         * Set unsupported fields *
         **************************/
        for (l = FileTag->other; l != NULL; l = g_list_next (l))
        {
            vc_block_append_other_tag (vc_block, (gchar *)l->data);
        }

        // Add the block to the the chain (so we don't need to free the block)
        FLAC__metadata_iterator_insert_block_after(iter, vc_block);
    }
    
    
    
    //
    // Create and insert PICTURE blocks
    //
    
    /***********
     * Picture *
     ***********/
    {
        EtPicture *pic = FileTag->picture;

        while (pic)
        {
            /* TODO: Can this ever be NULL? */
            if (pic->bytes)
            {
                const gchar *violation;
                FLAC__StreamMetadata *picture_block; // For picture data
                Picture_Format format;
                gconstpointer data;
                gsize data_size;
                
                // Allocate block for picture data
                picture_block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PICTURE);
                
                // Type
                picture_block->data.picture.type = (FLAC__StreamMetadata_Picture_Type)pic->type;
                
                // Mime type
                format = Picture_Format_From_Data(pic);
                /* Safe to pass a const string, according to the FLAC API
                 * reference. */
                FLAC__metadata_object_picture_set_mime_type(picture_block, (gchar *)Picture_Mime_Type_String(format), TRUE);

                // Description
                if (pic->description)
                {
                    FLAC__metadata_object_picture_set_description(picture_block, (FLAC__byte *)pic->description, TRUE);
                }
                
                // Resolution
                picture_block->data.picture.width  = pic->width;
                picture_block->data.picture.height = pic->height;
                picture_block->data.picture.depth  = 0;

                /* Picture data. */
                data = g_bytes_get_data (pic->bytes, &data_size);
                /* Safe to pass const data, if the last argument (copy) is
                 * TRUE, according the the FLAC API reference. */
                FLAC__metadata_object_picture_set_data (picture_block,
                                                        (FLAC__byte *)data,
                                                        (FLAC__uint32)data_size,
                                                        true);
                
                if (!FLAC__metadata_object_picture_is_legal (picture_block,
                                                             &violation))
                {
                    g_critical ("Created an invalid picture block: ‘%s’",
                                violation);
                    FLAC__metadata_object_delete (picture_block);
                }
                else
                {
                    // Add the block to the the chain (so we don't need to free the block)
                    FLAC__metadata_iterator_insert_block_after(iter, picture_block);
                }
            }
            
            pic = pic->next;
        }
    }
    
    FLAC__metadata_iterator_delete (iter);
    
    //
    // Prepare for writing tag
    //
    
    FLAC__metadata_chain_sort_padding (chain);
 
    /* Write tag. */
    if (FLAC__metadata_chain_check_if_tempfile_needed (chain, true))
    {
        EtFlacWriteState temp_state;
        GFile *temp_file;
        GFileIOStream *temp_iostream;
        GError *temp_error = NULL;

        temp_file = g_file_new_tmp ("easytag-XXXXXX", &temp_iostream,
                                    &temp_error);

        if (temp_file == NULL)
        {
            FLAC__metadata_chain_delete (chain);
            g_propagate_error (error, temp_error);
            et_flac_write_close_func (&state);
            return FALSE;
        }

        temp_state.file = temp_file;
        temp_state.error = NULL;
        temp_state.istream = G_FILE_INPUT_STREAM (g_io_stream_get_input_stream (G_IO_STREAM (temp_iostream)));
        temp_state.ostream = G_FILE_OUTPUT_STREAM (g_io_stream_get_output_stream (G_IO_STREAM (temp_iostream)));
        temp_state.seekable = G_SEEKABLE (temp_iostream);
        temp_state.iostream = temp_iostream;

        if (!FLAC__metadata_chain_write_with_callbacks_and_tempfile (chain,
                                                                     true,
                                                                     &state,
                                                                     callbacks,
                                                                     &temp_state,
                                                                     callbacks))
        {
            const FLAC__Metadata_ChainStatus status = FLAC__metadata_chain_status (chain);
            flac_error_msg = FLAC__Metadata_ChainStatusString[status];

            FLAC__metadata_chain_delete (chain);
            et_flac_write_close_func (&temp_state);
            et_flac_write_close_func (&state);

            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         _("Failed to write comments to file ‘%s’: %s"),
                         filename_utf8, flac_error_msg);
            return FALSE;
        }

        if (!g_file_move (temp_file, file, G_FILE_COPY_OVERWRITE, NULL, NULL,
                          NULL, &state.error))
        {
            FLAC__metadata_chain_delete (chain);
            et_flac_write_close_func (&temp_state);

            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         _("Failed to write comments to file ‘%s’: %s"),
                         filename_utf8, state.error->message);
            et_flac_write_close_func (&state);
            return FALSE;
        }

        et_flac_write_close_func (&temp_state);
    }
    else
    {
        if (!FLAC__metadata_chain_write_with_callbacks (chain, true, &state,
                                                        callbacks))
        {
            const FLAC__Metadata_ChainStatus status = FLAC__metadata_chain_status (chain);
            flac_error_msg = FLAC__Metadata_ChainStatusString[status];

            FLAC__metadata_chain_delete (chain);
            et_flac_write_close_func (&state);

            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         _("Failed to write comments to file ‘%s’: %s"),
                         filename_utf8, flac_error_msg);
            return FALSE;
        }
    }

    FLAC__metadata_chain_delete (chain);
    et_flac_write_close_func (&state);

#ifdef ENABLE_MP3
    {
        // Delete the ID3 tags (create a dummy ETFile for the Id3tag_... function)
        ET_File   *ETFile_tmp    = ET_File_Item_New();
        File_Name *FileName_tmp = et_file_name_new ();
        File_Tag  *FileTag_tmp = et_file_tag_new ();
        // Same file...
        FileName_tmp->value      = g_strdup(filename);
        FileName_tmp->value_utf8 = g_strdup(filename_utf8); // Not necessary to fill 'value_ck'
        ETFile_tmp->FileNameList = g_list_append(NULL,FileName_tmp);
        ETFile_tmp->FileNameCur  = ETFile_tmp->FileNameList;
        // With empty tag...
        ETFile_tmp->FileTagList  = g_list_append(NULL,FileTag_tmp);
        ETFile_tmp->FileTag      = ETFile_tmp->FileTagList;
        id3tag_write_file_tag (ETFile_tmp, NULL);
        ET_Free_File_List_Item(ETFile_tmp);
    }
#endif
    
    return TRUE;
}


#endif /* ENABLE_FLAC */
