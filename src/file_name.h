/* EasyTAG - tag editor for audio files
 * Copyright (C) 2024-2026  Marcel Müller <github@maazl.de>
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

#include "misc.h"
#include "setting.h"
#include "undo_list.h"
#include <glib.h>

#include "xstring.h"
#include <string>

struct UpdateDirectoyNameArgs
{	gString OldPath; ///< Old name in file system encoding
	gString NewPath; ///< New name in file system encoding
	gString OldPathUTF8; ///< Old name as normalized UTF-8
	gString NewPathUTF8; ///< New name as normalized UTF-8
	const char* OldPathRelUTF8; ///< Old name relative to root as UTF-8 if applicable
	const char* NewPathRelUTF8; ///< New name relative to root as UTF-8 if applicable
	PathRel RelationToCurrentRoot; ///< How is the renamed directory related to the currently loaded root.
	/// @param old_path Absolute path of the previous directory name in file system encoding.
	/// @param new_path Absolute path of the new directory name in file system encoding.
	/// @param root Current root path if any.
	UpdateDirectoyNameArgs(const gchar *old_path, const gchar *new_path, const gchar* root);
};

/**
 * Item of the FileNameList list, all UTF-8.
 */
class File_Name : public UndoList<File_Name>::Intrusive
{
	xStringD0 Path; ///< Path component as UTF-8, maybe relative to the current root path. May be empty.
	xStringD0 File; ///< File name within \ref Path as UTF-8 with extension.

private:
	static void (*const prepare_funcs[3][3])(std::string&, unsigned);

public:
	/// Initialize from file name.
	/// @param path Path of the file (UTF-8).
	/// @remarks \a path may be relative or absolute.
	File_Name(const char* path);

	friend bool operator==(const File_Name& l, const File_Name& r) { return l.File == r.File && l.Path == r.Path; }
	friend bool operator!=(const File_Name& l, const File_Name& r) { return !(l == r); }

	/// Path component as UTF-8, maybe relative to the current root path. May be empty.
	const xStringD0& path() const { return Path; }
		/// File name within \ref path as UTF-8 with extension.
	const xStringD0& file() const { return File; }
	/// Get file name with relative path (UTF-8).
	gString full_name() const;

	/// Notify about a directory rename operation.
	/// @returns \c true if the operation caused a change.
	/// @remarks This is basically a find and replace operation.
	bool update_directory_name(const UpdateDirectoyNameArgs& args);

	/// Create new file path by applying a new path and file name.
	/// @param new_filepath new UTF-8 file name <em>w/o extension</em> and path to apply to \a current.
	/// If the path is absolute it will completely replace the file path.
	/// @param keep_path Keep the current \ref Path.
	/// @return Generated file path with (current) extension.
	/// @remarks The result of this function is typically passed to the constructor.
	gString generate_name(const char* new_filepath, bool keep_path) const;

	/// Convert filename extension (lower/upper/no change)
	/// @return true if the operation made a change.
	bool format_extension();
	/// Convert filename and path according to character replacement rules.
	/// @return true if the operation made a change.
	bool format_filepath();

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
};

#endif /* !ET_FILE_NAME_H_ */
