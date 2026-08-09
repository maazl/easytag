/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022-2026  Marcel Müller <github@maazl.de>
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

ET_FileList::ET_FileList()
:	BrowserMode(ET_BROWSER_MODE_FILE)
,	SortFunc(nullptr)
{}

void ET_FileList::sort_list()
{	if (FileList.empty())
		return;

	if (BrowserMode != ET_BROWSER_MODE_FILE)
	{	auto cmp = ET_File::get_comp_func(ET_BROWSER_MODE_ARTIST_ALBUM);
		// sort by artist, album and SortFunc in one step.
		sort(FileList.begin(), FileList.end(), [cmp, SortFunc = this->SortFunc](const ET_File* l, const ET_File* r)
		{	int c = cmp(l, r);
			if (c) return c < 0;
			if (SortFunc) return (*SortFunc)(l, r) < 0;
			return false;
		});
	} else if (SortFunc)
	{	sort(FileList.begin(), FileList.end(), [SortFunc = this->SortFunc](const ET_File* l, const ET_File* r)
		{	return SortFunc(l, r) < 0;
		});
	}

	// renumber
	const ET_File* last = nullptr;
	bool activate_bg_color = false;
	unsigned index = 0;

	for (auto& file : FileList)
	{	file->IndexKey = index++;
		file->activate_bg_color = activate_bg_color ^= last && SortFunc && abs(SortFunc(last, file)) == 1;
		last = file;
	}
}

void ET_FileList::calc_totals()
{
	TotalSize = 0;
	TotalDuration = 0;
	auto range = visible_range();
	for (auto i = range.first; i != range.second; ++i)
	{	TotalSize     += (*i)->FileSize;
		TotalDuration += (*i)->ETFileInfo.duration;
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
	// apply sort order & refresh indices
	set_display_mode(BrowserMode);
}

ET_File* ET_FileList::is_valid(ET_File* file) const noexcept
{	return file && file->IndexKey >= Start && file->IndexKey < End && FileList[file->IndexKey] == file
		? file : nullptr;
}

void ET_FileList::set_visible_range(const xStringD0* artist, const xStringD0* album)
{
	if (!artist)
	{	Start = 0;
		End = FileList.size();
		BrowserMode = ET_BROWSER_MODE_FILE;
	} else
	{	g_return_if_fail(ArtistAlbumIndex.size() || !FileList.size());

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
	}

	calc_totals();
}

void ET_FileList::set_display_mode(EtBrowserMode mode)
{
	BrowserMode = mode;
	ArtistAlbumIndex.clear();
	sort_list();

	if (mode == ET_BROWSER_MODE_FILE || FileList.empty())
	{	Start = 0;
		End = FileList.size();
		return; // mode file requires no update
	}

	// create ArtistAlbumIndex
	xStringD0 lastArtist;
	xStringD0 lastAlbum;
	unsigned i = 0;
	for (const ET_File* file : FileList)
	{	const File_Tag* tag = file->FileTagNew();
		if (ArtistAlbumIndex.empty() || lastAlbum != tag->album || lastArtist != tag->artist)
		{	lastArtist = tag->artist;
			lastAlbum = tag->album;
			ArtistAlbumIndex.emplace_back(lastArtist, lastAlbum, i);
		}
		++i;
	}
}

void ET_FileList::set_sort_func(gint (*func)(const ET_File *ETFile1, const ET_File *ETFile2))
{
	g_return_if_fail(func);
	if (SortFunc == func)
		return;

	SortFunc = func;
	sort_list();
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

bool ET_FileList::any_unsaved_in_range(const index_range_type& range)
{	auto file_range = to_file_range(range);
	for (auto i = file_range.first; i < file_range.second; ++i)
		if (!i->get()->is_saved())
			return true;
	return false;
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
	etfile->IndexKey = ~0; // invalidate

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
void ET_FileList::update_directory_name(const UpdateDirectoyNameArgs& args)
{
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

gboolean EtFileList::to_iter(GtkTreeIter* iter, const ET_File* file, const ET_FileList& model) noexcept
{	memset(iter, 0, sizeof *iter);
	if (!file)
		return FALSE;
	iter->user_data = (gpointer)file;
	iter->stamp = stamp(model);
	return TRUE;
};

ET_File* EtFileList::from_iter(const GtkTreeIter& iter) const noexcept
{	return is_valid(from_iter(&iter, *this));
}

bool EtFileList::to_iter(GtkTreeIter& iter, const ET_File* file) const noexcept
{	return to_iter(&iter, is_valid(file), *this);
}

void EtFileList::file_changed(const ET_File* etfile)
{
	if (!is_valid(etfile))
		return;

	GtkTreeIter iter;
	to_iter(iter, etfile);
	GtkTreePath* path = gtk_tree_path_new_from_indices(visible_index(etfile), -1);
	g_signal_emit_by_name(this, "row-changed", path, &iter);
	gtk_tree_path_free(path);
}

void EtFileList::remove_file(ET_File *etfile)
{	GtkTreePath* path = NULL;
	if (is_valid(etfile))
		path = gtk_tree_path_new_from_indices(visible_index(etfile), -1);

	ET_FileList::remove_file(etfile);

	if (path)
	{	g_signal_emit_by_name(this, "row_deleted", path);
		gtk_tree_path_free(path);
	}
}

void EtFileList::init_TreeModel(GtkTreeModelIface* iface)
{
	iface->get_flags = [](GtkTreeModel* model) { return GTK_TREE_MODEL_ITERS_PERSIST; };
	iface->get_n_columns = [](GtkTreeModel* model) { return 0; };

	iface->get_iter = [](GtkTreeModel* model, GtkTreeIter* iter, GtkTreePath* path)
	{	ET_FileList& that = me(model);
		return to_iter(iter, that[*gtk_tree_path_get_indices(path)], that);
	};
	iface->get_path = [](GtkTreeModel* model, GtkTreeIter* iter) -> GtkTreePath*
	{	ET_FileList& that = me(model);
		const ET_File* file = that.is_valid(from_iter(iter, that));
		if (file)
			return gtk_tree_path_new_from_indices(that.visible_index(file), -1);
		else
			return nullptr;
	};

	iface->iter_next = [](GtkTreeModel* model, GtkTreeIter* iter)
	{	ET_FileList& that = me(model);
		const ET_File* file = that.is_valid(from_iter(iter, that));
		if (file)
			file = that[that.visible_index(file) + 1];
		return update_iter(iter, file);
	};
	iface->iter_previous = [](GtkTreeModel* model, GtkTreeIter* iter)
	{	ET_FileList& that = me(model);
		const ET_File* file = that.is_valid(from_iter(iter, that));
		if (file)
			file = that[that.visible_index(file) - 1];
		return update_iter(iter, file);
	};
	iface->iter_children = [](GtkTreeModel* model, GtkTreeIter* iter, GtkTreeIter* parent)
	{	ET_FileList& that = me(model);
		return to_iter(iter, parent ? nullptr : that[0], that);
	};
	iface->iter_has_child = [](GtkTreeModel* model, GtkTreeIter* iter)
	{ return FALSE; };
	iface->iter_n_children = [](GtkTreeModel* model, GtkTreeIter* iter)
	{	return iter ? 0 : (gint)me(model).visible_size(); };
	iface->iter_nth_child = [](GtkTreeModel* model, GtkTreeIter* iter, GtkTreeIter* parent, gint n)
	{	ET_FileList& that = me(model);
		return to_iter(iter, parent ? nullptr : that[(unsigned)n], that);
	};
	iface->iter_parent = [](GtkTreeModel* model, GtkTreeIter* iter, GtkTreeIter* child)
	{	memset(iter, 0, sizeof *iter);
		return FALSE;
	};
}

G_DEFINE_TYPE_WITH_CODE(EtFileList, et_file_list, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL, &EtFileList::init_TreeModel));

static void et_file_list_init(EtFileList* fl)
{	new((ET_FileList*)fl) ET_FileList();
}

static void et_file_list_class_init(EtFileListClass* klass)
{	klass->parent_class.finalize = [](GObject* object)
	{	((ET_FileList*)ET_FILE_LIST(object))->~ET_FileList();
		G_OBJECT_CLASS(et_file_list_parent_class)->finalize(object);
	};
}
