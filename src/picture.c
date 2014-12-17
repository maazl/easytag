/* EasyTAG - Tag editor for audio files
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

#include "picture.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "easytag.h"
#include "log.h"
#include "misc.h"
#include "setting.h"
#include "charset.h"

#include "win32/win32dep.h"

/*
 * Note :
 * -> MP4_TAG :
 *      Just has one picture (ET_PICTURE_TYPE_FRONT_COVER).
 *      The format's don't matter to the MP4 side.
 *
 */

/*
 * et_picture_type_from_filename:
 * @filename: UTF-8 representation of a filename
 *
 * Use some heuristics to provide an estimate of the type of the picture, based
 * on the filename.
 *
 * Returns: the picture type, or %ET_PICTURE_TYPE_FRONT_COVER if the type could
 * not be estimated
 */
EtPictureType
et_picture_type_from_filename (const gchar *filename_utf8)
{
    EtPictureType picture_type = ET_PICTURE_TYPE_FRONT_COVER;

    /* TODO: Use g_str_tokenize_and_fold(). */
    static const struct
    {
        const gchar *type_str;
        const EtPictureType pic_type;
    } type_mappings[] =
    {
        { "front", ET_PICTURE_TYPE_FRONT_COVER },
        { "back", ET_PICTURE_TYPE_BACK_COVER },
        { "inlay", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "inside", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "leaflet", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "page", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "CD", ET_PICTURE_TYPE_MEDIA },
        { "media", ET_PICTURE_TYPE_MEDIA },
        { "artist", ET_PICTURE_TYPE_ARTIST_PERFORMER },
        { "performer", ET_PICTURE_TYPE_ARTIST_PERFORMER },
        { "conductor", ET_PICTURE_TYPE_CONDUCTOR },
        { "band", ET_PICTURE_TYPE_BAND_ORCHESTRA },
        { "orchestra", ET_PICTURE_TYPE_BAND_ORCHESTRA },
        { "composer", ET_PICTURE_TYPE_COMPOSER },
        { "lyricist", ET_PICTURE_TYPE_LYRICIST_TEXT_WRITER },
        { "illustration", ET_PICTURE_TYPE_ILLUSTRATION },
        { "publisher", ET_PICTURE_TYPE_PUBLISHER_STUDIO_LOGOTYPE }
    };
    gsize i;
    gchar *folded_filename;

    g_return_val_if_fail (filename_utf8 != NULL, ET_PICTURE_TYPE_FRONT_COVER);

    folded_filename = g_utf8_casefold (filename_utf8, -1);

    for (i = 0; i < G_N_ELEMENTS (type_mappings); i++)
    {
        gchar *folded_type = g_utf8_casefold (type_mappings[i].type_str, -1);
        if (strstr (folded_filename, folded_type) != NULL)
        {
            picture_type = type_mappings[i].pic_type;
            g_free (folded_type);
            break;
        }
        else
        {
            g_free (folded_type);
        }
    }

    g_free (folded_filename);

    return picture_type;
}

/* FIXME: Possibly use gnome_vfs_get_mime_type_for_buffer. */
Picture_Format
Picture_Format_From_Data (const Picture *pic)
{
    gsize size;
    gconstpointer data;

    g_return_val_if_fail (pic != NULL, PICTURE_FORMAT_UNKNOWN);

    data = g_bytes_get_data (pic->bytes, &size);

    /* JPEG : "\xff\xd8". */
    if (size > 2 && memcmp (data, "\xff\xd8", 2))
    {
        return PICTURE_FORMAT_JPEG;
    }

    /* PNG : "\x89PNG\x0d\x0a\x1a\x0a". */
    if (size > 8 && memcmp (data, "\x89PNG\x0d\x0a\x1a\x0a", 8))
    {
        return PICTURE_FORMAT_PNG;
    }
    
    /* GIF: "GIF87a" */
    if (size > 6 && memcmp (data, "GIF87a", 6))
    {
        return PICTURE_FORMAT_GIF;
    }

    /* GIF: "GIF89a" */
    if (size > 6 && memcmp (data, "GIF89a", 6))
    {
        return PICTURE_FORMAT_GIF;
    }
    
    return PICTURE_FORMAT_UNKNOWN;
}

const gchar *
Picture_Mime_Type_String (Picture_Format format)
{
    switch (format)
    {
        case PICTURE_FORMAT_JPEG:
            return "image/jpeg";
        case PICTURE_FORMAT_PNG:
            return "image/png";
        case PICTURE_FORMAT_GIF:
            return "image/gif";
        default:
            g_debug ("%s", "Unrecognised image MIME type");
            return "application/octet-stream";
    }
}


static const gchar *
Picture_Format_String (Picture_Format format)
{
    switch (format)
    {
        case PICTURE_FORMAT_JPEG:
            return _("JPEG image");
        case PICTURE_FORMAT_PNG:
            return _("PNG image");
        case PICTURE_FORMAT_GIF:
            return _("GIF image");
        default:
            return _("Unknown image");
    }
}

const gchar *
Picture_Type_String (EtPictureType type)
{
    switch (type)
    {
        case ET_PICTURE_TYPE_OTHER:
            return _("Other");
        case ET_PICTURE_TYPE_FILE_ICON:
            return _("32×32 pixel PNG file icon");
        case ET_PICTURE_TYPE_OTHER_FILE_ICON:
            return _("Other file icon");
        case ET_PICTURE_TYPE_FRONT_COVER:
            return _("Cover (front)");
        case ET_PICTURE_TYPE_BACK_COVER:
            return _("Cover (back)");
        case ET_PICTURE_TYPE_LEAFLET_PAGE:
            return _("Leaflet page");
        case ET_PICTURE_TYPE_MEDIA:
            return _("Media (such as label side of CD)");
        case ET_PICTURE_TYPE_LEAD_ARTIST_LEAD_PERFORMER_SOLOIST:
            return _("Lead artist/lead performer/soloist");
        case ET_PICTURE_TYPE_ARTIST_PERFORMER:
            return _("Artist/performer");
        case ET_PICTURE_TYPE_CONDUCTOR:
            return _("Conductor");
        case ET_PICTURE_TYPE_BAND_ORCHESTRA:
            return _("Band/Orchestra");
        case ET_PICTURE_TYPE_COMPOSER:
            return _("Composer");
        case ET_PICTURE_TYPE_LYRICIST_TEXT_WRITER:
            return _("Lyricist/text writer");
        case ET_PICTURE_TYPE_RECORDING_LOCATION:
            return _("Recording location");
        case ET_PICTURE_TYPE_DURING_RECORDING:
            return _("During recording");
        case ET_PICTURE_TYPE_DURING_PERFORMANCE:
            return _("During performance");
        case ET_PICTURE_TYPE_MOVIE_VIDEO_SCREEN_CAPTURE:
            return _("Movie/video screen capture");
        case ET_PICTURE_TYPE_A_BRIGHT_COLOURED_FISH:
            return _("A bright colored fish");
        case ET_PICTURE_TYPE_ILLUSTRATION:
            return _("Illustration");
        case ET_PICTURE_TYPE_BAND_ARTIST_LOGOTYPE:
            return _("Band/Artist logotype");
        case ET_PICTURE_TYPE_PUBLISHER_STUDIO_LOGOTYPE:
            return _("Publisher/studio logotype");
        
        case ET_PICTURE_TYPE_UNDEFINED:
        default:
            return _("Unknown image type");
    }
}

gchar *
Picture_Info (const Picture *pic,
              ET_Tag_Type tag_type)
{
    const gchar *format, *desc, *type;
    gchar *r, *size_str;

    format = Picture_Format_String(Picture_Format_From_Data(pic));

    if (pic->description)
        desc = pic->description;
    else
        desc = "";

    type = Picture_Type_String (pic->type);
    size_str = g_format_size (g_bytes_get_size (pic->bytes));

    // Behaviour following the tag type...
    switch (tag_type)
    {
        case MP4_TAG:
        {
            r = g_strdup_printf ("%s (%s - %d×%d %s)\n%s: %s", format,
                                 size_str, pic->width, pic->height,
                                 _("pixels"), _("Type"), type);
            break;
        }

        // Other tag types
        default:
        {
            r = g_strdup_printf ("%s (%s - %d×%d %s)\n%s: %s\n%s: %s", format,
                                 size_str, pic->width, pic->height,
                                 _("pixels"), _("Type"), type,
                                 _("Description"), desc);
            break;
        }
    }

    g_free (size_str);

    return r;
}

Picture *
Picture_Allocate (void)
{
    Picture *pic = g_slice_new0 (Picture);
    return pic;
}

Picture *
Picture_Copy_One (const Picture *pic)
{
    Picture *pic2;

    g_return_val_if_fail (pic != NULL, NULL);

    pic2 = Picture_Allocate ();
    pic2->type = pic->type;
    pic2->width  = pic->width;
    pic2->height = pic->height;

    if (pic->description)
    {
        pic2->description = g_strdup (pic->description);
    }

    pic2->bytes = g_bytes_ref (pic->bytes);

    return pic2;
}

Picture *
Picture_Copy (const Picture *pic)
{
    Picture *pic2 = Picture_Copy_One(pic);
    if (pic->next)
        pic2->next = Picture_Copy(pic->next);
    return pic2;
}

void
Picture_Free (Picture *pic)
{
    if (pic == NULL)
    {
        return;
    }

    if (pic->next)
    {
        Picture_Free (pic->next);
    }

    g_free (pic->description);
    g_bytes_unref (pic->bytes);
    pic->bytes = NULL;

    g_slice_free (Picture, pic);
}


/*
 * et_picture_load_file_data:
 * @file: the GFile from which to load an image
 * @error: a #GError to provide information on errors, or %NULL to ignore
 *
 * Load an image from the supplied @file.
 *
 * Returns: an image on success, %NULL otherwise
 */
Picture *
et_picture_load_file_data (GFile *file, GError **error)
{
    gsize size;
    GFileInfo *info;
    GFileInputStream *file_istream;
    GOutputStream *ostream;

    info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                              G_FILE_QUERY_INFO_NONE, NULL, error);

    if (!info)
    {
        g_assert (error == NULL || *error != NULL);
        return NULL;
    }

    file_istream = g_file_read (file, NULL, error);

    if (!file_istream)
    {
        g_assert (error == NULL || *error != NULL);
        return NULL;
    }

    size = g_file_info_get_size (info);
    g_object_unref (info);

    /* HTTP servers may not report a size, or the file could be empty. */
    if (size == 0)
    {
        ostream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
    }
    else
    {
        gchar *buffer;

        buffer = g_malloc (size);
        ostream = g_memory_output_stream_new (buffer, size, g_realloc, g_free);
    }

    if (g_output_stream_splice (ostream, G_INPUT_STREAM (file_istream),
                                G_OUTPUT_STREAM_SPLICE_NONE, NULL,
                                error) == -1)
    {
        g_object_unref (ostream);
        g_object_unref (file_istream);
        g_assert (error == NULL || *error != NULL);
        return NULL;
    }
    else
    {
        /* Image loaded. */
        Picture *pic;
        gpointer data;
        gsize data_size;

        g_object_unref (file_istream);

        if (!g_output_stream_close (ostream, NULL, error))
        {
            g_object_unref (ostream);
            g_assert (error == NULL || *error != NULL);
            return NULL;
        }

        g_assert (error == NULL || *error == NULL);

        pic = Picture_Allocate ();
        data = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (ostream));
        data_size = g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (ostream));
        pic->bytes = g_bytes_new_take (data, data_size);
        /* TODO: Use g_memory_output_stream_steal_as_bytes(). */

        g_object_unref (ostream);
        g_assert (error == NULL || *error == NULL);
        return pic;
    }
}

/*
 * et_picture_save_file_data:
 * @pic: the #Picture from which to take an image
 * @file: the #GFile for which to save an image
 * @error: a #GError to provide information on errors, or %NULL to ignore
 *
 * Saves an image from @pic to the supplied @file.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 */
gboolean
et_picture_save_file_data (const Picture *pic, GFile *file, GError **error)
{
    GFileOutputStream *file_ostream;
    gconstpointer data;
    gsize data_size;
    gsize bytes_written;

    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    file_ostream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL,
                                   error);

    if (!file_ostream)
    {
        g_assert (error == NULL || *error != NULL);
        return FALSE;
    }

    data = g_bytes_get_data (pic->bytes, &data_size);

    if (!g_output_stream_write_all (G_OUTPUT_STREAM (file_ostream), data,
                                    data_size, &bytes_written, NULL, error))
    {
        g_debug ("Only %" G_GSIZE_FORMAT " bytes out of %" G_GSIZE_FORMAT
                 " bytes of picture data were written", bytes_written,
                 g_bytes_get_size (pic->bytes));
        g_object_unref (file_ostream);
        g_assert (error == NULL || *error != NULL);
        return FALSE;
    }

    if (!g_output_stream_close (G_OUTPUT_STREAM (file_ostream), NULL, error))
    {
        g_object_unref (file_ostream);
        g_assert (error == NULL || *error != NULL);
        return FALSE;
    }

    g_assert (error == NULL || *error == NULL);
    g_object_unref (file_ostream);
    return TRUE;
}
