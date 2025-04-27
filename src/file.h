/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022,2024  Marcel Müller <github@maazl.de>
 * Copyright (C) 2014,2015  David King <amigadave@amigadave.com>
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

#ifndef ET_FILE_H_
#define ET_FILE_H_

#include <glib.h>

#include "file_description.h"
#include "file_name.h"
#include "file_tag.h"
#include "misc.h"
#include "undo_list.h"
#include "xptr.h"

#include <vector>
#include <atomic>


enum EtBrowserMode : int;


/*
 * Structure containing informations of the header of file
 * Nota: This struct was copied from an "MP3 structure", and will change later.
 */
typedef struct
{
    gint version;               /* Version of bitstream (mpeg version for mp3, encoder version for ogg) */
    gsize layer; /* "MP3 data" */
    gint bitrate;               /* Bitrate (b/s) */
    gboolean variable_bitrate;  /* Is a VBR file? */
    gint samplerate;            /* Samplerate (Hz) */
    gint mode;                  /* Stereo, ... or channels for ogg */
    double duration;            /* The duration of file (in seconds) */
    gchar *mpc_profile;         /* MPC data */
    gchar *mpc_version;         /* MPC data : encoder version  (also for Speex) */
} ET_File_Info;

/*
 * Description of each item of the ETFileList list
 */
class ET_File : public xObj
{
public:
	gString FilePath;             ///< Full raw path of the file, do not use in UI.

	guint64 FileSize;             ///< File size in bytes
	guint64 FileModificationTime; ///< Save modification time of the file

	const ET_File_Description *ETFileDescription;
	ET_File_Info      ETFileInfo; ///< Header infos: bitrate, duration, ...
	std::unique_ptr<gString[]> other; ///< NULL terminated array of other tags, used for Vorbis comments

private:
	UndoList<File_Name> FileName; ///< File name data with change history
	UndoList<File_Tag>  FileTag;  ///< File tag data with change history
	bool force_tag_save_;

	/// Key for Undo, strongly monotonic
	static std::atomic<unsigned> ETUndoKey;

	/// History list of file changes for undo/redo actions
	static std::vector<xPtr<ET_File>> ETHistoryFileList;
	/// Current redo index in ETHistoryFileList. Equals ETHistoryFileList.size() unless redo available.
	static unsigned ETHistoryFileListRedo;

#ifndef NDEBUG
	static std::atomic<unsigned> Instances;
#endif

public: // injections from ET_FileList
	bool activate_bg_color;       ///< For browser list: alternating background due to sub directory change.
	/// Position in the global file list, intrusion from ET_FileList
	/// @remarks Must be renumbered after resorting the list.
	/// This value varies when resorting list and should not be used directly.
	/// Use ET_FileList::visible_index to get a file's position.
	unsigned IndexKey;

private:
	/// Populate FileSize and FileModificationTime
	bool read_fileinfo(GFile* file, GError **error = nullptr);

public:
	/// Create file
	/// @param filepath Full file path in file system encoding.
	ET_File(gString&& filepath);
	ET_File(const gString& filepath) : ET_File(gString(g_strdup(filepath))) {}
	~ET_File();
#ifndef NDEBUG
	static unsigned instances() { return Instances; }
#endif

	/// Currently saved file name
	const File_Name* FileNameCur() const { return FileName.Cur; }
	/// Current, possibly unsaved file name
	const File_Name* FileNameNew() const { return FileName.New; }

	/// Currently saved tag data
	const File_Tag* FileTagCur() const { return FileTag.Cur; }
	/// Current, possibly unsaved tag data
	const File_Tag* FileTagNew() const { return FileTag.New; }

	bool read_file(GFile *file, const gchar *root, GError **error);

	/// Add new version of file and tag data to the undo list.
	/// @return Undo key generated, i.e. at least one of \a fileName or \a fileTag caused a change.
	/// @details The function always takes the ownership of \a fileName and \a fileTag.
	/// If the values are identical to the current state or an argument is \c nullptr no action is taken.
	bool apply_changes(File_Name *fileName, File_Tag *fileTag);

	/// @return \c true if file contains undo data (filename or tag)
	bool has_undo_data() const
	{	return FileName.undo_key() || FileTag.undo_key(); }
	/// @return \c true if file contains redo data (filename or tag)
	bool has_redo_data() const
	{	return FileName.redo_key() || FileTag.redo_key(); }

	/// Applies one undo to the ETFile data (to reload the previous data).
	/// @return \c true if an undo had been applied.
	bool undo();
	/// Applies one redo to the ETFile data (to reload the previous data).
	/// @return \c true if an undo had been applied.
	bool redo();

	/// Last used UndoKey.
	/// @remarks All changes with keys larger than this value are done after the time of the function call.
	static unsigned current_undo_key() { return ETUndoKey; }

	/// \c TRUE if undo file list contains undo data.
	static gboolean has_global_undo() { return ETHistoryFileListRedo > 0; }
	/// \c TRUE if undo file list contains redo data.
	static gboolean has_global_redo() { return ETHistoryFileListRedo < ETHistoryFileList.size(); }

	/// Execute one 'undo' in the main undo list
	static ET_File* global_undo();
	/// Execute one 'redo' in the main undo list
	static ET_File* global_redo();

	/// Discard any global undo history
	static void reset_undo_history() { ETHistoryFileListRedo = 0; ETHistoryFileList.clear(); }

	bool is_filename_saved() const { return FileName.is_saved(); }
	bool is_filetag_saved() const { return FileTag.is_saved() && !force_tag_save_; }
	/// Checks if the current files had been changed but not saved.
	/// @return \c true if the file has been saved. \c false if some changes haven't been saved.
	bool is_saved() const { return is_filename_saved() && is_filetag_saved(); }

	/// Used by read_tag implementations to identify invisible changes,
	/// e.g. automatic tag version upgrades.
	void force_tag_save() { force_tag_save_ = true; }

	bool autofix();

	gboolean save_file_tag(GError **error);
	gboolean rename_file(GError **error);

	struct UpdateDirectoyNameArgs
	{	gString OldPath; ///< Old name in file system encoding
		gString NewPath; ///< New name in file system encoding
		gString OldPathUTF8; ///< Old name as normalized UTF-8
		gString NewPathUTF8; ///< New name as normalized UTF-8
		const char* OldPathRelUTF8; ///< Old name relative to root as UTF-8 if applicable
		const char* NewPathRelUTF8; ///< New name relative to root as UTF-8 if applicable
		/// @param old_path Absolute path of the previous directory name in file system encoding.
		/// @param new_path Absolute path of the new directory name in file system encoding.
		/// @param root Current root path if any.
		UpdateDirectoyNameArgs(const gchar *old_path, const gchar *new_path, const gchar* root);
	};
	/// Notify about a directory rename operation.
	/// @returns \c true if the operation caused a change.
	/// @remarks This is basically a find and replace operation.
	/// But only persisted file names are updated. Unsaved matches are ignored.
	bool update_directory_name(const UpdateDirectoyNameArgs& args);

	static gint (*get_comp_func(EtSortMode sort_mode))(const ET_File *ETFile1, const ET_File *ETFile2);
	static gint (*get_comp_func(EtBrowserMode browser_mode))(const ET_File *ETFile1, const ET_File *ETFile2);
};

#endif /* !ET_FILE_H_ */
