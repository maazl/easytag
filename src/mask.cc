/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022  Marcel Müller <github@maazl.de>
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
	static gchar* Validate(const gchar* mask);
	const string& Evaluate(const gchar* mask) { EvaluateRecursive(mask, true); return Result; }
};

string MaskEvaluator::TagFieldFromMaskCode(gchar code)
{	const File_Tag *tag = File.Tag();
	gchar* r;
	size_t maxlen = 0;
	switch (code)
	{
	case 't': /* Title */
		r = tag->title;
		break;
	case 's': /* Subtitle */
		r = tag->subtitle;
		break;
	case 'a': /* Artist */
		r = tag->artist;
		break;
	case 'T': /* Album */
	case 'b': /* for compatibility with earlier versions */
		r = tag->album;
		break;
	case 'S': /* Disc subtitle */
		r = tag->disc_subtitle;
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
		maxlen = 4;
		break;
	case 'Y': /* Release year */
		r = tag->release_year;
		maxlen = 4;
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
		maxlen = 4;
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
	case '|':
	case '{':
	case '}':
		return string(1, code);
	case 'i': /* Ignored */
	default:
		return string();
	}
	return !r ? string() : maxlen ? string(r, maxlen) : string(r);
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
			if (!field.length())	// placeholder empty => fail
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

gchar* et_check_mask(const gchar* mask)
{
	return MaskEvaluator::Validate(mask);
}

gchar* et_evaluate_mask(const ET_File *file, const gchar *mask, gboolean no_dir_check_or_conversion)
{
	void (*postprocess)(string& val, unsigned start) = nullptr;
	if (!no_dir_check_or_conversion)
		postprocess = File_Name::prepare_func(
			(EtFilenameReplaceMode)g_settings_get_enum(MainSettings, "rename-replace-illegal-chars"),
			(EtConvertSpaces)g_settings_get_enum(MainSettings, "rename-convert-spaces"));

	return g_strdup(MaskEvaluator(*file, postprocess).Evaluate(mask).c_str());
}
