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
#include "charset.h"
#include "gio_wrapper.h"
#include "file.h"
#include "file_description.h"
#include "taglib_base.h"
#include "file_name.h"
#include "file_tag.h"

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
M4B_Description(".m4b", _("MPEG4 File")),
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
File_Tag* mp4_read_file(GFile *file, ET_File *ETFile, GError **error)
{
    g_return_val_if_fail (file != NULL && ETFile != NULL, FALSE);

    /* Get data from tag. */
    GIO_InputStream stream (file);

    if (!stream.isOpen ())
    {
        const GError *tmp_error = stream.getError ();
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Error while opening file: %s"), tmp_error->message);
        return nullptr;
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

        return nullptr;
    }

    // base processing
    File_Tag* FileTag = taglib_read_tag(mp4file, ETFile, error);
    if (!FileTag)
        return nullptr;

    // additional info's for MP4
    ET_File_Info* ETFileInfo = &ETFile->ETFileInfo;

    switch (mp4file.audioProperties()->codec())
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
    if (ETFileInfo->bitrate == 1)
        ETFileInfo->bitrate = 0; // avoid unreasonable small bitrates of some files.

    /* tag metadata */
    MP4::Tag *tag = mp4file.tag();

    const MP4::ItemMap &extra_items = tag->itemMap ();

    /* Description */
#if (TAGLIB_MAJOR_VERSION == 1) && (TAGLIB_MINOR_VERSION < 12)
    /* No "PODCASTDESC" support in TagLib until 1.12; use atom directly. */
    if (extra_items.contains ("desc"))
        et_file_tag_set_description(FileTagNew, extra_items["desc"].toStringList().front().toCString(true));
#endif

#if TAGLIB_MAJOR_VERSION >= 2
    const PropertyMap& fields = tag->properties();
    /**************
     * ReplayGain *
     **************/
    FileTag->track_gain_str(taglib_fetch_property(fields, nullptr, "REPLAYGAIN_TRACK_GAIN").c_str());
    FileTag->track_peak_str(taglib_fetch_property(fields, nullptr, "REPLAYGAIN_TRACK_PEAK").c_str());
    FileTag->album_gain_str(taglib_fetch_property(fields, nullptr, "REPLAYGAIN_ALBUM_GAIN").c_str());
    FileTag->album_peak_str(taglib_fetch_property(fields, nullptr, "REPLAYGAIN_ALBUM_PEAK").c_str());
#endif

    /***********
     * Picture *
     ***********/
    // TODO: since TagLib 2.0 an API to access pictures is available.
    if (extra_items.contains ("covr"))
    {
        const MP4::Item &cover = extra_items["covr"];
        const MP4::CoverArtList &covers = cover.toCoverArtList ();
        const MP4::CoverArt &art = covers.front ();

        /* MP4 does not support image types, nor descriptions. */
        FileTag->pictures.emplace_back(ET_PICTURE_TYPE_FRONT_COVER, nullptr,
            0, 0, art.data().data(), art.data().size());
    }
    else
    {
        FileTag->pictures.clear();
    }

    // validate date fields
    FileTag->check_dates(1, false, *ETFile->FileNameCur()); // Year only

    return FileTag;
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
    MP4::Tag *tag;

    g_return_val_if_fail (ETFile != NULL && ETFile->FileTagNew() != NULL, FALSE);

    const File_Tag* FileTag = ETFile->FileTagNew();
    const File_Name& filename = *ETFile->FileNameCur();

    /* Open file for writing */
    GFile *file = g_file_new_for_path(ETFile->FilePath);
    GIO_IOStream stream (file);
    g_object_unref (file);

    if (!stream.isOpen ())
    {
        const GError *tmp_error = stream.getError ();
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Error while opening file ‘%s’: %s"), filename.full_name().get(),
                     tmp_error->message);
        return FALSE;
    }

#if TAGLIB_MAJOR_VERSION >= 2
    MP4::File mp4file(&stream, false, MP4::Properties::Average, &ItemFactory);
#else
    MP4::File mp4file(&stream, false);
#endif

    if (!mp4file.isOpen ())
    {
        const GError *tmp_error = stream.getError ();

        if (tmp_error)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         _("Error while opening file ‘%s’: %s"), filename.full_name().get(),
                         tmp_error->message);
        }
        else
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         _("Error while opening file ‘%s’: %s"), filename.full_name().get(),
                         _("MP4 format invalid"));
        }

        return FALSE;
    }

    if (!(tag = mp4file.tag ()))
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Error reading tags from file ‘%s’"), filename.full_name().get());
        return FALSE;
    }

    // main processing in generic base implementation
    PropertyMap fields = tag->properties();
    taglib_write_file_tag(fields, ETFile, 0);

#if TAGLIB_MAJOR_VERSION >= 2
    /**************
     * ReplayGain *
     **************/
    taglib_set_property(fields, nullptr, "REPLAYGAIN_TRACK_GAIN", FileTag->track_gain_str().c_str());
    taglib_set_property(fields, nullptr, "REPLAYGAIN_TRACK_PEAK", FileTag->track_peak_str().c_str());
    taglib_set_property(fields, nullptr, "REPLAYGAIN_ALBUM_GAIN", FileTag->album_gain_str().c_str());
    taglib_set_property(fields, nullptr, "REPLAYGAIN_ALBUM_PEAK", FileTag->album_peak_str().c_str());
#endif

    /***********
     * Picture *
     ***********/
    if (FileTag->pictures.size())
    {
        const EtPicture& pic = FileTag->pictures.front();

        MP4::CoverArt::Format f;
        switch (pic.Format())
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

        MP4::CoverArt art(f, ByteVector((char*)pic.storage->Bytes, pic.storage->Size));
        tag->setItem("covr", MP4::Item(MP4::CoverArtList().append(art)));
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
    fields->version_label = "MPEG";
    if (info->mpc_profile)
        fields->version = info->mpc_profile;

    /* Mode */
    /* mpeg4ip library seems to always return -1 */
    fields->mode_label = _("Channels:");

    if (info->mode == -1)
        fields->mode = "Unknown";
    else
        fields->mode = strprintf("%d", info->mode);
}

unsigned mp4tag_unsupported_fields(const ET_File* file)
{
#if TAGLIB_MAJOR_VERSION >= 2
	return ET_COLUMN_VERSION | ET_COLUMN_ORIG_ARTIST | ET_COLUMN_URL; // fields not supported by MP4
#elif TAGLIB_MAJOR_VERSION == 1 && TAGLIB_MINOR_VERSION < 12
	return ET_COLUMN_VERSION | ET_COLUMN_RELEASE_YEAR | ET_COLUMN_ORIG_ARTIST
		| ET_COLUMN_ORIG_YEAR | ET_COLUMN_URL | ET_COLUMN_REPLAYGAIN;
#else
	return ET_COLUMN_VERSION | ET_COLUMN_RELEASE_YEAR | ET_COLUMN_ORIG_ARTIST
		| ET_COLUMN_URL | ET_COLUMN_REPLAYGAIN;
#endif
}

#endif /* ENABLE_MP4 */
