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
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "mask.h"
#include "misc.h"
#include "file_name.h"

#include <glib/gi18n.h>

#include <string>
#include <limits>
using namespace std;

/// Placeholder mapping table. MUST BE ORDERED BY FIRST FIELD!
static const std::pair<char, xStringD0 File_Tag::*> fieldmap[] =
{	{	'A', &File_Tag::album_artist }
,	{	'D', &File_Tag::disc_total }
,	{	'N', &File_Tag::track_total }
,	{	'S', &File_Tag::disc_subtitle }
,	{	'T', &File_Tag::album }
,	{	'Y', &File_Tag::release_year }
,	{	'a', &File_Tag::artist }
,	{	'b', &File_Tag::album }
,	{	'c', &File_Tag::comment }
,	{	'd', &File_Tag::disc_number }
,	{	'e', &File_Tag::encoded_by }
,	{	'g', &File_Tag::genre }
,	{	'l', &File_Tag::track_total } // for compatibility with earlier versions
,	{	'n', &File_Tag::track }
,	{	'o', &File_Tag::orig_artist }
,	{	'p', &File_Tag::composer }
,	{	'r', &File_Tag::copyright }
,	{	's', &File_Tag::subtitle }
,	{	't', &File_Tag::title }
,	{	'u', &File_Tag::url }
,	{	'v', &File_Tag::version }
,	{	'w', &File_Tag::orig_year }
,	{	'x', &File_Tag::disc_total } // for compatibility with earlier versions
,	{	'y', &File_Tag::year }
,	{	'z', &File_Tag::album_artist } // for compatibility with earlier versions
};

xStringD0 File_Tag::* et_mask_field(char code)
{
	auto res = binary_find(fieldmap, fieldmap + G_N_ELEMENTS(fieldmap), code,
		[](const std::pair<char, xStringD0 File_Tag::*>& l, char r) { return l.first - (int)r; });
	return res.second ? res.first->second : nullptr;
}

/* Keep up to date with the format specifiers shown in the UI. */
static const gchar allowed_specifiers[] = "abcdegilnoprtuxyzADNTY%{|}";

class MaskEvaluator
{	const ET_File& File;
	void (*Postprocess)(string& val, unsigned start);
	string Result;
	string Error;
	string TagFieldFromMaskCode(gchar code);
	bool EvaluateRecursive(const gchar*& mask, bool enabled);
public:
	MaskEvaluator(const ET_File &file, void (*postprocess)(string& val, unsigned start) = nullptr)
	: File(file), Postprocess(postprocess) {}
	static string Validate(const gchar* mask);
	const string& Evaluate(const gchar* mask) { EvaluateRecursive(mask, true); return Result; }
};

string MaskEvaluator::TagFieldFromMaskCode(gchar code)
{	const File_Tag *tag = File.FileTagNew();
	size_t maxlen = numeric_limits<size_t>::max();
	switch (code)
	{
	case 'y': /* Year */
	case 'Y': /* Release year */
	case 'w': /* Original year */
		maxlen = 4;
	default:
		{	xStringD0 File_Tag::*field = et_mask_field(code);
			if (field)
			{	const xStringD0& r = tag->*field;
				string ret;
				if (r)
					ret.assign(r, min(maxlen, strlen(r)));
				return ret;
			}
		}
	case 'i': /* Ignored */
		return string();
	case '%': /* escape sequence */
	case '|':
	case '{':
	case '}':
		return string(1, code);
	}
}

bool MaskEvaluator::EvaluateRecursive(const gchar*& mask, bool enabled)
{	size_t old_len = Result.length();
	bool old_enabled = enabled;
	bool success = enabled;
	for (; true; ++mask)
	{
		switch (*mask)
		{
		case '\0':
		case '}':
			return success;

		case '%': // placeholder
		{	if (!*++mask)
				return success;
			if (!enabled)
				break;
			string field = TagFieldFromMaskCode(*mask);
			if (field.empty())	// placeholder empty => fail
			{	enabled = success = false;
				Result.erase(old_len);
				break;
			}
			size_t start = Result.length();
			Result += field;
			if (Postprocess)
				Postprocess(Result, start);
			break;
		}
		case '|': // alternation => process next
			enabled = old_enabled & !success;
			if (enabled)
				success = old_enabled;
			break;
		case '{': // begin group
		{	if (!EvaluateRecursive(++mask, enabled) && enabled)
			{	enabled = success = false;
				Result.erase(old_len);
			}
			if (*mask == '\0')
				return success;
			break;
		}
		case G_DIR_SEPARATOR:
			// Handle additional invalid characters at end of path component
			if (Postprocess && Result.length() && (Result.back() == ' ' || Result.back() == '.'))
				Result.erase(Result.length() - 1);
		default:
			if (enabled)
				Result += *mask;
		}
	}
}

string MaskEvaluator::Validate(const gchar* mask)
{
	int braces = 0;
	for (; true; ++mask)
	{
		switch (*mask)
		{
		case '{':
			++braces;
			break;
		case '}':
			--braces;
			break;
		case '%':
			if (!*++mask)
				return _("Incomplete placeholder at end of mask.");
			if (strchr(allowed_specifiers, *mask) == nullptr)
				return strprintf(_("Invalid placeholder '%%%c'."), *mask);
			break;
		case 0:
			if (braces > 0)
				return _("Opening brace '{' without closing brace '}'.");
			if (braces < 0)
				return _("Closing brace '}' without opening brace '{'.");
			return string();
		}
	}
}

string et_check_mask(const gchar* mask)
{
	return MaskEvaluator::Validate(mask);
}

string et_evaluate_mask(const ET_File *file, const gchar *mask, gboolean no_dir_check_or_conversion)
{
	void (*postprocess)(string& val, unsigned start) = nullptr;
	if (!no_dir_check_or_conversion)
		postprocess = File_Name::prepare_func(
			(EtFilenameReplaceMode)g_settings_get_enum(MainSettings, "rename-replace-illegal-chars"),
			(EtConvertSpaces)g_settings_get_enum(MainSettings, "rename-convert-spaces"));

	return MaskEvaluator(*file, postprocess).Evaluate(mask);
}

void entry_check_mask (GtkEntry *entry, gpointer user_data)
{
	g_return_if_fail (entry != NULL);

	const gchar* mask = gtk_entry_get_text (entry);
	string error;

	if (et_str_empty (mask))
		error = _("Empty scanner mask.");
	else
	{	error = et_check_mask(mask);
		if (error.empty())
		{
			gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, NULL);
			return;
		}
	}

	gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, "emblem-unreadable");
	gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_SECONDARY, error.c_str());
}
