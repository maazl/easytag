/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022-2025  Marcel Müller <github@maazl.de>
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

#include "file.h"

#include <vector>
#include <utility>


struct File_Name;
enum EtBrowserMode : int;
struct UpdateDirectoyNameArgs;

class ET_FileList
{
public:
	typedef std::vector<xPtr<ET_File>> list_type;
	typedef list_type::const_iterator iterator_type;
	typedef std::pair<iterator_type,iterator_type> range_type;

	struct index_entry
	{	friend class ET_FileList;
		xStringD0 Artist;
		xStringD0 Album;
	private:
		unsigned Start;
	public:
		template<typename A, typename B>
		index_entry(A&& a, B&& b, unsigned start = 0) noexcept : Artist(std::forward<A>(a)), Album(std::forward<B>(b)), Start(start) {}
	};
	typedef std::vector<index_entry> index_type;
	typedef index_type::const_iterator index_iterator_type;
	typedef std::pair<index_iterator_type, index_iterator_type> index_range_type;

private:
	/// Files in the currently selected root directory and optionally subdirectories.
	/// @remarks The list contains all files in the currently selected root.
	/// The list may be ordered by artist/album in case of album view to allow visible ranges to be selected by \ref Start and \ref End.
	/// @p ET_File.IndexKey of the entries in the list always match their index in this list.
	list_type FileList;

	EtBrowserMode BrowserMode;

	unsigned Start = 0;         ///< Start of currently visible window of files from ETFilesList.
	unsigned End = 0;           ///< End of currently visible window of files from ETFilesList.
	guint64 TotalSize = 0;      ///< Total of the size of files in displayed list (in bytes)
	double TotalDuration = 0;   ///< Total of duration of files in displayed list (in seconds)

	/// Index of starting points of all albums within FileList ordered by artist, album.
	/// @remarks The indices refer to FileList ordered by artist, album too.
	/// This is not true if ET_BROWSER_MODE_ARTIST, i.e. "all albums" view.
	/// In this case the entire artist range need to be scanned if a single album should be accessed.\n
	/// The index is not available in ET_BROWSER_MODE_FILE.
	index_type ArtistAlbumIndex;

	gint (*SortFunc)(const ET_File *ETFile1, const ET_File *ETFile2);

	void clear();
	void sort_list();
	void calc_totals();

	/// Get the range of matching entries in ArtistAlbumIndex that contains an IndexKey of a file.
	/// @remarks This must be exactly one match.
	index_range_type artist_album_index_find(unsigned index);
	/// Enlarge range to cover artist rather than only one album.
	void index_artist_range(index_range_type& range);

public:
	ET_FileList();

	bool empty() const noexcept { return FileList.empty(); }
	const list_type& all_files() noexcept { return FileList; }
	/// Set a new list of visible files.
	/// @details This resets all state information except for SortMode and BrowserMode.
	/// You should call \ref display_file afterwards to set the focus to a certain file.
	void set_file_list(list_type&& list);

	range_type visible_range() noexcept
	{	return range_type(FileList.begin() + Start, FileList.begin() + End); }
	/// Number of currently visible files.
	unsigned visible_size() const noexcept { return End - Start; }
	/// Check whether a file pointer is valid within the current visible range.
	ET_File* is_valid(ET_File* file) const noexcept;
	const ET_File* is_valid(const ET_File* file) const noexcept { return is_valid((ET_File*)file); }
	/// Offset of e file within the current visible range.
	/// @pre \a file must be within the current visible range.
	unsigned visible_index(const ET_File* file) const { return file->IndexKey - Start; }

	ET_File* operator[](unsigned index) { return index < visible_size() ? FileList[index + Start].get() : nullptr; }

	guint64 visible_total_bytes() const noexcept { return TotalSize; }
	double visible_total_duration() const noexcept { return TotalDuration; }

	/// Switch the display mode
	/// @param artist Only show this artist. Passing a null string selects files w/o an artist.
	/// @param album Only show this album. Passing a null string selects files w/o an artist.
	/// @details Omitting \a album or \a artist implicitly selects EtBrowserMode.
	/// - both omitted => `ET_BROWSER_MODE_FILE` => use the entire file list.\n
	/// - album omitted => `ET_BROWSER_MODE_ARTIST` => filter by artist.\n
	/// - both supplied => `ET_BROWSER_MODE_ARTIST_ALBUM` => filter by current artist and album.
	/// @remarks Calling this function updates (or discards) ArtistAlbumIndex
	/// and adjusts the result of \ref all_files.
	void set_visible_range(const xStringD0* artist = nullptr, const xStringD0* album = nullptr);

	/// Switch the display mode
	/// @param mode
	/// - `ET_BROWSER_MODE_FILE` => use the entire file list.\n
	/// - `ET_BROWSER_MODE_ARTIST` => filter by current artist.\n
	/// - `ET_BROWSER_MODE_ARTIST_ALBUM` => filter by current artist and album.
	/// @remarks Calling this function updates (or discards) ArtistAlbumIndex
	/// and adjusts the result of \ref all_files. The function preselect artist an album
	/// in a way that the currently displayed file is still visible.
	void set_display_mode(EtBrowserMode mode);

	/// Set the sort mode of the files in the visible range.
	void set_sort_func(gint (*func)(const ET_File *ETFile1, const ET_File *ETFile2));

	/// Locate index entry within the global file list.
	iterator_type to_file_index(index_iterator_type ix)
	{ return ix == ArtistAlbumIndex.end() ? FileList.end() : FileList.begin() + ix->Start; }
	/// Slices for each album.
	/// @remarks Only valid unless `ET_BROWSER_MODE_FILE`.
	const index_type& artist_album_index() noexcept { return ArtistAlbumIndex; }
	/// Get the range of matching entries in ArtistAlbumIndex that contains an IndexKey of a file.
	/// @remarks This must be exactly one match.
	index_range_type artist_album_index_find(const ET_File* file)
	{	return artist_album_index_find(file->IndexKey); }

	/// Convert an ArtistAlbumIndex range to a FileList range.
	range_type to_file_range(index_range_type range);
	/// Check whether a file is in an index range.
	bool is_in_range(const index_range_type& range, const ET_File* file)
	{	return file->IndexKey >= range.first->Start && (range.second == ArtistAlbumIndex.end() || range.second->Start > file->IndexKey); }
	/// Count the number of files in an index range.
	unsigned files_in_range(const index_range_type& range)
	{ return (range.second == ArtistAlbumIndex.end() ? FileList.size() : range.second->Start) - range.first->Start; }
	/// All file in range saved?
	bool any_unsaved_in_range(const index_range_type& range);
	/// Return the range of files matching the given artist.
	/// @param artist Artist to search for.
	/// @return matching range, empty if artist is not found.
	/// @remarks The result may not reflect recent changes.
	/// This is intended to prevent files from hopping out of the current scope.
	index_range_type matching_range(xStringD0 artist);
	/// Return the range of files matching the given artist and album.
	/// @param artist Artist to search for.
	/// @param album Album of the artist to search for.
	/// @return matching range, empty if no match is not found.
	/// @remarks The result may not reflect recent changes.
	/// This is intended to prevent files from hopping out of the current scope.
	index_range_type matching_range(xStringD0 artist, xStringD0 album);

	/// Returns: \c true if all files have been saved, \c false otherwise.
	bool check_all_saved();

	/// Notify about a directory rename operation.
	/// @returns \c true if the operation caused a change.
	/// @remarks This is basically a find and replace operation.
	void update_directory_name(const UpdateDirectoyNameArgs& args);
	/// Remove a file from the list.
	void remove_file(ET_File *etfile);
};


/// GObject wrapper with GtkTreeModel interface.
class EtFileList;
struct EtFileListClass
{	GObjectClass parent_class;
};

GType et_file_list_get_type();

#define ET_TYPE_FILE_LIST (et_file_list_get_type())
#define ET_FILE_LIST(object) (G_TYPE_CHECK_INSTANCE_CAST((object), ET_TYPE_FILE_LIST, EtFileList))

/// Facade class to expose ET_FileList as tree model.
/// @details The class only exposes the currently visible range of the underlying file list.
/// The model has no columns. But the iterators contain all required data to fetch the data.
class EtFileList : public GObject, public ET_FileList
{	EtFileList() = delete;
	EtFileList(const EtFileList&) = delete;
	~EtFileList() = delete;
	void operator=(const EtFileList&) = delete;

public:
	static void init_TreeModel(GtkTreeModelIface* iface);

private: // Interface helper functions
	static ET_FileList& me(GtkTreeModel* model) { return *ET_FILE_LIST(model); }
	static gint stamp(const ET_FileList& model) { return (gint)(((intptr_t)&model) >> 3); }
	/// Extract file pointer from iterator
	static ET_File* from_iter(const GtkTreeIter* iter, const ET_FileList& model) noexcept
	{ return iter->stamp == stamp(model) ? (ET_File*)iter->user_data : nullptr; }
	/// Initialize an iterator to reference a file in the model.
	/// @param iter [out] Target iterator
	/// @param file File pointer
	/// @param model context.
	/// @return true unless \a file is NULL.
	static gboolean to_iter(GtkTreeIter* iter, const ET_File* file, const ET_FileList& model) noexcept;
	static gboolean update_iter(GtkTreeIter* iter, const ET_File* file)
	{	iter->user_data = (gpointer)file;
		if (file)
			return TRUE;
		iter->stamp = 0;
		return FALSE;
	}

public:
	/// Extract file pointer from iterator
	ET_File* from_iter(const GtkTreeIter& iter) const noexcept;
	bool to_iter(GtkTreeIter& iter, const ET_File* file) const noexcept;

	/// Send a row changed signal to the tree view if the file is visible.
	void file_changed(const ET_File* etfile);
	/// Remove a file from the list.
	void remove_file(ET_File *etfile);
};

#endif /* !ET_FILE_H_ */
