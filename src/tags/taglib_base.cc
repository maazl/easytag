/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2024  Marcel Müller
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

#include "taglib_base.h"
#if defined(ENABLE_ASF) || defined(ENABLE_MP4)

#include "../file.h"

#include <glib/gi18n.h>
#include <taglib/audioproperties.h>

#include <string>
#include <limits>
using namespace std;
using namespace TagLib;

static gString Newline(g_strdup("\n"));

string taglib_fetch_property(const PropertyMap& fields, gString* delimiter, const char* property)
{	auto it = fields.find(property);
	string res;
	if (it != fields.end())
		for (String value : it->second)
		{	if (res.size()) // delimiter required?
			{	// on reading the delimiter is always applied.
				if (!delimiter)
					break;
				if (!*delimiter)
					delimiter->reset(g_settings_get_string(MainSettings, "split-delimiter"));
				res += *delimiter;
				res += value.to8Bit(true);
			} else
				res = value.to8Bit(true);
		}
	return res;
};

gboolean taglib_read_tag(const TagLib::File& tfile, ET_File *ETFile, GError **error)
{
	/* ET_File_Info header data */
	const AudioProperties* properties = tfile.audioProperties();
	if (properties == NULL)
	{	g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
			_("Error reading properties from file"));
		return FALSE;
	}

	ET_File_Info* ETFileInfo = &ETFile->ETFileInfo;

	ETFileInfo->bitrate = properties->bitrate() * 1000;
	ETFileInfo->samplerate = properties->sampleRate();
	ETFileInfo->mode = properties->channels();
	ETFileInfo->duration = properties->lengthInSeconds();
	if (ETFileInfo->duration < numeric_limits<int>::max() / 1000.)
		ETFileInfo->duration = properties->lengthInMilliseconds() / 1000.; // try more precisely

	/* tag metadata */
	Tag *tag = tfile.tag();
	if (!tag)
	{	g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
			_("Error reading tags from file"));
		return FALSE;
	}

	const PropertyMap& extra_tag = tag->properties();
	gString delimiter;

	auto fetch_property = [&extra_tag, &delimiter](const char* property) -> string
	{ return taglib_fetch_property(extra_tag, &delimiter, property);
	};

	File_Tag* FileTag = ETFile->FileTag->data;
	// the taglib tags are basically a dictionary, so querying keys
	// that do not exist for a certain tag type is just a no-op.
	FileTag->title.assignNFC(tag->title().toCString(true));
	FileTag->subtitle.assignNFC(fetch_property("SUBTITLE"));
	FileTag->artist.assignNFC(tag->artist().toCString(true));

	FileTag->album.assignNFC(tag->album().toCString(true));
	FileTag->disc_subtitle.assignNFC(fetch_property("DISCSUBTITLE"));
	FileTag->album_artist.assignNFC(fetch_property("ALBUMARTIST"));

	/* Disc number. */
	/* Total disc number support in TagLib reads multiple disc numbers and
	 * joins them with a "/". */
	FileTag->disc_and_total(fetch_property("DISCNUMBER").c_str());
	FileTag->track_and_total(fetch_property("TRACKNUMBER").c_str());

	FileTag->year.assignNFC(fetch_property("DATE"));
	FileTag->release_year.assignNFC(fetch_property("RELEASEDATE"));

	FileTag->genre.assignNFC(tag->genre().toCString(true));
	if (g_settings_get_boolean(MainSettings, "tag-multiline-comment"))
		FileTag->comment.assignNFC(taglib_fetch_property(extra_tag, &Newline, "COMMENT"));
	else
		FileTag->comment.assignNFC(tag->comment().toCString(true));
	FileTag->description.assignNFC(fetch_property("PODCASTDESC"));

	FileTag->orig_artist.assignNFC(fetch_property("ORIGINALARTIST"));
	FileTag->orig_year.assignNFC(fetch_property("ORIGINALDATE"));
	FileTag->composer.assignNFC(fetch_property("COMPOSER"));
	FileTag->copyright.assignNFC(fetch_property("COPYRIGHT"));
	FileTag->encoded_by.assignNFC(fetch_property("ENCODEDBY"));

	return TRUE;
}

void taglib_set_property(TagLib::PropertyMap& fields, gString* delimiter, const char* property, const char* value)
{
	fields.erase(property);

	if (et_str_empty(value))
		return;

	if (!delimiter)
	{nodelim:
		fields.insert(property, String(value, String::UTF8));
		return;
	}

	if (!*delimiter)
		delimiter->reset(g_settings_get_string(MainSettings, "split-delimiter"));

	// search for delimiter
	const char* p = strstr(value, *delimiter);
	if (!p)
		goto nodelim;

	StringList list;
	size_t delimlen = strlen(*delimiter);
	do
	{	list.append(String(string(value, p - value), String::UTF8));
		value = p + delimlen;
		p = strstr(value, *delimiter);
	} while (p);
	list.append(String(value, String::UTF8)); // last fragment

	fields.insert(property, list);
}

void taglib_write_file_tag(TagLib::PropertyMap& fields, const ET_File *ETFile, unsigned split_fields)
{
	const File_Tag *FileTag = ETFile->FileTag->data;
	const unsigned supported = ~ETFile->ETFileDescription->unsupported_fields(ETFile);
	gString delimiter;

	auto add_field = [&fields, supported, split_fields, &delimiter](const gchar* value, const char* propertyname, EtColumn col)
	{	if (supported & col)
			taglib_set_property(fields, split_fields & col ? &delimiter : nullptr, propertyname, value);
	};

	add_field(FileTag->title, "TITLE", ET_COLUMN_TITLE);
	add_field(FileTag->subtitle, "SUBTITLE", ET_COLUMN_SUBTITLE);
	add_field(FileTag->artist, "ARTIST", ET_COLUMN_ARTIST);

	add_field(FileTag->album, "ALBUM", ET_COLUMN_ALBUM);
	add_field(FileTag->disc_subtitle, "DISCSUBTITLE", ET_COLUMN_DISC_SUBTITLE);
	add_field(FileTag->disc_and_total().c_str(), "DISCNUMBER", ET_COLUMN_DISC_NUMBER);
	add_field(FileTag->album_artist, "ALBUMARTIST", ET_COLUMN_ALBUM_ARTIST);

	add_field(FileTag->year, "DATE", ET_COLUMN_YEAR);
	add_field(FileTag->release_year, "RELEASEDATE", ET_COLUMN_RELEASE_YEAR);

	add_field(FileTag->track_and_total().c_str(), "TRACKNUMBER", ET_COLUMN_TRACK_NUMBER);

	add_field(FileTag->genre, "GENRE", ET_COLUMN_GENRE);

	if ((split_fields & ET_COLUMN_COMMENT) && g_settings_get_boolean(MainSettings, "tag-multiline-comment"))
		taglib_set_property(fields, &Newline, "COMMENT", FileTag->comment);
	else
		add_field(FileTag->comment, "COMMENT", ET_COLUMN_COMMENT);

	add_field(FileTag->orig_artist, "ORIGINALARTIST", ET_COLUMN_ORIG_ARTIST);
	add_field(FileTag->orig_year, "ORIGINALDATE", ET_COLUMN_ORIG_YEAR);
	add_field(FileTag->composer, "COMPOSER", ET_COLUMN_COMPOSER);
	add_field(FileTag->copyright, "COPYRIGHT", ET_COLUMN_COPYRIGHT);
	add_field(FileTag->encoded_by, "ENCODEDBY", ET_COLUMN_ENCODED_BY);

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
}

#endif
