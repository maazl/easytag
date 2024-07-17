/* EasyTAG - tag editor for audio files
 * Copyright (C) 2024 Marcel Müller
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

#include "config.h" // For definition of ENABLE_ASF

#ifdef ENABLE_ASF

#include "asf_tag.h"
#include "taglib_base.h"
#include "../file.h"
#include "../picture.h"
#include "gio_wrapper.h"

#include <glib/gi18n.h>

/* Shadow warning in public TagLib headers. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#ifdef G_OS_WIN32
#undef NOMINMAX /* Warning in TagLib headers, fixed in git. */
#endif /* G_OS_WIN32 */
#include <taglib/asffile.h>

using namespace std;
using namespace TagLib;


// registration
const struct Asf_Description : ET_File_Description
{	Asf_Description(const char* extension, const char* description)
	{	Extension = extension;
		FileType = description;
		TagType = _("MP4/QuickTime Tag");
		read_file = asf_read_file;
		write_file_tag = asftag_write_file_tag;
		display_file_info_to_ui = et_asf_header_display_file_info_to_ui;
		unsupported_fields = asftag_unsupported_fields;
		support_multiple_pictures = [](const ET_File*) { return false; };
	}
}
WMA_Description(".wma", _("Windows Media File")),
ASF_Description(".asf", _("ASF File"));


gboolean asf_read_file(GFile *file, ET_File *ETFile, GError **error)
{
	g_return_val_if_fail (file != NULL && ETFile != NULL, FALSE);

	/* Get data from tag. */
	GIO_InputStream stream(file);

	if (!stream.isOpen())
	{	const GError *tmp_error = stream.getError();
		g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			_("Error while opening file: %s"), tmp_error->message);
		return FALSE;
	}

	ASF::File asffile(&stream);

	if (!asffile.isOpen())
	{	const GError *tmp_error = stream.getError();
		g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			_("Error while opening file: %s"), tmp_error ? tmp_error->message : _("ASF format invalid"));
		return FALSE;
	}

	// base processing
	if (!taglib_read_tag(asffile, ETFile, error))
		return FALSE;

  /* ET_File_Info header data */
	const ASF::Properties* properties = asffile.audioProperties();

	ET_File_Info* ETFileInfo = &ETFile->ETFileInfo;

	switch (properties->codec())
	{
	case ASF::Properties::WMA1:
		ETFileInfo->mpc_profile = g_strdup("WMA 1");
		break;
	case ASF::Properties::WMA2:
		ETFileInfo->mpc_profile = g_strdup("WMA 2");
		break;
	case ASF::Properties::WMA9Pro:
		ETFileInfo->mpc_profile = g_strdup("WMA 9 Pro");
		break;
	case ASF::Properties::WMA9Lossless:
		ETFileInfo->mpc_profile = g_strdup("WMA 9 Lossless");
		break;
	default:
		ETFileInfo->mpc_profile = g_strdup("Unknown");
	};

	ETFileInfo->variable_bitrate = TRUE;

	/* tag metadata */
	ASF::Tag *tag = asffile.tag();

	auto fetch_string = [tag](const char* property, const char* property2) -> const char*
	{	ASF::AttributeList list = tag->attribute(property);
		if (!list.size())
		{	if (!property2)
				return nullptr;
			list = tag->attribute(property2);
			if (!list.size())
				return nullptr;
		}
		return list[0].toString().toCString(true);
	};

	File_Tag* FileTag = ETFile->FileTag->data;

	// tolerate case differences
	FileTag->track_gain_str(fetch_string("REPLAYGAIN_TRACK_GAIN", "replaygain_track_gain"));
	FileTag->track_peak_str(fetch_string("REPLAYGAIN_TRACK_PEAK", "replaygain_track_peak"));
	FileTag->album_gain_str(fetch_string("REPLAYGAIN_ALBUM_GAIN", "replaygain_album_gain"));
	FileTag->album_peak_str(fetch_string("REPLAYGAIN_ALBUM_PEAK", "replaygain_album_peak"));

	// picture
	const ASF::AttributeList pictures = tag->attribute("WM/Picture");
	EtPicture** pic = &FileTag->picture;
	for (const ASF::Attribute &attr : pictures)
	{	const ASF::Picture& picture = attr.toPicture();
		const ByteVector& data = picture.picture();
		*pic = et_picture_new(
			(EtPictureType)picture.type(), // EtPictureType and ASF::Picture::Type are compatible
			picture.description().toCString(true),
			0, 0,
			g_bytes_new(data.data(), data.size()));
		pic = &(*pic)->next;
	}

	return TRUE;
}

gboolean asftag_write_file_tag (const ET_File *ETFile, GError **error)
{
	g_return_val_if_fail (ETFile != NULL && ETFile->FileTag != NULL, FALSE);

	const gchar *filename_utf8 = ETFile->FileNameCur->data->value_utf8;

	/* Open file for writing */
	GFile *file = g_file_new_for_path(ETFile->FileNameCur->data->value);
	GIO_IOStream stream (file);
	g_object_unref (file);
	if (!stream.isOpen())
	{	const GError *tmp_error = stream.getError();
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			_("Error while opening file ‘%s’: %s"), filename_utf8,
			tmp_error->message);
		return FALSE;
	}

	ASF::File asffile(&stream, false);
	if (!asffile.isOpen())
	{	const GError *tmp_error = stream.getError();
		g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			_("Error while opening file ‘%s’: %s"), filename_utf8,
			tmp_error ? tmp_error->message : _("ASF format invalid"));
		return FALSE;
	}

	guint split_fields = g_settings_get_flags(MainSettings, "ogg-split-fields")
		&	(ET_COLUMN_ALBUM_ARTIST|ET_COLUMN_COMPOSER|ET_COLUMN_GENRE);

	// main processing in generic base implementation
	ASF::Tag* tag = asffile.tag();
	if (!tag)
	{	g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			_("Error reading tags from file ‘%s’"), filename_utf8);
		return FALSE;
	}

	PropertyMap fields = tag->properties();
	taglib_write_file_tag(fields, ETFile, split_fields);

	const File_Tag *FileTag = ETFile->FileTag->data;

	fields.erase("WM/Track"); // remove deprecated track information that might not match WM/TrackNumber

	// ReplayGain
	auto set_string = [tag](const char* property, const char* property2, const string& value)
	{	tag->removeItem(property);
		tag->removeItem(property2);
		if (value.size())
			tag->setAttribute(property, ASF::Attribute(value));
	};
	set_string("REPLAYGAIN_TRACK_GAIN", "replaygain_track_gain", FileTag->track_gain_str());
	set_string("REPLAYGAIN_TRACK_PEAK", "replaygain_track_peak", FileTag->track_peak_str());
	set_string("REPLAYGAIN_ALBUM_GAIN", "replaygain_album_gain", FileTag->album_gain_str());
	set_string("REPLAYGAIN_ALBUM_PEAK", "replaygain_album_peak", FileTag->album_peak_str());

	// Pictures
	tag->removeItem("WM/Picture");
	EtPicture* pic = FileTag->picture;
	while (pic)
	{	ASF::Picture picture;
		picture.setType((ASF::Picture::Type)pic->type); // EtPictureType and ASF::Picture::Type are compatible
		picture.setMimeType(Picture_Mime_Type_String(Picture_Format_From_Data(pic)));
		unsigned long data_size;
		const char* data = (const char*)g_bytes_get_data(pic->bytes, &data_size);
		picture.setPicture(ByteVector(data, data_size));
		picture.setDescription(pic->description);

		tag->setAttribute("WM/Picture", ASF::Attribute(picture));
		pic = pic->next;
	}

	tag->setProperties(fields);

	return asffile.save();
}

void et_asf_header_display_file_info_to_ui(EtFileHeaderFields *fields, const ET_File *ETFile)
{
	const ET_File_Info *info = &ETFile->ETFileInfo;

	/* MPEG, Layer versions */
	fields->version_label = "ASF";
	if (info->mpc_profile)
		fields->version = info->mpc_profile;

	/* Mode */
	fields->mode_label = _("Channels:");
	fields->mode = strprintf("%d", info->mode);
}

unsigned asftag_unsupported_fields(const ET_File* file)
{
#if TAGLIB_MAJOR_VERSION >= 2
	return ET_COLUMN_VERSION | ET_COLUMN_RELEASE_YEAR | ET_COLUMN_COPYRIGHT | ET_COLUMN_URL; // fields not supported by ASF
#else
	return ET_COLUMN_VERSION | ET_COLUMN_RELEASE_YEAR | ET_COLUMN_ORIG_ARTIST | ET_COLUMN_COPYRIGHT | ET_COLUMN_URL;
#endif
}

#endif
