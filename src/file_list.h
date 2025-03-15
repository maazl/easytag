/* EasyTAG - tag editor for audio files
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
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

#ifndef ET_FILE_LIST_H_
#define ET_FILE_LIST_H_

#include <glib.h>

#include "setting.h"
#include "file.h"

#include <vector>

struct File_Name;


class ET_FileList
{
	/// Files in the currently selected root directory and optionally subdirectories.
	static std::vector<xPtr<ET_File>> ETFileList;

	/// History list of file changes for undo/redo actions
	static std::vector<xPtr<ET_File>> ETHistoryFileList;
	/// Current redo index in ETHistoryFileList. Equals ETHistoryFileList.size() unless redo available.
	static unsigned ETHistoryFileListRedo;

public:
	static void reset();

	/// \c TRUE if undo file list contains undo data.
	static gboolean has_undo() { return ETHistoryFileListRedo > 0; }
	/// \c TRUE if undo file list contains redo data.
	static gboolean has_redo() { return ETHistoryFileListRedo < ETHistoryFileList.size(); }

	/// Add a ET_File item to the main undo list of files.
	static void history_list_add(ET_File* ETFile);

	/// Execute one 'undo' in the main undo list (it selects the last ETFile changed,
	/// before to apply an undo action)
	static ET_File* undo();
	static ET_File* redo();
};


void ET_Remove_File_From_File_List (ET_File *ETFile);
gboolean et_file_list_check_all_saved (GList *etfilelist);
/// Update path of file names after directory rename
/// @param file_list this pointer
/// @param old_path Old <em>relative</em> path with respect to current root.
/// @param new_path New <em>relative</em> path with respect to current root.
void et_file_list_update_directory_name (GList *file_list, const gchar *old_path, const gchar *new_path);
guint et_file_list_get_n_files_in_path (GList *file_list, const gchar *path_utf8);
void et_file_list_free (GList *file_list);

GList * et_artist_album_list_new_from_file_list (GList *file_list);
void et_artist_album_file_list_free (GList *file_list);

GList * ET_Displayed_File_List_First (void);
GList * ET_Displayed_File_List_Previous (void);
GList * ET_Displayed_File_List_Next (void);
GList * ET_Displayed_File_List_Last (void);
GList * ET_Displayed_File_List_By_Etfile (const ET_File *ETFile);

void et_displayed_file_list_set (GList *ETFileList);
void et_displayed_file_list_free (GList *file_list);

void et_history_file_list_free (GList *file_list);

GList *ET_Sort_File_List (GList *ETFileList, EtSortMode Sorting_Type);

#endif /* !ET_FILE_H_ */
