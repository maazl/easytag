/* EasyTAG - tag editor for audio files
 * Copyright (C) 2024  Marcel Müller
 * Copyright (C) 2015  David King <amigadave@amigadave.com>
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

#include "file_name.h"

#include "charset.h"
#include "file_description.h"
#include "misc.h"

#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
using namespace std;

/* FAT has additional restrictions on the last character of a filename.
 * https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx#naming_conventions */
static void replace_trailing_space(EtConvertSpaces convert_mode, string& val)
{
	for (string::iterator p = val.end(); p != val.begin() && *--p == ' '; )
		switch (convert_mode)
		{
		default:
			p = val.erase(p);
			break;
		case ET_CONVERT_SPACES_UNDERSCORES:
			*p = '_';
		}
}

static void replace_chars_spaces(EtConvertSpaces convert_mode, string& val, size_t start)
{
	for (string::iterator p = val.begin() + start; p != val.end(); )
	{	switch (*p)
		{
		case ' ':
			switch (convert_mode)
			{
			case ET_CONVERT_SPACES_REMOVE:
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
			*p = '-';
		}
		++p;
	}

	replace_trailing_space(convert_mode, val);
}

static void replace_chars_ascii(EtConvertSpaces convert_mode, string& val, size_t start)
{
	for (string::iterator p = val.begin() + start; p != val.end(); )
	{	switch (*p)
		{
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 14:
		case 15:
		case 16:
		case 17:
		case 18:
		case 19:
		case 20:
		case 21:
		case 22:
		case 23:
		case 24:
		case 25:
		case 26:
		case 27:
		case 28:
		case 29:
		case 30:
		case 31:
		case 127:
		case ' ':
			switch (convert_mode)
			{case ET_CONVERT_SPACES_REMOVE:
				p = val.erase(p);
				continue;
			case ET_CONVERT_SPACES_UNDERSCORES:
				*p = '_';
				break;
			default:
				*p = ' ';
			}
			break;
		case 13:
			p = val.erase(p);
			continue;
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

	replace_trailing_space(convert_mode, val);
}

static void replace_chars_unicode(EtConvertSpaces convert_mode, string& val, size_t start)
{
	string::iterator p;
	auto replace = [&](const char* repl)
	{	size_t i = val.end() - p;
		val.replace(p, p+1, repl);
		p = val.end() - i;
	};

	for (p = val.begin() + start; p != val.end(); )
	{	switch (*p)
		{
		case 1:
			replace("\xe2\x90\x81"); // ␁ Symbol for Start of Heading
			break;
		case 2:
			replace("\xe2\x90\x82"); // ␂ Symbol for Start of Text
			break;
		case 3:
			replace("\xe2\x90\x83"); // ␃ Symbol for End of Text
			break;
		case 4:
			replace("\xe2\x90\x84"); // ␄ Symbol for End of Transmission
			break;
		case 5:
			replace("\xe2\x90\x85"); // ␅ Symbol for Enquiry
			break;
		case 6:
			replace("\xe2\x90\x86"); // ␆ Symbol for Acknowledge
			break;
		case 7:
			replace("\xe2\x90\x87"); // ␇ Symbol for Bell
			break;
		case 8:
			replace("\xe2\x90\x88"); // ␈ Symbol for Backspace
			break;
		case 9:
			replace("\xe2\x90\x89"); // ␉ Symbol for Horizontal Tabulation
			break;
		case 10:
			replace("\xe2\x90\x8a"); // ␊ Symbol for Line Feed
			break;
		case 11:
			replace("\xe2\x90\x8b"); // ␋ Symbol for Vertical Tabulation
			break;
		case 12:
			replace("\xe2\x90\x8c"); // ␌ Symbol for Form Feed
			break;
		case 13:
			replace("\xe2\x90\x8d"); // ␍ Symbol for Carriage Return
			break;
		case 14:
			replace("\xe2\x90\x8e"); // ␎ Symbol for Shift Out
			break;
		case 15:
			replace("\xe2\x90\x8f"); // ␏ Symbol for Shift In
			break;
		case 16:
			replace("\xe2\x90\x90"); // ␐ Symbol for Data Link Escape
			break;
		case 17:
			replace("\xe2\x90\x91"); // ␑ Symbol for Device Control One
			break;
		case 18:
			replace("\xe2\x90\x92"); // ␒ Symbol for Device Control Two
			break;
		case 19:
			replace("\xe2\x90\x93"); // ␓ Symbol for Device Control Three
			break;
		case 20:
			replace("\xe2\x90\x94"); // ␔ Symbol for Device Control Four
			break;
		case 21:
			replace("\xe2\x90\x95"); // ␕ Symbol for Negative Acknowledge
			break;
		case 22:
			replace("\xe2\x90\x96"); // ␖ Symbol for Synchronous Idle
			break;
		case 23:
			replace("\xe2\x90\x97"); // ␗ Symbol for End of Transmission Block
			break;
		case 24:
			replace("\xe2\x90\x98"); // ␘ Symbol for Cancel
			break;
		case 25:
			replace("\xe2\x90\x99"); // ␙ Symbol for End of Medium
			break;
		case 26:
			replace("\xe2\x90\x9a"); // ␚ Symbol for Substitute
			break;
		case 27:
			replace("\xe2\x90\x9b"); // ␛ Symbol for Escape
			break;
		case 28:
			replace("\xe2\x90\x9c"); // ␜ Symbol for File Separator
			break;
		case 29:
			replace("\xe2\x90\x9d"); // ␝ Symbol for Group Separator
			break;
		case 30:
			replace("\xe2\x90\x9e"); // ␞ Symbol for Record Separator
			break;
		case 31:
			replace("\xe2\x90\x9f"); // ␟ Symbol for Unit Separator
			break;
		case 127:
			replace("\xe2\x90\xa1"); // ␡ Symbol for Delete
			break;
		case ' ':
			switch (convert_mode)
			{case ET_CONVERT_SPACES_REMOVE:
				p = val.erase(p);
				continue;
			case ET_CONVERT_SPACES_UNDERSCORES:
				replace("\xe2\x90\xa0"); // ␠ Symbol for Space
				continue;
			}
		default:
			++p;
			break;
		case '\\':
			replace("\xe2\x88\x96"); // ∖ Set Minus
			break;
		case '/':
			replace("\xe2\x88\x95"); // / Division Slash
			break;
		case ':':
			replace("\xe2\x88\xb6"); // ∶ Ratio
			break;
		case '|':
			replace("\xe2\x88\xa3"); // ∣ Divides
			break;
		case '*':
			replace("\xe2\x88\x97"); // ∗ Asterisk Operator
			break;
		case '?':
			replace("\xe2\x80\xbd"); // ‽ Interrobang
			break;
		case '"':
			replace("\xe2\x80\x9d"); // ” Right Double Quotation Mark
			break;
		case '<':
			replace("\xe2\x89\xba"); // ≺ Precedes
			break;
		case '>':
			replace("\xe2\x89\xbb"); // ≻ Succeeds
			break;
		}
	}

	replace_trailing_space(convert_mode, val);
}

void (*const File_Name::prepare_funcs[3][3])(std::string& filename_utf8, unsigned start) =
{	{	[](std::string& filename_utf8, unsigned start) { replace_chars_ascii(ET_CONVERT_SPACES_UNDERSCORES, filename_utf8, start); }
	,	[](std::string& filename_utf8, unsigned start) { replace_chars_ascii(ET_CONVERT_SPACES_REMOVE, filename_utf8, start); }
	,	[](std::string& filename_utf8, unsigned start) { replace_chars_ascii(ET_CONVERT_SPACES_SPACES, filename_utf8, start); }
	}
,	{	[](std::string& filename_utf8, unsigned start) { replace_chars_unicode(ET_CONVERT_SPACES_UNDERSCORES, filename_utf8, start); }
	,	[](std::string& filename_utf8, unsigned start) { replace_chars_unicode(ET_CONVERT_SPACES_REMOVE, filename_utf8, start); }
	,	[](std::string& filename_utf8, unsigned start) { replace_chars_unicode(ET_CONVERT_SPACES_SPACES, filename_utf8, start); }
	}
,	{	[](std::string& filename_utf8, unsigned start) { replace_chars_spaces(ET_CONVERT_SPACES_UNDERSCORES, filename_utf8, start); }
	,	[](std::string& filename_utf8, unsigned start) { replace_chars_spaces(ET_CONVERT_SPACES_REMOVE, filename_utf8, start); }
	,	[](std::string& filename_utf8, unsigned start) { replace_chars_spaces(ET_CONVERT_SPACES_SPACES, filename_utf8, start); }
	}
};

File_Name::File_Name(const char* filename)
{	// separate file name from path
	const char* separator = strrchr(filename, G_DIR_SEPARATOR);
	if (separator)
	{	File.assignNFC(separator + 1);
		Path.assignNFC(filename, separator - filename);
	} else
	{	File.assignNFC(filename);
	}
}

gString File_Name::generate_name(const char* new_filepath, bool keep_path) const
{	// keep extension
	gString np(g_strconcat(new_filepath, ET_Get_File_Extension(File.get()), NULL));

	if (keep_path && Path && !g_path_is_absolute(new_filepath))
		np = g_strconcat(Path, G_DIR_SEPARATOR_S, np.get(), NULL);
	// else: Just add the extension.

	return np;
}

gString File_Name::full_name() const
{	if (Path.empty())
		return gString(g_strdup(File));
	return gString(g_strconcat(Path, G_DIR_SEPARATOR_S, File.get(), NULL));
}

/* Convert filename extension (lower/upper/no change). */
bool File_Name::format_extension()
{	gchar* (*func)(const gchar*, gssize);
	switch (g_settings_get_enum(MainSettings, "rename-extension-mode"))
	{
	default:
		return false;
	case ET_FILENAME_EXTENSION_LOWER_CASE:
		func = g_ascii_strdown;
		break;
	case ET_FILENAME_EXTENSION_UPPER_CASE:
		func = g_ascii_strup;
	};

	const char* ext = ET_Get_File_Extension(File);
	if (ext == nullptr)
		return false;

	gString new_ext((*func)(ext, -1));
	if (strcmp(new_ext, ext) == 0)
		return false;

	size_t len = ext - File.get();
	size_t ext_len = strlen(new_ext) + 1;
	char cp[len + ext_len];
	memcpy(cp, File, len);
	memcpy(cp + len, new_ext, ext_len);
	File = cp;
	return true;
}

bool File_Name::format_filepath()
{
	auto prep = File_Name::prepare_func(
		(EtFilenameReplaceMode)g_settings_get_enum(MainSettings, "rename-replace-illegal-chars"),
		(EtConvertSpaces)g_settings_get_enum(MainSettings, "rename-convert-spaces"));

	bool changed = false;

	string tmp = File.get();
	prep(tmp, 0);
	if (tmp != File.get())
	{	File = tmp;
		changed = true;
	}

	tmp = Path.get();
	// replace path delimiters by 0 characters to prevent replacement.
	for (auto& c : tmp)
		if (G_IS_DIR_SEPARATOR(c))
			c = 0;

	prep(tmp, 0);

	// undo replacement
	for (auto& c : tmp)
		if (!c)
			c = G_DIR_SEPARATOR;

	if (tmp != Path.get())
	{	Path = tmp;
		changed = true;
	}

	return changed;
}
