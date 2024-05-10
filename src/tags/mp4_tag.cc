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
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#pragma GCC diagnostic pop
#include <taglib/tpropertymap.h>

#include <limits>
using namespace std;
using namespace TagLib;

#if TAGLIB_MAJOR_VERSION >= 2
#include <taglib/mp4itemfactory.h>
#include <taglib/tmap.h>


// registration
const struct QuickTime_Description : ET_File_Description
{	QuickTime_Description(const char* extension, const char* description)
	{	Extension = extension;
		FileType = description;
		TagType = _("MP4/QuickTime Tag");
		read_file = mp4_read_file;
		write_file_tag = mp4tag_write_file_tag;
		display_file_info_to_ui = et_mp4_header_display_file_info_to_ui;
		unsupported_fields = mp4tag_unsupported_fields;
		support_multiple_pictures = [](const ET_File*) { return false; };
	}
}
MP4_Description(".mp4", _("MPEG4 File")),
M4A_Description(".m4a", _("MPEG4 File")),
M4P_Description(".m4p", _("MPEG4 File")),
M4V_Description(".m4v", _("MPEG4 File")),
AAC_Description(".aac", _("AAC File")); // TODO .aac is typically ADTS rather than MPEG4


/// Add support for ReplayGain tags
static class CustomItemFactory : public MP4::ItemFactory
{
protected:
	virtual Map<ByteVector, String> namePropertyMap() const;
} ItemFactory;

Map<ByteVector, String> CustomItemFactory::namePropertyMap() const
{
	auto map = MP4::ItemFactory::namePropertyMap();
	map.insert("----:com.apple.iTunes:replaygain_track_gain", "REPLAYGAIN_TRACK_GAIN");
	map.insert("----:com.apple.iTunes:replaygain_track_peak", "REPLAYGAIN_TRACK_PEAK");
	map.insert("----:com.apple.iTunes:replaygain_album_gain", "REPLAYGAIN_ALBUM_GAIN");
	map.insert("----:com.apple.iTunes:replaygain_album_peak", "REPLAYGAIN_ALBUM_PEAK");
	return map;
}
#endif

/*
 * Mp4_Tag_Read_File_Tag:
 *
 * Read tag data into an Mp4 file.
 */
gboolean mp4_read_file(GFile *file, ET_File *ETFile, GError **error)
{
    guint year;

    g_return_val_if_fail (file != NULL && ETFile != NULL, FALSE);

    /* Get data from tag. */
    GIO_InputStream stream (file);

    if (!stream.isOpen ())
    {
        const GError *tmp_error = stream.getError ();
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Error while opening file: %s"), tmp_error->message);
        return FALSE;
    }

#if TAGLIB_MAJOR_VERSION >= 2
    MP4::File mp4file(&stream, true, MP4::Properties::Average, &ItemFactory);
#else
    MP4::File mp4file(&stream);
#endif

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


    /* ET_File_Info header data */

    const TagLib::MP4::Properties* properties = mp4file.audioProperties ();
    if (properties == NULL)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                     _("Error reading properties from file"));
        return FALSE;
    }

    ET_File_Info* ETFileInfo = &ETFile->ETFileInfo;

    /* Get format/subformat */
    ETFileInfo->mpc_version = g_strdup ("MPEG");

    switch (properties->codec ())
    {
    case TagLib::MP4::Properties::AAC:
        ETFileInfo->mpc_profile = g_strdup ("4, AAC");
        break;
    case TagLib::MP4::Properties::ALAC:
        ETFileInfo->mpc_profile = g_strdup ("4, ALAC");
        break;
    default:
        ETFileInfo->mpc_profile = g_strdup ("4, Unknown");
    };

    ETFileInfo->version = 4;
    ETFileInfo->layer = 14;

    ETFileInfo->variable_bitrate = TRUE;
    ETFileInfo->bitrate = properties->bitrate ();
    if (ETFileInfo->bitrate == 1)
        ETFileInfo->bitrate = 0; // avoid unreasonable small bitrates of some files.
    ETFileInfo->samplerate = properties->sampleRate ();
    ETFileInfo->mode = properties->channels ();
    ETFileInfo->duration = properties->lengthInSeconds();


    /* tag metadata */

    MP4::Tag *tag = mp4file.tag();
    if (!tag)
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                     _("Error reading tags from file"));
        return FALSE;
    }

    const PropertyMap& extra_tag = tag->properties ();

    auto fetch_property = [&extra_tag](const char* property) -> const char*
    {   auto it = extra_tag.find(property);
        if (it == extra_tag.end())
            return nullptr;
        return it->second.front().toCString(true);
    };

    File_Tag* FileTag = ETFile->FileTag->data;

    et_file_tag_set_title(FileTag, tag->title().toCString(true));
    et_file_tag_set_subtitle(FileTag, fetch_property("SUBTITLE"));
    et_file_tag_set_artist(FileTag, tag->artist().toCString(true));

    et_file_tag_set_album(FileTag, tag->album().toCString(true));
    et_file_tag_set_disc_subtitle(FileTag, fetch_property("DISCSUBTITLE"));
    et_file_tag_set_album_artist(FileTag, fetch_property("ALBUMARTIST"));

    /* Disc number. */
    /* Total disc number support in TagLib reads multiple disc numbers and
     * joins them with a "/". */
    FileTag->disc_and_total(fetch_property("DISCNUMBER"));
    FileTag->track_and_total(fetch_property("TRACKNUMBER"));

    year = tag->year ();
    if (year != 0)
        FileTag->year = g_strdup_printf ("%u", year);
#if TAGLIB_MAJOR_VERSION >= 2
    et_file_tag_set_release_year(FileTag, fetch_property("RELEASEDATE"));
#endif

    et_file_tag_set_genre(FileTag, tag->genre().toCString(true));
    et_file_tag_set_comment(FileTag, tag->comment().toCString(true));

    const MP4::ItemMap &extra_items = tag->itemMap ();

    /* Description */
#if (TAGLIB_MAJOR_VERSION == 1) && (TAGLIB_MINOR_VERSION < 12)
    /* No "PODCASTDESC" support in TagLib until 1.12; use atom directly. */
    if (extra_items.contains ("desc"))
        et_file_tag_set_description(FileTag, extra_items["desc"].toStringList().front().toCString(true));
#else
    et_file_tag_set_description(FileTag, fetch_property("PODCASTDESC"));
#endif

    et_file_tag_set_composer(FileTag, fetch_property("COMPOSER"));
    et_file_tag_set_copyright(FileTag, fetch_property("COPYRIGHT"));
    et_file_tag_set_encoded_by(FileTag, fetch_property("ENCODEDBY"));

#if TAGLIB_MAJOR_VERSION >= 2
    /**************
     * ReplayGain *
     **************/
    FileTag->track_gain_str(fetch_property("REPLAYGAIN_TRACK_GAIN"));
    FileTag->track_peak_str(fetch_property("REPLAYGAIN_TRACK_PEAK"));
    FileTag->album_gain_str(fetch_property("REPLAYGAIN_ALBUM_GAIN"));
    FileTag->album_peak_str(fetch_property("REPLAYGAIN_ALBUM_PEAK"));
#endif

    /***********
     * Picture *
     ***********/
    if (extra_items.contains ("covr"))
    {
        const MP4::Item &cover = extra_items["covr"];
        const MP4::CoverArtList &covers = cover.toCoverArtList ();
        const MP4::CoverArt &art = covers.front ();

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

    // validate date fields
    ETFile->check_dates(1, false); // Year only

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
    MP4::Tag *tag;

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

#if TAGLIB_MAJOR_VERSION >= 2
    MP4::File mp4file(&stream, true, MP4::Properties::Average, &ItemFactory);
#else
    MP4::File mp4file(&stream);
#endif

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

    PropertyMap fields;

    auto add_field = [&fields](const gchar* value, const char* propertyname)
    {
        if (!et_str_empty(value))
            fields.insert(propertyname, String(value, String::UTF8));
    };

    add_field(FileTag->title, "TITLE");
    add_field(FileTag->subtitle, "SUBTITLE");
    add_field(FileTag->artist, "ARTIST");

    add_field(FileTag->album, "ALBUM");
    add_field(FileTag->disc_subtitle, "DISCSUBTITLE");
    add_field(FileTag->disc_and_total().c_str(), "DISCNUMBER");
  	add_field(FileTag->album_artist, "ALBUMARTIST");

    add_field(FileTag->year, "DATE");

#if TAGLIB_MAJOR_VERSION >= 2
    add_field(FileTag->release_year, "RELEASEDATE");
#endif

    add_field(FileTag->track_and_total().c_str(), "TRACKNUMBER");

    add_field(FileTag->genre, "GENRE");
    add_field(FileTag->comment, "COMMENT");
    add_field(FileTag->composer, "COMPOSER");
    add_field(FileTag->copyright, "COPYRIGHT");
    add_field(FileTag->encoded_by, "ENCODEDBY");

    /* Description. */
    if (!et_str_empty (FileTag->description))
    {
        String string (FileTag->description, String::UTF8);
#if (TAGLIB_MAJOR_VERSION == 1) && (TAGLIB_MINOR_VERSION < 12)
        /* No "PODCASTDESC" support in TagLib until 1.12; use atom directly. */
        extra_items.insert ("desc", MP4::Item (string));
    }
    else
    {
        extra_items.erase ("desc");
#else
        fields.insert ("PODCASTDESC", string);
#endif
    }

#if TAGLIB_MAJOR_VERSION >= 2
    /**************
     * ReplayGain *
     **************/
    add_field(FileTag->track_gain_str().c_str(), "REPLAYGAIN_TRACK_GAIN");
    add_field(FileTag->track_peak_str().c_str(), "REPLAYGAIN_TRACK_PEAK");
    add_field(FileTag->album_gain_str().c_str(), "REPLAYGAIN_ALBUM_GAIN");
    add_field(FileTag->album_peak_str().c_str(), "REPLAYGAIN_ALBUM_PEAK");
#endif

    /***********
     * Picture *
     ***********/
    if (FileTag->picture)
    {
        Picture_Format pf;
        MP4::CoverArt::Format f;
        gconstpointer data;
        gsize data_size;

        pf = Picture_Format_From_Data (FileTag->picture);

        switch (pf)
        {
            case PICTURE_FORMAT_JPEG:
                f = MP4::CoverArt::JPEG;
                break;
            case PICTURE_FORMAT_PNG:
                f = MP4::CoverArt::PNG;
                break;
            case PICTURE_FORMAT_GIF:
                f = MP4::CoverArt::GIF;
                break;
            case PICTURE_FORMAT_UNKNOWN:
            default:
                g_critical ("Unknown format");
                f = MP4::CoverArt::JPEG;
                break;
        }

        data = g_bytes_get_data (FileTag->picture->bytes, &data_size);
        MP4::CoverArt art (f, ByteVector((char *)data,
                                                         data_size));

        tag->setItem("covr",
                            MP4::Item (MP4::CoverArtList ().append (art)));
    }
    else
    {
        tag->removeItem("covr");
    }

    tag->setProperties (fields);

    return mp4file.save();
}

/*
 * et_mp4_header_display_file_info_to_ui:
 *
 * Display header info in the main window
 */
void
et_mp4_header_display_file_info_to_ui (EtFileHeaderFields *fields, const ET_File *ETFile)
{
    const ET_File_Info *info = &ETFile->ETFileInfo;

    /* MPEG, Layer versions */
    if (info->mpc_version)
        fields->version_label = info->mpc_version;
    if (info->mpc_profile)
        fields->version = info->mpc_profile;

    /* Mode */
    /* mpeg4ip library seems to always return -1 */
    fields->mode_label = _("Channels:");

    if (info->mode == -1)
    {
        fields->mode = "Unknown";
    }
    else
    {
        fields->mode = strprintf("%d", info->mode);
    }
}

unsigned mp4tag_unsupported_fields(const ET_File* file)
{
#if TAGLIB_MAJOR_VERSION >= 2
	return ET_COLUMN_VERSION | ET_COLUMN_ORIG_ARTIST | ET_COLUMN_ORIG_YEAR | ET_COLUMN_URL;
#else
	return ET_COLUMN_VERSION | ET_COLUMN_RELEASE_YEAR | ET_COLUMN_ORIG_ARTIST
		| ET_COLUMN_ORIG_YEAR | ET_COLUMN_URL | ET_COLUMN_REPLAYGAIN;
#endif
}

#endif /* ENABLE_MP4 */
