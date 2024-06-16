/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2014-2015  David King <amigadave@amigadave.com>
 * Copyright (C) 2001-2003  Jerome Couderc <easytag@gmail.com>
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

#include "config.h"

#include <glib/gi18n.h>
#include <string.h>
#include <errno.h>

#include "id3_tag.h"
#include "ape_tag.h"
#include "picture.h"
#include "genres.h"
#include "setting.h"
#include "misc.h"
#include "file.h"
#include "charset.h"

#ifdef ENABLE_MP3

#ifdef ENABLE_ID3LIB
#include <id3/tag.h>
#endif

#include <string>
using namespace std;

/****************
 * Declarations *
 ****************/
/** Restrict the length of ID3 string tags on read to avoid excessive resource use. */
#define ID3V2_MAX_STRING_LEN 4096


#ifdef ENABLE_ID3LIB

/**************
 * Prototypes *
 **************/
static void Id3tag_Prepare_ID3v1 (ID3_Tag& id3_tag);
static gchar *Id3tag_Rules_For_ISO_Fields (const gchar *string,
                                           const gchar *from_codeset,
                                           const gchar *to_codeset);
static gchar *Id3tag_Get_Field (const ID3_Frame& id3_frame,
                                ID3_FieldID id3_fieldid);
static ID3_TextEnc Id3tag_Set_Field (const ID3_Frame& id3_frame,
                                     ID3_FieldID id3_fieldid,
                                     const gchar *string);

static size_t ID3Tag_Link_1         (ID3_Tag& id3tag, const char *filename);

static gboolean id3tag_check_if_id3lib_is_buggy (GError **error);



/*************
 * Functions *
 *************/

/* et_id3_error_quark:
 *
 * Quark for EtID3Error domain.
 *
 * Returns: a GQuark for the EtID3Error domain
 */
GQuark
et_id3_error_quark (void)
{
    return g_quark_from_static_string ("et-id3-error-quark");
}

static bool id3tag_set_text_frame(ID3_Tag& id3_tag, ID3_FrameID frame_id, const gchar* value, const char* desc = nullptr)
{
	// To avoid problem with a corrupted field, we remove it before to create a new one.
	ID3_Frame *id3_frame, *first = nullptr;
	while ((id3_frame = id3_tag.Find(frame_id)) != first)
	{	if (desc)
		{	// Find by description cannot deal with different encodings
			// id3_frame = id3_tag.Find(frame_id, ID3FN_DESCRIPTION, desc);
			if (!first)
				first = id3_frame;
			// check description
			gString tmp(Id3tag_Get_Field(*id3_frame, ID3FN_DESCRIPTION));
			if (g_ascii_strcasecmp(desc, tmp))
				continue; // description does not match => skip
		}

		delete id3_tag.RemoveFrame(id3_frame);
		first = nullptr; // RemoveFrame resets the internal cursor
	}

	if (et_str_empty(value))
		return false;

	id3_frame = new ID3_Frame(frame_id);
	id3_tag.AttachFrame(id3_frame);
	if (desc) Id3tag_Set_Field(*id3_frame, ID3FN_DESCRIPTION, desc);
	Id3tag_Set_Field(*id3_frame, ID3FN_TEXT, value);
	return true;
}

/*
 * Write the ID3 tags to the file. Returns TRUE on success, else 0.
 */
static gboolean
id3tag_write_file_v23tag (const ET_File *ETFile,
                          GError **error)
{
    const File_Tag *FileTag;
    gboolean success = TRUE;
    gint number_of_frames;
    gboolean has_data = FALSE;
    //gboolean has_song_len    = FALSE;
    static gboolean flag_first_check = TRUE;
    static gboolean flag_id3lib_bugged = TRUE;

    ID3_Frame *id3_frame;
    ID3_Field *id3_field;
    string tmp;
    EtPicture *pic;

    g_return_val_if_fail (ETFile != NULL && ETFile->FileTag != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    // When writing the first MP3 file, we check if the version of id3lib of the
    // system doesn't contain a bug when writting Unicode tags
    if (flag_first_check
        && g_settings_get_boolean (MainSettings, "id3v2-enable-unicode"))
    {
        flag_first_check = FALSE;
        flag_id3lib_bugged = id3tag_check_if_id3lib_is_buggy (NULL);
    }

    FileTag  = ETFile->FileTag->data;
    const char* filename = ETFile->FileNameCur->data->value();

    gObject<GFile> file(g_file_new_for_path(filename));

    /* This is a protection against a bug in id3lib that enters an infinite
     * loop with corrupted MP3 files (files containing only zeroes) */
    if (!et_id3tag_check_if_file_is_valid (file.get(), error))
    {
        if (error)
        {
            g_debug ("Error while checking if ID3 tag is valid: %s",
                     (*error)->message);
        }

        g_clear_error (error);
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s",
                     _("Corrupted file"));
        return FALSE;
    }

  try
  {
    /* We get again the tag from the file to keep also unused data (by EasyTAG), then
     * we replace the changed data */
    ID3_Tag id3_tag;

#ifdef G_OS_WIN32
    /* On Windows, id3lib expects filenames to be in the system codepage. */
    {
        gString locale_filename(g_win32_locale_filename_from_utf8(filename));

        if (!locale_filename)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s",
                         g_strerror (EINVAL));
            return FALSE;
        }

        id3_tag.Link(locale_filename);
    }
#else
    id3_tag.Link(filename);
#endif

    /* Set padding when tag was changed, for faster writing */
    id3_tag.SetPadding(TRUE);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_TITLE, FileTag->title);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_SUBTITLE, FileTag->subtitle);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_LEADARTIST, FileTag->artist);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_BAND, FileTag->album_artist);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_ALBUM, FileTag->album);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_SETSUBTITLE, FileTag->disc_subtitle);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_PARTINSET, FileTag->disc_and_total().c_str());

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_YEAR, FileTag->year);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_TRACKNUM, FileTag->track_and_total().c_str());

    /* Genre is written like this :
     *    - "(<genre_id>)"              -> "(3)"
     *    - "(<genre_id>)<refinement>"  -> "(3)EuroDance"
     */
    tmp.clear();
    if (!et_str_empty (FileTag->genre))
    {
        guchar genre_value = Id3tag_String_To_Genre(FileTag->genre);
        // If genre not defined don't write genre value between brackets! (priority problem noted with some tools)
        if ((genre_value == ID3_INVALID_GENRE)
            || g_settings_get_boolean (MainSettings, "id3v2-text-only-genre"))
            tmp = FileTag->genre;
        else
            tmp = '(' + to_string(genre_value) + ')';
    }
    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_CONTENTTYPE, tmp.c_str());

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_COMMENT, FileTag->comment);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_COMPOSER, FileTag->composer);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_ORIGARTIST, FileTag->orig_artist);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_ORIGYEAR, FileTag->orig_year);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_COPYRIGHT, FileTag->copyright);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_WWWUSER, FileTag->url);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_ENCODEDBY, FileTag->encoded_by);

    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_USERTEXT, FileTag->track_gain_str().c_str(), "REPLAYGAIN_TRACK_GAIN");
    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_USERTEXT, FileTag->track_peak_str().c_str(), "REPLAYGAIN_TRACK_PEAK");
    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_USERTEXT, FileTag->album_gain_str().c_str(), "REPLAYGAIN_ALBUM_GAIN");
    has_data |= id3tag_set_text_frame(id3_tag, ID3FID_USERTEXT, FileTag->album_peak_str().c_str(), "REPLAYGAIN_ALBUM_PEAK");

    /***********
     * Picture *
     ***********/
    while ( (id3_frame = id3_tag.Find(ID3FID_PICTURE)) )
        delete id3_tag.RemoveFrame(id3_frame);

    for (pic = FileTag->picture; pic != NULL; pic = pic->next)
    {
        Picture_Format format = Picture_Format_From_Data(pic);

        id3_frame = new ID3_Frame(ID3FID_PICTURE);
        id3_tag.AttachFrame(id3_frame);

        switch (format)
        {
            case PICTURE_FORMAT_JPEG:
                if ((id3_field = id3_frame->GetField(ID3FN_MIMETYPE)))
                    id3_field->Set(Picture_Mime_Type_String(format));
                if ((id3_field = id3_frame->GetField(ID3FN_IMAGEFORMAT)))
                    id3_field->Set("JPG");
                break;

            case PICTURE_FORMAT_PNG:
                if ((id3_field = id3_frame->GetField(ID3FN_MIMETYPE)))
                    id3_field->Set(Picture_Mime_Type_String(format));
                if ((id3_field = id3_frame->GetField(ID3FN_IMAGEFORMAT)))
                    id3_field->Set("PNG");
                break;
            case PICTURE_FORMAT_GIF:
                if ((id3_field = id3_frame->GetField(ID3FN_MIMETYPE)))
                    id3_field->Set(Picture_Mime_Type_String(format));
                if ((id3_field = id3_frame->GetField(ID3FN_IMAGEFORMAT)))
                    /* I could find no reference for what ID3FN_IMAGEFORMAT
                     * should contain, so this is a guess. */
                    id3_field->Set("GIF");
                break;
            case PICTURE_FORMAT_UNKNOWN:
            default:
                break;
        }

        if ((id3_field = id3_frame->GetField(ID3FN_PICTURETYPE)))
            id3_field->Set(pic->type);

        if (pic->description)
            Id3tag_Set_Field(*id3_frame, ID3FN_DESCRIPTION, pic->description);

        if ((id3_field = id3_frame->GetField(ID3FN_DATA)))
        {
            gconstpointer data;
            gsize data_size;

            data = g_bytes_get_data (pic->bytes, &data_size);
            id3_field->Set((const uchar*)data, data_size);
        }

        has_data = TRUE;
    }


    /*********************************
     * File length (in milliseconds) *
     *********************************/
    /* Don't write this field, not useful? *
    while ( (id3_frame = id3_tag.Find(ID3FID_SONGLEN)) )
        delete id3_tag.RemoveFrame(id3_frame);
    if (ETFile->ETFileInfo && ((ET_File_Info *)ETFile->ETFileInfo)->duration > 0 )
    {
        id3_frame = new ID3_Frame(ID3FID_SONGLEN);
        id3_tag.AttachFrame(id3_frame);

        gString string(g_strdup_printf("%d",((ET_File_Info *)ETFile->ETFileInfo)->duration * 1000));
        Id3tag_Set_Field(*id3_frame, ID3FN_TEXT, string);
        has_data = TRUE;
    }*/


    /******************************
     * Delete an APE tag if found *
     ******************************/
    {
        // Delete the APE tag (create a dummy ETFile for the Ape_Tag_... function)
        ET_File   *ETFile_tmp    = ET_File_Item_New();
        // Same file...
        ETFile_tmp->FileNameCur  =
        ETFile_tmp->FileNameList = gListP<File_Name*>(new File_Name(*ETFile->FileNameCur->data));
        // With empty tag...
        ETFile_tmp->FileTag      =
        ETFile_tmp->FileTagList  = gListP<File_Tag*>(new File_Tag());
        ape_tag_write_file_tag (ETFile_tmp, NULL);
        ET_Free_File_List_Item(ETFile_tmp);
    }


    /*********************************
     * Update id3v1.x and id3v2 tags *
     *********************************/
    /* Get the number of frames into the tag, cause if it is
     * equal to 0, id3lib-3.7.12 doesn't update the tag */
    number_of_frames = id3_tag.NumFrames();

    /* If all fields (managed in the UI) are empty and option id3-strip-empty
     * is set to TRUE, we strip the ID3v1.x and ID3v2 tags. Else, write ID3v2
     * and/or ID3v1. */
    if (g_settings_get_boolean (MainSettings, "id3-strip-empty") && !has_data)
    {
        id3_tag.Strip(ID3TT_ID3V1);
        id3_tag.Strip(ID3TT_ID3V2);
        g_debug (_("Removed tag of ‘%s’"), ETFile->FileNameCur->data->file_value_utf8());
    }
    else
    {
        /* It's better to remove the id3v1 tag before, to synchronize it with the
         * id3v2 tag (else id3lib doesn't do it correctly)
         */
        id3_tag.Strip(ID3TT_ID3V1);

        /*
         * ID3v2 tag
         */
        if (number_of_frames != 0 && g_settings_get_boolean(MainSettings, "id3v2-enabled"))
        {
            id3_tag.Update(ID3TT_ID3V2);
        }
        else
        {
            id3_tag.Strip(ID3TT_ID3V2);
        }

        /*
         * ID3v1 tag
         * Must be set after ID3v2 or ID3Tag_UpdateByTagType cause damage to unicode strings
         */
        // id3lib writes incorrectly the ID3v2 tag if unicode used when writing ID3v1 tag
        if (number_of_frames != 0 && g_settings_get_boolean(MainSettings, "id3v1-enabled"))
        {
            // By default id3lib converts id3tag to ISO-8859-1 (single byte character set)
            // Note : converting UTF-16 string (two bytes character set) to ISO-8859-1
            //        remove only the second byte => a strange string appears...
            Id3tag_Prepare_ID3v1(id3_tag);

            id3_tag.Update(ID3TT_ID3V1);
        }else
        {
            id3_tag.Strip(ID3TT_ID3V1);
        }
    }
  } catch (...)
  { // The C API of ID3lib always handles exceptions with "on error resume next".
    // Let's improve the situation slightly...
    return FALSE;
  }

    /* Do a one-time check that the id3lib version is not buggy. */
    if (g_settings_get_boolean (MainSettings, "id3v2-enabled")
        && number_of_frames != 0 && success)
    {
        /* See known problem on the top : [ 1016290 ] Unicode16 writing bug.
         * When we write the tag in Unicode, we try to check if it was
         * correctly written. So to test it : we read again the tag, and then
         * compare with the previous one. We check up to find an error (as only
         * some characters are affected).
         * If the patch to id3lib was applied to fix the problem (tested
         * by id3tag_check_if_id3lib_is_buggy) we didn't make the following
         * test => OK */
        if (flag_id3lib_bugged
            && g_settings_get_boolean (MainSettings,
                                       "id3v2-enable-unicode"))
        {
            ET_File   *ETFile_tmp    = ET_File_Item_New();
            File_Tag  *FileTag_tmp   = new File_Tag();
            ETFile_tmp->FileTagList  = gListP<File_Tag*>(FileTag_tmp);
            ETFile_tmp->FileTag      = ETFile_tmp->FileTagList;

            if (id3_read_file(file.get(), ETFile_tmp, NULL) == TRUE
                && *FileTag != *FileTag_tmp)
            {
                flag_id3lib_bugged = FALSE; /* Report the error only once. */
                success = FALSE;
                g_set_error (error, ET_ID3_ERROR,
                             ET_ID3_ERROR_BUGGY_ID3LIB, "%s",
                             _("Buggy id3lib"));
            }

            delete FileTag_tmp;
            ET_Free_File_List_Item(ETFile_tmp);
        }
    }

    return success;
}


/*
 * As the ID3Tag_Link function of id3lib-3.8.0pre2 returns the ID3v1 tags
 * when a file has both ID3v1 and ID3v2 tags, we first try to explicitely
 * get the ID3v2 tags with ID3Tag_LinkWithFlags and, if we cannot get them,
 * fall back to the ID3v1 tags.
 * (Written by Holger Schemel).
 */
static size_t ID3Tag_Link_1 (ID3_Tag& id3tag, const char *filename)
{
    size_t offset;

#ifdef G_OS_WIN32
    /* On Windows, id3lib expects filenames to be in the system codepage. */
    gString locale_filename(g_win32_locale_filename_from_utf8(filename));

    if (!locale_filename)
    {
        g_debug ("Error converting filename '%s' to system codepage",
                 filename);
        return 0;
    }

    filename = locale_filename;
#endif

#   if (0) // Link the file with the both tags may cause damage to unicode strings
//#   if ( (ID3LIB_MAJOR >= 3) && (ID3LIB_MINOR >= 8) && (ID3LIB_PATCH >= 1) ) // Same test used in Id3tag_Read_File_Tag to use ID3Tag_HasTagType
        /* No problem of priority, so we link the file with the both tags
         * to manage => ID3Tag_HasTagType works correctly */
        offset = id3tag.Link(filename, ID3TT_ID3V1 | ID3TT_ID3V2);
#   elif ( (ID3LIB_MAJOR >= 3) && (ID3LIB_MINOR >= 8) )
        /* Version 3.8.0pre2 gives priority to tag id3v1 instead of id3v2, so we
         * try to fix it by linking the file with the id3v2 tag first. This bug
         * was fixed in the final version of 3.8.0 but we can't know it... */
        /* First, try to get the ID3v2 tags */
        offset = id3tag.Link(filename, ID3TT_ID3V2);

        if (offset == 0)
        {
            /* No ID3v2 tags available => try to get the ID3v1 tags */
            offset = id3tag.Link(filename, ID3TT_ID3V1);
        }
#   else
        /* Function 'ID3Tag_LinkWithFlags' is not defined up to id3lib-.3.7.13 */
        offset = id3tag.Link(filename);
#   endif
    //g_print("ID3 TAG SIZE: %d\t%s\n",offset,g_path_get_basename(filename));

    return offset;
}

/*
 * Source : "http://www.id3.org/id3v2.4.0-structure.txt"
 *
 * Frames that allow different types of text encoding contains a text
 *   encoding description byte. Possible encodings:
 *
 *     $00   ISO-8859-1 [ISO-8859-1]. Terminated with $00.
 *     $01   UTF-16 [UTF-16] encoded Unicode [UNICODE] with BOM ($FF FE
 *           or $FE FF). All strings in the same frame SHALL have the same
 *           byteorder. Terminated with $00 00.
 *     $02   UTF-16BE [UTF-16] encoded Unicode [UNICODE] without BOM.
 *           Terminated with $00 00.
 *     $03   UTF-8 [UTF-8] encoded Unicode [UNICODE]. Terminated with $00.
 *
 * For example :
 *         T  P  E  1  .  .  .  .  .  .  .  ?  ?  J  .  o  .  n  .     .  G  .  i  .  n  .  d  .  i  .  c  .  k  .
 * Hex :   54 50 45 31 00 00 00 19 00 00 01 ff fe 4a 00 6f 00 6e 00 20 00 47 00 69 00 6e 00 64 00 6e 00 63 00 6b 00
 *                                       ^
 *                                       |___ UTF-16
 */
/*
 * Read the content (ID3FN_TEXT, ID3FN_URL, ...) of the id3_field of the
 * id3_frame, and convert the string if needed to UTF-8.
 */
gchar *Id3tag_Get_Field (const ID3_Frame& id3_frame, ID3_FieldID id3_fieldid)
{
    ID3_Field *id3_field = NULL;
    ID3_Field *id3_field_encoding = NULL;
    size_t num_chars = 0;
    const gchar *string;
    gchar *string1 = NULL;

    //g_print("Id3tag_Get_Field - ID3Frame '%s'\n",ID3FrameInfo_ShortName(ID3Frame_GetID(id3_frame)));

    if ( (id3_field = id3_frame.GetField(id3_fieldid)) )
    {
        ID3_TextEnc enc = ID3TE_NONE;

        // Data of the field must be a TEXT (ID3FTY_TEXTSTRING)
        if (id3_field->GetType() != ID3FTY_TEXTSTRING)
        {
            g_critical ("%s",
                        "Id3tag_Get_Field() must be used only for fields containing text");
            return NULL;
        }

        /*
         * We prioritize the encoding of the field. If the encoding of the field
         * is ISO-8859-1, it can be read with another single byte encoding.
         */
        // Get encoding from content of file...
        id3_field_encoding = id3_frame.GetField(ID3FN_TEXTENC);
        if (id3_field_encoding)
            enc = (ID3_TextEnc)id3_field_encoding->Get();
        // Else, get encoding from the field
        //enc = id3_field->GetEncoding();

        if (enc != ID3TE_UTF16 && enc != ID3TE_UTF8) // Encoding is ISO-8859-1?
        {
            /* Override with another character set? */
            if (g_settings_get_boolean (MainSettings,
                                        "id3-override-read-encoding"))
            {
                /* Encoding set by user to ???. */
                gint id3v1v2_charset;
                const gchar *charset;

                id3v1v2_charset = g_settings_get_enum (MainSettings,
                                                       "id3v1v2-charset");
                charset = et_charset_get_name_from_index (id3v1v2_charset);
                if (strcmp (charset, "ISO-8859-1") == 0)
                {
                    enc = ID3TE_ISO8859_1;
                }
                else if (strcmp (charset, "UTF-16BE") == 0
                         || strcmp (charset, "UTF-16LE") == 0)
                {
                    enc = ID3TE_UTF16;
                }
                else if (strcmp (charset, "UTF-8") == 0)
                {
                    enc = ID3TE_UTF8;
                }
                else if (id3_field->IsEncodable())
                {
                    string1 = convert_string(id3_field->GetRawText(), charset, "UTF-8", FALSE);
                    /* Override to a non-standard character encoding. */
                    goto out;
                }
            }
        }

        // Some fields, as URL, aren't encodable, so there were written using ISO characters.
        if (!id3_field->IsEncodable())
        {
            enc = ID3TE_ISO8859_1;
        }

        // Action according the encoding...
        switch ( enc )
        {
            case ID3TE_ISO8859_1:
                string1 = convert_string_1(id3_field->GetRawText(), id3_field->Size(), "ISO-8859-1", "UTF-8", FALSE);
                break;

            case ID3TE_UTF8: // Shouldn't work with id3lib 3.8.3 (supports only ID3v2.3, not ID3v2.4)
                string = id3_field->GetRawText();
                num_chars = id3_field->Size();
                //string1 = convert_string(string,"UTF-8","UTF-8",FALSE); // Nothing to do
                if (g_utf8_validate(string, num_chars, NULL))
                    string1 = g_strndup(string, num_chars);
                break;

            case ID3TE_UTF16:
                // Id3lib (3.8.3 at least) always returns Unicode strings in UTF-16BE.
            case ID3TE_UTF16BE:
                // "convert_string_1" as we need to pass length for UTF-16
                string1 = convert_string_1((const gchar*)id3_field->GetRawUnicodeText(), id3_field->Size(), "UTF-16BE", "UTF-8", FALSE);
                break;

            case ID3TE_NONE:
            case ID3TE_NUMENCODINGS:
            default:
                string = id3_field->GetRawText();
                num_chars = id3_field->Size();

                if (g_utf8_validate(string, num_chars, NULL))
                    string1 = g_strndup(string, num_chars);
                else
                {
                    GError *error = NULL;

                    string1 = g_locale_to_utf8(string, num_chars, NULL, NULL, &error);

                    if (string1 == NULL)
                    {
                        g_debug ("Error converting string from locale to UTF-8 encoding: %s",
                                 error->message);
                        g_error_free (error);
                    }
                }
                break;
        }
    }
    //g_print(">>ID:%d >'%s' (string1:'%s') (num_chars:%d)\n",ID3Field_GetINT(id3_field_encoding),string,string1,num_chars);

out:
    /* In case the conversion fails, try character fix. */
    if (num_chars && !string1)
    {
        std::string truncated(string, num_chars);
        gString escaped_str(g_strescape(truncated.c_str(), NULL));
        g_debug ("Id3tag_Get_Field: Trying to fix string '%s'…", escaped_str.get());

        string1 = g_filename_display_name(truncated.c_str());

        /* TODO: Set a GError instead. */
        if (!string1)
        {
            g_warning ("%s", "Error converting ID3 tag field encoding");
        }
    }

    return string1;
}


/*
 * Set the content (ID3FN_TEXT, ID3FN_URL, ...) of the id3_field of the
 * id3_frame. Check also if the string must be written from UTF-8 (gtk2) in the
 * ISO-8859-1 encoding or UTF-16.
 *
 * Return the encoding used as if UTF-16 was used, we musn't write the ID3v1 tag.
 *
 * Known problem with id3lib
 * - [ 1016290 ] Unicode16 writing bug
 *               http://sourceforge.net/tracker/index.php?func=detail&aid=1016290&group_id=979&atid=300979
 *               For example with Latin-1 characters (like éöäüß) not saved correctly
 *               in Unicode, due to id3lib (for "é" it will write "E9 FF" instead of "EF 00")
 */
/*
 * OLD NOTE : PROBLEM with ID3LIB
 * - [ 1074169 ] Writing ID3v1 tag breaks Unicode string in v2 tags
 *               http://sourceforge.net/tracker/index.php?func=detail&aid=1074169&group_id=979&atid=100979
 *      => don't write id3v1 tag if Unicode is used, up to patch applied
 */
ID3_TextEnc
Id3tag_Set_Field (const ID3_Frame &id3_frame,
                  ID3_FieldID id3_fieldid,
                  const gchar *string)
{
    ID3_Field *id3_field = NULL;
    ID3_Field *id3_field_encoding = NULL;
    gchar *string_converted = NULL;

    if ((id3_field = id3_frame.GetField(id3_fieldid)))
    {
        ID3_TextEnc enc = ID3TE_NONE;

        // Data of the field must be a TEXT (ID3FTY_TEXTSTRING)
        if (id3_field->GetType() != ID3FTY_TEXTSTRING)
        {
            g_critical ("%s",
                        "Id3tag_Set_Field() must be used only for fields containing text");
            return ID3TE_NONE;
        }

         /* We prioritize the rule selected in options. If the encoding of the
         * field is ISO-8859-1, we can write it to another single byte encoding.
         */
        if (g_settings_get_boolean (MainSettings, "id3v2-enable-unicode"))
        {
            // Check if we can write the tag using ISO-8859-1 instead of UTF-16...
            if ( (string_converted = g_convert(string, strlen(string), "ISO-8859-1",
                                               "UTF-8", NULL, NULL ,NULL)) )
            {
                enc = ID3TE_ISO8859_1;
                g_free(string_converted);
            }else
            {
                // Force to UTF-16 as UTF-8 isn't supported
                enc = ID3TE_UTF16;
            }
        }
        else
        {
            gint id3v2_charset;
            const gchar *charset;

            id3v2_charset = g_settings_get_enum (MainSettings,
                                                 "id3v2-no-unicode-charset");
            charset = et_charset_get_name_from_index (id3v2_charset);

            /* Other encoding selected. Encoding set by user to ???. */
            if (strcmp (charset, "ISO-8859-1") == 0)
            {
                enc = ID3TE_ISO8859_1;
            }
            else if (strcmp (charset, "UTF-16BE") == 0
                     || strcmp (charset, "UTF-16LE") == 0)
            {
                enc = ID3TE_UTF16;
            }
            else if (strcmp (charset, "UTF-8") == 0)
            {
                enc = ID3TE_UTF8;
            }
            else if (id3_field->IsEncodable())
            {
                goto override;
            }
        }

        // Some fields, as URL, aren't encodable, so there were written using ISO characters!
        if (!id3_field->IsEncodable())
        {
            enc = ID3TE_ISO8859_1;
        }

        // Action according the encoding...
        switch ( enc )
        {
            case ID3TE_ISO8859_1:
                // Write into ISO-8859-1
                //string_converted = convert_string(string,"UTF-8","ISO-8859-1",TRUE);
                string_converted = Id3tag_Rules_For_ISO_Fields(string,"UTF-8","ISO-8859-1");
                id3_field->SetEncoding(ID3TE_ISO8859_1); // Not necessary for ISO-8859-1, but better to precise if field has another encoding...
                id3_field->Set(string_converted);
                g_free(string_converted);

                id3_field_encoding = id3_frame.GetField(ID3FN_TEXTENC);
                if (id3_field_encoding)
                    id3_field_encoding->Set(ID3TE_ISO8859_1);

                return ID3TE_ISO8859_1;
                break;

            /*** Commented as it doesn't work with id3lib 3.8.3 :
             ***  - it writes a strange UTF-8 string (2 bytes per character. Second char is FF) with a BOM
             ***  - it set the frame encoded to UTF-8 : "$03" which is OK
            case ID3TE_UTF8: // Shouldn't work with id3lib 3.8.3 (supports only ID3v2.3, not ID3v2.4)
                ID3Field_SetEncoding(id3_field,ID3TE_UTF8);
                ID3Field_SetASCII(id3_field,string);

                id3_field_encoding = ID3Frame_GetField(id3_frame,ID3FN_TEXTENC);
                if (id3_field_encoding)
                    ID3Field_SetINT(id3_field_encoding,ID3TE_UTF8);

                return ID3TE_UTF8;
                break;
             ***/

            case ID3TE_UTF16:
            //case ID3TE_UTF16BE:

                /* See known problem on the top : [ 1016290 ] Unicode16 writing bug */
                // Write into UTF-16
                string_converted = convert_string_1(string, strlen(string), "UTF-8",
                                                    "UTF-16BE", FALSE);

                // id3lib (3.8.3 at least) always takes big-endian input for Unicode
                // fields, even if the field is set little-endian.
                id3_field->SetEncoding(ID3TE_UTF16);
                id3_field->Set((const unicode_t*)string_converted);
                g_free(string_converted);

                id3_field_encoding = id3_frame.GetField(ID3FN_TEXTENC);
                if (id3_field_encoding)
                    id3_field_encoding->Set(ID3TE_UTF16);

                return ID3TE_UTF16;
                break;

override:
            case ID3TE_UTF16BE:
            case ID3TE_UTF8:
            case ID3TE_NUMENCODINGS:
            case ID3TE_NONE:
            default:
            {
                //string_converted = convert_string(string,"UTF-8",charset,TRUE);
                gint id3v2_charset;
                const gchar *charset;

                id3v2_charset = g_settings_get_enum (MainSettings,
                                                     "id3v2-no-unicode-charset");
                charset = et_charset_get_name_from_index (id3v2_charset);

                string_converted = Id3tag_Rules_For_ISO_Fields (string,
                                                                "UTF-8",
                                                                charset);
                id3_field->SetEncoding(ID3TE_ISO8859_1);
                id3_field->Set(string_converted);
                g_free(string_converted);

                id3_field_encoding = id3_frame.GetField(ID3FN_TEXTENC);
                if (id3_field_encoding)
                    id3_field_encoding->Set(ID3TE_ISO8859_1);

                return ID3TE_NONE;
                break;
            }
        }
    }

    return ID3TE_NONE;
}


/*
 * By default id3lib converts id3tag to ISO-8859-1 (single byte character set)
 * Note : converting UTF-16 string (two bytes character set) to ISO-8859-1
 *        remove only the second byte => a strange string appears...
 */
static void Id3tag_Prepare_ID3v1(ID3_Tag& id3_tag)
{
    ID3_Frame *frame;
    ID3_Field *id3_field_encoding;
    ID3_Field *id3_field_text;

    gchar *string1, *string_converted;

    unique_ptr<ID3_Tag::Iterator> id3_tag_iterator(id3_tag.CreateIterator());
    while ( NULL != (frame = id3_tag_iterator->GetNext()) )
    {
        ID3_TextEnc enc = ID3TE_ISO8859_1;
        ID3_FrameID frameid = frame->GetID();

        if (frameid != ID3FID_TITLE
        &&  frameid != ID3FID_LEADARTIST
        &&  frameid != ID3FID_BAND
        &&  frameid != ID3FID_ALBUM
        &&  frameid != ID3FID_YEAR
        &&  frameid != ID3FID_TRACKNUM
        &&  frameid != ID3FID_CONTENTTYPE
        &&  frameid != ID3FID_COMMENT)
            continue;

        id3_field_encoding = frame->GetField(ID3FN_TEXTENC);
        if (id3_field_encoding != NULL)
            enc = (ID3_TextEnc)id3_field_encoding->Get();
        id3_field_text = frame->GetField(ID3FN_TEXT);

        /* The frames in ID3TE_ISO8859_1 are already converted to the selected
         * single-byte character set if used. So we treat only Unicode frames */
        if ( (id3_field_text != NULL)
        &&   (enc != ID3TE_ISO8859_1) )
        {
            gint id3v1_charset;
            const gchar *charset;

            /* Read UTF-16 frame. */
            // "convert_string_1" as we need to pass length for UTF-16
            string1 = convert_string_1((gchar*)id3_field_text->GetRawUnicodeText(), id3_field_text->Size(), "UTF-16BE", "UTF-8", FALSE);

            id3v1_charset = g_settings_get_enum (MainSettings,
                                                 "id3v1-charset");
            charset = et_charset_get_name_from_index (id3v1_charset);

            string_converted = Id3tag_Rules_For_ISO_Fields (string1,
                                                            "UTF-8",
                                                            charset);

            if (string_converted)
            {
                id3_field_text->SetEncoding(ID3TE_ISO8859_1); // Not necessary for ISO-8859-1
                id3_field_text->Set(string_converted);
                id3_field_encoding->Set(ID3TE_ISO8859_1);
                g_free(string_converted);
            }
            g_free(string1);
        }
    }
}

/*
 * This function must be used for tag fields containing ISO data.
 * We use specials functionalities of iconv : //TRANSLIT and //IGNORE (the last
 * one doesn't work on my system) to force conversion to the target encoding.
 */
gchar *
Id3tag_Rules_For_ISO_Fields (const gchar *string,
                             const gchar *from_codeset,
                             const gchar *to_codeset)
{
    gchar *string_converted = NULL;
    EtTagEncoding iconv_option;

    g_return_val_if_fail (string != NULL && from_codeset != NULL
                          && to_codeset != NULL, NULL);

    iconv_option = (EtTagEncoding)g_settings_get_enum (MainSettings, "id3v1-encoding-option");

    switch (iconv_option)
    {
        default:
        case ET_TAG_ENCODING_NONE:
            string_converted = convert_string (string, from_codeset,
                                               to_codeset, TRUE);
            break;
        case ET_TAG_ENCODING_TRANSLITERATE:
        {
            /* iconv_open (3):
             * When the string "//TRANSLIT" is appended to tocode,
             * transliteration is activated. This means that when a character
             * cannot be represented in the target character set, it can be
             * approximated through one or several similarly looking
             * characters.
             */
            gchar *to_enc = g_strconcat (to_codeset, "//TRANSLIT", NULL);
            string_converted = convert_string (string, from_codeset, to_enc,
                                               TRUE);
            g_free (to_enc);
            break;
        }
        case ET_TAG_ENCODING_IGNORE:
        {
            /* iconv_open (3):
             * When the string "//IGNORE" is appended to tocode, characters
             * that cannot be represented in the target character set will be
             * silently discarded.
             */
            gchar *to_enc = g_strconcat (to_codeset, "//IGNORE", NULL);
            string_converted = convert_string (string, from_codeset, to_enc,
                                               TRUE);
            g_free (to_enc);
            break;
        }
    }

    return string_converted;
}

/*
 * Some files which contains only zeroes create an infinite loop in id3lib...
 * To generate a file with zeroes : dd if=/dev/zero bs=1M count=6 of=test-corrupted-mp3-zero-contend.mp3
 */
gboolean
et_id3tag_check_if_file_is_valid (GFile *file, GError **error)
{
    unsigned char tmp[256];
    unsigned char tmp0[256];
    gssize bytes_read;
    gboolean valid = FALSE;
    GFileInputStream *file_istream;

    g_return_val_if_fail (file != NULL, FALSE);

    file_istream = g_file_read (file, NULL, error);

    if (!file_istream)
    {
        g_assert (error == NULL || *error != NULL);
        return valid;
    }

    memset (&tmp0, 0, 256);

    /* Keep reading until EOF. */
    while ((bytes_read = g_input_stream_read (G_INPUT_STREAM (file_istream),
                                              tmp, 256, NULL, error)) != 0)
    {
        if (bytes_read == -1)
        {
            /* Error in reading file. */
            g_assert (error == NULL || *error != NULL);
            g_object_unref (file_istream);
            return valid;
        }

        /* Break out of the loop if there is a non-zero byte in the file. */
        if (memcmp (tmp, tmp0, bytes_read) != 0)
        {
            valid = TRUE;
            break;
        }
    }

    g_object_unref (file_istream);

    /* The error was not set by g_input_stream_read(), so the file must be
     * empty. */
    if (!valid)
    {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s",
                     _("Input truncated or empty"));
    }

    return valid;
}


/*
 * Function to detect if id3lib isn't bugged when writting to Unicode
 * Returns TRUE if bugged, else FALSE
 */
static gboolean
id3tag_check_if_id3lib_is_buggy (GError **error)
{
    GFile *file;
    GFileIOStream *iostream = NULL;
    GOutputStream *ostream = NULL;
    guchar tmp[16] = {0xFF, 0xFB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    gchar *path;
    gchar *result = NULL;
    ID3_Frame *id3_frame;
    gboolean use_unicode;
    gsize bytes_written;
    const gchar test_str[] = "\xe5\x92\xbb";

    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    /* Create a temporary file. */
    file = g_file_new_tmp ("easytagXXXXXX.mp3", &iostream, error);

    if (!file)
    {
        /* TODO: Investigate whether error can be unset in this case. */
        if (!error)
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, "%s",
                         _("Error while creating temporary file"));
        }

        return FALSE;
    }

    /* Set data in the file. */
    ostream = g_io_stream_get_output_stream (G_IO_STREAM (iostream));

    if (!g_output_stream_write_all (G_OUTPUT_STREAM (ostream), tmp,
                                    sizeof (tmp), &bytes_written, NULL,
                                    error))
    {
        g_debug ("Only %" G_GSIZE_FORMAT " bytes out of %" G_GSIZE_FORMAT
                 " bytes of data were written", bytes_written, sizeof (tmp));

        g_object_unref (file);
        g_object_unref (iostream);

        return FALSE;
    }

    g_output_stream_close (G_OUTPUT_STREAM (ostream), NULL, NULL);
    g_object_unref (iostream);

    /* Save state of switches as we must force to Unicode before writing.
     * FIXME! */
    g_settings_delay (MainSettings);
    use_unicode = g_settings_get_boolean (MainSettings,
                                          "id3v2-enable-unicode");
    g_settings_set_boolean (MainSettings, "id3v2-enable-unicode", TRUE);

    path = g_file_get_path (file);

  try
  { ID3_Tag id3_tag;
    ID3Tag_Link_1 (id3_tag, path);

    // Create a new 'title' field for testing
    id3_frame = new ID3_Frame(ID3FID_TITLE);
    id3_tag.AttachFrame(id3_frame);
    /* Test a string that exposes an id3lib bug when converted to UTF-16.
     * http://sourceforge.net/p/id3lib/patches/64/ */
    Id3tag_Set_Field (*id3_frame, ID3FN_TEXT, test_str);

    // Update the tag
    id3_tag.Update(ID3TT_ID3V2);
  } catch (...)
  { // The C API of ID3lib always handles exceptions with "on error resume next".
  }

    g_settings_set_boolean (MainSettings, "id3v2-enable-unicode", use_unicode);
    g_settings_revert (MainSettings);

  try
  { ID3_Tag id3_tag;
    ID3Tag_Link_1 (id3_tag, path);
    // Read the written field
    if ( (id3_frame = id3_tag.Find(ID3FID_TITLE)) )
    {
        result = Id3tag_Get_Field(*id3_frame, ID3FN_TEXT);
    }
  } catch (...)
  { // The C API of ID3lib always handles exceptions with "on error resume next".
  }

    g_free (path);
    g_file_delete (file, NULL, NULL);

    g_object_unref (file);

    /* Same string found? if yes => not buggy. */
    if (result && strcmp (result, test_str) != 0)
    {
        g_free (result);
        return TRUE;
    }

    g_free (result);
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                 "Buggy id3lib detected");
    return FALSE;
}

#endif /* ENABLE_ID3LIB */


/*
 * Write tag according the version selected by the user
 */
gboolean
id3tag_write_file_tag (const ET_File *ETFile,
                       GError **error)
{
#ifdef ENABLE_ID3LIB
    if (g_settings_get_boolean (MainSettings, "id3v2-version-4"))
    {
        return id3tag_write_file_v24tag (ETFile, error);
    }
    else
    {
        return id3tag_write_file_v23tag (ETFile, error);
    }
#else
    return id3tag_write_file_v24tag (ETFile, error);
#endif /* !ENABLE_ID3LIB */
}

static const gchar *
channel_mode_name (int mode)
{
    static const gchar * const channel_mode[] =
    {
        N_("Stereo"),
        N_("Joint stereo"),
        N_("Dual channel"),
        N_("Single channel")
    };

    if (mode < 0 || mode > 3)
    {
        return "";
    }

    return _(channel_mode[mode]);
}

/* For displaying header information in the main window. */
void
et_mpeg_header_display_file_info_to_ui (EtFileHeaderFields *fields, const ET_File *ETFile)
{
    const ET_File_Info *info = &ETFile->ETFileInfo;

    /* MPEG, Layer versions */
    fields->version_label = _("MPEG");

    fields->version = info->version == 3 ? "2.5" : to_string(info->version);
    fields->version += ", Layer ";
    fields->version += info->layer >= 1 && info->layer <= 3 ? "III" + (3-info->layer) : "?";

    /* Mode */
    fields->mode_label = _("Mode:");
    fields->mode = _(channel_mode_name (info->mode));
}

unsigned id3tag_unsupported_fields(const ET_File* file)
{
	unsigned hide = ET_COLUMN_VERSION | ET_COLUMN_DESCRIPTION;

	if (!g_settings_get_boolean (MainSettings, "id3v2-enabled"))
		hide |= ET_COLUMN_VERSION | ET_COLUMN_SUBTITLE | ET_COLUMN_ALBUM_ARTIST
			| ET_COLUMN_DISC_SUBTITLE | ET_COLUMN_TRACK_NUMBER | ET_COLUMN_DISC_NUMBER
			| ET_COLUMN_RELEASE_YEAR | ET_COLUMN_COMPOSER | ET_COLUMN_ORIG_ARTIST
			| ET_COLUMN_ORIG_YEAR | ET_COLUMN_COPYRIGHT | ET_COLUMN_URL
			| ET_COLUMN_ENCODED_BY | ET_COLUMN_IMAGE | ET_COLUMN_DESCRIPTION;
	else if (!g_settings_get_boolean (MainSettings, "id3v2-version-4"))
		hide |= ET_COLUMN_RELEASE_YEAR;

	return hide;
}

#endif /* ENABLE_MP3 */


// Placed out #ifdef ENABLE_MP3 as not dependant of id3lib, and used in CDDB
/*
 * Returns the corresponding genre value of the input string (for ID3v1.x),
 * else returns 0xFF (unknown genre, but not invalid).
 */
guchar
Id3tag_String_To_Genre (const gchar *genre)
{
    guint i;

    if (genre != NULL)
    {
        for (i=0; i<=GENRE_MAX; i++)
            if (strcasecmp(genre,id3_genres[i])==0)
                return (guchar)i;
    }
    return (guchar)0xFF;
}


/*
 * Returns the name of a genre code if found
 * Three states for genre code :
 *    - defined (0 to GENRE_MAX)
 *    - undefined/unknown (GENRE_MAX+1 to ID3_INVALID_GENRE-1)
 *    - invalid (>ID3_INVALID_GENRE)
 */
const gchar *
Id3tag_Genre_To_String (unsigned char genre_code)
{
    if (genre_code>=ID3_INVALID_GENRE)    /* empty */
        return "";
    else if (genre_code>GENRE_MAX)        /* unknown tag */
        return "Unknown";
    else                                  /* known tag */
        return id3_genres[genre_code];
}
