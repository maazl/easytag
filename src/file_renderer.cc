/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022  Marcel Müller <gitlab@maazl.de>
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
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "file_renderer.h"
#include "misc.h"
#include "enums.h"

#include <vector>
#include <utility>
#include <algorithm>
using namespace std;

namespace
{
vector<pair<const char*, const FileColumnRenderer*>> Renderers;

const GdkRGBA LIGHT_BLUE = { 0.866, 0.933, 1.0, 1.0 };
const GdkRGBA RED = { 1., 0., 0., 1.0 };
}

FileColumnRenderer::FileColumnRenderer(EtSortMode col) : Column(col)
{	Renderers.emplace_back(nullptr, this);
	// All renderers are static, no need to remove them
}

void FileColumnRenderer::SetText(GtkCellRendererText* renderer, const gchar* text, bool zebra, Highlight highlight)
{
	gboolean bold = g_settings_get_boolean (MainSettings, "file-changed-bold");
	g_object_set(renderer,
		"text", text,
		"weight", (int)highlight > !bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		"foreground-rgba", (int)highlight > bold ? &RED : NULL,
		"background-rgba", zebra ? &LIGHT_BLUE : NULL,
		NULL);
}

namespace
{
class FileNameColumnRenderer : public FileColumnRenderer
{
protected:
	gchar* File_Name::* const Field;
	virtual string RenderText(const ET_File* file, bool original) const;
public:
	FileNameColumnRenderer(EtSortMode col, gchar* File_Name::* const field)
	:	FileColumnRenderer(col), Field(field) {}
};

class TagColumnRenderer : public FileColumnRenderer
{
protected:
	gchar* File_Tag::* const Field;
	virtual string RenderText(const ET_File* file, bool original) const;
public:
	TagColumnRenderer(EtSortMode col, gchar* File_Tag::* const field)
	:	FileColumnRenderer(col), Field(field) {}
};

class TagPartColumnRenderer : public TagColumnRenderer
{
	gchar* File_Tag::* const Field2;
protected:
	virtual string RenderText(const ET_File* file, bool original) const;
public:
	TagPartColumnRenderer(EtSortMode col, gchar* File_Tag::* const field1, gchar* File_Tag::* const field2)
	:	TagColumnRenderer(col, field1), Field2(field2) {}
};

// Static renderer instances
const FileNameColumnRenderer
	R_Path(ET_SORT_MODE_ASCENDING_FILEPATH, &File_Name::path_value_utf8),
	R_File(ET_SORT_MODE_ASCENDING_FILENAME, &File_Name::file_value_utf8);
const TagColumnRenderer
	R_Title(ET_SORT_MODE_ASCENDING_TITLE, &File_Tag::title),
	R_Artist(ET_SORT_MODE_ASCENDING_ARTIST, &File_Tag::artist),
	R_AlbumArtist(ET_SORT_MODE_ASCENDING_ALBUM_ARTIST, &File_Tag::album_artist),
	R_Album(ET_SORT_MODE_ASCENDING_ALBUM, &File_Tag::album),
	R_Year(ET_SORT_MODE_ASCENDING_YEAR, &File_Tag::year),
	R_ReleaseYear(ET_SORT_MODE_ASCENDING_RELEASE_YEAR, &File_Tag::release_year);
const TagPartColumnRenderer
	R_DiscNumber(ET_SORT_MODE_ASCENDING_DISC_NUMBER, &File_Tag::disc_number, &File_Tag::disc_total),
	R_Track_Number(ET_SORT_MODE_ASCENDING_TRACK_NUMBER, &File_Tag::track, &File_Tag::track_total);
const TagColumnRenderer
	R_Genre(ET_SORT_MODE_ASCENDING_GENRE, &File_Tag::genre),
	R_Comment(ET_SORT_MODE_ASCENDING_COMMENT, &File_Tag::comment),
	R_Composer(ET_SORT_MODE_ASCENDING_COMPOSER, &File_Tag::composer),
	R_OrigArtist(ET_SORT_MODE_ASCENDING_ORIG_ARTIST, &File_Tag::orig_artist),
	R_OrigYear(ET_SORT_MODE_ASCENDING_ORIG_YEAR, &File_Tag::orig_year),
	R_Copyright(ET_SORT_MODE_ASCENDING_COPYRIGHT, &File_Tag::copyright),
	R_Url(ET_SORT_MODE_ASCENDING_URL, &File_Tag::url),
	R_EncodedBy(ET_SORT_MODE_ASCENDING_ENCODED_BY, &File_Tag::encoded_by);

string FileNameColumnRenderer::RenderText(const ET_File* file, bool original) const
{	return EmptfIfNull((original ? file->CurFileName() : file->FileName())->*Field);
}

string TagColumnRenderer::RenderText(const ET_File* file, bool original) const
{	return EmptfIfNull((original ? file->CurTag() : file->Tag())->*Field);
}

string TagPartColumnRenderer::RenderText(const ET_File* file, bool original) const
{	const File_Tag* tag = original ? file->CurTag() : file->Tag();
	const gchar* value = EmptfIfNull(tag->*Field);
	const gchar* value2 = EmptfIfNull(tag->*Field2);
	string svalue;
	if (!et_str_empty(value2))
	{
		if (!et_str_empty(value))
			svalue += value;
		svalue += "/";
		svalue += value2;
		value = svalue.c_str();
	}
	return svalue;
}

}

const FileColumnRenderer* FileColumnRenderer::Get_Renderer(const gchar* column_id)
{
	GEnumClass* ec = (GEnumClass*)g_type_class_ref(ET_TYPE_SORT_MODE);
	typedef pair<const char*, const FileColumnRenderer*> itemtype;

	if (!Renderers[0].first)
	{	// first call => initialize and sort
		for (auto& rdr : Renderers)
			rdr.first = strchr(g_enum_get_value(ec, rdr.second->Column)->value_nick, '-') + 1;
		sort(Renderers.begin(), Renderers.end(),
			[](const itemtype& l, const itemtype& r) { return strcmp(l.first, r.first) < 0; });
	}

	// strip "-column"
	int len = strlen(column_id) - 7;
	string tmp;
	if (len > 0 && memcmp(column_id + len, "-column", 7) == 0)
		column_id = tmp.assign(column_id, len).c_str();

	// binary search renderer
	auto i = lower_bound(Renderers.begin(), Renderers.end(), column_id,
		[](const itemtype& e, const gchar* v) { return strcmp(e.first, v) < 0; });
	const FileColumnRenderer* r = i != Renderers.end() && strcmp(i->first, column_id) == 0
		? i->second : nullptr;

	g_type_class_unref(ec);
	return r;
}
