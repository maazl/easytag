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
	static list_type FileList;

	static EtBrowserMode BrowserMode;

	static unsigned Start;         ///< Start of currently visible window of files from ETFilesList.
	static unsigned End;           ///< End of currently visible window of files from ETFilesList.
	static guint64 TotalSize;      ///< Total of the size of files in displayed list (in bytes)
	static double TotalDuration;   ///< Total of duration of files in displayed list (in seconds)

	/// Index of starting points of all albums within FileList ordered by artist, album.
	/// @remarks The indices refer to FileList ordered by artist, album too.
	/// This is not true if ET_BROWSER_MODE_ARTIST, i.e. "all albums" view.
	/// In this case the entire artist range need to be scanned if a single album should be accessed.\n
	/// The index is not available in ET_BROWSER_MODE_FILE.
	static index_type ArtistAlbumIndex;

	static void renumber();
	static void calc_totals();

	/// Get the range of matching entries in ArtistAlbumIndex that contains an IndexKey of a file.
	/// @remarks This must be exactly one match.
	static index_range_type artist_album_index_find(unsigned index);
	/// Enlarge range to cover artist rather than only one album.
	static void index_artist_range(index_range_type& range);

public:
	static void clear();

	static bool empty() { return FileList.empty(); }
	static const list_type& all_files() { return FileList; }
	/// Set a new list of visible files.
	/// @details This resets all state information except for SortMode and BrowserMode.
	/// You should call \ref display_file afterwards to set the focus to a certain file.
	static void set_file_list(list_type&& list);

	static range_type visible_range()
	{	return range_type(FileList.begin() + Start, FileList.begin() + End); }
	/// Number of currently visible files.
	static unsigned visible_size() { return End - Start; }
	/// Offset of e file within the current visible range.
	/// @pre \a file must be within the current visible range.
	static unsigned visible_index(const ET_File* file) { return file->IndexKey - Start; }

	static guint64 visible_total_bytes() { return TotalSize; }
	static double visible_total_duration() { return TotalDuration; }

	/// Switch the display mode
	/// @param artist Only show this artist. Passing a null string selects files w/o an artist.
	/// @param album Only show this album. Passing a null string selects files w/o an artist.
	/// @details Omitting \a album or \a artist implicitly selects EtBrowserMode.
	/// - both omitted => `ET_BROWSER_MODE_FILE` => use the entire file list.\n
	/// - album omitted => `ET_BROWSER_MODE_ARTIST` => filter by artist.\n
	/// - both supplied => `ET_BROWSER_MODE_ARTIST_ALBUM` => filter by current artist and album.
	/// @remarks Calling this function updates (or discards) ArtistAlbumIndex
	/// and adjusts the result of \ref all_files.
	static void set_visible_range(const xStringD0* artist = nullptr, const xStringD0* album = nullptr);

	/// Switch the display mode
	/// @param mode
	/// - `ET_BROWSER_MODE_FILE` => use the entire file list.\n
	/// - `ET_BROWSER_MODE_ARTIST` => filter by current artist.\n
	/// - `ET_BROWSER_MODE_ARTIST_ALBUM` => filter by current artist and album.
	/// @remarks Calling this function updates (or discards) ArtistAlbumIndex
	/// and adjusts the result of \ref all_files. The function preselect artist an album
	/// in a way that the currently displayed file is still visible.
	static void set_display_mode(EtBrowserMode mode);

	/// Locate index entry within the global file list.
	static iterator_type to_file_index(index_iterator_type ix)
	{ return ix == ArtistAlbumIndex.end() ? FileList.end() : FileList.begin() + ix->Start; }
	/// Slices for each album.
	/// @remarks Only valid unless `ET_BROWSER_MODE_FILE`.
	static const index_type& artist_album_index() { return ArtistAlbumIndex; }
	/// Get the range of matching entries in ArtistAlbumIndex that contains an IndexKey of a file.
	/// @remarks This must be exactly one match.
	static index_range_type artist_album_index_find(const ET_File* file)
	{	return artist_album_index_find(file->IndexKey); }

	/// Convert an ArtistAlbumIndex range to a FileList range.
	static range_type to_file_range(index_range_type range);
	/// Check wether a file is in an index range.
	static bool is_in_range(const index_range_type& range, const ET_File* file)
	{	return file->IndexKey >= range.first->Start && (range.second == ArtistAlbumIndex.end() || range.second->Start > file->IndexKey); }
	/// Count the number of files in an index range.
	static unsigned files_in_range(const index_range_type& range)
	{ return (range.second == ArtistAlbumIndex.end() ? FileList.size() : range.second->Start) - range.first->Start; }
	/// Return the range of files matching the given artist.
	/// @param artist Artist to search for.
	/// @return matching range, empty if artist is not found.
	/// @remarks The result may not reflect recent changes.
	/// This is intended to prevent files from hopping out of the current scope.
	static index_range_type matching_range(xStringD0 artist);
	/// Return the range of files matching the given artist and album.
	/// @param artist Artist to search for.
	/// @param album Album of the artist to search for.
	/// @return matching range, empty if no match is not found.
	/// @remarks The result may not reflect recent changes.
	/// This is intended to prevent files from hopping out of the current scope.
	static index_range_type matching_range(xStringD0 artist, xStringD0 album);

	/// Returns: \c true if all files have been saved, \c false otherwise.
	static bool check_all_saved();

	/// Update path of file names after directory rename
	/// @param file_list this pointer
	/// @param old_path Old <em>relative</em> path with respect to current root.
	/// @param new_path New <em>relative</em> path with respect to current root.
	static void update_directory_name(const gchar *old_path, const gchar *new_path);
	/// Remove a file from the list.
	static void remove_file(ET_File *etfile);
};


#endif /* !ET_FILE_H_ */
