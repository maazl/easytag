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

#ifndef ET_FILE_NAME_H_
#define ET_FILE_NAME_H_

#include "setting.h"
#include <glib.h>

#ifdef __cplusplus
#include <string>
#endif

/*
 * Description of each item of the FileNameList list
 */
typedef struct File_Name
{
    guint key;
    gboolean saved; /* Set to TRUE if this filename had been saved */
    gchar *value; /* The filename containing the full path and the extension of the file */
    gchar *value_utf8; /* Same than "value", but converted to UTF-8 to avoid multiple call to the conversion function */
    gchar *rel_value_utf8; /* part of value_utf8 beyond the root path passed during initialization. */
    gchar *path_value_utf8; /* directory part of rel_value_utf8 if any, "" in doubj. */
    gchar *file_value_utf8; /* file base name part of (rel_)value_utf8. */
    gchar *path_value_ck; /* collation string for path_value_utf8. */
    gchar *file_value_ck; /* collation string for file_value_utf8. */

#ifdef __cplusplus
private:
	static void (*const prepare_funcs[3][3])(std::string&, unsigned);
public:
	/**
	 * Get function to replace illegal characters in UTF-8 file name.
	 * @param replace_illegal Replacement mode for illegal characters
	 * @param convert_spaces Replacement mode for spaces.
	 * @return Function with
	 * - filename as UTF-8 string, will be modified in place,
	 * - (optional) start position in filename.
	 * @remarks The function will always replace characters
	 * that cause severe problems like path delimiters.
	 */
	static void (*prepare_func(EtFilenameReplaceMode replace_illegal, EtConvertSpaces convert_spaces))(std::string& filename_utf8, unsigned start)
	{	return prepare_funcs[std::min((unsigned)replace_illegal, 2U)][std::min((unsigned)convert_spaces - 1, 2U)]; }
#endif
} File_Name;

G_BEGIN_DECLS

File_Name * et_file_name_new (void);
void et_file_name_free (File_Name *file_name);
void ET_Set_Filename_File_Name_Item (File_Name *FileName, const File_Name *root, const gchar *filename_utf8, const gchar *filename);
gboolean et_file_name_set_from_components (File_Name *file_name, const File_Name *root, const gchar *new_name, const gchar *dir_name, EtFilenameReplaceMode replace_illegal);
gboolean et_file_name_detect_difference (const File_Name *a, const File_Name *b);

G_END_DECLS

#endif /* !ET_FILE_NAME_H_ */
