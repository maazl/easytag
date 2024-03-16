/* EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 * Copyright (C) 2012-1014  David King <amigadave@amigadave.com>
 * Copyright (C) 2001-2005  Jerome Couderc <easytag@gmail.com>
 * Copyright (C) 2005  Michael Ihde <mike.ihde@randomwalking.com>
 * Copyright (C) 2005  Stewart Whitman <swhitman@cox.net>
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

#include "config.h" // For definition of ENABLE_MP4

#ifdef ENABLE_MP4

#include <glib/gi18n.h>

#include "mp4_header.h"
#include "mp4_tag.h"
#include "picture.h"
#include "misc.h"
#include "et_core.h"
#include "charset.h"
#include "gio_wrapper.h"

/* Shadow warning in public TagLib headers. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#ifdef G_OS_WIN32
#undef NOMINMAX /* Warning in TagLib headers, fixed in git. */
#endif /* G_OS_WIN32 */
#include <mp4file.h>
#include <mp4tag.h>
#pragma GCC diagnostic pop
#include <tpropertymap.h>

/* Include mp4_header.cc directly. */
#include "mp4_header.cc"

/*
 * Mp4_Tag_Read_File_Tag:
 *
 * Read tag data into an Mp4 file.
 */
gboolean
mp4tag_read_file_tag (GFile *file,
                      File_Tag *FileTag,
                      GError **error)
{
    TagLib::MP4::Tag *tag;
    guint year;

    g_return_val_if_fail (file != NULL && FileTag != NULL, FALSE);

    /* Get data from tag. */
    GIO_InputStream stream (file);

    if (!stream.isOpen ())
    {
        const GError *tmp_error = stream.getError ();
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Error while opening file: %s"), tmp_error->message);
        return FALSE;
    }

    TagLib::MP4::File mp4file (&stream);

    if (!mp4file.isOpen ())
    {
        const GError *tmp_error = stream.getError ();

        if (tmp_error)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         _("Error while opening file: %s"),
                         tmp_error->message);
        }
        else
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         _("Error while opening file: %s"),
                         _("MP4 format invalid"));
        }

        return FALSE;
    }

    if (!(tag = mp4file.tag ()))
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                     _("Error reading tags from file"));
        return FALSE;
    }

    const TagLib::PropertyMap& extra_tag = tag->properties ();

    et_file_tag_set_title (FileTag, tag->title().toCString(true));

    if (extra_tag.contains ("SUBTITLE"))
        et_file_tag_set_subtitle(FileTag, extra_tag["SUBTITLE"].front().toCString(true));

    et_file_tag_set_artist(FileTag, tag->artist().toCString(true));

    et_file_tag_set_album(FileTag, tag->album().toCString(true));

    if (extra_tag.contains ("DISCSUBTITLE"))
        et_file_tag_set_disc_subtitle(FileTag, extra_tag["DISCSUBTITLE"].front().toCString(true));

    /* Disc number. */
    /* Total disc number support in TagLib reads multiple disc numbers and
     * joins them with a "/". */
    if (extra_tag.contains ("DISCNUMBER"))
    {
        const TagLib::StringList& disc_numbers = extra_tag["DISCNUMBER"];
        int offset = disc_numbers.front ().find ("/");

        if (offset != -1)
        {
            FileTag->disc_total = et_disc_number_to_string (disc_numbers.front().substr(offset + 1).toCString(true));
        }

        FileTag->disc_number = et_disc_number_to_string (disc_numbers.front().toCString(true));
    }

    year = tag->year ();
    if (year != 0)
    {
        FileTag->year = g_strdup_printf ("%u", year);
    }

    /*************************
     * Track and Total Track *
     *************************/
    if (extra_tag.contains ("TRACKNUMBER"))
    {
        const TagLib::StringList& track_numbers = extra_tag["TRACKNUMBER"];
        int offset = track_numbers.front ().find ("/");

        if (offset != -1)
        {
            FileTag->track_total = et_track_number_to_string (track_numbers.front ().substr (offset + 1).toCString(true));
        }

        FileTag->track = et_track_number_to_string (track_numbers.front ().toCString(true));
    }

    et_file_tag_set_genre(FileTag, tag->genre().toCString(true));

    et_file_tag_set_comment(FileTag, tag->comment().toCString(true));

    if (extra_tag.contains ("COMPOSER"))
        et_file_tag_set_composer(FileTag, extra_tag["COMPOSER"].front().toCString(true));

    if (extra_tag.contains ("COPYRIGHT"))
        et_file_tag_set_copyright(FileTag, extra_tag["COPYRIGHT"].front().toCString(true));

    if (extra_tag.contains ("ENCODEDBY"))
        et_file_tag_set_encoded_by(FileTag, extra_tag["ENCODEDBY"].front().toCString(true));

    const TagLib::MP4::ItemListMap &extra_items = tag->itemListMap ();

    /* Album Artist */
#if (TAGLIB_MAJOR_VERSION == 1) && (TAGLIB_MINOR_VERSION < 10)
    /* No "ALBUMARTIST" support in TagLib until 1.10; use atom directly. */
    if (extra_items.contains ("aART"))
        et_file_tag_set_album_artist(FileTag, extra_items["aART"].toStringList().front().toCString(true));
#else
    if (extra_tag.contains ("ALBUMARTIST"))
        et_file_tag_set_album_artist(FileTag, extra_tag["ALBUMARTIST"].front().toCString(true));
#endif

    /* Description */
#if (TAGLIB_MAJOR_VERSION == 1) && (TAGLIB_MINOR_VERSION < 12)
    /* No "PODCASTDESC" support in TagLib until 1.12; use atom directly. */
    if (extra_items.contains ("desc"))
        et_file_tag_set_description(FileTag, extra_items["desc"].toStringList().front().toCString(true));
#else
    if (extra_tag.contains ("PODCASTDESC"))
        et_file_tag_set_description(FileTag, extra_tag["PODCASTDESC"].front().toCString(true));
#endif

    // TOD support ReplayGain, requires TagLib 2.0

    /***********
     * Picture *
     ***********/
    if (extra_items.contains ("covr"))
    {
        const TagLib::MP4::Item &cover = extra_items["covr"];
        const TagLib::MP4::CoverArtList &covers = cover.toCoverArtList ();
        const TagLib::MP4::CoverArt &art = covers.front ();

        /* TODO: Use g_bytes_new_with_free_func()? */
        GBytes *bytes = g_bytes_new (art.data ().data (), art.data ().size ());

        /* MP4 does not support image types, nor descriptions. */
        FileTag->picture = et_picture_new (ET_PICTURE_TYPE_FRONT_COVER, "", 0,
                                           0, bytes);
        g_bytes_unref (bytes);
    }
    else
    {
        et_file_tag_set_picture (FileTag, NULL);
    }

    return TRUE;
}


/*
 * Mp4_Tag_Write_File_Tag:
 *
 * Write tag data into an Mp4 file.
 */
gboolean
mp4tag_write_file_tag (const ET_File *ETFile,
                       GError **error)
{
    const File_Tag *FileTag;
    const gchar *filename;
    const gchar *filename_utf8;
    TagLib::MP4::Tag *tag;

    g_return_val_if_fail (ETFile != NULL && ETFile->FileTag != NULL, FALSE);

    FileTag = (File_Tag *)ETFile->FileTag->data;
    filename      = ((File_Name *)ETFile->FileNameCur->data)->value;
    filename_utf8 = ((File_Name *)ETFile->FileNameCur->data)->value_utf8;

    /* Open file for writing */
    GFile *file = g_file_new_for_path (filename);
    GIO_IOStream stream (file);

    if (!stream.isOpen ())
    {
        const GError *tmp_error = stream.getError ();
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Error while opening file ‘%s’: %s"), filename_utf8,
                     tmp_error->message);
        return FALSE;
    }

    TagLib::MP4::File mp4file (&stream);

    g_object_unref (file);

    if (!mp4file.isOpen ())
    {
        const GError *tmp_error = stream.getError ();

        if (tmp_error)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         _("Error while opening file ‘%s’: %s"), filename_utf8,
                         tmp_error->message);
        }
        else
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         _("Error while opening file ‘%s’: %s"), filename_utf8,
                         _("MP4 format invalid"));
        }


        return FALSE;
    }

    if (!(tag = mp4file.tag ()))
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Error reading tags from file ‘%s’"), filename_utf8);
        return FALSE;
    }

    TagLib::PropertyMap fields;

    auto add_field = [&fields](const gchar* value, const char* propertyname)
    {
        if (!et_str_empty(value))
            fields.insert(propertyname, TagLib::String(value, TagLib::String::UTF8));
    };

    add_field(FileTag->title, "TITLE");

    add_field(FileTag->subtitle, "SUBTITLE");

    add_field(FileTag->artist, "ARTIST");

    add_field(FileTag->album, "ALBUM");

    add_field(FileTag->disc_subtitle, "DISCSUBTITLE");

    add_field(FileTag->disc_and_total().c_str(), "DISCNUMBER");

    add_field(FileTag->year, "DATE");

    add_field(FileTag->track_and_total().c_str(), "TRACKNUMBER");

    add_field(FileTag->genre, "GENRE");

    add_field(FileTag->comment, "COMMENT");

    add_field(FileTag->composer, "COMPOSER");

    add_field(FileTag->copyright, "COPYRIGHT");

    add_field(FileTag->encoded_by, "ENCODEDBY");

    TagLib::MP4::ItemListMap &extra_items = tag->itemListMap ();

    /* Album artist. */
    if (!et_str_empty (FileTag->album_artist))
    {
        TagLib::String string (FileTag->album_artist, TagLib::String::UTF8);
#if (TAGLIB_MAJOR_VERSION == 1) && (TAGLIB_MINOR_VERSION < 10)
        /* No "ALBUMARTIST" support in TagLib until 1.10; use atom directly. */
        extra_items.insert ("aART", TagLib::MP4::Item (string));
    }
    else
    {
        extra_items.erase ("aART");
#else
        fields.insert ("ALBUMARTIST", string);
#endif
    }

    /* Description. */
    if (!et_str_empty (FileTag->description))
    {
        TagLib::String string (FileTag->description, TagLib::String::UTF8);
#if (TAGLIB_MAJOR_VERSION == 1) && (TAGLIB_MINOR_VERSION < 12)
        /* No "PODCASTDESC" support in TagLib until 1.12; use atom directly. */
        extra_items.insert ("desc", TagLib::MP4::Item (string));
    }
    else
    {
        extra_items.erase ("desc");
#else
        fields.insert ("PODCASTDESC", string);
#endif
    }

    // TOD support ReplayGain, requires TagLib 2.0

    /***********
     * Picture *
     ***********/
    if (FileTag->picture)
    {
        Picture_Format pf;
        TagLib::MP4::CoverArt::Format f;
        gconstpointer data;
        gsize data_size;

        pf = Picture_Format_From_Data (FileTag->picture);

        switch (pf)
        {
            case PICTURE_FORMAT_JPEG:
                f = TagLib::MP4::CoverArt::JPEG;
                break;
            case PICTURE_FORMAT_PNG:
                f = TagLib::MP4::CoverArt::PNG;
                break;
            case PICTURE_FORMAT_GIF:
                f = TagLib::MP4::CoverArt::GIF;
                break;
            case PICTURE_FORMAT_UNKNOWN:
            default:
                g_critical ("Unknown format");
                f = TagLib::MP4::CoverArt::JPEG;
                break;
        }

        data = g_bytes_get_data (FileTag->picture->bytes, &data_size);
        TagLib::MP4::CoverArt art (f, TagLib::ByteVector((char *)data,
                                                         data_size));

        extra_items.insert ("covr",
                            TagLib::MP4::Item (TagLib::MP4::CoverArtList ().append (art)));
    }
    else
    {
        extra_items.erase ("covr");
    }

    tag->setProperties (fields);

    return mp4file.save();
}

#endif /* ENABLE_MP4 */
