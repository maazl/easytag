/* EasyTAG - tag editor for audio files
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
#include "file_tag.h"
#include "file_list.h"
#ifdef ENABLE_MP3
#   include "id3_tag.h"
#endif
#include "picture.h"
#include "ape_tag.h"
#ifdef ENABLE_OGG
#include "ogg_tag.h"
#endif
#ifdef ENABLE_FLAC
#include "flac_tag.h"
#endif
#ifdef ENABLE_MP4
#include "mp4_tag.h"
#endif
#ifdef ENABLE_WAVPACK
#include "wavpack_tag.h"
#endif
#ifdef ENABLE_OPUS
#include "opus_tag.h"
#endif
#include "browser.h"
#include "log.h"
#include "misc.h"
#include "setting.h"
#include "charset.h"

#include "win32/win32dep.h"

static gboolean ET_Free_File_Name_List (GList *FileNameList);
static gboolean ET_Free_File_Tag_List (GList *FileTagList);

static void ET_Mark_File_Tag_As_Saved (ET_File *ETFile);

static gboolean ET_Add_File_Name_To_List (ET_File *ETFile,
                                          File_Name *FileName);
static gboolean ET_Add_File_Tag_To_List (ET_File *ETFile, File_Tag  *FileTag);

/*
 * Create a new ET_File structure
 */
ET_File *
ET_File_Item_New (void)
{
    ET_File *ETFile;

    ETFile = g_slice_new0 (ET_File);

    return ETFile;
}

/*
 * Comparison function for sorting by ascending path.
 */
static gint CmpFilepath(const ET_File* ETFile1, const ET_File* ETFile2)
{
	const File_Name *file1 = ETFile1->FileNameCur->data;
	const File_Name *file2 = ETFile2->FileNameCur->data;
	// !!!! : Must be the same rules as "Cddb_Track_List_Sort_Func" to be
	// able to sort in the same order files in cddb and in the file list.
	int r = strcmp(file1->path_value_ck, file2->path_value_ck);
	if (r)
		return sign(r);
	return 2 * sign(strcmp(file1->file_value_ck, file2->file_value_ck));
}

/*
 * Comparison function for sorting by ascending filename.
 */
static gint CmpFilename(const ET_File* ETFile1, const ET_File* ETFile2)
{
	// !!!! : Must be the same rules as "Cddb_Track_List_Sort_Func" to be
	// able to sort in the same order files in cddb and in the file list.
	return sign(strcmp(ETFile1->FileNameCur->data->file_value_ck, ETFile2->FileNameCur->data->file_value_ck));
}

/*
 * Compare strings that areliekly integers (e.g. track number)
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
  const File_Tag *file1 = ETFile1->FileTag->data;
  const File_Tag *file2 = ETFile2->FileTag->data;
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
  const File_Tag *file1 = ETFile1->FileTag->data;
  const File_Tag *file2 = ETFile2->FileTag->data;
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
    file = g_file_new_for_path (ETFile1->FileNameCur->data->value);
    info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_CHANGED,
                              G_FILE_QUERY_INFO_NONE, NULL, NULL);

    g_object_unref (file);

    if (info)
    {
        time1 = g_file_info_get_attribute_uint64 (info,
                                                  G_FILE_ATTRIBUTE_TIME_CHANGED);
        g_object_unref (info);
    }

    file = g_file_new_for_path (ETFile2->FileNameCur->data->value);
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
template <gchar* File_Tag::*V, bool CS, bool ST>
static gint CmpTagString2(const ET_File* file1, const ET_File* file2)
{
	const gchar* str1 = file1->FileTag->data->*V;
	const gchar* str2 = file2->FileTag->data->*V;

	if (!str2)
		return !!str1;
	if (!str1)
		return -1;

	if (strcmp(str1, str2) != 0)
	{	gint result = CS ? et_normalized_strcmp0(str1, str2) : et_normalized_strcasecmp0(str1, str2);
		if (result != 0)
			return sign(result);
	}

	/* Secondary criterion. */
	return 2 * (ST ? CmpDiscNumber(file1, file2) : CmpFilepath(file1, file2));
}

/*
 * Comparison function for sorting by ascending year.
 */
template <gchar* File_Tag::*V>
static gint CmpTagInt(const ET_File* file1, const ET_File* file2)
{
	gint r = CmpInt(file1->FileTag->data->*V, file2->FileTag->data->*V);
	if (r)
		return r;
	// 2nd criterion
	return 2 * CmpFilepath(file1, file2);
}

/*
 * Comparison function for sorting by ascending file type (mp3, ogg, ...).
 */
static gint CmpFileType(const ET_File* ETFile1, const ET_File* ETFile2)
{
	if ( !ETFile1->ETFileDescription ) return -1;
	if ( !ETFile2->ETFileDescription ) return 1;

	if (ETFile1->ETFileDescription->FileType != ETFile2->ETFileDescription->FileType)
		return sign(ETFile1->ETFileDescription->FileType - ETFile2->ETFileDescription->FileType);
	// Second criterion
	return 2 * CmpFilepath(ETFile1,ETFile2);
}

/*
 * Comparison function for sorting by ascending file size.
 */
static gint CmpFileSize(const ET_File* ETFile1, const ET_File* ETFile2)
{
	if ( !ETFile1->ETFileInfo ) return -1;
	if ( !ETFile2->ETFileInfo ) return 1;

	// Second criterion
	if (ETFile1->ETFileInfo->size != ETFile2->ETFileInfo->size)
		return sign((gint64)ETFile1->ETFileInfo->size - ETFile2->ETFileInfo->size);
	// Second criterion
	return 2 * CmpFilepath(ETFile1,ETFile2);
}

/*
 * Comparison function for sorting by ascending file int field.
 */
template <gint ET_File_Info::*V>
static gint CmpInfoInt(const ET_File* ETFile1, const ET_File* ETFile2)
{
	if ( !ETFile1->ETFileInfo ) return -1;
	if ( !ETFile2->ETFileInfo ) return 1;

	if (ETFile1->ETFileInfo->*V != ETFile2->ETFileInfo->*V)
		return sign(ETFile1->ETFileInfo->*V - ETFile2->ETFileInfo->*V);
	// Second criterion
	return 2 * CmpFilepath(ETFile1,ETFile2);
}

template <gint (*F)(const ET_File *file1, const ET_File *file2)>
static gint CmpRev(const ET_File *file1, const ET_File *file2)
{	return F(file2, file1);
}

template <gchar* File_Tag::*V, bool R = false, bool ST = false>
static gint (*CmpTagString())(const ET_File *file1, const ET_File *file2)
{	return g_settings_get_boolean(MainSettings, "sort-case-sensitive")
		? (R ? CmpRev<CmpTagString2<V,true,ST>> : CmpTagString2<V,true,ST>)
		: (R ? CmpRev<CmpTagString2<V,false,ST>> : CmpTagString2<V,false,ST>);
}

/*
 * Get sort function by sort mode.
 */
gint (*ET_Get_Comp_Func_Sort_File(EtSortMode sort_mode))(const ET_File *ETFile1, const ET_File *ETFile2)
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
		return CmpInfoInt<&ET_File_Info::duration>;
	case ET_SORT_MODE_DESCENDING_FILE_DURATION:
		return CmpRev<CmpInfoInt<&ET_File_Info::duration>>;
	case ET_SORT_MODE_ASCENDING_FILE_BITRATE:
		return CmpInfoInt<&ET_File_Info::bitrate>;
	case ET_SORT_MODE_DESCENDING_FILE_BITRATE:
		return CmpRev<CmpInfoInt<&ET_File_Info::bitrate>>;
	case ET_SORT_MODE_ASCENDING_FILE_SAMPLERATE:
		return CmpInfoInt<&ET_File_Info::samplerate>;
	case ET_SORT_MODE_DESCENDING_FILE_SAMPLERATE:
		return CmpRev<CmpInfoInt<&ET_File_Info::samplerate>>;
	default:
		return nullptr;
	}
}

/*********************
 * Freeing functions *
 *********************/

/*
 * Frees one item of the full main list of files.
 */
void
ET_Free_File_List_Item (ET_File *ETFile)
{
    if (ETFile)
    {
        /* Frees the lists */
        if (ETFile->FileNameList)
        {
            ET_Free_File_Name_List(ETFile->FileNameList);
        }
        if (ETFile->FileNameListBak)
        {
            ET_Free_File_Name_List(ETFile->FileNameListBak);
        }
        if (ETFile->FileTagList)
        {
            ET_Free_File_Tag_List (ETFile->FileTagList);
        }
        if (ETFile->FileTagListBak)
        {
            ET_Free_File_Tag_List (ETFile->FileTagListBak);
        }
        /* Frees infos of ETFileInfo */
        if (ETFile->ETFileInfo)
        {
            et_file_info_free (ETFile->ETFileInfo);
        }

        g_free(ETFile->ETFileExtension);
        g_slice_free (ET_File, ETFile);
    }
}


/*
 * Frees the full list: GList *FileNameList.
 */
static gboolean
ET_Free_File_Name_List (GList *FileNameList)
{
    g_return_val_if_fail (FileNameList != NULL, FALSE);

    FileNameList = g_list_first (FileNameList);

    g_list_free_full (FileNameList, (GDestroyNotify)et_file_name_free);

    return TRUE;
}

/*
 * Frees the full list: GList *TagList.
 */
static gboolean
ET_Free_File_Tag_List (GList *FileTagList)
{
    GList *l;

    g_return_val_if_fail (FileTagList != NULL, FALSE);

    FileTagList = g_list_first (FileTagList);

    for (l = FileTagList; l != NULL; l = g_list_next (l))
    {
        if ((File_Tag *)l->data)
        {
            et_file_tag_free ((File_Tag *)l->data);
        }
    }

    g_list_free (FileTagList);

    return TRUE;
}


/*********************
 * Copying functions *
 *********************/

/********************
 * Saving functions *
 ********************/

/*
 * Do the same thing of ET_Save_File_Name_From_UI, but without getting the
 * data from the UI.
 */
gboolean
ET_Save_File_Name_Internal (const ET_File *ETFile,
                            File_Name *FileName)
{
    gchar *filename_new = NULL;
    gchar *dirname = NULL;
    gchar *filename;
    gchar *extension;
    gchar *pos;
    gboolean success;

    g_return_val_if_fail (ETFile != NULL && FileName != NULL, FALSE);

    // Get the current path to the file
    dirname = g_path_get_dirname(ETFile->FileNameNew->data->value);

    // Get the name of file (and rebuild it with extension with a 'correct' case)
    filename = g_path_get_basename(ETFile->FileNameNew->data->value);

    // Remove the extension
    if ((pos=strrchr(filename, '.'))!=NULL)
        *pos = 0;

    /* Convert filename extension (lower/upper). */
    extension = ET_File_Format_File_Extension (ETFile);

    // Check length of filename
    //ET_File_Name_Check_Length(ETFile,filename);

    // Regenerate the new filename (without path)
    filename_new = g_strconcat(filename,extension,NULL);
    g_free(extension);
    g_free(filename);

    success = et_file_name_set_from_components (FileName, ETFile->FileNameCur->data, filename_new, dirname,
        (EtFilenameReplaceMode)g_settings_get_enum(MainSettings, "rename-replace-illegal-chars"));

    g_free (filename_new);
    g_free (dirname);
    return success;
}

static gchar* strip_value(const gchar* value)
{
    if (et_str_empty (value))
        return NULL;
    else
    {
        gchar *value2 = g_strdup(value);
        g_strstrip (value2);
        return value2;
    }
}

/*
 * Do the same thing of et_tag_area_create_file_tag without getting the data from the UI.
 */
gboolean
ET_Save_File_Tag_Internal (ET_File *ETFile, File_Tag *FileTag)
{
    g_return_val_if_fail (ETFile != NULL && ETFile->FileTag != NULL
                          && FileTag != NULL, FALSE);

    File_Tag *FileTagCur = (File_Tag *)ETFile->FileTag->data;

    FileTag->title = strip_value(FileTagCur->title);
    FileTag->version = strip_value(FileTagCur->version);
    FileTag->subtitle = strip_value(FileTagCur->subtitle);

    FileTag->artist = strip_value(FileTagCur->artist);
    FileTag->album_artist = strip_value(FileTagCur->album_artist);
    FileTag->album = strip_value(FileTagCur->album);
    FileTag->disc_subtitle = strip_value(FileTagCur->disc_subtitle);

    FileTag->disc_number = et_disc_number_to_string(FileTagCur->disc_number);
    FileTag->disc_total = et_disc_number_to_string(FileTagCur->disc_total);

    FileTag->year = strip_value(FileTagCur->year);
    FileTag->release_year = strip_value(FileTagCur->release_year);

    FileTag->track = et_track_number_to_string(FileTagCur->track);
    FileTag->track_total = et_track_number_to_string(FileTagCur->track_total);

    FileTag->genre = strip_value(FileTagCur->genre);
    FileTag->comment = strip_value(FileTagCur->comment);

    FileTag->composer = strip_value(FileTagCur->composer);
    FileTag->orig_artist = strip_value(FileTagCur->orig_artist);
    FileTag->orig_year = strip_value(FileTagCur->orig_year);

    FileTag->copyright = strip_value(FileTagCur->copyright);
    FileTag->url = strip_value(FileTagCur->url);
    FileTag->encoded_by = strip_value(FileTagCur->encoded_by);
    FileTag->description = strip_value(FileTagCur->description);

    FileTag->track_gain = FileTagCur->track_gain;
    FileTag->track_peak = FileTagCur->track_peak;
    FileTag->album_gain = FileTagCur->album_gain;
    FileTag->album_peak = FileTagCur->album_peak;

    /* Picture */
    et_file_tag_set_picture (FileTag, FileTagCur->picture);

    return TRUE;
}


/*
 * Save data contained into File_Tag structure to the file on hard disk.
 */
gboolean
ET_Save_File_Tag_To_HD (ET_File *ETFile, GError **error)
{
    const ET_File_Description *description;
    const gchar *cur_filename;
    const gchar *cur_filename_utf8;
    gboolean state;
    GFile *file;
    GFileInfo *fileinfo;

    g_return_val_if_fail (ETFile != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    cur_filename = ETFile->FileNameCur->data->value;
    cur_filename_utf8 = ETFile->FileNameCur->data->value_utf8;

    description = ETFile->ETFileDescription;

    /* Store the file timestamps (in case they are to be preserved) */
    file = g_file_new_for_path (cur_filename);
    fileinfo = g_file_query_info (file, "time::*", G_FILE_QUERY_INFO_NONE,
                                  NULL, NULL);

    switch (description->TagType)
    {
#ifdef ENABLE_MP3
        case ID3_TAG:
            state = id3tag_write_file_tag (ETFile, error);
            break;
#endif
#ifdef ENABLE_OGG
        case OGG_TAG:
            state = ogg_tag_write_file_tag (ETFile, error);
            break;
#endif
#ifdef ENABLE_FLAC
        case FLAC_TAG:
            state = flac_tag_write_file_tag (ETFile, error);
            break;
#endif
        case APE_TAG:
            state = ape_tag_write_file_tag (ETFile, error);
            break;
#ifdef ENABLE_MP4
        case MP4_TAG:
            state = mp4tag_write_file_tag (ETFile, error);
            break;
#endif
#ifdef ENABLE_WAVPACK
        case WAVPACK_TAG:
            state = wavpack_tag_write_file_tag (ETFile, error);
            break;
#endif
#ifdef ENABLE_OPUS
        case OPUS_TAG:
            state = ogg_tag_write_file_tag (ETFile, error);
            break;
#endif
#ifndef ENABLE_MP3
        case ID3_TAG:
#endif
#ifndef ENABLE_OGG
        case OGG_TAG:
#endif
#ifndef ENABLE_FLAC
        case FLAC_TAG:
#endif
#ifndef ENABLE_MP4
        case MP4_TAG:
#endif
#ifndef ENABLE_WAVPACK
        case WAVPACK_TAG:
#endif
#ifndef ENABLE_OPUS
        case OPUS_TAG:
#endif
        case UNKNOWN_TAG:
        default:
            Log_Print (LOG_ERROR,
                       "Saving to HD: Undefined function for tag type '%d' (file %s).",
                       (gint)description->TagType, cur_filename_utf8);
            state = FALSE;
            break;
    }

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
    fileinfo = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                  G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if (fileinfo)
    {
        ETFile->FileModificationTime = g_file_info_get_attribute_uint64 (fileinfo,
                                                                         G_FILE_ATTRIBUTE_TIME_MODIFIED);
        g_object_unref (fileinfo);
    }

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
            gchar *path = g_path_get_dirname (cur_filename);
            g_utime (path, NULL);
            g_free (path);
        }

        ET_Mark_File_Tag_As_Saved(ETFile);
        return TRUE;
    }
    else
    {
        g_assert (error == NULL || *error != NULL);

        return FALSE;
    }
}

/*
 * Check if 'FileName' and 'FileTag' differ with those of 'ETFile'.
 * Manage undo feature for the ETFile and the main undo list.
 */
gboolean
ET_Manage_Changes_Of_File_Data (ET_File *ETFile,
                                File_Name *FileName,
                                File_Tag *FileTag)
{
    gboolean undo_added = FALSE;

    g_return_val_if_fail (ETFile != NULL, FALSE);

    /*
     * Detect changes of filename and generate the filename undo list
     */
    if (FileName)
    {
        if (ETFile->FileNameNew
            && et_file_name_detect_difference (ETFile->FileNameNew->data, FileName) == TRUE)
        {
            ET_Add_File_Name_To_List(ETFile,FileName);
            undo_added |= TRUE;
        }else
        {
            et_file_name_free (FileName);
        }
    }

    /*
     * Detect changes in tag data and generate the tag undo list
     */
    if (FileTag)
    {
        if (ETFile->FileTag
            && et_file_tag_detect_difference (ETFile->FileTag->data, FileTag) == TRUE)
        {
            ET_Add_File_Tag_To_List(ETFile,FileTag);
            undo_added |= TRUE;
        }
        else
        {
            et_file_tag_free (FileTag);
        }
    }

    /*
     * Generate main undo (file history of modifications)
     */
    if (undo_added)
    {
        ETCore->ETHistoryFileList = et_history_list_add (ETCore->ETHistoryFileList,
                                                         ETFile);
    }

    //return TRUE;
    return undo_added;
}

/*
 * Add a FileName item to the history list of ETFile
 */
static gboolean
ET_Add_File_Name_To_List (ET_File *ETFile, File_Name *FileName)
{
    gListP<File_Name*> cut_list = NULL;

    g_return_val_if_fail (ETFile != NULL && FileName != NULL, FALSE);

    /* How it works : Cut the FileNameList list after the current item,
     * and appends it to the FileNameListBak list for saving the data.
     * Then appends the new item to the FileNameList list */
    if (ETFile->FileNameList)
    {
        cut_list = ETFile->FileNameNew->next; // Cut after the current item...
        ETFile->FileNameNew->next = NULL;
    }
    if (cut_list)
        cut_list->prev = NULL;

    /* Add the new item to the list */
    ETFile->FileNameList = ETFile->FileNameList.append(FileName);
    /* Set the current item to use */
    ETFile->FileNameNew  = ETFile->FileNameList.last();
    /* Backup list */
    /* FIX ME! Keep only the saved item */
    ETFile->FileNameListBak = ETFile->FileNameListBak.concat(cut_list);

    return TRUE;
}

/*
 * Add a FileTag item to the history list of ETFile
 */
static gboolean
ET_Add_File_Tag_To_List (ET_File *ETFile, File_Tag *FileTag)
{
    gListP<File_Tag*> cut_list = NULL;

    g_return_val_if_fail (ETFile != NULL && FileTag != NULL, FALSE);

    if (ETFile->FileTag)
    {
        cut_list = ETFile->FileTag->next; // Cut after the current item...
        ETFile->FileTag->next = NULL;
    }
    if (cut_list)
        cut_list->prev = NULL;

    /* Add the new item to the list */
    ETFile->FileTagList = ETFile->FileTagList.append(FileTag);
    /* Set the current item to use */
    ETFile->FileTag     = ETFile->FileTagList.last();
    /* Backup list */
    ETFile->FileTagListBak = ETFile->FileTagListBak.concat(cut_list);

    return TRUE;
}

/*
 * Applies one undo to the ETFile data (to reload the previous data).
 * Returns TRUE if an undo had been applied.
 */
gboolean ET_Undo_File_Data (ET_File *ETFile)
{
    gboolean has_filename_undo_data = FALSE;
    gboolean has_filetag_undo_data  = FALSE;
    guint    filename_key, filetag_key, undo_key;

    g_return_val_if_fail (ETFile != NULL, FALSE);

    /* Find the valid key */
    if (ETFile->FileNameNew->prev && ETFile->FileNameNew->data)
        filename_key = ETFile->FileNameNew->data->key;
    else
        filename_key = 0;
    if (ETFile->FileTag->prev && ETFile->FileTag->data)
        filetag_key = ETFile->FileTag->data->key;
    else
        filetag_key = 0;
    // The key to use
    undo_key = MAX(filename_key,filetag_key);

    /* Undo filename */
    if (ETFile->FileNameNew->prev && ETFile->FileNameNew->data
    && (undo_key == ETFile->FileNameNew->data->key))
    {
        ETFile->FileNameNew = ETFile->FileNameNew->prev;
        has_filename_undo_data = TRUE; // To indicate that an undo has been applied
    }

    /* Undo tag data */
    if (ETFile->FileTag->prev && ETFile->FileTag->data
    && (undo_key == ETFile->FileTag->data->key))
    {
        ETFile->FileTag = ETFile->FileTag->prev;
        has_filetag_undo_data  = TRUE;
    }

    return has_filename_undo_data | has_filetag_undo_data;
}


/*
 * Returns TRUE if file contains undo data (filename or tag)
 */
gboolean
ET_File_Data_Has_Undo_Data (const ET_File *ETFile)
{
    gboolean has_filename_undo_data = FALSE;
    gboolean has_filetag_undo_data  = FALSE;

    g_return_val_if_fail (ETFile != NULL, FALSE);

    if (ETFile->FileNameNew && ETFile->FileNameNew->prev) has_filename_undo_data = TRUE;
    if (ETFile->FileTag && ETFile->FileTag->prev)         has_filetag_undo_data  = TRUE;

    return has_filename_undo_data | has_filetag_undo_data;
}


/*
 * Applies one redo to the ETFile data. Returns TRUE if a redo had been applied.
 */
gboolean ET_Redo_File_Data (ET_File *ETFile)
{
    gboolean has_filename_redo_data = FALSE;
    gboolean has_filetag_redo_data  = FALSE;
    guint    filename_key, filetag_key, undo_key;

    g_return_val_if_fail (ETFile != NULL, FALSE);

    /* Find the valid key */
    if (ETFile->FileNameNew->next && ETFile->FileNameNew->next->data)
        filename_key = ETFile->FileNameNew->next->data->key;
    else
        filename_key = (guint)~0; // To have the max value for guint
    if (ETFile->FileTag->next && ETFile->FileTag->next->data)
        filetag_key = ETFile->FileTag->next->data->key;
    else
        filetag_key = (guint)~0; // To have the max value for guint
    // The key to use
    undo_key = MIN(filename_key,filetag_key);

    /* Redo filename */
    if (ETFile->FileNameNew->next && ETFile->FileNameNew->next->data
    && (undo_key == ETFile->FileNameNew->next->data->key))
    {
        ETFile->FileNameNew = ETFile->FileNameNew->next;
        has_filename_redo_data = TRUE; // To indicate that a redo has been applied
    }

    /* Redo tag data */
    if (ETFile->FileTag->next && ETFile->FileTag->next->data
    && (undo_key == ETFile->FileTag->next->data->key))
    {
        ETFile->FileTag = ETFile->FileTag->next;
        has_filetag_redo_data  = TRUE;
    }

    return has_filename_redo_data | has_filetag_redo_data;
}


/*
 * Returns TRUE if file contains redo data (filename or tag)
 */
gboolean
ET_File_Data_Has_Redo_Data (const ET_File *ETFile)
{
    gboolean has_filename_redo_data = FALSE;
    gboolean has_filetag_redo_data  = FALSE;

    g_return_val_if_fail (ETFile != NULL, FALSE);

    if (ETFile->FileNameNew && ETFile->FileNameNew->next) has_filename_redo_data = TRUE;
    if (ETFile->FileTag && ETFile->FileTag->next)         has_filetag_redo_data  = TRUE;

    return has_filename_redo_data | has_filetag_redo_data;
}

/*
 * Checks if the current files had been changed but not saved.
 * Returns TRUE if the file has been saved.
 * Returns FALSE if some changes haven't been saved.
 */
gboolean
et_file_check_saved (const ET_File *ETFile)
{
    g_return_val_if_fail (ETFile != NULL, TRUE);

    /* Check if the tag has been changed. */
    if (ETFile->FileTag && !ETFile->FileTag->data->saved)
        return FALSE;

    /* Check if name of file has been changed. */
    if (ETFile->FileNameNew && !ETFile->FileNameNew->data->saved)
        return FALSE;

    /* No changes. */
    return TRUE;
}

/*******************
 * Extra functions *
 *******************/

/*
 * Set to TRUE the value of 'FileTag->saved' for the File_Tag item passed in parameter.
 * And set ALL other values of the list to FALSE.
 */
static void
Set_Saved_Value_Of_File_Tag_To_False (File_Tag *FileTag, void *dummy)
{
    if (FileTag) FileTag->saved = FALSE;
}

static void
ET_Mark_File_Tag_As_Saved (ET_File *ETFile)
{
    g_list_foreach(ETFile->FileTagList, (GFunc)Set_Saved_Value_Of_File_Tag_To_False, NULL); // All other FileTag set to FALSE
    ETFile->FileTag->data->saved = TRUE; // The current FileTag set to TRUE
}


void ET_Mark_File_Name_As_Saved (ET_File *ETFile)
{
    g_list_foreach(ETFile->FileNameList, (GFunc)Set_Saved_Value_Of_File_Tag_To_False, NULL);
    ETFile->FileNameNew->data->saved = TRUE; // The current FileName, to set to TRUE
}

/*
 * et_file_generate_name:
 * @ETFile: the file from which to read the existing name
 * @new_file_name_utf8: UTF-8 encoded new filename
 *
 * This function generates a new filename using path of the old file and the
 * new name.
 *
 * Returns: a newly-allocated filename, in UTF-8
 */
#if 1
gchar *
et_file_generate_name (const ET_File *ETFile,
                       const gchar *new_file_name_utf8)
{
    gchar *dirname_utf8;

    g_return_val_if_fail (ETFile && ETFile->FileNameNew->data, NULL);
    g_return_val_if_fail (new_file_name_utf8, NULL);

    if ((dirname_utf8 = g_path_get_dirname (ETFile->FileNameNew->data->value_utf8)))
    {
        gchar *extension;
        gchar *new_file_name_path_utf8;

        /* Convert filename extension (lower/upper). */
        extension = ET_File_Format_File_Extension (ETFile);

        // Check length of filename (limit ~255 characters)
        //ET_File_Name_Check_Length(ETFile,new_file_name_utf8);

        if (g_path_is_absolute (new_file_name_utf8))
        {
            /* Just add the extension. */
            new_file_name_path_utf8 = g_strconcat (new_file_name_utf8,
                                                   extension, NULL);
        }
        else
        {
            /* New path (with filename). */
            if (strcmp (dirname_utf8, G_DIR_SEPARATOR_S) == 0)
            {
                /* Root directory. */
                new_file_name_path_utf8 = g_strconcat (dirname_utf8,
                                                       new_file_name_utf8,
                                                       extension, NULL);
            }
            else
            {
                new_file_name_path_utf8 = g_strconcat (dirname_utf8,
                                                       G_DIR_SEPARATOR_S,
                                                       new_file_name_utf8,
                                                       extension, NULL);
            }
        }

        g_free (dirname_utf8);
        g_free (extension);

        return new_file_name_path_utf8;
    }

    return NULL;
}
#else
/* FOR TESTING */
/* Returns filename in file system encoding */
gchar *et_file_generate_name (ET_File *ETFile, gchar *new_file_name_utf8)
{
    gchar *dirname;

    if (ETFile && ETFile->FileNameNew->data && new_file_name_utf8
    && (dirname=g_path_get_dirname(ETFile->FileNameNew->data->value)) )
    {
        gchar *extension;
        gchar *new_file_name_path;
        gchar *new_file_name;

        new_file_name = filename_from_display(new_file_name_utf8);

        /* Convert filename extension (lower/upper). */
        extension = ET_File_Format_File_Extension (ETFile);

        // Check length of filename (limit ~255 characters)
        //ET_File_Name_Check_Length(ETFile,new_file_name_utf8);

        // If filemame starts with /, it's a full filename with path but without extension
        if (g_path_is_absolute(new_file_name))
        {
            // We just add the extension
            new_file_name_path = g_strconcat(new_file_name,extension,NULL);
        }else
        {
            // New path (with filename)
            if ( strcmp(dirname,G_DIR_SEPARATOR_S)==0 ) // Root directory?
                new_file_name_path = g_strconcat(dirname,new_file_name,extension,NULL);
            else
                new_file_name_path = g_strconcat(dirname,G_DIR_SEPARATOR_S,new_file_name,extension,NULL);
        }

        g_free(dirname);
        g_free(new_file_name);
        g_free(extension);
        return new_file_name_path; // in file system encoding
    }else
    {
        return NULL;
    }
}
#endif


/* Convert filename extension (lower/upper/no change). */
gchar *
ET_File_Format_File_Extension (const ET_File *ETFile)
{
    switch (g_settings_get_enum (MainSettings, "rename-extension-mode"))
    {
        case ET_FILENAME_EXTENSION_LOWER_CASE:
            return g_ascii_strdown (ETFile->ETFileDescription->Extension, -1);
        case ET_FILENAME_EXTENSION_UPPER_CASE:
            return g_ascii_strup (ETFile->ETFileDescription->Extension, -1);
        case ET_FILENAME_EXTENSION_NO_CHANGE:
        default:
            return g_strdup (ETFile->ETFileExtension);
    };
}
