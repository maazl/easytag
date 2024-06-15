/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022-2024  Marcel Müller <github@maazl.de>
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
#include <cmath>
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
class GenericColumnRenderer : public FileColumnRenderer
{
protected:
	string (*const Getter)(const File_Name* file_name);
	virtual string RenderText(const ET_File* file, bool original) const;
public:
	GenericColumnRenderer(EtSortMode col, string (*const getter)(const File_Name* file_name))
	:	FileColumnRenderer(col), Getter(getter) {}
};

class TagColumnRenderer : public FileColumnRenderer
{
protected:
	xString0 File_Tag::* const Field;
	virtual string RenderText(const ET_File* file, bool original) const;
public:
	TagColumnRenderer(EtSortMode col, xString0 File_Tag::* const field)
	:	FileColumnRenderer(col), Field(field) {}
};

class TagPartColumnRenderer : public TagColumnRenderer
{
	xString0 File_Tag::* const Field2;
protected:
	virtual string RenderText(const ET_File* file, bool original) const;
public:
	TagPartColumnRenderer(EtSortMode col, xString0 File_Tag::* const field1, xString0 File_Tag::* const field2)
	:	TagColumnRenderer(col, field1), Field2(field2) {}
};

class TagReplaygainRenderer : public FileColumnRenderer
{
protected:
	virtual string RenderText(const ET_File* file, bool original) const;
public:
	TagReplaygainRenderer(EtSortMode col)
	: FileColumnRenderer(col) {}
};

class FileSizeColumnRenderer : public FileColumnRenderer
{
protected:
	virtual string RenderText(const ET_File* file, bool original) const;
public:
	FileSizeColumnRenderer(EtSortMode col)
	:	FileColumnRenderer(col) {}
};

class FileDurationColumnRenderer : public FileColumnRenderer
{
protected:
	virtual string RenderText(const ET_File* file, bool original) const;
public:
	FileDurationColumnRenderer(EtSortMode col)
	:	FileColumnRenderer(col) {}
};

class FileInfoIntColumnRenderer : public FileColumnRenderer
{
protected:
	gint ET_File_Info::* const Field;
	virtual string RenderText(const ET_File* file, bool original) const;
public:
	FileInfoIntColumnRenderer(EtSortMode col, gint ET_File_Info::* const field)
	:	FileColumnRenderer(col), Field(field) {}
};

class BitrateColumnRenderer : FileInfoIntColumnRenderer
{
protected:
	virtual string RenderText(const ET_File* file, bool original) const;
public:
	BitrateColumnRenderer(EtSortMode col)
	:	FileInfoIntColumnRenderer(col, &ET_File_Info::bitrate) {}
};

// Static renderer instances
const GenericColumnRenderer
	R_Path(ET_SORT_MODE_ASCENDING_FILEPATH, [](const File_Name* file_name) { return file_name->path_value_utf8(); }),
	R_File(ET_SORT_MODE_ASCENDING_FILENAME, [](const File_Name* file_name) { return string(file_name->file_value_utf8()); });
const TagColumnRenderer
	R_Title(ET_SORT_MODE_ASCENDING_TITLE, &File_Tag::title),
	R_Version(ET_SORT_MODE_ASCENDING_VERSION, &File_Tag::version),
	R_Subtitle(ET_SORT_MODE_ASCENDING_SUBTITLE, &File_Tag::subtitle),
	R_Artist(ET_SORT_MODE_ASCENDING_ARTIST, &File_Tag::artist),
	R_AlbumArtist(ET_SORT_MODE_ASCENDING_ALBUM_ARTIST, &File_Tag::album_artist),
	R_Album(ET_SORT_MODE_ASCENDING_ALBUM, &File_Tag::album),
	R_DiscSubtitle(ET_SORT_MODE_ASCENDING_DISC_SUBTITLE, &File_Tag::disc_subtitle),
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
const FileSizeColumnRenderer R_FileSize(ET_SORT_MODE_ASCENDING_FILE_SIZE);
const FileDurationColumnRenderer R_FileDuration(ET_SORT_MODE_ASCENDING_FILE_DURATION);
const BitrateColumnRenderer	R_Bitrate(ET_SORT_MODE_ASCENDING_FILE_BITRATE);
const FileInfoIntColumnRenderer
	R_Samplerate(ET_SORT_MODE_ASCENDING_FILE_SAMPLERATE, &ET_File_Info::samplerate);
const TagReplaygainRenderer R_Replaygain(ET_SORT_MODE_ASCENDING_REPLAYGAIN);

string GenericColumnRenderer::RenderText(const ET_File* file, bool original) const
{	return Getter((original ? file->FileNameCur : file->FileNameNew)->data);
}

string TagColumnRenderer::RenderText(const ET_File* file, bool original) const
{	string s = ((original ? file->FileTagCur : file->FileTag)->data->*Field).get();
	size_t pos = s.find('\n');
	if (pos != string::npos && g_settings_get_boolean(MainSettings, "browse-limit-lines"))
	{	guint count = g_settings_get_uint(MainSettings, "browse-max-lines");
		while (--count)
		{	pos = s.find('\n', pos + 1);
			if (pos == string::npos)
				goto done;
		}
		if (pos && s[pos - 1] == '\r')
			--pos;
		s.erase(pos).append("\xE2\x80\xA6"); // UTF-8 ellipsis
	}
done:
	return s;
}

string TagPartColumnRenderer::RenderText(const ET_File* file, bool original) const
{	const File_Tag* tag = (original ? file->FileTagCur : file->FileTag)->data;
	string svalue = (tag->*Field).get();
	const gchar* value2 = tag->*Field2;
	if (!et_str_empty(value2))
	{	svalue += "/";
		svalue += value2;
	}
	return svalue;
}

string FileSizeColumnRenderer::RenderText(const ET_File* file, bool original) const
{	goffset size = file->FileSize;
	if (!size)
		return string();
	char buf[20];
	if (size >= 1024LL*1024*1024*1024)
		sprintf(buf, "%.1f T", size / (1024.*1024.*1024.*1024.));
	else if (size >= 1024*1024*1024)
		sprintf(buf, "%.1f G", size / (1024.*1024.*1024.));
	else if (size >= 1024*1024)
		sprintf(buf, "%.1f M", size / (1024.*1024.));
	else if (size >= 1024)
		sprintf(buf, "%.1f k", size / 1024.);
	else
		return to_string((int)size);
	return buf;
}

string FileDurationColumnRenderer::RenderText(const ET_File* file, bool original) const
{	gint duration = file->ETFileInfo.duration;
	if (!duration)
		return string();
	char buf[20];
	if (duration > 86400)
		sprintf(buf, "%i %02i:%02i:%02i", duration /86400, duration / 3600 % 24, duration / 60 % 60, duration % 60);
	else if (duration > 3600)
		sprintf(buf, "%i:%02i:%02i", duration / 3600, duration / 60 % 60, duration % 60);
	else
		sprintf(buf, "%i:%02i", duration / 60, duration % 60);
	return buf;
}

string FileInfoIntColumnRenderer::RenderText(const ET_File* file, bool original) const
{	gint value = file->ETFileInfo.*Field;
	if (!value)
		return string();
	return to_string(value);
}

string BitrateColumnRenderer::RenderText(const ET_File* file, bool original) const
{	string ret;
	gint value = file->ETFileInfo.*Field;
	if (!value)
		return ret;
	ret = to_string((value + 500) / 1000);
	if (file->ETFileInfo.variable_bitrate)
		ret.insert(0, 1, '~');
	return ret;
}

string TagReplaygainRenderer::RenderText(const ET_File* file, bool original) const
{	const File_Tag* tag = (original ? file->FileTagCur : file->FileTag)->data;
	char buf[40];
	char* dp = buf;
	if (isfinite(tag->track_gain))
		dp += sprintf(dp, "% .1f dB ", tag->track_gain);
	if (isfinite(tag->track_peak))
		dp += sprintf(dp, "(%.2f) ", tag->track_peak);
	if (isfinite(tag->album_gain) || isfinite(tag->album_peak))
	{	*dp++ = '[';
		if (isfinite(tag->album_gain))
			dp += sprintf(dp, "%.1f dB ", tag->album_gain);
		if (isfinite(tag->album_peak))
			dp += sprintf(dp, "(%.2f) ", tag->album_peak);
		dp[-1] = ']';
	} else if (dp != buf && dp[-1] == ' ')
		--dp;
	*dp = '\0';
	return buf;
}

} // namespace

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

	// binary search renderer
	auto i = lower_bound(Renderers.begin(), Renderers.end(), column_id,
		[](const itemtype& e, const gchar* v) { return strcmp(e.first, v) < 0; });
	const FileColumnRenderer* r = i != Renderers.end() && strcmp(i->first, column_id) == 0
		? i->second : nullptr;

	g_type_class_unref(ec);
	return r;
}

void FileColumnRenderer::ShowHideColumns(GtkTreeView* view, EtColumn columns)
{	GFlagsClass *enum_class = (GFlagsClass*)g_type_class_ref(ET_TYPE_COLUMN);
	for (guint i = 0; i < gtk_tree_view_get_n_columns(view); i++)
	{
		GtkTreeViewColumn *column = gtk_tree_view_get_column(view, i);
		string id = ColumnName2Nick(GTK_BUILDABLE(column));
		GFlagsValue* enum_value = g_flags_get_value_by_nick(enum_class, id.c_str());
		if (enum_value == NULL)
			g_warning("No column with name %s found.", id.c_str());
		else
			gtk_tree_view_column_set_visible(column, (columns & enum_value->value) != 0);
	}
	g_type_class_unref(enum_class);
}

string FileColumnRenderer::ColumnName2Nick(GtkBuildable* buildable)
{	const char* name = gtk_buildable_get_name(buildable);
	// strip "_column"
	int len = strlen(name) - 7;
	if (len <= 0 || memcmp(name + len, "_column", 7) != 0)
		len += 7;
	string tmp(name, len);
	// Replace '_' by '-'
	for (char& c : tmp)
		if (c == '_')
			c = '-';
	return tmp;
}
