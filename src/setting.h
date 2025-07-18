/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2022-2025  Marcel Müller <github@maazl.de>
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
 * Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef ET_SETTINGS_H_
#define ET_SETTINGS_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
#include "misc.h"
#endif

/* Categories to search in CDDB manual search. */
typedef enum
{
    ET_CDDB_SEARCH_CATEGORY_BLUES = 1 << 0,
    ET_CDDB_SEARCH_CATEGORY_CLASSICAL = 1 << 1,
    ET_CDDB_SEARCH_CATEGORY_COUNTRY = 1 << 2,
    ET_CDDB_SEARCH_CATEGORY_FOLK = 1 << 3,
    ET_CDDB_SEARCH_CATEGORY_JAZZ = 1 << 4,
    ET_CDDB_SEARCH_CATEGORY_MISC = 1 << 5,
    ET_CDDB_SEARCH_CATEGORY_NEWAGE = 1 << 6,
    ET_CDDB_SEARCH_CATEGORY_REGGAE = 1 << 7,
    ET_CDDB_SEARCH_CATEGORY_ROCK = 1 << 8,
    ET_CDDB_SEARCH_CATEGORY_SOUNDTRACK = 1 << 9
} EtCddbSearchCategory;

#ifdef __cplusplus
MAKE_FLAGS_ENUM(EtCddbSearchCategory);
#endif

/* Fields to use in CDDB manual search. */
typedef enum
{
    ET_CDDB_SEARCH_FIELD_ARTIST = 1 << 0,
    ET_CDDB_SEARCH_FIELD_TITLE = 1 << 1,
    ET_CDDB_SEARCH_FIELD_TRACK = 1 << 2,
    ET_CDDB_SEARCH_FIELD_OTHER = 1 << 3
} EtCddbSearchField;

#ifdef __cplusplus
MAKE_FLAGS_ENUM(EtCddbSearchField);
#endif

/* Fields to set from CDDB search results. */
typedef enum
{
    ET_CDDB_SET_FIELD_TITLE = 1 << 0,
    ET_CDDB_SET_FIELD_ARTIST = 1 << 1,
    ET_CDDB_SET_FIELD_ALBUM = 1 << 2,
    ET_CDDB_SET_FIELD_YEAR = 1 << 3,
    ET_CDDB_SET_FIELD_TRACK = 1 << 4,
    ET_CDDB_SET_FIELD_TRACK_TOTAL = 1 << 5,
    ET_CDDB_SET_FIELD_GENRE = 1 << 6,
    ET_CDDB_SET_FIELD_FILENAME = 1 << 7
} EtCddbSetField;

#ifdef __cplusplus
MAKE_FLAGS_ENUM(EtCddbSetField);
#endif

/* Character set for passing to iconv. */
typedef enum
{
    ET_CHARSET_IBM864,
    ET_CHARSET_ISO_8859_6,
    ET_CHARSET_WINDOWS_1256,
    ET_CHARSET_ISO_8859_13,
    ET_CHARSET_ISO_8859_4,
    ET_CHARSET_WINDOWS_1257,
    ET_CHARSET_ISO_8859_14,
    ET_CHARSET_IBM852,
    ET_CHARSET_ISO_8859_2,
    ET_CHARSET_WINDOWS_1250,
    ET_CHARSET_GB18030,
    ET_CHARSET_GB2312,
    ET_CHARSET_BIG5,
    ET_CHARSET_BIG5_HKSCS,
    ET_CHARSET_IBM855,
    ET_CHARSET_ISO_8859_5,
    ET_CHARSET_ISO_IR_111,
    ET_CHARSET_ISO_KOI8_R,
    ET_CHARSET_WINDOWS_1251,
    ET_CHARSET_IBM866,
    ET_CHARSET_KOI8_U,
    ET_CHARSET_US_ASCII,
    ET_CHARSET_ISO_8859_7,
    ET_CHARSET_WINDOWS_1253,
    ET_CHARSET_IBM862,
    ET_CHARSET_WINDOWS_1255,
    ET_CHARSET_EUC_JP,
    ET_CHARSET_ISO_2022_JP,
    ET_CHARSET_SHIFT_JIS,
    ET_CHARSET_EUC_KR,
    ET_CHARSET_ISO_8859_10,
    ET_CHARSET_ISO_8859_3,
    ET_CHARSET_TIS_620,
    ET_CHARSET_IBM857,
    ET_CHARSET_ISO_8859_9,
    ET_CHARSET_WINDOWS_1254,
    ET_CHARSET_UTF_8,
    ET_CHARSET_VISCII,
    ET_CHARSET_WINDOWS_1258,
    ET_CHARSET_ISO_8859_8,
    ET_CHARSET_IBM850,
    ET_CHARSET_ISO_8859_1,
    ET_CHARSET_ISO_8859_15,
    ET_CHARSET_WINDOWS_1252
} EtCharset;

/* Method for processing spaces when updating tags. */
typedef enum
{
    ET_CONVERT_SPACES_SPACES,
    ET_CONVERT_SPACES_UNDERSCORES,
    ET_CONVERT_SPACES_REMOVE,
    ET_CONVERT_SPACES_NO_CHANGE
} EtConvertSpaces;

typedef enum
{
    ET_FILENAME_REPLACE_ASCII,
    ET_FILENAME_REPLACE_UNICODE,
    ET_FILENAME_REPLACE_NONE
} EtFilenameReplaceMode;

typedef enum
{
    ET_FILENAME_EXTENSION_LOWER_CASE,
    ET_FILENAME_EXTENSION_UPPER_CASE,
    ET_FILENAME_EXTENSION_NO_CHANGE
} EtFilenameExtensionMode;

/* Scanner dialog process fields capitalization options. */
typedef enum
{
    ET_PROCESS_CAPITALIZE_ALL_UP,
    ET_PROCESS_CAPITALIZE_ALL_DOWN,
    ET_PROCESS_CAPITALIZE_FIRST_LETTER_UP,
    ET_PROCESS_CAPITALIZE_FIRST_WORDS_UP,
    ET_PROCESS_CAPITALIZE_NO_CHANGE
} EtProcessCapitalize;

/* Tag fields to process in the scanner. */
typedef enum
{
    ET_PROCESS_FIELD_FILENAME        = 1 << 0,
    ET_PROCESS_FIELD_TITLE           = 1 << 1,
    ET_PROCESS_FIELD_VERSION         = 1 << 2,
    ET_PROCESS_FIELD_SUBTITLE        = 1 << 3,
    ET_PROCESS_FIELD_ARTIST          = 1 << 4,
    ET_PROCESS_FIELD_ALBUM_ARTIST    = 1 << 5,
    ET_PROCESS_FIELD_ALBUM           = 1 << 6,
    ET_PROCESS_FIELD_DISC_SUBTITLE   = 1 << 7,
    ET_PROCESS_FIELD_GENRE           = 1 << 8,
    ET_PROCESS_FIELD_COMMENT         = 1 << 9,
    ET_PROCESS_FIELD_COMPOSER        = 1 << 10,
    ET_PROCESS_FIELD_ORIGINAL_ARTIST = 1 << 11,
    ET_PROCESS_FIELD_COPYRIGHT       = 1 << 12,
    ET_PROCESS_FIELD_URL             = 1 << 13,
    ET_PROCESS_FIELD_ENCODED_BY      = 1 << 14,
    ET_PROCESS_FIELD_DESCRIPTION     = 1 << 15
} EtProcessField;

#ifdef __cplusplus
MAKE_FLAGS_ENUM(EtProcessField);
#endif

typedef enum
{
    ET_PROCESS_FIELDS_CONVERT_SPACES,
    ET_PROCESS_FIELDS_CONVERT_UNDERSCORES,
    ET_PROCESS_FIELDS_CONVERT_CHARACTERS,
    ET_PROCESS_FIELDS_CONVERT_NO_CHANGE
} EtProcessFieldsConvert;

/* Content of generated playlists. */
typedef enum
{
    ET_PLAYLIST_CONTENT_FILENAMES,
    ET_PLAYLIST_CONTENT_EXTENDED,
    ET_PLAYLIST_CONTENT_EXTENDED_MASK
} EtPlaylistContent;

/* Encoding options when renaming files. */
typedef enum
{
    ET_RENAME_ENCODING_TRY_ALTERNATIVE,
    ET_RENAME_ENCODING_TRANSLITERATE,
    ET_RENAME_ENCODING_IGNORE
} EtRenameEncoding;

/*
 * The mode for the scanner window.
 */
typedef enum
{
    ET_SCAN_MODE_FILL_TAG,
    ET_SCAN_MODE_RENAME_FILE,
    ET_SCAN_MODE_PROCESS_FIELDS
} EtScanMode;

/* Types of sorting. See the GSettings key "sort-order". */
typedef enum
{
    ET_SORT_MODE_FILEPATH,
    ET_SORT_MODE_FILENAME,
    ET_SORT_MODE_TITLE,
    ET_SORT_MODE_VERSION,
    ET_SORT_MODE_SUBTITLE,
    ET_SORT_MODE_ARTIST,
    ET_SORT_MODE_ALBUM_ARTIST,
    ET_SORT_MODE_ALBUM,
    ET_SORT_MODE_DISC_SUBTITLE,
    ET_SORT_MODE_YEAR,
    ET_SORT_MODE_RELEASE_YEAR,
    ET_SORT_MODE_DISC_NUMBER,
    ET_SORT_MODE_TRACK_NUMBER,
    ET_SORT_MODE_GENRE,
    ET_SORT_MODE_COMMENT,
    ET_SORT_MODE_COMPOSER,
    ET_SORT_MODE_ORIG_ARTIST,
    ET_SORT_MODE_ORIG_YEAR,
    ET_SORT_MODE_COPYRIGHT,
    ET_SORT_MODE_URL,
    ET_SORT_MODE_ENCODED_BY,
    ET_SORT_MODE_CREATION_DATE,
    ET_SORT_MODE_FILE_TYPE,
    ET_SORT_MODE_FILE_SIZE,
    ET_SORT_MODE_FILE_DURATION,
    ET_SORT_MODE_FILE_BITRATE,
    ET_SORT_MODE_FILE_SAMPLERATE,
    ET_SORT_MODE_REPLAYGAIN
} EtSortMode;

/* Types of columns. */
typedef enum
{
    ET_COLUMN_FILEPATH        = 1 << 0,
    ET_COLUMN_FILENAME        = 1 << 1,
    ET_COLUMN_TITLE           = 1 << 2,
    ET_COLUMN_VERSION         = 1 << 3,
    ET_COLUMN_SUBTITLE        = 1 << 4,
    ET_COLUMN_ARTIST          = 1 << 5,
    ET_COLUMN_ALBUM_ARTIST    = 1 << 6,
    ET_COLUMN_ALBUM           = 1 << 7,
    ET_COLUMN_DISC_SUBTITLE   = 1 << 8,
    ET_COLUMN_YEAR            = 1 << 9,
    ET_COLUMN_RELEASE_YEAR    = 1 << 10,
    ET_COLUMN_DISC_NUMBER     = 1 << 11,
    ET_COLUMN_TRACK_NUMBER    = 1 << 12,
    ET_COLUMN_GENRE           = 1 << 13,
    ET_COLUMN_COMMENT         = 1 << 14,
    ET_COLUMN_COMPOSER        = 1 << 15,
    ET_COLUMN_ORIG_ARTIST     = 1 << 16,
    ET_COLUMN_ORIG_YEAR       = 1 << 17,
    ET_COLUMN_COPYRIGHT       = 1 << 18,
    ET_COLUMN_URL             = 1 << 19,
    ET_COLUMN_ENCODED_BY      = 1 << 20,
    ET_COLUMN_CREATION_DATE   = 1 << 21,
    ET_COLUMN_FILE_TYPE       = 1 << 22,
    ET_COLUMN_FILE_SIZE       = 1 << 23,
    ET_COLUMN_FILE_DURATION   = 1 << 24,
    ET_COLUMN_FILE_BITRATE    = 1 << 25,
    ET_COLUMN_FILE_SAMPLERATE = 1 << 26,
    ET_COLUMN_IMAGE           = 1 << 27,
    ET_COLUMN_REPLAYGAIN      = 1 << 28,
    ET_COLUMN_DESCRIPTION     = 1 << 30
} EtColumn;

#ifdef __cplusplus
MAKE_FLAGS_ENUM(EtColumn);
#endif

/* Additional options to be passed to iconv(). */
typedef enum
{
    ET_TAG_ENCODING_NONE,
    ET_TAG_ENCODING_TRANSLITERATE,
    ET_TAG_ENCODING_IGNORE
} EtTagEncoding;

/* ReplayGain calculation model */
typedef enum
{
	ET_REPLAYGAIN_MODEL_V1,
	ET_REPLAYGAIN_MODEL_V2,
	ET_REPLAYGAIN_MODEL_V15,
} EtReplayGainModel;

/* ReplayGain grouping for album gain */
typedef enum
{
	ET_REPLAYGAIN_GROUPBY_NONE,
	ET_REPLAYGAIN_GROUPBY_ALBUM,
	ET_REPLAYGAIN_GROUPBY_DISC,
	ET_REPLAYGAIN_GROUPBY_FILEPATH
} EtReplayGainGroupBy;

/*
 * Config variables
 */

extern GSettings *MainSettings;

void Init_Config_Variables (void);

gboolean Setting_Create_Files     (void);


/* MasksList */
void Load_Scan_Tag_Masks_List (GtkListStore *liststore, gint colnum,
                               const gchar * const *fallback);
void Save_Scan_Tag_Masks_List (GtkListStore *liststore, gint colnum);

/* RenameFileMasksList */
void Load_Rename_File_Masks_List (GtkListStore *liststore, gint colnum,
                                  const gchar * const *fallback);
void Save_Rename_File_Masks_List (GtkListStore *liststore, gint colnum);

/* 'BrowserEntry' combobox */
void Load_Path_Entry_List (GtkListStore *liststore, gint colnum);
void Save_Path_Entry_List (GtkListStore *liststore, gint colnum);

/* Run Program combobox (tree browser) */
void Load_Run_Program_With_Directory_List (GtkListStore *liststore, gint colnum);
void Save_Run_Program_With_Directory_List (GtkListStore *liststore, gint colnum);

/* Run Program combobox (file browser) */
void Load_Run_Program_With_File_List (GtkListStore *liststore, gint colnum);
void Save_Run_Program_With_File_List (GtkListStore *liststore, gint colnum);

/* 'SearchStringEntry' combobox */
void Load_Search_File_List (GtkListStore *liststore, gint colnum);
void Save_Search_File_List (GtkListStore *liststore, gint colnum);

gboolean et_settings_enum_get (GValue *value, GVariant *variant,
                               gpointer user_data);
GVariant *et_settings_enum_set (const GValue *value,
                                const GVariantType *expected_type,
                                gpointer user_data);
gboolean et_settings_enum_radio_get (GValue *value, GVariant *variant,
                                     gpointer user_data);
GVariant *et_settings_enum_radio_set (const GValue *value,
                                      const GVariantType *expected_type,
                                      gpointer user_data);
gboolean et_settings_flags_toggle_get (GValue *value, GVariant *variant,
                                       gpointer user_data);
GVariant *et_settings_flags_toggle_set (const GValue *value,
                                        const GVariantType *expected_type,
                                        gpointer user_data);

#endif /* ET_SETTINGS_H_ */
