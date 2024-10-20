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
#include "../file_name.h"
#include "../file_tag.h"
#include "../file.h"

#include <memory>

using namespace std;

// registration
const struct Flac_Description : ET_File_Description
{	Flac_Description(const char* extension)
	{	Extension = extension;
		FileType = _("FLAC File");
		TagType = _("FLAC Vorbis Tag");
		read_file = flac_read_file;
		write_file_tag = flac_tag_write_file_tag;
		display_file_info_to_ui = et_flac_header_display_file_info_to_ui;
	}
}
FLAC_Description(".flac"),
FLA_Description(".fla");


/*
 * Read tag data from a FLAC file using the level 2 flac interface,
 * Note:
 *  - if field is found but contains no info (strlen(str)==0), we don't read it
 */
File_Tag* flac_read_file (GFile *file, ET_File *ETFile, GError **error)
{
    EtFlacReadState state;
    FLAC__IOCallbacks callbacks = { et_flac_read_func,
                                    NULL, /* Do not set a write callback. */
                                    et_flac_seek_func, et_flac_tell_func,
                                    et_flac_eof_func,
                                    et_flac_read_close_func };
    g_return_val_if_fail (file != NULL && ETFile != NULL, nullptr);
    g_return_val_if_fail (error == NULL || *error == NULL, nullptr);

    auto chain = make_unique(FLAC__metadata_chain_new(), FLAC__metadata_chain_delete);

    if (chain == NULL)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM, "%s",
                     g_strerror (ENOMEM));
        return nullptr;
    }

    state.eof = FALSE;
    state.error = NULL;
    state.istream = g_file_read (file, NULL, &state.error);
    state.seekable = G_SEEKABLE (state.istream);

    if (!FLAC__metadata_chain_read_with_callbacks(chain.get(), &state, callbacks))
    {
        /* TODO: Provide a dedicated error enum corresponding to status. */
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                     _("Error opening FLAC file"));
        et_flac_read_close_func (&state);

        return nullptr;
    }

    auto iter = make_unique(FLAC__metadata_iterator_new(), FLAC__metadata_iterator_delete);

    if (iter == NULL)
    {
        et_flac_read_close_func (&state);
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOMEM, "%s",
                     g_strerror (ENOMEM));
        return nullptr;
    }

    File_Tag* FileTag = new File_Tag();
    ET_File_Info* ETFileInfo = &ETFile->ETFileInfo;

    FLAC__metadata_iterator_init(iter.get(), chain.get());
    uint32_t metadata_len = 0;
    do
    {
        FLAC__StreamMetadata *block;

        block = FLAC__metadata_iterator_get_block(iter.get());
        metadata_len += block->length;

        if (block->type == FLAC__METADATA_TYPE_VORBIS_COMMENT)
        {
            const FLAC__StreamMetadata_VorbisComment* vc = &block->data.vorbis_comment;
            vorbis_tags tags(vc->num_comments);

            /* Get comments from block. */
            for (int i = 0; i < vc->num_comments; i++)
            {
                auto& comment = vc->comments[i];
                tags.emplace((const char*)comment.entry, comment.length);
            }

            tags.to_file_tags(FileTag);

            /* Save unsupported fields. */
            tags.to_other_tags(ETFile);
        }
        else if (block->type == FLAC__METADATA_TYPE_PICTURE)
        {
            /* Picture. */
            const FLAC__StreamMetadata_Picture *p;
            /* Get picture data from block. */
            p = &block->data.picture;

            FileTag->pictures.emplace_back((EtPictureType)p->type,
                xStringD0((const char*)p->description), 0, 0, p->data, p->data_length);
        }
        else if (block->type == FLAC__METADATA_TYPE_STREAMINFO)
        {
            /* header info */
            const FLAC__StreamMetadata_StreamInfo *stream_info = &block->data.stream_info;
            if (stream_info->sample_rate == 0)
            {
                gchar *filename;

                /* This is invalid according to the FLAC specification, but
                 * such files have been observed in the wild. */
                ETFileInfo->duration = 0.;

                filename = g_file_get_path (file);
                g_debug ("Invalid FLAC sample rate of 0: %s", filename);
                g_free (filename);
            }
            else
            {
                ETFileInfo->duration = (double)stream_info->total_samples / stream_info->sample_rate;
            }

            ETFileInfo->mode = stream_info->channels;
            ETFileInfo->samplerate = stream_info->sample_rate;
            ETFileInfo->version = 0; /* Not defined in FLAC file. */
        }
    } while (FLAC__metadata_iterator_next(iter.get()));

    iter.reset();
    chain.reset();
    et_flac_read_close_func (&state);
    /* End of decoding FLAC file */

    if (ETFileInfo->duration > 0 && ETFile->FileSize > 0)
    {
        /* Ignore metadata blocks, and use the remainder to calculate the
         * average bitrate (including format overhead). */
        ETFileInfo->bitrate = (int)lround((ETFile->FileSize - metadata_len) / ETFileInfo->duration * 8.);
    }

#ifdef ENABLE_MP3
    /* If no FLAC vorbis tag found : we try to get the ID3 tag if it exists
     * (but it will be deleted when rewriting the tag) */
    if (FileTag->empty())
    {
        File_Tag* FileTag2 = id3_read_file(file, ETFile, NULL);
        if (FileTag2)
        {
            delete FileTag;
            FileTag = FileTag2;

            // If an ID3 tag has been found (and no FLAC tag), we mark the file as
            // unsaved to rewrite a flac tag.
            if (FileTag->empty())
                ETFile->force_tag_save();
        }
    }
#endif

    // validate date fields
    FileTag->check_dates(3, true, *ETFile->FileNameCur()); // From field 3 arbitrary strings are allowed

    return FileTag;
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
    GFile *file;
    GFileIOStream *iostream;
    EtFlacWriteState state;
    FLAC__IOCallbacks callbacks = { et_flac_read_func, et_flac_write_func,
                                    et_flac_seek_func, et_flac_tell_func,
                                    et_flac_eof_func,
                                    et_flac_write_close_func };
    const gchar *flac_error_msg;
    FLAC__Metadata_Chain *chain;
    FLAC__Metadata_Iterator *iter;
    FLAC__StreamMetadata_VorbisComment_Entry vce_field_vendor_string; // To save vendor string
    gboolean vce_field_vendor_string_found = FALSE;

    g_return_val_if_fail (ETFile != NULL && ETFile->FileTagNew() != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    const File_Tag* FileTag = ETFile->FileTagNew();
    const File_Name& filename = *ETFile->FileNameCur();

    /* libFLAC is able to detect (and skip) ID3v2 tags by itself */
    
    /* Create a new chain instance to get all blocks in one time. */
    chain = FLAC__metadata_chain_new ();

    if (chain == NULL)
    {
        flac_error_msg = FLAC__Metadata_ChainStatusString[FLAC__METADATA_CHAIN_STATUS_MEMORY_ALLOCATION_ERROR];
        
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Error while opening file ‘%s’ as FLAC: %s"),
                     filename.full_name().get(), flac_error_msg);
        return FALSE;
    }

    file = g_file_new_for_path(ETFile->FilePath);

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
                     filename.full_name().get(), flac_error_msg);
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
                     filename.full_name().get(), flac_error_msg);
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
        // Allocate a block for Vorbis comments
        FLAC__StreamMetadata *vc_block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);

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

        append_tag(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_TRACK_GAIN, FileTag->track_gain_str().c_str());
        append_tag(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_TRACK_PEAK, FileTag->track_peak_str().c_str());
        append_tag(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_ALBUM_GAIN, FileTag->album_gain_str().c_str());
        append_tag(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_ALBUM_PEAK, FileTag->album_peak_str().c_str());

        /**************************
         * Set unsupported fields *
         **************************/
        if (ETFile->other)
            for (gString* l = ETFile->other.get(); l; ++l)
                vc_block_append_other_tag(vc_block, *l);

        // Add the block to the the chain (so we don't need to free the block)
        FLAC__metadata_iterator_insert_block_after(iter, vc_block);
    }
    
    /***********
     * Picture *
     ***********/
    for (const EtPicture& pic : FileTag->pictures)
    {
        const gchar *violation;
        FLAC__StreamMetadata *picture_block; // For picture data

        // Allocate block for picture data
        picture_block = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PICTURE);

        // Type
        picture_block->data.picture.type = (FLAC__StreamMetadata_Picture_Type)pic.type;

        // Mime type
        /* Safe to pass a const string, according to the FLAC API
         * reference. */
        FLAC__metadata_object_picture_set_mime_type(picture_block,
            (gchar*)EtPicture::Mime_Type_String(pic.Format()), TRUE);

        // Description
        if (!pic.description.empty())
            FLAC__metadata_object_picture_set_description(picture_block,
                (FLAC__byte *)pic.description.get(), TRUE);

        // Resolution
        picture_block->data.picture.width  = pic.storage->Width;
        picture_block->data.picture.height = pic.storage->Height;
        picture_block->data.picture.depth  = 0;

        /* Picture data. */
        /* Safe to pass const data, if the last argument (copy) is
         * TRUE, according the the FLAC API reference. */
        FLAC__metadata_object_picture_set_data(picture_block,
            (FLAC__byte *)pic.storage->Bytes, (FLAC__uint32)pic.storage->Size, true);

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
                         filename.full_name().get(), flac_error_msg);
            return FALSE;
        }

        if (!g_file_move (temp_file, file, G_FILE_COPY_OVERWRITE, NULL, NULL,
                          NULL, &state.error))
        {
            FLAC__metadata_chain_delete (chain);
            et_flac_write_close_func (&temp_state);

            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         _("Failed to write comments to file ‘%s’: %s"),
                         filename.full_name().get(), state.error->message);
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
                         filename.full_name().get(), flac_error_msg);
            return FALSE;
        }
    }

    FLAC__metadata_chain_delete (chain);
    et_flac_write_close_func (&state);

#ifdef ENABLE_MP3
    {
        // Delete the ID3 tags (create a dummy ETFile for the Id3tag_... function)
        ET_File ETFile_tmp(ETFile->FilePath);
        // Same file with empty tag...
        ETFile_tmp.apply_changes(
            new File_Name(*ETFile->FileNameCur()),
            new File_Tag());
        id3tag_write_file_tag(&ETFile_tmp, NULL);
    }
#endif
    
    return TRUE;
}

void
et_flac_header_display_file_info_to_ui (EtFileHeaderFields *fields, const ET_File *ETFile)
{
    /* Nothing to display */
    fields->version_label = _("Encoder:");
    fields->version = "flac";

    /* Mode */
    fields->mode_label = _("Channels:");
}

#endif /* ENABLE_FLAC */
