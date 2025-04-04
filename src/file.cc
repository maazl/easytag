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

#include "config.h"

#include "file.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>

#include "application_window.h"
#include "easytag.h"
#include "file_name.h"
#include "file_tag.h"
#include "file_list.h"
#include "picture.h"
#include "browser.h"
#include "log.h"
#include "misc.h"
#include "setting.h"
#include "charset.h"

#include "win32/win32dep.h"

#include <cmath>
using namespace std;


/*
 * Create a new ET_File structure
 */
ET_File::ET_File(gString&& filepath)
:	FilePath(move(filepath))
,	FileSize(0)
,	FileModificationTime(0)
,	ETFileDescription(nullptr)
,	ETFileInfo{}
,	force_tag_save_(false)
,	activate_bg_color(false)
,	IndexKey(0)
{}

/*
 * Comparison function for sorting by ascending path.
 */
static gint CmpFilepath(const ET_File* ETFile1, const ET_File* ETFile2)
{
	const File_Name *file1 = ETFile1->FileNameCur();
	const File_Name *file2 = ETFile2->FileNameCur();
	// !!!! : Must be the same rules as "Cddb_Track_List_Sort_Func" to be
	// able to sort in the same order files in cddb and in the file list.
	int r = file1->path().compare(file2->path());
	if (r)
		return sign(r);
	return 2 * sign(file1->file().compare(file2->file()));
}

/*
 * Comparison function for sorting by ascending filename.
 */
static gint CmpFilename(const ET_File* ETFile1, const ET_File* ETFile2)
{
	// !!!! : Must be the same rules as "Cddb_Track_List_Sort_Func" to be
	// able to sort in the same order files in cddb and in the file list.
	return sign(ETFile1->FileNameCur()->file().compare(ETFile2->FileNameCur()->file()));
}

/*
 * Compare strings that are likely integers (e.g. track number)
 */
static gint CmpInt(const gchar* val1, const gchar* val2)
{
	if (!val2)
		return !!val1;
	if (!val1)
		return -1;

	int i1, i2, l;
	if (sscanf(val1, "%d%n", &i1, &l) == 1 && l == (int)strlen(val1)
		&& sscanf(val2, "%d%n", &i2, &l) == 1 && l == (int)strlen(val2))
		return sign(i1 - i2);

	// Fallback
	return sign(strcasecmp(val1, val2));
}

/*
 * Comparison function for sorting by ascending track number.
 */
static gint CmpTrackNumber(const ET_File* ETFile1, const ET_File* ETFile2)
{
  const File_Tag *file1 = ETFile1->FileTagNew();
  const File_Tag *file2 = ETFile2->FileTagNew();
	gint r = CmpInt(file1->track, file2->track);
	if (r)
		return r;
	// 2nd criterion
	r = CmpInt(file1->track_total, file2->track_total);
	if (r)
		return 2 * r;
	// 3rd criterion
	return 3 * CmpFilepath(ETFile1, ETFile2);
}

/*
 * Comparison function for sorting by ascending disc number.
 */
static gint CmpDiscNumber(const ET_File* ETFile1, const ET_File* ETFile2)
{
  const File_Tag *file1 = ETFile1->FileTagNew();
  const File_Tag *file2 = ETFile2->FileTagNew();
	gint r = CmpInt(file1->disc_number, file2->disc_number);
	if (r)
		return r;
	// 2nd criterion
	r = CmpInt(file1->disc_total, file2->disc_total);
	if (r)
		return 2 * r;
	// 3rd criterion
	return 3 * CmpTrackNumber(ETFile1, ETFile2);
}

/*
 * Comparison function for sorting by ascending creation date.
 */
static gint CmpCreationDate(const ET_File* ETFile1, const ET_File* ETFile2)
{
    GFile *file;
    GFileInfo *info;
    guint64 time1 = 0;
    guint64 time2 = 0;

    /* TODO: Report errors? */
    file = g_file_new_for_path (ETFile1->FilePath);
    info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_CHANGED,
                              G_FILE_QUERY_INFO_NONE, NULL, NULL);

    g_object_unref (file);

    if (info)
    {
        time1 = g_file_info_get_attribute_uint64 (info,
                                                  G_FILE_ATTRIBUTE_TIME_CHANGED);
        g_object_unref (info);
    }

    file = g_file_new_for_path (ETFile2->FilePath);
    info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_CHANGED,
                              G_FILE_QUERY_INFO_NONE, NULL, NULL);

    g_object_unref (file);

    if (info)
    {
        time2 = g_file_info_get_attribute_uint64 (info,
                                                  G_FILE_ATTRIBUTE_TIME_CHANGED);
        g_object_unref (info);
    }

    /* Second criterion. */
    if (time1 != time2)
      return sign((gint64)time1 - time2);

    return 2 * CmpFilepath(ETFile1, ETFile2);
}

/**
 * Compare two UTF-8 strings, normalizing them before doing so, falling back to
 * the filenames if the strings are otherwise identical, and obeying the
 * requested case-sensitivity.
 *
 * @tparam V Field to compare
 * @tparam CS Whether the sorting should obey case
 * @tparam ST Whether to sort by track as second criterion
 * @param file1 1st file
 * @param file2 2nd file
 * @return An integer less than, equal to, or greater than zero, if str1 is
 * less than, equal to or greater than str2
 */
template <xStringD0 File_Tag::*V, bool CS, bool ST>
static gint CmpTagString2(const ET_File* file1, const ET_File* file2)
{
	const xStringD0& str1 = file1->FileTagNew()->*V;
	const xStringD0& str2 = file2->FileTagNew()->*V;

	gint result;
	if (CS)
		result = str1.compare(str2);
	else
	{	if (!str2)
			return !!str1;
		if (!str1)
			return -1;
		if (strcmp(str1, str2) == 0)
			goto second;
		result = et_normalized_strcasecmp0(str1, str2);
	}
	if (result != 0)
		return sign(result);

	/* Secondary criterion. */
second:
	return 2 * (ST ? CmpDiscNumber(file1, file2) : CmpFilepath(file1, file2));
}

/*
 * Comparison function for sorting by ascending year.
 */
template <xStringD0 File_Tag::*V>
static gint CmpTagInt(const ET_File* file1, const ET_File* file2)
{
	gint r = CmpInt(file1->FileTagNew()->*V, file2->FileTagNew()->*V);
	if (r)
		return r;
	// 2nd criterion
	return 2 * CmpFilepath(file1, file2);
}

/*
 * Comparison function for replay gain values.
 */
template <float File_Tag::*V>
static gint CmpTagFloat(const ET_File* file1, const ET_File* file2)
{
	float v1 = file1->FileTagNew()->*V;
	float v2 = file2->FileTagNew()->*V;

	if (isnan(v2))
		return isnan(v1);
	if (isnan(v1) || v1 < v2)
		return -1;
	if (v1 > v2)
		return 1;

	// 2nd criterion - does this make sense here?
	return 2 * CmpFilepath(file1, file2);
}

/*
 * Comparison function for sorting by ascending file type (mp3, ogg, ...).
 */
static gint CmpFileType(const ET_File* ETFile1, const ET_File* ETFile2)
{
	if ( !ETFile1->ETFileDescription ) return -1;
	if ( !ETFile2->ETFileDescription ) return 1;

	int cmp = strcmp(ETFile1->ETFileDescription->Extension, ETFile2->ETFileDescription->Extension);
	if (cmp)
		return sign(cmp);
	// Second criterion
	return 2 * CmpFilepath(ETFile1,ETFile2);
}

/*
 * Comparison function for sorting by ascending file size.
 */
static gint CmpFileSize(const ET_File* ETFile1, const ET_File* ETFile2)
{
	// Second criterion
	if (ETFile1->FileSize != ETFile2->FileSize)
		return sign((gint64)ETFile1->FileSize - ETFile2->FileSize);
	// Second criterion
	return 2 * CmpFilepath(ETFile1,ETFile2);
}

/*
 * Comparison function for sorting by ascending file int field.
 */
template <typename N, N ET_File_Info::*V>
static gint CmpInfoNum(const ET_File* ETFile1, const ET_File* ETFile2)
{
	if (ETFile1->ETFileInfo.*V != ETFile2->ETFileInfo.*V)
		return sign(ETFile1->ETFileInfo.*V - ETFile2->ETFileInfo.*V);
	// Second criterion
	return 2 * CmpFilepath(ETFile1,ETFile2);
}

template <gint (*F)(const ET_File *file1, const ET_File *file2)>
static gint CmpRev(const ET_File *file1, const ET_File *file2)
{	return F(file2, file1);
}

template <xStringD0 File_Tag::*V, bool R = false, bool ST = false>
static gint (*CmpTagString())(const ET_File *file1, const ET_File *file2)
{	return g_settings_get_boolean(MainSettings, "sort-case-sensitive")
		? (R ? CmpRev<CmpTagString2<V,true,ST>> : CmpTagString2<V,true,ST>)
		: (R ? CmpRev<CmpTagString2<V,false,ST>> : CmpTagString2<V,false,ST>);
}

/*
 * Get sort function by sort mode.
 */
gint (*ET_File::get_comp_func(EtSortMode sort_mode))(const ET_File *ETFile1, const ET_File *ETFile2)
{
	switch (sort_mode)
	{
	case ET_SORT_MODE_ASCENDING_FILEPATH:
		return CmpFilepath;
	case ET_SORT_MODE_DESCENDING_FILEPATH:
		return CmpRev<CmpFilepath>;
	case ET_SORT_MODE_ASCENDING_FILENAME:
		return CmpFilename;
	case ET_SORT_MODE_DESCENDING_FILENAME:
		return CmpRev<CmpFilename>;
	case ET_SORT_MODE_ASCENDING_TITLE:
		return CmpTagString<&File_Tag::title, false>();
	case ET_SORT_MODE_DESCENDING_TITLE:
		return CmpTagString<&File_Tag::title, true>();
	case ET_SORT_MODE_ASCENDING_VERSION:
		return CmpTagString<&File_Tag::version, false>();
	case ET_SORT_MODE_DESCENDING_VERSION:
		return CmpTagString<&File_Tag::version, true>();
	case ET_SORT_MODE_ASCENDING_SUBTITLE:
		return CmpTagString<&File_Tag::subtitle, false, true>();
	case ET_SORT_MODE_DESCENDING_SUBTITLE:
		return CmpTagString<&File_Tag::subtitle, true, true>();
	case ET_SORT_MODE_ASCENDING_ARTIST:
		return CmpTagString<&File_Tag::artist, false>();
	case ET_SORT_MODE_DESCENDING_ARTIST:
		return CmpTagString<&File_Tag::artist, true>();
	case ET_SORT_MODE_ASCENDING_ALBUM_ARTIST:
		return CmpTagString<&File_Tag::album_artist, false, true>();
	case ET_SORT_MODE_DESCENDING_ALBUM_ARTIST:
		return CmpTagString<&File_Tag::album_artist, true, true>();
	case ET_SORT_MODE_ASCENDING_ALBUM:
		return CmpTagString<&File_Tag::album, false, true>();
	case ET_SORT_MODE_DESCENDING_ALBUM:
		return CmpTagString<&File_Tag::album, true, true>();
	case ET_SORT_MODE_ASCENDING_DISC_SUBTITLE:
		return CmpTagString<&File_Tag::disc_subtitle, false, true>();
	case ET_SORT_MODE_DESCENDING_DISC_SUBTITLE:
		return CmpTagString<&File_Tag::disc_subtitle, true, true>();
	case ET_SORT_MODE_ASCENDING_YEAR:
		return CmpTagInt<&File_Tag::year>;
	case ET_SORT_MODE_DESCENDING_YEAR:
		return CmpRev<CmpTagInt<&File_Tag::year>>;
	case ET_SORT_MODE_ASCENDING_RELEASE_YEAR:
		return CmpTagInt<&File_Tag::release_year>;
	case ET_SORT_MODE_DESCENDING_RELEASE_YEAR:
		return CmpRev<CmpTagInt<&File_Tag::release_year>>;
	case ET_SORT_MODE_ASCENDING_DISC_NUMBER:
		return CmpDiscNumber;
	case ET_SORT_MODE_DESCENDING_DISC_NUMBER:
		return CmpRev<CmpDiscNumber>;
	case ET_SORT_MODE_ASCENDING_TRACK_NUMBER:
		return CmpTrackNumber;
	case ET_SORT_MODE_DESCENDING_TRACK_NUMBER:
		return CmpRev<CmpTrackNumber>;
	case ET_SORT_MODE_ASCENDING_GENRE:
		return CmpTagString<&File_Tag::genre, false>();
	case ET_SORT_MODE_DESCENDING_GENRE:
		return CmpTagString<&File_Tag::genre, true>();
	case ET_SORT_MODE_ASCENDING_COMMENT:
		return CmpTagString<&File_Tag::comment, false>();
	case ET_SORT_MODE_DESCENDING_COMMENT:
		return CmpTagString<&File_Tag::comment, true>();
	case ET_SORT_MODE_ASCENDING_COMPOSER:
		return CmpTagString<&File_Tag::composer, false>();
	case ET_SORT_MODE_DESCENDING_COMPOSER:
		return CmpTagString<&File_Tag::composer, true>();
	case ET_SORT_MODE_ASCENDING_ORIG_ARTIST:
		return CmpTagString<&File_Tag::orig_artist, false>();
	case ET_SORT_MODE_DESCENDING_ORIG_ARTIST:
		return CmpTagString<&File_Tag::orig_artist, true>();
	case ET_SORT_MODE_ASCENDING_ORIG_YEAR:
		return CmpTagInt<&File_Tag::orig_year>;
	case ET_SORT_MODE_DESCENDING_ORIG_YEAR:
		return CmpRev<CmpTagInt<&File_Tag::orig_year>>;
	case ET_SORT_MODE_ASCENDING_COPYRIGHT:
		return CmpTagString<&File_Tag::copyright, false>();
	case ET_SORT_MODE_DESCENDING_COPYRIGHT:
		return CmpTagString<&File_Tag::copyright, true>();
	case ET_SORT_MODE_ASCENDING_URL:
		return CmpTagString<&File_Tag::url, false>();
	case ET_SORT_MODE_DESCENDING_URL:
		return CmpTagString<&File_Tag::url, true>();
	case ET_SORT_MODE_ASCENDING_ENCODED_BY:
		return CmpTagString<&File_Tag::encoded_by, false>();
	case ET_SORT_MODE_DESCENDING_ENCODED_BY:
		return CmpTagString<&File_Tag::encoded_by, true>();
	case ET_SORT_MODE_ASCENDING_CREATION_DATE:
		return CmpCreationDate;
	case ET_SORT_MODE_DESCENDING_CREATION_DATE:
		return CmpRev<CmpCreationDate>;
	case ET_SORT_MODE_ASCENDING_FILE_TYPE:
		return CmpFileType;
	case ET_SORT_MODE_DESCENDING_FILE_TYPE:
		return CmpRev<CmpFileType>;
	case ET_SORT_MODE_ASCENDING_FILE_SIZE:
		return CmpFileSize;
	case ET_SORT_MODE_DESCENDING_FILE_SIZE:
		return CmpRev<CmpFileSize>;
	case ET_SORT_MODE_ASCENDING_FILE_DURATION:
		return CmpInfoNum<double,&ET_File_Info::duration>;
	case ET_SORT_MODE_DESCENDING_FILE_DURATION:
		return CmpRev<CmpInfoNum<double,&ET_File_Info::duration>>;
	case ET_SORT_MODE_ASCENDING_FILE_BITRATE:
		return CmpInfoNum<gint,&ET_File_Info::bitrate>;
	case ET_SORT_MODE_DESCENDING_FILE_BITRATE:
		return CmpRev<CmpInfoNum<gint,&ET_File_Info::bitrate>>;
	case ET_SORT_MODE_ASCENDING_FILE_SAMPLERATE:
		return CmpInfoNum<gint,&ET_File_Info::samplerate>;
	case ET_SORT_MODE_DESCENDING_FILE_SAMPLERATE:
		return CmpRev<CmpInfoNum<gint,&ET_File_Info::samplerate>>;
	case ET_SORT_MODE_ASCENDING_REPLAYGAIN:
		return CmpTagFloat<&File_Tag::track_gain>;
	case ET_SORT_MODE_DESCENDING_REPLAYGAIN:
		return CmpRev<CmpTagFloat<&File_Tag::track_gain>>;
	default:
		return nullptr;
	}
}

ET_File::~ET_File()
{
	/* Frees infos of ETFileInfo */
	g_free(ETFileInfo.mpc_profile);
	g_free(ETFileInfo.mpc_version);
}

bool ET_File::autofix()
{
  /*
   * Process the filename and tag to generate undo if needed...
   */
  File_Name* FileName = new File_Name(*FileNameNew());

  /* Convert filename extension (lower/upper). */
  FileName->format_extension();

  // Convert the illegal characters.
  FileName->format_filepath();

  File_Tag* FileTag = new File_Tag(*FileTagNew());
  FileTag->autofix();

  /*
   * Generate undo for the file and the main undo list.
   * If no changes detected, FileName and FileTag item are deleted.
   */
  return apply_changes(FileName, FileTag);
}


/********************
 * Saving functions *
 ********************/

/*
 * Save data contained into File_Tag structure to the file on hard disk.
 */
gboolean ET_File::save_file_tag(GError **error)
{
    gboolean state = FALSE;
    GFile *file;
    GFileInfo *fileinfo;

    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    /* Store the file timestamps (in case they are to be preserved) */
    file = g_file_new_for_path (FilePath);
    fileinfo = g_file_query_info (file, "time::*", G_FILE_QUERY_INFO_NONE,
                                  NULL, NULL);

    /* execute write operation */
    if (ETFileDescription->write_file_tag)
        state = (*ETFileDescription->write_file_tag)(this, error);
    else
        Log_Print (LOG_ERROR, "Saving unsupported for %s (%s).",
            ETFileDescription->FileType, FileName.Cur->full_name().get());

    /* Update properties for the file. */
    if (fileinfo)
    {
        if (g_settings_get_boolean (MainSettings,
                                    "file-preserve-modification-time"))
        {
            g_file_set_attributes_from_info (file, fileinfo,
                                             G_FILE_QUERY_INFO_NONE,
                                             NULL, NULL);
        }

        g_object_unref (fileinfo);
    }

    /* Update the stored file modification time to prevent EasyTAG from warning
     * that an external program has changed the file. */
    read_fileinfo(file);

    g_object_unref (file);

    if (state==TRUE)
    {
        /* Update date and time of the parent directory of the file after
         * changing the tag value (ex: needed for Amarok for refreshing). Note
         * that when renaming a file the parent directory is automatically
         * updated.
         */
        if (g_settings_get_boolean (MainSettings,
                                    "file-update-parent-modification-time"))
        {
            gchar *path = g_path_get_dirname (FilePath);
            g_utime (path, NULL);
            g_free (path);
        }

        // mark as saved
        force_tag_save_ = false;
        FileTag.mark_saved();
        return TRUE;
    }
    else
    {
        g_assert (error == NULL || *error != NULL);

        return FALSE;
    }
}

gboolean ET_File::rename_file(GError **error)
{
	// Make absolute path of the file in file system notation.
	gString raw_name(filename_from_display(FileName.New->full_name().get()));
	raw_name = g_canonicalize_filename(raw_name.get(),
		et_application_window_get_current_path_name(MainWindow));

	gboolean rc = et_rename_file(FilePath, raw_name, error);
	if (rc)
	{	FileName.mark_saved();
		FilePath.swap(raw_name);
	}

	return rc;
}

bool ET_File::read_fileinfo(GFile* file, GError** error)
{
	GFileInfo* fileinfo = g_file_query_info(file,
		G_FILE_ATTRIBUTE_STANDARD_SIZE "," G_FILE_ATTRIBUTE_TIME_MODIFIED,
		G_FILE_QUERY_INFO_NONE, NULL, error);
	if (!fileinfo)
		return false;

	FileSize = g_file_info_get_attribute_uint64(fileinfo, G_FILE_ATTRIBUTE_STANDARD_SIZE);
	FileModificationTime = g_file_info_get_attribute_uint64(fileinfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	g_object_unref (fileinfo);
	return true;
}

bool ET_File::read_file(GFile *file, const gchar *root, GError **error)
{
  /* Get description of the file */
  const char* filename = FilePath;
	// make filename relative if possible
	if (root)
	{	unsigned root_len = strlen(root);
		if (root_len && strncmp(filename, root, root_len) == 0)
		{	if (filename[root_len - 1] == G_DIR_SEPARATOR)
				filename += root_len;
			else if (filename[root_len] == G_DIR_SEPARATOR)
				filename += root_len + 1;
		}
	}

	/* Attach all data to this ETFile item */
	FileName.add(new File_Name(gString(g_filename_display_name(filename))), 0);
	FileName.mark_saved();

	File_Tag* fileTag = nullptr;
	/* Store the size and the modification time of the file
	 * to check if the file was changed before saving */
	if (!read_fileinfo(file, error))
	{	// bypass handler if we did not even get the file size.
		ETFileDescription = ET_File_Description::Get(nullptr);
	} else
	{	ETFileDescription = ET_File_Description::Get(filename);
		if (ETFileDescription->read_file)
			fileTag = (*ETFileDescription->read_file)(file, this, error);
	}

	bool rc = true;
	if (!fileTag)
	{	fileTag = new File_Tag(); // add empty tag in doubt
		rc = false;
	}
	FileTag.add(fileTag, 0);
	FileTag.mark_saved();

	return rc;
}

/* Key for Undo */
static atomic<unsigned> ETUndoKey(0);

/*
 * Check if 'FileName' and 'FileTag' differ with those of 'ETFile'.
 * Manage undo feature for the ETFile and the main undo list.
 */
bool ET_File::apply_changes(File_Name *fileName, File_Tag *fileTag)
{
	/*
	 * Detect changes of filename and generate the filename undo list
	 */
	if (fileName && FileName.New && *FileName.New == *fileName)
	{	delete fileName;
		fileName = nullptr;
	}

	/*
	 * Detect changes in tag data and generate the tag undo list
	 */
	if (fileTag && FileTag.New && *FileTag.New == *fileTag)
	{	delete fileTag;
		fileTag = nullptr;
	}

	if (!fileName && !fileTag)
			return false;

	/*
	 * Generate main undo (file history of modifications)
	 */
	guint undo_key = 0;
	if ((fileName && FileName.New) || (fileTag && FileTag.New))
		undo_key = ++ETUndoKey;
	if (fileName)
		FileName.add(fileName, undo_key);
	if (fileTag)
		FileTag.add(fileTag, undo_key);

	if (undo_key)
		ET_FileList::history_list_add(this);

	return true;
}

/*
 * Applies one undo to the ETFile data (to reload the previous data).
 * Returns TRUE if an undo had been applied.
 */
bool ET_File::undo()
{
	/* Find the valid key */
	unsigned undo_key = max(FileName.undo_key(), FileTag.undo_key());

	if (undo_key == 0)
		return false;

	/* Undo filename */
	if (undo_key == FileName.undo_key())
		FileName.undo();

	/* Undo tag data */
	if (undo_key == FileTag.undo_key())
		FileTag.undo();

	return true;
}

/*
 * Applies one redo to the ETFile data. Returns TRUE if a redo had been applied.
 */
bool ET_File::redo()
{
	/* Find the valid key */
	unsigned undo_key = min(FileName.redo_key() - 1, FileTag.redo_key() - 1);

	if (++undo_key == 0)
		return false;

	/* Redo filename */
	if (undo_key == FileName.redo_key())
		FileName.redo();

	/* Redo tag data */
	if (undo_key == FileTag.redo_key())
		FileTag.redo();

	return true;
}

ET_File::UpdateDirectoyNameArgs::UpdateDirectoyNameArgs(const gchar *old_path, const gchar *new_path, const gchar* root)
:	OldPathRelUTF8(nullptr)
,	NewPathRelUTF8(nullptr)
{
	size_t path_len = strlen(old_path);
	if (path_len && G_IS_DIR_SEPARATOR(old_path[path_len - 1]))
		--path_len;
	OldPath = g_strndup(old_path, path_len);
	OldPathUTF8 = g_filename_display_name(OldPath);

	path_len = strlen(new_path);
	if (path_len && G_IS_DIR_SEPARATOR(new_path[path_len - 1]))
		--path_len;
	NewPath = g_strndup(new_path, path_len);
	NewPathUTF8 = g_filename_display_name(NewPath);

	if (!et_str_empty(root))
	{	gString rootUTF8(g_filename_display_name(root));
		path_len = strlen(rootUTF8);
		if (path_len && G_IS_DIR_SEPARATOR(rootUTF8[path_len - 1]))
			--path_len;
		if (strncmp(OldPathUTF8, rootUTF8, path_len) == 0 && G_IS_DIR_SEPARATOR(OldPathUTF8[path_len]))
			OldPathRelUTF8 = OldPathUTF8 + path_len + 1;
		if (strncmp(NewPathUTF8, rootUTF8, path_len) == 0 && G_IS_DIR_SEPARATOR(NewPathUTF8[path_len]))
			NewPathRelUTF8 = NewPathUTF8 + path_len + 1;
	}
}

bool ET_File::update_directory_name(const UpdateDirectoyNameArgs& args)
{
	return false;

  /*TODO: for (gListP<File_Name*> filenamelist = file->FileNameList; filenamelist; filenamelist = filenamelist->next)
  {
      const char* path = filenamelist->data->Path;

      if (strncmp(path, old_path, old_path_len) == 0
          // Check for '/' at the end of path
          && (path[old_path_len] == 0 || path[old_path_len] == G_DIR_SEPARATOR))
      {   // Replace path of filename.
          size_t path_len = strlen(path);
          xString newpath;
          char* cp = newpath.alloc(path_len - old_path_len + new_path_len);
          memcpy(cp, new_path, new_path_len);
          memcpy(cp + new_path_len, path + old_path_len, path_len - old_path_len);
      }
  }*/

}
