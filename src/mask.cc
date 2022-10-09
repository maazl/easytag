/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022 Marcel Müller <gitlab@maazl.de>
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

#include <glib/gi18n.h>

#include <string>
using namespace std;

/* Keep up to date with the format specifiers shown in the UI. */
static const gchar allowed_specifiers[] = "abcdegilnoprtuxyzADNTY%{|}";

const gchar *et_tag_field_from_mask_code(const File_Tag *tag, gchar code)
{
	gchar* r;
	switch (code)
	{
	case 't': /* Title */
		r = tag->title;
		break;
	case 'a': /* Artist */
		r = tag->artist;
		break;
	case 'T': /* Album */
	case 'b': /* for compatibility with earlier versions */
		r = tag->album;
		break;
	case 'd': /* Disc Number */
		r = tag->disc_number;
		break;
	case 'D': /* Total number of discs. */
	case 'x': /* for compatibility with earlier versions */
		r = tag->disc_total;
		break;
	case 'y': /* Year */
		r = tag->year;
		break;
	case 'Y': /* Release year */
		r = tag->release_year;
		break;
	case 'n': /* Track */
		r = tag->track;
		break;
	case 'N': /* Track Total */
	case 'l': /* for compatibility with earlier versions */
		r = tag->track_total;
		break;
	case 'g': /* Genre */
		r = tag->genre;
		break;
	case 'c': /* Comment */
		r = tag->comment;
		break;
	case 'p': /* Composer */
		r = tag->composer;
		break;
	case 'o': /* Orig. Artist */
		r = tag->orig_artist;
		break;
	case 'w': /* Original year */
		r = tag->orig_year;
		break;
	case 'r': /* Copyright */
		r = tag->copyright;
		break;
	case 'u': /* URL */
		r = tag->url;
		break;
	case 'e': /* Encoded by */
		r = tag->encoded_by;
		break;
	case 'A': /* Album Artist */
	case 'z': /* for compatibility with earlier versions */
		r = tag->album_artist;
		break;
	case '%': /* escape sequence */
		return "%";
	case '|':
		return "|";
	case '{':
		return "{";
	case '}':
		return "}";
	case 'i': /* Ignored */
	default:
		return "";
	}
	return et_str_empty(r) ? nullptr : r;
}

class MaskEvaluator
{	const ET_File& File;
	void (*Postprocess)(string& val, size_t start);
	string Result;
	string Error;
	const File_Tag& Tag() const { return *(File_Tag*)File.FileTag->data; }
	bool EvaluateRecursive(const gchar*& mask, bool enabled);
public:
	MaskEvaluator(const ET_File &file, void (*postprocess)(string& val, size_t start) = nullptr)
	: File(file), Postprocess(postprocess) {}
	static gchar* Validate(const gchar* mask);
	const string& Evaluate(const gchar* mask) { EvaluateRecursive(mask, true); return Result; }
};

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
			const gchar* field = et_tag_field_from_mask_code(&Tag(), *mask);
			if (!field)	// placeholder empty => fail
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

gchar* MaskEvaluator::Validate(const gchar* mask)
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
				return g_strdup(_("Incomplete placeholder at end of mask."));
			if (strchr(allowed_specifiers, *mask) == nullptr)
				return g_strdup_printf(_("Invalid placeholder '%%%c'."), *mask);
			break;
		case 0:
			if (braces > 0)
				return g_strdup(_("Opening brace '{' without closing brace '}'."));
			if (braces < 0)
				return g_strdup(_("Closing brace '}' without opening brace '{'."));
			return nullptr;
		}
	}
}

static void replace_chars(string& val, size_t start)
{
	gint convert_mode = g_settings_get_enum(MainSettings, "rename-convert-spaces");

	string::size_type p = 0;
	while ((p = val.find(' ', p)) != string::npos)
	{	switch (convert_mode)
		{case ET_CONVERT_SPACES_REMOVE:
			val.erase(p, 1);
			continue;
		case ET_CONVERT_SPACES_UNDERSCORES:
			val[p++] = '_';
		}
	}
}

static void replace_chars2(string& val, size_t start)
{
	gint convert_mode = g_settings_get_enum(MainSettings, "rename-convert-spaces");

	for (string::iterator p = val.begin() + start; p != val.end(); )
	{	switch (*p)
		{
		case ' ':
			switch (convert_mode)
			{case ET_CONVERT_SPACES_REMOVE:
				p = val.erase(p);
				continue;
			case ET_CONVERT_SPACES_UNDERSCORES:
				*p = '_';
			}
			break;
		case G_DIR_SEPARATOR:
		#ifdef G_OS_WIN32
		case '/': /* Convert character '/' on WIN32 to '-' too. */
		#endif
		case ':':
		case '|':
			*p = '-';
			break;
		case '*':
			*p = '+';
			break;
		case '?':
			*p = '_';
			break;
		case '"':
			*p = '\'';
			break;
		case '<':
			*p = '(';
			break;
		case '>':
			*p = ')';
			break;
		}
		++p;
	}
}

gchar* et_check_mask(const gchar* mask)
{
	return MaskEvaluator::Validate(mask);
}

gchar* et_evaluate_mask(const ET_File *file, const gchar *mask, gboolean no_dir_check_or_conversion)
{
	void (*postprocess)(string& val, size_t start) = no_dir_check_or_conversion ? nullptr
		: g_settings_get_boolean(MainSettings, "rename-replace-illegal-chars") ? replace_chars2 : replace_chars;

	return g_strdup(MaskEvaluator(*file, postprocess).Evaluate(mask).c_str());
}
