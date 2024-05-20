/* EasyTAG - tag editor for audio files
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
#include "misc.h"

#include <string.h>
#include <string>
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
		case 0:
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
		case 0:
			replace("\xe2\x90\x80"); // ␀ Symbol for Null
			break;
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

File_Name::File_Name()
:	key(et_undo_key_new())
,	saved(false)
,	_rel_start_utf8(0U)
,	_file_start_utf8(0U)
{}

File_Name::File_Name(const File_Name& r)
:	key(r.key) // is this correct?
,	saved(false)
,	_value(r._value)
,	_value_utf8(r._value_utf8)
,	_rel_start_utf8(r._rel_start_utf8)
,	_file_start_utf8(r._file_start_utf8)
,	_path_value_ck(r._path_value_ck)
,	_file_value_ck(r._file_value_ck)
{}

File_Name::~File_Name()
{}

void File_Name::reset()
{	_value.reset();
	_value_utf8.reset();
	_rel_start_utf8 = 0;
	_file_start_utf8 = 0;
	_path_value_ck.reset();
	_file_value_ck.reset();
}

/*
 * Fill content of a FileName item according to the filename passed in argument (UTF-8 filename or not)
 * Calculate also the collate key.
 * It treats numbers intelligently so that "file1" "file10" "file5" is sorted as "file1" "file5" "file10"
 */
void File_Name::set_filename(const File_Name *root, const char *filename_utf8, const char *filename)
{
	if (!filename && !filename_utf8)
	{	reset();
		return;
	}

	_path_value_ck.reset();
	_file_value_ck.reset();
	_rel_start_utf8 = 0;

	if (filename_utf8 && filename)
	{	_value = filename;
		if (strcmp(filename_utf8, _value))
			_value_utf8 = filename_utf8;
		else
			_value_utf8 = _value;
	}
	else if (filename_utf8)
	{	_value_utf8 = filename_utf8;
		gString v(filename_from_display(filename_utf8));
		if (strcmp(_value_utf8, v) == 0)
			_value = _value_utf8;
		else
			_value = xString(v.get());
	}
	else // if (filename)
	{	_value = filename;
		gString utf8(g_filename_display_name(filename));
		if (strcmp(_value, utf8) == 0)
			_value_utf8 = _value;
		else
			_value_utf8 = utf8.get();
	}

	// be aware of aliasing root == FileName
	size_t root_value_utf8_len = 0;
	if (root)
	{	root_value_utf8_len = root->_rel_start_utf8;
		if (--root_value_utf8_len <= 0)
			root_value_utf8_len = strlen(root->_value_utf8);
	}

	if ((!root || strncmp(_value_utf8, root->_value_utf8, root_value_utf8_len) == 0)
		&& _value_utf8[root_value_utf8_len] == G_DIR_SEPARATOR)
		_rel_start_utf8 = root_value_utf8_len + 1;

	const char* separator = strrchr(rel_value_utf8(), G_DIR_SEPARATOR);
	if (separator)
	{	_file_start_utf8 = separator - _value + 1;
	} else
	{	_file_start_utf8 = _rel_start_utf8;
		_path_value_ck = xString::empty_str;
	}
}

string File_Name::path_value_utf8() const
{	string ret;
	if (_value_utf8 && _file_start_utf8)
		ret.assign(_value_utf8, _file_start_utf8 - 1);
	return ret;
}

string File_Name::file_value_noext_utf8() const
{	string ret;
	if (_value_utf8)
	{	const char* dot = strrchr(file_value_utf8(), '.');
		if (dot)
			ret.assign(file_value_utf8(), dot - file_value_utf8());
		else
			ret.assign(_value_utf8);
	}
	return ret;
}

const xString& File_Name::path_value_ck() const
{	if (!_path_value_ck && _value_utf8)
	{	// Replace dir separator by dot for more reasonable sort order
		string path(path_value_utf8());
		replace(path.begin(), path.end(), G_DIR_SEPARATOR, '.');
		_path_value_ck = gString(g_utf8_collate_key_for_filename(path.c_str(), -1)).get();
	}
	return _path_value_ck;
}

const xString& File_Name::file_value_ck() const
{	if (!_file_value_ck && _value_utf8)
		_file_value_ck = gString(g_utf8_collate_key_for_filename(file_value_utf8(), -1)).get();
	return _file_value_ck;
}

bool File_Name::SetFromComponents(const File_Name *root,
	const char *new_name, const char *dir_name, EtFilenameReplaceMode replace_illegal)
{	if (!new_name)
	{	reset();
		return false;
	}

	// Check if new filename seems to be correct.
	string filename_new = new_name;
	// Convert the illegal characters.
	File_Name::prepare_func(replace_illegal, (EtConvertSpaces)g_settings_get_enum(MainSettings, "rename-convert-spaces"))(filename_new, 0);

	/* Set the new filename (in file system encoding). */
	gString path_new(g_build_filename(dir_name, filename_new.c_str(), NULL));
	set_filename(root, NULL, path_new);
	return true;
}
