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
#include "xstring.h"
#include <string>

/**
 * Item of the FileNameList list
 */
typedef class File_Name
{
public:
	guint key;
	bool saved; ///< Set to \c true if this filename had been saved
private:
	xString _value; ///< The filename containing the full path and the extension of the file
	xString _value_utf8; ///< Same than "value", but converted to UTF-8 to avoid multiple call to the conversion function
	unsigned _rel_start_utf8; ///< start in value_utf8 beyond the root path passed during initialization.
	unsigned _file_start_utf8; ///< start file base name part in \c _value_utf8.
	mutable xString _path_value_ck; ///< collation string for path_value_utf8.
	mutable xString _file_value_ck; ///< collation string for file_value_utf8.

	static void (*const prepare_funcs[3][3])(std::string&, unsigned);
public:
	File_Name();
	File_Name(const File_Name& r);
	~File_Name();
	void reset();
	void set_filename_raw(const File_Name* root, const char* filename);
	void set_filename_utf8(const File_Name* root, const char* filename_utf8);
	bool SetFromComponents(const File_Name* root, const char* new_name, const char* dir_name, EtFilenameReplaceMode replace_illegal);

	/// Raw file name in file system representation.
	const xString& value() const noexcept { return _value; }
	/// File name in UTF-8 representation.
	const xString& value_utf8() const noexcept { return _value_utf8; }
	/// File name relative to the root path at creation time.
	/// @remarks This is typically the currently selected directory.\n
	/// If no root was specified it is the same as \ref value_utf8.
	const char* rel_value_utf8() const noexcept { return _value_utf8 + _rel_start_utf8; }
	/// Path component of \ref rel_value_utf8 without trailing path delimiter.
	std::string path_value_utf8() const;
	/// File name component of \ref value_utf8.
	const char* file_value_utf8() const noexcept { return _value_utf8 + _file_start_utf8; }
	/// File name component of \ref value_utf8 w/o file extension.
	std::string file_value_noext_utf8() const;
	/// \ref path_value_utf8 as GLib collation string.
	/// @remarks The returned value is initialized lazily on first access.
	const xString& path_value_ck() const;
	/// \ref file_value_utf8 as GLib collation string.
	/// @remarks The returned value is initialized lazily on first access.
	const xString& file_value_ck() const;

	/// Get function to replace illegal characters in UTF-8 file name.
	/// @param replace_illegal Replacement mode for illegal characters
	/// @param convert_spaces Replacement mode for spaces.
	/// @return Function with
	/// - filename as UTF-8 string, will be modified in place,
	/// - (optional) start position in filename.
	/// @remarks The function will always replace characters
	/// that cause severe problems like path delimiters.
	static void (*prepare_func(EtFilenameReplaceMode replace_illegal, EtConvertSpaces convert_spaces))(std::string& filename_utf8, unsigned start)
	{	return prepare_funcs[std::min((unsigned)replace_illegal, 2U)][std::min((unsigned)convert_spaces - 1, 2U)]; }
} File_Name;

#else
struct File_Name;
#endif

#endif /* !ET_FILE_NAME_H_ */
