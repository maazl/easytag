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
#include "charset.h"
#include "easytag.h"
#include "log.h"
#include "misc.h"
#include "file.h"
#include "picture.h"
#include "file_name.h"
#include "file_tag.h"

using namespace std;


vector<xPtr<ET_File>> ET_FileList::ETHistoryFileList;
unsigned ET_FileList::ETHistoryFileListRedo = 0;


void ET_FileList::reset()
{
	ETHistoryFileList.clear();
	ETHistoryFileListRedo = 0;
}


/*
 * et_file_list_free:
 * @file_list: (element-type ET_File) (allow-none): a list of files
 *
 * Frees the full list of files.
 */
void
et_file_list_free (GList *file_list)
{
    g_return_if_fail (file_list != NULL);

    g_list_free_full (file_list, [](gpointer file) { xPtr<ET_File>::fromCptr((ET_File*)file); });
}

/*
 * "Display" list contains only pointers, so NOTHING to free
 */
void
et_displayed_file_list_free (GList *file_list)
{
}

/*
 * ArtistAlbum list contains 3 levels of lists
 */
void
et_artist_album_file_list_free (GList *file_list)
{
    GList *l;

    g_return_if_fail (file_list != NULL);

    /* Pointers are stored inside the artist/album list-stores, so free them
     * first. */
    et_application_window_browser_clear_artist_model (ET_APPLICATION_WINDOW (MainWindow));
    et_application_window_browser_clear_album_model (ET_APPLICATION_WINDOW (MainWindow));

    for (l = file_list; l != NULL; l = g_list_next (l))
    {
        GList *m;

        for (m = (GList *)l->data; m != NULL; m = g_list_next (m))
        {
            GList *n = (GList *)m->data;
            if (n)
                g_list_free (n);
        }

        if (l->data) /* Free AlbumList list. */
            g_list_free ((GList *)l->data);
    }

    g_list_free (file_list);
}

/*
 * Comparison function for sorting by ascending artist in the ArtistAlbumList.
 */
static gint
ET_Comp_Func_Sort_Artist_Item_By_Ascending_Artist (const GList *AlbumList1,
                                                   const GList *AlbumList2)
{
    const GList *etfilelist1 = NULL;
    const GList *etfilelist2 = NULL;
    const ET_File *etfile1 = NULL;
    const ET_File *etfile2 = NULL;
    const gchar *etfile1_artist;
    const gchar *etfile2_artist;

    if (!AlbumList1 || !(etfilelist1 = (GList *)AlbumList1->data)
        || !(etfile1 = (ET_File *)etfilelist1->data))
    {
        return -1;
    }

    if (!AlbumList2 || !(etfilelist2 = (GList *)AlbumList2->data)
        || !(etfile2 = (ET_File *)etfilelist2->data))
    {
        return 1;
    }

    etfile1_artist = etfile1->FileTagNew()->artist;
    etfile2_artist = etfile2->FileTagNew()->artist;

    if (g_settings_get_boolean (MainSettings, "sort-case-sensitive"))
    {
        return et_normalized_strcmp0 (etfile1_artist, etfile2_artist);
    }
    else
    {
        return et_normalized_strcasecmp0 (etfile1_artist, etfile2_artist);
    }
}

/*
 * Comparison function for sorting by ascending album in the ArtistAlbumList.
 */
static gint
ET_Comp_Func_Sort_Album_Item_By_Ascending_Album (const GList *etfilelist1,
                                                 const GList *etfilelist2)
{
    const ET_File *etfile1;
    const ET_File *etfile2;
    const gchar *etfile1_album;
    const gchar *etfile2_album;

    if (!etfilelist1 || !(etfile1 = (ET_File *)etfilelist1->data))
    {
        return -1;
    }

    if (!etfilelist2 || !(etfile2 = (ET_File *)etfilelist2->data))
    {
        return 1;
    }

    etfile1_album  = etfile1->FileTagNew()->album;
    etfile2_album  = etfile2->FileTagNew()->album;

    if (g_settings_get_boolean (MainSettings, "sort-case-sensitive"))
    {
        return et_normalized_strcmp0 (etfile1_album, etfile2_album);
    }
    else
    {
        return et_normalized_strcasecmp0 (etfile1_album, etfile2_album);
    }
}

/*
 * The ETArtistAlbumFileList contains 3 levels of lists to sort the ETFile by artist then by album :
 *  - "ETArtistAlbumFileList" list is a list of "ArtistList" items,
 *  - "ArtistList" list is a list of "AlbumList" items,
 *  - "AlbumList" list is a list of ETFile items.
 * Note : use the function ET_Debug_Print_Artist_Album_List(...) to understand how it works, it needed...
 */
static void
et_artist_album_list_add_file (gListP<gListP<gListP<ET_File*>>>& file_list, ET_File *ETFile)
{
    const gchar *ETFile_Artist;
    const gchar *ETFile_Album;
    const gchar *etfile_artist;
    const gchar *etfile_album;
    gListP<gListP<ET_File*>> AlbumList = NULL;
    gListP<ET_File*> etfilelist = NULL;
    ET_File *etfile = NULL;

    g_return_if_fail (ETFile != NULL);

    /* Album value of the ETFile passed in parameter. */
    ETFile_Album = ETFile->FileTagNew()->album;
    /* Artist value of the ETFile passed in parameter. */
    ETFile_Artist = ETFile->FileTagNew()->artist;

    for (gListP<gListP<gListP<ET_File*>>> ArtistList = file_list; ArtistList; ArtistList = ArtistList->next)
    {
        AlbumList = ArtistList->data;  /* Take the first item */
        /* Take the first item, and the first etfile item. */
        if (AlbumList && (etfilelist = AlbumList->data)
            && (etfile = etfilelist->data)
            && etfile->FileTagNew() != NULL)
        {
            etfile_artist = etfile->FileTagNew()->artist;
        }
        else
        {
            etfile_artist = NULL;
        }

        if ((etfile_artist &&  ETFile_Artist && strcmp (etfile_artist,
                                                        ETFile_Artist) == 0)
            || (!etfile_artist && !ETFile_Artist)) /* The "artist" values correspond? */
        {
            /* The "ArtistList" item was found! */
            while (AlbumList)
            {
                if ((etfilelist = AlbumList->data)
                    && (etfile = etfilelist->data)
                    && etfile->FileTagNew() != NULL)
                {
                    etfile_album = etfile->FileTagNew()->album;
                }
                else
                {
                    etfile_album = NULL;
                }

                if ((etfile_album && ETFile_Album && strcmp (etfile_album,
                                                             ETFile_Album) == 0)
                    || (!etfile_album && !ETFile_Album))
                    /* The "album" values correspond? */
                {
                    /* The "AlbumList" item was found!
                     * Add the ETFile to this AlbumList item */
                    AlbumList->data = AlbumList->data.insert_sorted(ETFile, ET_File::get_comp_func(ET_SORT_MODE_ASCENDING_FILENAME));
                    return;
                }

                AlbumList = AlbumList->next;
            }

            /* The "AlbumList" item was NOT found! => Add a new "AlbumList"
             * item (+...) item to the "ArtistList" list. */
            etfilelist = gListP<ET_File*>(ETFile);
            ArtistList->data = ArtistList->data.insert_sorted(etfilelist, &ET_Comp_Func_Sort_Album_Item_By_Ascending_Album);
            return;
        }
    }

    /* The "ArtistList" item was NOT found! => Add a new "ArtistList" to the
     * main list (=ETArtistAlbumFileList). */
    etfilelist = gListP<ET_File*>(ETFile);
    AlbumList  = gListP<gListP<ET_File*>>(etfilelist);

    /* Sort the list by ascending Artist. */
    file_list = file_list.insert_sorted(AlbumList, ET_Comp_Func_Sort_Artist_Item_By_Ascending_Artist);
}

GList *
et_artist_album_list_new_from_file_list (GList *file_list)
{
    gListP<gListP<gListP<ET_File*>>> result = NULL;
    GList *l;

    for (l = g_list_first (file_list); l != NULL; l = l-> next)
    {
        ET_File *ETFile = (ET_File *)l->data;
        et_artist_album_list_add_file (result, ETFile);
    }

    return result;
}

/*
 * Delete the corresponding file (allocated data was previously freed!). Return TRUE if deleted.
 */
static gboolean
ET_Remove_File_From_Artist_Album_List (ET_File *ETFile)
{
    GList *ArtistList;
    GList *AlbumList;
    GList *etfilelist;

    g_return_val_if_fail (ETFile != NULL, FALSE);

    /* Search for the ETFile in the list. */
    for (ArtistList = ETCore->ETArtistAlbumFileList; ArtistList != NULL;
         ArtistList = g_list_next (ArtistList))
    {
        for (AlbumList = g_list_first ((GList *)ArtistList->data);
             AlbumList != NULL; AlbumList = g_list_next (AlbumList))
        {
            for (etfilelist = g_list_first ((GList *)AlbumList->data);
                 etfilelist != NULL; etfilelist = g_list_next (etfilelist))
            {
                ET_File *etfile = (ET_File *)etfilelist->data;

                if (ETFile == etfile) /* The ETFile to delete was found! */
                {
                    etfilelist = g_list_remove (etfilelist, ETFile);

                    if (etfilelist) /* Delete from AlbumList. */
                    {
                        AlbumList->data = (gpointer) g_list_first (etfilelist);
                    }
                    else
                    {
                        AlbumList = g_list_remove (AlbumList, AlbumList->data);

                        if (AlbumList) /* Delete from ArtistList. */
                        {
                            ArtistList->data = AlbumList;
                        }
                        else
                        {
                            /* Delete from the main list. */
                            ETCore->ETArtistAlbumFileList = g_list_remove (ArtistList,ArtistList->data);

                            return TRUE;
                        }

                        return TRUE;
                    }

                    return TRUE;
                }
            }
        }
    }

    return FALSE; /* ETFile is NUL, or not found in the list. */
}

/*
 * Returns the length of the list of displayed files
 */
static guint
et_displayed_file_list_length (GList *displayed_list)
{
    GList *list;

    list = g_list_first (displayed_list);
    return g_list_length (list);
}

/*
 * Renumber the list of displayed files (IndexKey) from 1 to n
 */
static void
et_displayed_file_list_renumber (GList *displayed_list)
{
    GList *l = NULL;
    guint i = 1;

    for (l = g_list_first (displayed_list); l != NULL; l = g_list_next (l))
    {
        ((ET_File *)l->data)->IndexKey = i++;
    }
}

/*
 * Delete the corresponding file and free the allocated data. Return TRUE if deleted.
 */
void
ET_Remove_File_From_File_List (ET_File *ETFile)
{
    GList *ETFileList = NULL;          // Item containing the ETFile to delete... (in ETCore->ETFileList)
    GList *ETFileDisplayedList = NULL; // Item containing the ETFile to delete... (in ETCore->ETFileDisplayedList)

    // Remove infos of the file
    ETCore->ETFileDisplayedList_TotalSize     -= ETFile->FileSize;
    ETCore->ETFileDisplayedList_TotalDuration -= ETFile->ETFileInfo.duration;

    // Find the ETFileList containing the ETFile item
    ETFileDisplayedList = g_list_find(g_list_first(ETCore->ETFileDisplayedList),ETFile);
    ETFileList = g_list_find (ETCore->ETFileList, ETFile);

    // Note : this ETFileList must be used only for ETCore->ETFileDisplayedList, and not ETCore->ETFileDisplayed
    if (ETCore->ETFileDisplayedList == ETFileDisplayedList)
    {
        if (ETFileList->next)
            ETCore->ETFileDisplayedList = ETFileDisplayedList->next;
        else if (ETFileList->prev)
            ETCore->ETFileDisplayedList = ETFileDisplayedList->prev;
        else
            ETCore->ETFileDisplayedList = NULL;
    }
    // If the current displayed file is just removing, it will be unable to display it again!
    if (ETCore->ETFileDisplayed == ETFile)
    {
        if (ETCore->ETFileDisplayedList)
            ETCore->ETFileDisplayed = (ET_File *)ETCore->ETFileDisplayedList->data;
        else
            ETCore->ETFileDisplayed = (ET_File *)NULL;
    }

    /* Remove the file from the ETFileList list. */
    ETCore->ETFileList = g_list_remove (ETCore->ETFileList, ETFile);

    // Remove the file from the ETArtistAlbumList list
    ET_Remove_File_From_Artist_Album_List(ETFile);

    /* Remove the file from the ETFileDisplayedList list (if not already). */
    ETCore->ETFileDisplayedList = g_list_remove (g_list_first (ETCore->ETFileDisplayedList),
                                                 ETFile);

    // Free data of the file
    xPtr<ET_File>::fromCptr(ETFile);

    /* Recalculate length of ETFileDisplayedList list. */
    ETCore->ETFileDisplayedList_Length = et_displayed_file_list_length (ETCore->ETFileDisplayedList);

    /* To number the ETFile in the list. */
    et_displayed_file_list_renumber (ETCore->ETFileDisplayedList);

    // Displaying...
    if (ETCore->ETFileDisplayedList)
    {
        if (ETCore->ETFileDisplayed)
        {
            ET_Displayed_File_List_By_Etfile(ETCore->ETFileDisplayed);
        }else if (ETCore->ETFileDisplayedList->data)
        {
            // Select the new file (synchronize index,...)
            ET_Displayed_File_List_By_Etfile((ET_File *)ETCore->ETFileDisplayedList->data);
        }
    }else
    {
        // Reinit the tag and file area
        et_application_window_file_area_clear (ET_APPLICATION_WINDOW (MainWindow));
        et_application_window_tag_area_clear (ET_APPLICATION_WINDOW (MainWindow));
        et_application_window_update_actions (ET_APPLICATION_WINDOW (MainWindow));
    }
}

/**************************
 * File sorting functions *
 **************************/

/*
 * Sort an 'ETFileList'
 */
GList *
ET_Sort_File_List (GList *ETFileList,
                   EtSortMode Sorting_Type)
{
    GList *etfilelist;

    /* Important to rewind before. */
    etfilelist = g_list_first(ETFileList);

    /* Sort... */
    etfilelist = g_list_sort(etfilelist, (GCompareFunc)ET_File::get_comp_func(Sorting_Type));

    return etfilelist;
}

/*
 * Returns the first item of the "displayed list"
 */
GList *
ET_Displayed_File_List_First (void)
{
    ETCore->ETFileDisplayedList = g_list_first(ETCore->ETFileDisplayedList);
    return ETCore->ETFileDisplayedList;
}

/*
 * Returns the previous item of the "displayed list". When no more item, it returns NULL.
 */
GList *
ET_Displayed_File_List_Previous (void)
{
    if (ETCore->ETFileDisplayedList && ETCore->ETFileDisplayedList->prev)
        return ETCore->ETFileDisplayedList = ETCore->ETFileDisplayedList->prev;
    else
        return NULL;
}

/*
 * Returns the next item of the "displayed list".
 * When no more item, it returns NULL to don't "overwrite" the list.
 */
GList *
ET_Displayed_File_List_Next (void)
{
    if (ETCore->ETFileDisplayedList && ETCore->ETFileDisplayedList->next)
        return ETCore->ETFileDisplayedList = ETCore->ETFileDisplayedList->next;
    else
        return NULL;
}

/*
 * Returns the last item of the "displayed list"
 */
GList *
ET_Displayed_File_List_Last (void)
{
    ETCore->ETFileDisplayedList = g_list_last(ETCore->ETFileDisplayedList);
    return ETCore->ETFileDisplayedList;
}

/*
 * Returns the item of the "displayed list" which correspond to the given 'ETFile' (used into browser list).
 */
GList *
ET_Displayed_File_List_By_Etfile (const ET_File *ETFile)
{
    GList *etfilelist;

    etfilelist = g_list_find (g_list_first (ETCore->ETFileDisplayedList),
                              ETFile);

    if (etfilelist)
    {
        /* To "save" the position like in ET_File_List_Next... (not very good -
         * FIXME) */
        ETCore->ETFileDisplayedList = etfilelist;
    }

    return etfilelist;
}

/*
 * Load the list of displayed files (calculate length, size, ...)
 * It contains part (filtrated : view by artists and albums) or full ETCore->ETFileList list
 */
void
et_displayed_file_list_set (GList *ETFileList)
{
    GList *l = NULL;

    ETCore->ETFileDisplayedList = g_list_first(ETFileList);

    ETCore->ETFileDisplayedList_Length = et_displayed_file_list_length (ETCore->ETFileDisplayedList);
    ETCore->ETFileDisplayedList_TotalSize     = 0;
    ETCore->ETFileDisplayedList_TotalDuration = 0.;

    // Get size and duration of files in the list
    for (l = ETCore->ETFileDisplayedList; l != NULL; l = g_list_next (l))
    {
        ETCore->ETFileDisplayedList_TotalSize += ((ET_File *)l->data)->FileSize;
        ETCore->ETFileDisplayedList_TotalDuration += ((ET_File *)l->data)->ETFileInfo.duration;
    }

    /* Sort the file list. */
    ET_Sort_File_List (ETCore->ETFileDisplayedList,
        (EtSortMode)g_settings_get_enum(MainSettings, "sort-mode"));

    /* Synchronize, so that the core file list pointer always points to the
     * head of the list. */
    ETCore->ETFileList = g_list_first (ETCore->ETFileList);

    /* Should renums ETCore->ETFileDisplayedList only! */
    et_displayed_file_list_renumber (ETCore->ETFileDisplayedList);
}

/*
 * Function used to update path of filenames into list after renaming a parent directory
 * (for ex: "/mp3/old_path/file.mp3" to "/mp3/new_path/file.mp3"
 */
void et_file_list_update_directory_name(GList *file_list, const gchar *old_path, const gchar *new_path)
{
	g_return_if_fail (file_list != NULL);
	g_return_if_fail (!et_str_empty (old_path));
	g_return_if_fail (!et_str_empty (new_path));

	ET_File::UpdateDirectoyNameArgs args(old_path, new_path,
		et_application_window_get_current_path_name(ET_APPLICATION_WINDOW(MainWindow)));

	for (GList *filelist = g_list_first(file_list); filelist; filelist = g_list_next(filelist))
	{
		ET_File *file = (ET_File*)filelist->data;
		if (file)
			file->update_directory_name(args);
	}
}

ET_File* ET_FileList::undo()
{
	g_return_val_if_fail(has_undo(), NULL);

	ET_File* ETFile = ETHistoryFileList[--ETHistoryFileListRedo].get();
	ET_Displayed_File_List_By_Etfile(ETFile);
	ETFile->undo();
	return ETFile;
}

/*
 * Execute one 'redo' in the main undo list
 */
ET_File* ET_FileList::redo()
{
	if (!has_redo())
		return NULL;

	ET_File* ETFile = ETHistoryFileList[ETHistoryFileListRedo++].get();
	ET_Displayed_File_List_By_Etfile(ETFile);
	ETFile->redo();
	return ETFile;
}

/*
 * Add a ETFile item to the main undo list of files
 */
void ET_FileList::history_list_add(ET_File *ETFile)
{
    g_return_if_fail(ETFile != NULL);

    /* Add the item to the list (cut end of list from the current element) */
    ETHistoryFileList.erase(ETHistoryFileList.begin() + ETHistoryFileListRedo++, ETHistoryFileList.end());
    ETHistoryFileList.emplace_back(ETFile);
}

/*
 * et_file_list_check_all_saved:
 * @etfilelist: (element-type ET_File) (allow-none): a list of files
 *
 * Checks if some files, in the list, have been changed but not saved.
 *
 * Returns: %TRUE if all files have been saved, %FALSE otherwise
 */
gboolean
et_file_list_check_all_saved (GList *etfilelist)
{
    if (!etfilelist)
    {
        return TRUE;
    }
    else
    {
        GList *l;

        for (l = g_list_first (etfilelist); l != NULL; l = g_list_next (l))
        {
            if (!((ET_File *)l->data)->is_saved())
            {
                return FALSE;
            }
        }

        return TRUE;
    }
}

/*
 * Returns the number of file in the directory of the selected file.
 * Parameter "path" should be in UTF-8
 */
guint
et_file_list_get_n_files_in_path (GList *file_list,
                                  const gchar *path_utf8)
{
    GList *l;
    guint  count = 0;

    g_return_val_if_fail (path_utf8 != NULL, count);

    for (l = g_list_first (file_list); l != NULL; l = g_list_next (l))
    {
        ET_File *ETFile = (ET_File *)l->data;

        if (strcmp(ETFile->FileNameCur()->path().get(), path_utf8) == 0)
            count++;
    }

    return count;
}
