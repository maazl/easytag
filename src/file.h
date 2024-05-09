/* EasyTAG - tag editor for audio files
 * Copyright (C) 2014,2015  David King <amigadave@amigadave.com>
 * Copyright (C) 2022  Marcel Müller <github@maazl.de>
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

#include "core_types.h"
#include "file_description.h"
#include "file_name.h"
#include "file_tag.h"
#include "setting.h"
#ifdef __cplusplus
#include "misc.h"

/*
 * Structure containing informations of the header of file
 * Nota: This struct was copied from an "MP3 structure", and will change later.
 */
typedef struct
{
    gint version;               /* Version of bitstream (mpeg version for mp3, encoder version for ogg) */
    gsize layer; /* "MP3 data" */
    gint bitrate;               /* Bitrate (kb/s) */
    gboolean variable_bitrate;  /* Is a VBR file? */
    gint samplerate;            /* Samplerate (Hz) */
    gint mode;                  /* Stereo, ... or channels for ogg */
    goffset size;               /* The size of file (in bytes) */
    gint duration;              /* The duration of file (in seconds) */
    gchar *mpc_profile;         /* MPC data */
    gchar *mpc_version;         /* MPC data : encoder version  (also for Speex) */
} ET_File_Info;

/*
 * Description of each item of the ETFileList list
 */
typedef struct
{
    guint IndexKey;           /* Value used to display the position in the list (and in the BrowserList) - Must be renumered after resorting the list - This value varies when resorting list */

    guint ETFileKey;          /* Primary key to identify each item of the list (no longer used?) */

    guint64 FileModificationTime; /* Save modification time of the file */

    const ET_File_Description *ETFileDescription;
    gchar               *ETFileExtension;   /* Real extension of the file (keeping the case) (should be placed in ETFileDescription?) */
    ET_File_Info        ETFileInfo;        /* Header infos: bitrate, duration, ... */

    gListP<File_Name*> FileNameCur;      /* Points to item of FileNameList that represents the current value of filename state (i.e. file on hard disk) */
    gListP<File_Name*> FileNameNew;      /* Points to item of FileNameList that represents the new value of filename state */
    gListP<File_Name*> FileNameList;     /* Contains the history of changes about the filename. */
    gListP<File_Name*> FileNameListBak;  /* Contains items of FileNameList removed by 'undo' procedure but have data currently saved (for example, when you save your last changes, make some 'undo', then make new changes) */

    gListP<File_Tag*> FileTagCur;        /* Points to the current item used of FileTagList */
    gListP<File_Tag*> FileTag;           /* Points to the new item used of FileTagList */
    gListP<File_Tag*> FileTagList;       /* Contains the history of changes about file tag data */
    gListP<File_Tag*> FileTagListBak;    /* Contains items of FileTagList removed by 'undo' procedure but have data currently saved */

    gboolean activate_bg_color; // For browser list: alternating background due to sub directory change.
} ET_File;

#else
typedef struct ET_File ET_File;
#endif

/*
 * Description of each item of the ETHistoryFileList list
 */
typedef struct
{
    ET_File *ETFile;           /* Pointer to item of ETFileList changed */
} ET_History_File;

G_BEGIN_DECLS

gboolean et_file_check_saved (const ET_File *ETFile);

ET_File * ET_File_Item_New (void);
void ET_Free_File_List_Item (ET_File *ETFile);

void ET_Save_File_Data_From_UI (ET_File *ETFile);
gboolean ET_Save_File_Name_Internal (const ET_File *ETFile, File_Name *FileName);
gboolean ET_Save_File_Tag_To_HD (ET_File *ETFile, GError **error);
gboolean ET_Save_File_Tag_Internal (ET_File *ETFile, File_Tag *FileTag);

gboolean ET_Undo_File_Data (ET_File *ETFile);
gboolean ET_Redo_File_Data (ET_File *ETFile);
gboolean ET_File_Data_Has_Undo_Data (const ET_File *ETFile);
gboolean ET_File_Data_Has_Redo_Data (const ET_File *ETFile);

gboolean ET_Manage_Changes_Of_File_Data (ET_File *ETFile, File_Name *FileName, File_Tag *FileTag);
void ET_Mark_File_Name_As_Saved (ET_File *ETFile);
gchar *et_file_generate_name (const ET_File *ETFile, const gchar *new_file_name);
gchar * ET_File_Format_File_Extension (const ET_File *ETFile);

gint (*ET_Get_Comp_Func_Sort_File(EtSortMode sort_mode))(const ET_File *ETFile1, const ET_File *ETFile2);

G_END_DECLS

#endif /* !ET_FILE_H_ */
