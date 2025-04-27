/* EasyTAG - tag editor for audio files
 * Copyright (C) 2014-2016  David King <amigadave@amigadave.com>
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

#include "config.h"

#include "file_list.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "application_window.h"
#include "browser.h"
#include "charset.h"
#include "easytag.h"
#include "log.h"
#include "misc.h"
#include "file.h"
#include "picture.h"
#include "file_name.h"
#include "file_tag.h"

#include <algorithm>

using namespace std;

vector<xPtr<ET_File>> ET_FileList::FileList;

unsigned ET_FileList::Start = 0;
unsigned ET_FileList::End = 0;
guint64 ET_FileList::TotalSize = 0;
double ET_FileList::TotalDuration = 0;

EtBrowserMode ET_FileList::BrowserMode = ET_BROWSER_MODE_FILE;

ET_FileList::index_type ET_FileList::ArtistAlbumIndex;


void ET_FileList::renumber()
{	unsigned index = 0;
	for (auto& file : FileList)
		file->IndexKey = index++;
}

void ET_FileList::calc_totals()
{
	TotalSize = 0;
	TotalDuration = 0;
	auto range = visible_range();
	for (auto i = range.first; i != range.second; ++i)
	{	TotalSize     += (*i)->FileSize;
		TotalDuration -= (*i)->ETFileInfo.duration;
	}
}

ET_FileList::index_range_type ET_FileList::artist_album_index_find(unsigned index)
{	index_range_type result;
	result.first = result.second = upper_bound(ArtistAlbumIndex.begin(), ArtistAlbumIndex.end(), index_entry(nullptr, nullptr, index),
		[](const index_entry& l, const index_entry& r) { return l.Start < r.Start; });
	--result.first;
	return result;
}

void ET_FileList::index_artist_range(index_range_type& range)
{	// Linear search is typically faster than binary search in this case.
	const xStringD0& artist = range.first->Artist; // due to recent changes this might not match file->FileTagNew()->Artist
	while (range.second != ArtistAlbumIndex.end() && range.second->Artist == artist)
		++range.second;
	while (range.first != ArtistAlbumIndex.begin() && range.first[-1].Artist == artist)
		--range.first;
}

void ET_FileList::clear()
{
	Start = End = 0;
	TotalSize = 0;
	TotalDuration = 0;
	FileList.clear();
	ArtistAlbumIndex.clear();
}

void ET_FileList::set_file_list(list_type&& list)
{	clear();
	FileList.swap(list);
	renumber();

	set_display_mode(BrowserMode);
}

void ET_FileList::set_visible_range(const xStringD0* artist, const xStringD0* album)
{
	if (!artist)
	{	Start = 0;
		End = FileList.size();
		BrowserMode = ET_BROWSER_MODE_FILE;
		return; // mode file requires no update
	}

	g_return_if_fail(ArtistAlbumIndex.size() || !FileList.size());

	BrowserMode = !album ? ET_BROWSER_MODE_ARTIST : ET_BROWSER_MODE_ARTIST_ALBUM;

	// adjust visible range
	auto range = album ? matching_range(*artist, *album) : matching_range(*artist);
	End = FileList.size();
	if (range.first == range.second)
		Start = End;
	else
	{	Start = range.first->Start;
		if (range.second != ArtistAlbumIndex.end())
			End = range.second->Start;
	}

	calc_totals();
}

void ET_FileList::set_display_mode(EtBrowserMode mode)
{
	if (!mode == ET_BROWSER_MODE_FILE)
	{	Start = 0;
		End = FileList.size();
		BrowserMode = ET_BROWSER_MODE_FILE;
		return; // mode file requires no update
	}

	BrowserMode = mode;

	ArtistAlbumIndex.clear();

	if (!FileList.empty())
	{	//(re)create ArtistAlbumIndex
		auto cmp = ET_File::get_comp_func(ET_BROWSER_MODE_ARTIST_ALBUM);
		sort(FileList.begin(), FileList.end(), [cmp](const ET_File* l, const ET_File* r)
		{	return cmp(l, r) < 0;
		});

		renumber();

		// create ArtistAlbumIndex
		xStringD0 lastArtist;
		xStringD0 lastAlbum;
		unsigned i = 0;
		for (const ET_File* file : FileList)
		{	const File_Tag* tag = file->FileTagNew();
			if (ArtistAlbumIndex.empty() || lastAlbum != tag->album || lastArtist != tag->artist)
			{	lastArtist = tag->artist;
				lastAlbum = tag->album;
				ArtistAlbumIndex.emplace_back(index_entry(lastArtist, lastAlbum, i));
			}
			++i;
		}
	}
}

ET_FileList::range_type ET_FileList::to_file_range(index_range_type range)
{	range_type result;
	result.second = FileList.end();
	if (range.first == range.second)
		result.first = result.second;
	else
	{	result.first = FileList.begin() + range.first->Start;
		if (range.second != ArtistAlbumIndex.end())
			result.second = FileList.begin() + range.second->Start;
	}
	return result;
}

ET_FileList::index_range_type ET_FileList::matching_range(xStringD0 artist)
{	return equal_range(ArtistAlbumIndex.begin(), ArtistAlbumIndex.end(),
		index_entry(move(artist), nullptr, 0),
		[](const index_entry& l, const index_entry& r)
		{	return l.Artist.compare(r.Artist) < 0;
		});
}

ET_FileList::index_range_type ET_FileList::matching_range(xStringD0 artist, xStringD0 album)
{	return equal_range(ArtistAlbumIndex.begin(), ArtistAlbumIndex.end(),
		index_entry(move(artist), move(album), 0),
		[](const index_entry& l, const index_entry& r)
		{	int c = l.Artist.compare(r.Artist);
			return c < 0 || (c == 0 && l.Album.compare(r.Album) < 0);
		});
}

/*
 * Delete the corresponding file and free the allocated data.
 */
void ET_FileList::remove_file(ET_File *etfile)
{
	unsigned index = etfile->IndexKey;
	g_return_if_fail(index < FileList.size());
	g_return_if_fail(FileList[index] == etfile);

	// Remove infos of the file
	if (index >= Start && index < End)
	{	TotalSize     -= etfile->FileSize;
		TotalDuration -= etfile->ETFileInfo.duration;
	}

	// adjust positions
	if (Start > index)
		--Start;
	if (End > index)
		--End;

	// erase and renumber
	for (auto i = FileList.begin() + index; ++i != FileList.end();)
	{	(*i)->IndexKey = index++;
		i[-1] = move(*i);
	}
	FileList.erase(--FileList.end());

	// adjust artist/album index too
	auto i = lower_bound(ArtistAlbumIndex.begin(), ArtistAlbumIndex.end(), index_entry(nullptr, nullptr, index),
			[](const index_entry& l, const index_entry& r) { return l.Start < r.Start; });
	if (i != ArtistAlbumIndex.end())
		while (++i != ArtistAlbumIndex.end())
			--i->Start;
}


/*
 * Function used to update path of filenames into list after renaming a parent directory
 * (for ex: "/mp3/old_path/file.mp3" to "/mp3/new_path/file.mp3"
 */
void ET_FileList::update_directory_name(const gchar *old_path, const gchar *new_path)
{
	g_return_if_fail (!et_str_empty (old_path));
	g_return_if_fail (!et_str_empty (new_path));

	ET_File::UpdateDirectoyNameArgs args(old_path, new_path,
		et_application_window_get_current_path_name(MainWindow));

	for (auto& file : FileList)
		file->update_directory_name(args);
}

/*
 * et_file_list_check_all_saved:
 * @etfilelist: (element-type ET_File) (allow-none): a list of files
 *
 * Checks if some files, in the list, have been changed but not saved.
 *
 * Returns: %TRUE if all files have been saved, %FALSE otherwise
 */
bool ET_FileList::check_all_saved()
{
	for (const ET_File* ETFile : FileList)
		if (!ETFile->is_saved())
			return false;
	return true;
}
