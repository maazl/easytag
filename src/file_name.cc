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


static gchar empty_singleton[] = "";

/*
 * Create a new File_Name structure
 */
File_Name *
et_file_name_new (void)
{
    File_Name *file_name;

    file_name = g_slice_new (File_Name);
    file_name->key = et_undo_key_new ();
    file_name->saved = FALSE;
    file_name->value = NULL;
    file_name->value_utf8 = NULL;
    file_name->rel_value_utf8 = NULL;
    file_name->path_value_utf8 = NULL;
    file_name->file_value_utf8 = NULL;
    file_name->path_value_ck = NULL;
    file_name->file_value_ck = NULL;

    return file_name;
}

static void
et_file_name_free_value(File_Name *file_name)
{
    g_free (file_name->value);
    if (file_name->value_utf8 != file_name->value)
        g_free (file_name->value_utf8);
    if (file_name->path_value_utf8 != empty_singleton)
        g_free (file_name->path_value_utf8);
    if (file_name->path_value_ck != empty_singleton)
        g_free (file_name->path_value_ck);
    g_free (file_name->file_value_ck);
}

/*
 * Frees a File_Name item.
 */
void
et_file_name_free (File_Name *file_name)
{
    g_return_if_fail (file_name != NULL);
    et_file_name_free_value(file_name);
    g_slice_free (File_Name, file_name);
}

/*
 * Fill content of a FileName item according to the filename passed in argument (UTF-8 filename or not)
 * Calculate also the collate key.
 * It treats numbers intelligently so that "file1" "file10" "file5" is sorted as "file1" "file5" "file10"
 */
void
ET_Set_Filename_File_Name_Item (File_Name *FileName,
                                const File_Name *root,
                                const gchar *filename_utf8,
                                const gchar *filename)
{
    string root_value_utf8;

    g_return_if_fail (FileName != NULL);

    // be aware of aliasing root == FileName
    if (root)
    {
        size_t len = root->rel_value_utf8 - root->value_utf8;
        if (--len > 0)
            root_value_utf8 = string(root->value_utf8, len);
        else
            root_value_utf8 = root->value_utf8;
    }

    et_file_name_free_value(FileName);

    if (filename_utf8 && filename)
    {
        FileName->value = g_strdup (filename);
        if (strcmp(filename_utf8, FileName->value) == 0)
            FileName->value_utf8 = g_strdup (filename_utf8);
        else
            FileName->value_utf8 = FileName->value;
    }
    else if (filename_utf8)
    {
        FileName->value_utf8 = g_strdup (filename_utf8);
        FileName->value = filename_from_display (filename_utf8);

        if (strcmp(FileName->value_utf8, FileName->value) == 0)
        {
            g_free(FileName->value_utf8);
            FileName->value_utf8 = FileName->value;
        }
    }
    else if (filename)
    {
        FileName->value_utf8 = g_filename_display_name (filename);
        FileName->value = g_strdup (filename);

        if (strcmp(FileName->value_utf8, FileName->value) == 0)
        {
            g_free(FileName->value_utf8);
            FileName->value_utf8 = FileName->value;
        }
    }

    FileName->rel_value_utf8 = FileName->value_utf8;
    if (strncmp(FileName->value_utf8, root_value_utf8.c_str(), root_value_utf8.length()) == 0
        && FileName->rel_value_utf8[root_value_utf8.length()] == G_DIR_SEPARATOR)
        FileName->rel_value_utf8 += root_value_utf8.length() + 1;

    gchar* separator = strrchr(FileName->rel_value_utf8, G_DIR_SEPARATOR);
    if (separator)
    {
        FileName->file_value_utf8 = separator + 1;
        string path(FileName->rel_value_utf8, separator - FileName->rel_value_utf8);
        FileName->path_value_utf8 = g_strdup(path.c_str());
        // Replace dir separator by dot for more reasonable sort order
        replace(path.begin(), path.end(), G_DIR_SEPARATOR, '.');
        FileName->path_value_ck = g_utf8_collate_key_for_filename(path.c_str(), -1);
    } else
    {
        FileName->file_value_utf8 = FileName->rel_value_utf8;
        FileName->path_value_utf8 = empty_singleton;
        FileName->path_value_ck = empty_singleton;
    }
    FileName->file_value_ck = g_utf8_collate_key_for_filename(FileName->file_value_utf8, -1);
}

gboolean
et_file_name_set_from_components (File_Name *file_name,
                                  const File_Name *root,
                                  const gchar *new_name,
                                  const gchar *dir_name,
                                  EtFilenameReplaceMode replace_illegal)
{
    /* Check if new filename seems to be correct. */
    if (new_name)
    {
        string filename_new = new_name;
        /* Convert the illegal characters. */
        File_Name::prepare_func(replace_illegal, (EtConvertSpaces)g_settings_get_enum(MainSettings, "rename-convert-spaces"))(filename_new, 0);

        /* Set the new filename (in file system encoding). */
        gchar *path_new = g_build_filename (dir_name, filename_new.c_str(), NULL);
        ET_Set_Filename_File_Name_Item (file_name, root, NULL, path_new);

        g_free (path_new);
        return TRUE;
    }
    else
    {
        et_file_name_free_value(file_name);
        file_name->value = NULL;
        file_name->value_utf8 = NULL;
        file_name->path_value_ck = NULL;

        return FALSE;
    }
}

/*
 * Compares two File_Name items :
 *  - returns TRUE if there aren't the same
 *  - else returns FALSE
 */
gboolean
et_file_name_detect_difference (const File_Name *a,
                                const File_Name *b)
{
    g_return_val_if_fail (a && b, FALSE);

    if (a && !b) return TRUE;
    if (!a && b) return TRUE;

    /* Both a and b are != NULL. */
    if (!a->value && !b->value) return FALSE;
    if (a->value && !b->value) return TRUE;
    if (!a->value && b->value) return TRUE;

    /* Compare collate keys (with FileName->value converted to UTF-8 as it
     * contains raw data). */
    /* Filename changed ? (we check path + file). */
    return strcmp (a->file_value_ck, b->file_value_ck) != 0
        || strcmp (a->path_value_ck, b->path_value_ck) != 0;
}
