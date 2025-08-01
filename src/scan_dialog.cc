/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2022-2024  Marcel Müller <github@maazl.de>
 * Copyright (C) 2014-2015  David King <amigadave@amigadave.com>
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

#include "config.h"

#include "scan_dialog.h"

#include <string.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "application_window.h"
#include "easytag.h"
#include "enums.h"
#include "preferences_dialog.h"
#include "scan.h"
#include "setting.h"
#include "id3_tag.h"
#include "browser.h"
#include "log.h"
#include "misc.h"
#include "crc32.h"
#include "charset.h"
#include "mask.h"
#include "file_name.h"
#include "file_tag.h"
#include "file_list.h"

#include <algorithm>
#include <string>
using namespace std;

typedef struct
{
    GtkListStore *rename_masks_model;
    GtkListStore *fill_masks_model;

    GtkWidget *mask_entry;
    GtkWidget *mask_view;

    GtkWidget *notebook;
    GtkWidget *fill_grid;
    GtkWidget *rename_grid;

    GtkWidget *fill_combo;
    GtkWidget *rename_combo;

    GtkWidget *legend_grid;
    GtkWidget *editor_grid;

    GtkWidget *legend_toggle;
    GtkWidget *mask_editor_toggle;

    GtkWidget *process_filename_check;
    GtkWidget *process_title_check;
    GtkWidget *process_artist_check;
    GtkWidget *process_album_artist_check;
    GtkWidget *process_album_check;
    GtkWidget *process_genre_check;
    GtkWidget *process_comment_check;
    GtkWidget *process_composer_check;
    GtkWidget *process_orig_artist_check;
    GtkWidget *process_copyright_check;
    GtkWidget *process_url_check;
    GtkWidget *process_encoded_by_check;

    GtkWidget *convert_space_radio;
    GtkWidget *convert_underscores_radio;
    GtkWidget *convert_string_radio;
    GtkWidget *convert_none_radio;
    GtkWidget *convert_to_entry;
    GtkWidget *convert_from_entry;
    GtkWidget *convert_to_label;

    GtkWidget *capitalize_all_radio;
    GtkWidget *capitalize_lower_radio;
    GtkWidget *capitalize_first_radio;
    GtkWidget *capitalize_first_style_radio;
    GtkWidget *capitalize_none_radio;
    GtkWidget *capitalize_roman_check;

    GtkWidget *spaces_remove_radio;
    GtkWidget *spaces_insert_radio;
    GtkWidget *spaces_insert_one_radio;

    GtkWidget *fill_preview_label;
    GtkWidget *rename_preview_label;
} EtScanDialogPrivate;

// learn correct return type for et_scan_dialog_get_instance_private
#define et_scan_dialog_get_instance_private et_scan_dialog_get_instance_private_
G_DEFINE_TYPE_WITH_PRIVATE (EtScanDialog, et_scan_dialog, GTK_TYPE_DIALOG)
#undef et_scan_dialog_get_instance_private
#define et_scan_dialog_get_instance_private(x) (EtScanDialogPrivate*)et_scan_dialog_get_instance_private_(x)

/* Some predefined masks -- IMPORTANT: Null-terminate me! */
static const gchar *Scan_Masks [] =
{
    "%a - %T" G_DIR_SEPARATOR_S "%n - %t",
    "%a_-_%T" G_DIR_SEPARATOR_S "%n_-_%t",
    "%a - %T (%y)" G_DIR_SEPARATOR_S "%n - %a - %t",
    "%a_-_%T_(%y)" G_DIR_SEPARATOR_S "%n_-_%a_-_%t",
    "%a - %T (%y) - %g" G_DIR_SEPARATOR_S "%n - %a - %t",
    "%a_-_%T_(%y)_-_%g" G_DIR_SEPARATOR_S "%n_-_%a_-_%t",
    "%a - %T" G_DIR_SEPARATOR_S "%n. %t",
    "%a_-_%T" G_DIR_SEPARATOR_S "%n._%t",
    "%a-%T" G_DIR_SEPARATOR_S "%n-%t",
    "%T" G_DIR_SEPARATOR_S "%n. %a - %t",
    "%T" G_DIR_SEPARATOR_S "%n._%a_-_%t",
    "%T" G_DIR_SEPARATOR_S "%n - %a - %t",
    "%T" G_DIR_SEPARATOR_S "%n_-_%a_-_%t",
    "%T" G_DIR_SEPARATOR_S "%n-%a-%t",
    "%a-%T" G_DIR_SEPARATOR_S "%n-%t",
    "%a" G_DIR_SEPARATOR_S "%T" G_DIR_SEPARATOR_S "%n. %t",
    "%g" G_DIR_SEPARATOR_S "%a" G_DIR_SEPARATOR_S "%T" G_DIR_SEPARATOR_S "%t",
    "%a_-_%T-%n-%t-%y",
    "%a - %T" G_DIR_SEPARATOR_S "%n. %t(%c)",
    "%t",
    "Track%n",
    "Track%i %n",
    NULL
};

static const gchar *Rename_File_Masks [] =
{
    "{%n - |}%a - %t{ (%v)|}",
    "{%n. |}%a - %t{ (%v)|}",
    "{%A|%a} - %T" G_DIR_SEPARATOR_S "%n - %t{ (%v)|}",
    "{%A|%a} - %T ({%Y|%y}){ - %g|}" G_DIR_SEPARATOR_S "{%d.|}%n - %t{ (%v)|}",
    "{%A|%a} - %T ({%Y|%y}){ - %g|}" G_DIR_SEPARATOR_S "%n - %t{ (%v)|}",
    "{%A|%a}" G_DIR_SEPARATOR_S "%T ({%Y|%y})" G_DIR_SEPARATOR_S "{%n - %t{ (%v)|}|Track %n}",
    "%n - %t{ (%v)|}",
    "%n. %t{ (%v)|}",
    "%n - %a - %T - %t{ (%v)|}",
    "%a - %T - %t{ (%v)}",
    "%a - %T - %n - %t{ (%v)|}",
    "%a - %t{ (%v)|}",
    "Track %n",
    NULL
};

enum {
    MASK_EDITOR_TEXT,
    MASK_EDITOR_COUNT
};

/*
 * Used into Scan Tag Scanner
 */
typedef struct _Scan_Mask_Item Scan_Mask_Item;
struct _Scan_Mask_Item
{
    gchar  code;   // The code of the mask without % (ex: %a => a)
    gchar *string; // The string found by the scanner for the code defined the line above
};


/**************
 * Prototypes *
 **************/
static void Scan_Option_Button (void);

static GList *Scan_Generate_New_Tag_From_Mask (ET_File *ETFile, string&& mask);
static void Scan_Free_File_Fill_Tag_List (GList *list);

static void et_scan_on_response (GtkDialog *dialog, gint response_id,
                                 gpointer user_data);

static void et_scan_dialog_set_file_tag_for_mask_item
(File_Tag *file_tag, const Scan_Mask_Item *item, gboolean overwrite)
{	if (item->code == 'i')
		return; // ignore field
	xStringD0 File_Tag::*field = et_mask_field(item->code);
	if (!field)
		Log_Print(LOG_ERROR, "Scanner: Invalid code '%%%c' found!", item->code);
	else if (overwrite || et_str_empty(file_tag->*field))
		(file_tag->*field).assignNFC(item->string);
}

/*
 * Uses the filename and path to fill tag information
 * Note: mask and source are read from the right to the left
 */
static void
Scan_Tag_With_Mask (EtScanDialog *self, ET_File *ETFile)
{
    EtScanDialogPrivate *priv;
    GList *fill_tag_list = NULL;
    GList *l;
    File_Tag *FileTag;

    g_return_if_fail (ETFile != NULL);

    priv = et_scan_dialog_get_instance_private (self);

    string mask = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(priv->fill_combo))));
    if (mask.empty())
        return;

    // Create a new File_Tag item
    FileTag = new File_Tag(*ETFile->FileTagNew());

    // Process this mask with file
    fill_tag_list = Scan_Generate_New_Tag_From_Mask(ETFile, move(mask));
    gboolean overwrite = g_settings_get_boolean(MainSettings, "fill-overwrite-tag-fields");

    for (l = fill_tag_list; l != NULL; l = g_list_next (l))
    {
        const Scan_Mask_Item *mask_item = (const Scan_Mask_Item*)l->data;
        /* We display the text affected to the code. */
        et_scan_dialog_set_file_tag_for_mask_item (FileTag, mask_item, overwrite);
    }

    Scan_Free_File_Fill_Tag_List(fill_tag_list);

    /* Set the default text to comment. */
    if (g_settings_get_boolean (MainSettings, "fill-set-default-comment")
        && (g_settings_get_boolean (MainSettings, "fill-overwrite-tag-fields")
            || et_str_empty (FileTag->comment)))
    {
        gchar *default_comment = g_settings_get_string (MainSettings,
                                                        "fill-default-comment");
        FileTag->comment.assignNFC(default_comment);
        g_free (default_comment);
    }

#ifdef ENABLE_MP3
    /* Set CRC-32 value as default comment (for files with ID3 tag only). */
    if (g_settings_get_boolean (MainSettings, "fill-crc32-comment")
        && (g_settings_get_boolean (MainSettings, "fill-overwrite-tag-fields")
            || et_str_empty (FileTag->comment)))
    {
        GFile *file;
        GError *error = NULL;
        guint32 crc32_value;

        if (g_ascii_strcasecmp(ETFile->ETFileDescription->Extension, ".mp3"))
        {
            file = g_file_new_for_path (ETFile->FilePath);

            if (crc32_file_with_ID3_tag (file, &crc32_value, &error))
            {
                FileTag->comment = strprintf("%.8" G_GUINT32_FORMAT, crc32_value).c_str();
            }
            else
            {
                Log_Print (LOG_ERROR,
                           _("Cannot calculate CRC value of file ‘%s’"),
                           error->message);
                g_error_free (error);
            }

            g_object_unref (file);
        }
    }
#endif

    // Save changes of the 'File_Tag' item
    ETFile->apply_changes(nullptr, FileTag);

    et_application_window_status_bar_message(MainWindow, _("Tag successfully scanned"), TRUE);
    Log_Print (LOG_OK, _("Tag successfully scanned ‘%s’"), ETFile->FileNameNew()->file().get());
}

static GList *
Scan_Generate_New_Tag_From_Mask (ET_File *ETFile, string&& mask)
{
    GList *fill_tag_list = NULL;
    gchar *tmp;
    gchar *buf;
    gchar *separator;
    gchar *string;
    gsize len, loop=0;
    gchar **mask_splitted;
    gchar **file_splitted;
    guint mask_splitted_number;
    guint file_splitted_number;
    guint mask_splitted_index;
    guint file_splitted_index;
    Scan_Mask_Item *mask_item;
    EtConvertSpaces convert_mode;

    g_return_val_if_fail (ETFile != NULL && !mask.empty(), NULL);

    std::string filename_utf8(ETFile->FileNameNew()->full_name());
    if (filename_utf8.empty()) return NULL;

    // Remove extension of file (if found)
    const ET_File_Description* desc = ET_File_Description::Get(filename_utf8.c_str());
    if (desc->IsSupported())
        filename_utf8[filename_utf8.length() - strlen(desc->Extension)] = 0; //strrchr(source,'.') = 0;
    else
        Log_Print(LOG_ERROR, _("The extension ‘%s’ was not found in filename ‘%s’"),
            ET_Get_File_Extension(filename_utf8.c_str()), ETFile->FileNameNew()->file().get());

    /* Replace characters into mask and filename before parsing. */
    convert_mode = (EtConvertSpaces)g_settings_get_enum (MainSettings, "fill-convert-spaces");

    switch (convert_mode)
    {
        case ET_CONVERT_SPACES_SPACES:
            Scan_Convert_Underscore_Into_Space (mask);
            Scan_Convert_Underscore_Into_Space (filename_utf8);
            Scan_Convert_P20_Into_Space (mask);
            Scan_Convert_P20_Into_Space (filename_utf8);
            break;
        case ET_CONVERT_SPACES_UNDERSCORES:
            Scan_Convert_Space_Into_Underscore (mask);
            Scan_Convert_Space_Into_Underscore (filename_utf8);
            break;
        case ET_CONVERT_SPACES_NO_CHANGE:
            break;
        /* FIXME: Check if this is intentional. */
        case ET_CONVERT_SPACES_REMOVE:
        default:
            g_assert_not_reached ();
    }

    // Split the Scanner mask
    mask_splitted = g_strsplit(mask.c_str(), G_DIR_SEPARATOR_S, 0);
    // Get number of arguments into 'mask_splitted'
    for (mask_splitted_number = 0; mask_splitted[mask_splitted_number]; mask_splitted_number++);

    // Split the File Path
    file_splitted = g_strsplit(filename_utf8.c_str(), G_DIR_SEPARATOR_S, 0);
    // Get number of arguments into 'file_splitted'
    for (file_splitted_number = 0; file_splitted[file_splitted_number]; file_splitted_number++);

    // Set the starting position for each tab
    if (mask_splitted_number <= file_splitted_number)
    {
        mask_splitted_index = 0;
        file_splitted_index = file_splitted_number - mask_splitted_number;
    }else
    {
        mask_splitted_index = mask_splitted_number - file_splitted_number;
        file_splitted_index = 0;
    }

    loop = 0;
    while ( mask_splitted[mask_splitted_index]!= NULL && file_splitted[file_splitted_index]!=NULL )
    {
        gchar *mask_seq = mask_splitted[mask_splitted_index];
        gchar *file_seq = file_splitted[file_splitted_index];
        gchar *file_seq_utf8 = g_filename_display_name (file_seq);

        //g_print(">%d> seq '%s' '%s'\n",loop,mask_seq,file_seq);
        while (!et_str_empty (mask_seq))
        {

            /*
             * Determine (first) code and destination
             */
            if ( (tmp=strchr(mask_seq,'%')) == NULL || strlen(tmp) < 2 )
            {
                break;
            }

            /*
             * Allocate a new iten for the fill_tag_list
             */
            mask_item = g_slice_new0 (Scan_Mask_Item);

            // Get the code (used to determine the corresponding target entry)
            mask_item->code = tmp[1];

            /*
             * Delete text before the code
             */
            if ( (len = strlen(mask_seq) - strlen(tmp)) > 0 )
            {
                // Get this text in 'mask_seq'
                buf = g_strndup(mask_seq,len);
                // We remove it in 'mask_seq'
                mask_seq = mask_seq + len;
                // Find the same text at the begining of 'file_seq' ?
                if ( (strstr(file_seq,buf)) == file_seq )
                {
                    file_seq = file_seq + len; // We remove it
                }else
                {
                    Log_Print (LOG_ERROR,
                               _("Cannot find separator ‘%s’ within ‘%s’"),
                               buf, file_seq_utf8);
                }
                g_free(buf);
            }

            // Remove the current code into 'mask_seq'
            mask_seq = mask_seq + 2;

            /*
             * Determine separator between two code or trailing text (after code)
             */
            if (!et_str_empty (mask_seq))
            {
                if ( (tmp=strchr(mask_seq,'%')) == NULL || strlen(tmp) < 2 )
                {
                    // No more code found
                    len = strlen(mask_seq);
                }else
                {
                    len = strlen(mask_seq) - strlen(tmp);
                }
                separator = g_strndup(mask_seq,len);

                // Remove the current separator in 'mask_seq'
                mask_seq = mask_seq + len;

                // Try to find the separator in 'file_seq'
                if ( (tmp=strstr(file_seq,separator)) == NULL )
                {
                    Log_Print (LOG_ERROR,
                               _("Cannot find separator ‘%s’ within ‘%s’"),
                               separator, file_seq_utf8);
                    separator[0] = 0; // Needed to avoid error when calculting 'len' below
                }

                // Get the string affected to the code (or the corresponding entry field)
                len = strlen(file_seq) - (tmp!=NULL?strlen(tmp):0);
                string = g_strndup(file_seq,len);

                // Remove the current separator in 'file_seq'
                file_seq = file_seq + strlen(string) + strlen(separator);
                g_free(separator);

                // We get the text affected to the code
                mask_item->string = string;
            }else
            {
                // We display the remaining text, affected to the code (no more data in 'mask_seq')
                mask_item->string = g_strdup(file_seq);
            }

            // Add the filled mask_iten to the list
            fill_tag_list = g_list_append(fill_tag_list,mask_item);
        }

        g_free(file_seq_utf8);

        // Next sequences
        mask_splitted_index++;
        file_splitted_index++;
        loop++;
    }

    g_strfreev(mask_splitted);
    g_strfreev(file_splitted);

    // The 'fill_tag_list' must be freed after use
    return fill_tag_list;
}

static void
Scan_Fill_Tag_Generate_Preview (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    gchar *preview_text = NULL;
    GList *fill_tag_list = NULL;
    GList *l;

    priv = et_scan_dialog_get_instance_private (self);

    ET_File* etfile = MainWindow->get_displayed_file();
    if (!etfile
        || gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)) != ET_SCAN_MODE_FILL_TAG)
        return;

    string mask = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(priv->fill_combo))));
    if (mask.empty())
        return;

    preview_text = g_strdup("");
    fill_tag_list = Scan_Generate_New_Tag_From_Mask(etfile, move(mask));
    for (l = fill_tag_list; l != NULL; l = g_list_next (l))
    {
        Scan_Mask_Item *mask_item = (Scan_Mask_Item*)l->data;
        gchar *tmp_code   = g_strdup_printf("%c",mask_item->code);
        gchar *tmp_string = g_markup_printf_escaped("%s",mask_item->string); // To avoid problem with strings containing characters like '&'
        gchar *tmp_preview_text = preview_text;

        preview_text = g_strconcat(tmp_preview_text,"<b>","%",tmp_code," = ",
                                   "</b>","<i>",tmp_string,"</i>",NULL);
        g_free(tmp_code);
        g_free(tmp_string);
        g_free(tmp_preview_text);

        tmp_preview_text = preview_text;
        preview_text = g_strconcat(tmp_preview_text,"  ||  ",NULL);
        g_free(tmp_preview_text);
    }

    Scan_Free_File_Fill_Tag_List(fill_tag_list);

    if (GTK_IS_LABEL (priv->fill_preview_label))
    {
        if (preview_text)
        {
            //gtk_label_set_text(GTK_LABEL(priv->fill_preview_label),preview_text);
            gtk_label_set_markup (GTK_LABEL (priv->fill_preview_label),
                                  preview_text);
        } else
        {
            gtk_label_set_text (GTK_LABEL (priv->fill_preview_label), "");
        }

        /* Force the window to be redrawn. */
        gtk_widget_queue_resize (GTK_WIDGET (self));
    }

    g_free(preview_text);
}

static void
Scan_Rename_File_Generate_Preview (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    gchar *mask = NULL;

    priv = et_scan_dialog_get_instance_private (self);

    ET_File* etfile = MainWindow->get_displayed_file();
    if (!etfile || !priv->rename_combo || !priv->rename_preview_label)
        return;

    if (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)) != ET_SCAN_MODE_RENAME_FILE)
        return;

    mask = g_strdup (gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->rename_combo)))));
    if (!mask)
        return;

    string preview_text = et_evaluate_mask(etfile, mask, FALSE);

    if (GTK_IS_LABEL (priv->rename_preview_label))
    {
        if (!preview_text.empty())
        {
            //gtk_label_set_text(GTK_LABEL(priv->rename_preview_label),preview_text);
            gchar *tmp_string = g_markup_printf_escaped("%s",preview_text.c_str()); // To avoid problem with strings containing characters like '&'
            gchar *str = g_strdup_printf("<i>%s</i>",tmp_string);
            gtk_label_set_markup (GTK_LABEL (priv->rename_preview_label), str);
            g_free(tmp_string);
            g_free(str);
        } else
        {
            gtk_label_set_text (GTK_LABEL (priv->rename_preview_label), "");
        }

        /* Force the window to be redrawn. */
        gtk_widget_queue_resize (GTK_WIDGET (self));
    }

    g_free(mask);
}


void
et_scan_dialog_update_previews (EtScanDialog *self)
{
    g_return_if_fail (ET_SCAN_DIALOG (self));

    Scan_Fill_Tag_Generate_Preview (self);
    Scan_Rename_File_Generate_Preview (self);
}

static void
Scan_Free_File_Fill_Tag_List (GList *list)
{
    GList *l;

    list = g_list_first (list);

    for (l = list; l != NULL; l = g_list_next (l))
    {
        if (l->data)
        {
            g_free (((Scan_Mask_Item *)l->data)->string);
            g_slice_free (Scan_Mask_Item, l->data);
        }
    }

    g_list_free (list);
}



/**************************
 * Scanner To Rename File *
 **************************/
/*
 * Uses tag information (displayed into tag entries) to rename file
 * Note: mask and source are read from the right to the left.
 * Note1: a mask code may be used severals times...
 */
static void
Scan_Rename_File_With_Mask (EtScanDialog *self, ET_File *ETFile)
{
    g_return_if_fail (ETFile != NULL);
    EtScanDialogPrivate *priv = et_scan_dialog_get_instance_private (self);

    const gchar* mask = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->rename_combo))));
    if (!mask)
    	return;

    // Note : if the first character is '/', we have a path with the filename,
    // else we have only the filename. The both are in UTF-8.
    string filename_generated_utf8 = et_evaluate_mask(ETFile, mask, FALSE);
    if (filename_generated_utf8.empty())
        return;

    // Convert filename to file-system encoding
    gString filename_generated(filename_from_display(filename_generated_utf8.c_str()));
    if (!filename_generated)
    {
        GtkWidget *msgdialog;
        msgdialog = gtk_message_dialog_new (GTK_WINDOW (self),
                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                             GTK_MESSAGE_ERROR,
                             GTK_BUTTONS_CLOSE,
                             _("Could not convert filename ‘%s’ into system filename encoding"),
                             filename_generated_utf8.c_str());
        gtk_window_set_title(GTK_WINDOW(msgdialog), _("Filename translation"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
        return;
    }

    /* Build the filename with the full path or relative to old path */
    File_Name *FileName = new File_Name(ETFile->FileNameNew()->generate_name(filename_generated_utf8.c_str(), false));
    // Save changes of the 'File_Name' item
    ETFile->apply_changes(FileName, nullptr);

    et_application_window_status_bar_message(MainWindow, _("New filename successfully scanned"), TRUE);

    Log_Print (LOG_OK, _("New filename successfully scanned ‘%s’"), ETFile->FileNameNew()->file().get());

    return;
}

/*
 * Adds the current path of the file to the mask on the "Rename File Scanner" entry
 */
static void
Scan_Rename_File_Prefix_Path (EtScanDialog *self)
{
    ET_File* etfile = MainWindow->get_displayed_file();
    if (!etfile)
        return;

    EtScanDialogPrivate *priv = et_scan_dialog_get_instance_private (self);

    /* The current text in the combobox. */
    const gchar *combo_text = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->rename_combo))));
    if (!combo_text || g_path_is_absolute(combo_text))
        return;

    // The path to prefix
    const char* filepath = etfile->FileNameCur()->path();
    string path_tmp;
    if (!g_path_is_absolute(filepath))
    {   // Current root path
        path_tmp = gString(g_filename_display_name(et_application_window_get_current_path_name(MainWindow)));
        if (!G_IS_DIR_SEPARATOR(path_tmp.back()))
            path_tmp += G_DIR_SEPARATOR;
    }
    if (*filepath)
        (path_tmp += filepath) += G_DIR_SEPARATOR;

    // If the path already exists we don't add it again
    // Use g_utf8_collate_key instead of strncmp
    if (!path_tmp.empty()
        && strncmp(combo_text, path_tmp.c_str(), path_tmp.length()) != 0)
    {
        gint pos = 0;
        gtk_editable_insert_text (GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (priv->rename_combo))),
                                  path_tmp.c_str(), -1, &pos);
    }
}


/*
 * Replace something with something else ;)
 * Here use Regular Expression, to search and replace.
 */
static void
Scan_Convert_Character (EtScanDialog *self, string& s)
{
    EtScanDialogPrivate *priv;
    gchar *from;
    gchar *to;
    GRegex *regex;
    GError *regex_error = NULL;
    gchar *new_string;

    priv = et_scan_dialog_get_instance_private (self);

    from = gtk_editable_get_chars (GTK_EDITABLE (priv->convert_from_entry), 0,
                                 -1);
    to = gtk_editable_get_chars (GTK_EDITABLE (priv->convert_to_entry), 0, -1);

    regex = g_regex_new (from, (GRegexCompileFlags)0, (GRegexMatchFlags)0, &regex_error);
    if (regex_error != NULL)
    {
        goto handle_error;
    }

    new_string = g_regex_replace (regex, s.c_str(), -1, 0, to, (GRegexMatchFlags)0, &regex_error);
    if (regex_error != NULL)
    {
        g_free (new_string);
        g_regex_unref (regex);
        goto handle_error;
    }

    /* Success. */
    g_regex_unref (regex);
    s = new_string;
    g_free(new_string);

out:
    g_free (from);
    g_free (to);
    return;

handle_error:
    Log_Print (LOG_ERROR, _("Error while processing fields ‘%s’"),
               regex_error->message);

    g_error_free (regex_error);

    goto out;
}

static void Scan_Process_Fields_Functions (EtScanDialog *self, string& str)
{
    switch (g_settings_get_enum (MainSettings, "process-convert"))
    {
        case ET_PROCESS_FIELDS_CONVERT_SPACES:
            Scan_Convert_Underscore_Into_Space (str);
            Scan_Convert_P20_Into_Space (str);
            break;
        case ET_PROCESS_FIELDS_CONVERT_UNDERSCORES:
            Scan_Convert_Space_Into_Underscore (str);
            break;
        case ET_PROCESS_FIELDS_CONVERT_CHARACTERS:
            Scan_Convert_Character (self, str);
            break;
        case ET_PROCESS_FIELDS_CONVERT_NO_CHANGE:
            break;
        default:
            g_assert_not_reached ();
            break;
    }

    if (g_settings_get_boolean (MainSettings, "process-insert-capital-spaces"))
        Scan_Process_Fields_Insert_Space (str);

    if (g_settings_get_boolean (MainSettings,
        "process-remove-duplicate-spaces"))
        Scan_Process_Fields_Keep_One_Space (str);

    switch (g_settings_get_enum(MainSettings, "process-capitalize"))
    {
    case ET_PROCESS_CAPITALIZE_ALL_UP:
        Scan_Process_Fields_All_Uppercase (str);
        break;

    case ET_PROCESS_CAPITALIZE_ALL_DOWN:
        Scan_Process_Fields_All_Downcase (str);
        break;

    case ET_PROCESS_CAPITALIZE_FIRST_LETTER_UP:
        Scan_Process_Fields_Letter_Uppercase (str);
        break;

    case ET_PROCESS_CAPITALIZE_FIRST_WORDS_UP:
        Scan_Process_Fields_First_Letters_Uppercase (str,
            g_settings_get_boolean (MainSettings, "process-uppercase-prepositions"),
            g_settings_get_boolean (MainSettings, "process-detect-roman-numerals"));
    }

    if (g_settings_get_boolean (MainSettings, "process-remove-spaces"))
        Scan_Process_Fields_Remove_Space (str);
}

static void Scan_Process_Tag_Field(EtScanDialog *self, File_Tag* FileTag, xStringD0 File_Tag::*field)
{	if ((FileTag->*field).empty())
		return;
	string s(FileTag->*field);
	Scan_Process_Fields_Functions(self, s);
	(FileTag->*field).assignNFC(s);
}

/*****************************
 * Scanner To Process Fields *
 *****************************/
/* See also functions : Convert_P20_And_Undescore_Into_Spaces, ... in easytag.c */
static void
Scan_Process_Fields (EtScanDialog *self, ET_File *ETFile)
{
    File_Name *FileName = NULL;
    File_Tag  *FileTag  = NULL;
    guint process_fields;

    g_return_if_fail (ETFile != NULL);

    auto st_filename = ETFile->FileNameNew();
    auto st_filetag  = ETFile->FileTagNew();
    process_fields = g_settings_get_flags (MainSettings, "process-fields");

    /* Process the filename */
    if (st_filename != NULL)
    {
        if (process_fields & ET_PROCESS_FIELD_FILENAME)
        {
            // Remove the extension to set it to lower case (to avoid problem with undo)
            string s(ET_Remove_File_Extension(st_filename->file()));

            Scan_Process_Fields_Functions (self, s);

            FileName = new File_Name(st_filename->generate_name(s.c_str(), true));
        }
    }

    /* Process data of the tag */
    if (st_filetag != NULL && (process_fields & ~ET_PROCESS_FIELD_FILENAME))
    {
        FileTag = new File_Tag(*st_filetag);

        if (process_fields & ET_PROCESS_FIELD_TITLE)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::title);

        if (process_fields & ET_PROCESS_FIELD_VERSION)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::version);

        if (process_fields & ET_PROCESS_FIELD_SUBTITLE)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::subtitle);

        if (process_fields & ET_PROCESS_FIELD_ARTIST)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::artist);

        if (process_fields & ET_PROCESS_FIELD_ALBUM_ARTIST)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::album_artist);

        if (process_fields & ET_PROCESS_FIELD_ALBUM)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::album);

        if (process_fields & ET_PROCESS_FIELD_DISC_SUBTITLE)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::disc_subtitle);

        if (process_fields & ET_PROCESS_FIELD_GENRE)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::genre);

        if (process_fields & ET_PROCESS_FIELD_COMMENT)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::comment);

        if (process_fields & ET_PROCESS_FIELD_COMPOSER)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::composer);

        if (process_fields & ET_PROCESS_FIELD_ORIGINAL_ARTIST)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::orig_artist);

        if (process_fields & ET_PROCESS_FIELD_COPYRIGHT)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::copyright);

        if (process_fields & ET_PROCESS_FIELD_URL)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::url);

        if (process_fields & ET_PROCESS_FIELD_ENCODED_BY)
            Scan_Process_Tag_Field(self, FileTag, &File_Tag::encoded_by);
    }

    ETFile->apply_changes(FileName, FileTag);

}

/******************
 * Scanner Window *
 ******************/
/*
 * Function when you select an item of the option menu
 */
static void
on_scan_mode_changed (EtScanDialog *self,
                      const gchar *key,
                      GSettings *settings)
{
    EtScanDialogPrivate *priv;
    EtScanMode mode;

    priv = et_scan_dialog_get_instance_private (self);

    mode = (EtScanMode)g_settings_get_enum (settings, key);

    switch (mode)
    {
        case ET_SCAN_MODE_FILL_TAG:
            gtk_widget_show(priv->mask_editor_toggle);
            gtk_widget_show(priv->legend_toggle);
            gtk_tree_view_set_model (GTK_TREE_VIEW (priv->mask_view),
                                     GTK_TREE_MODEL (priv->fill_masks_model));
            Scan_Fill_Tag_Generate_Preview (self);
            g_signal_emit_by_name(G_OBJECT(priv->legend_toggle),"toggled");        /* To hide or show legend frame */
            g_signal_emit_by_name(G_OBJECT(priv->mask_editor_toggle),"toggled");    /* To hide or show mask editor frame */
            break;

        case ET_SCAN_MODE_RENAME_FILE:
            gtk_widget_show(priv->mask_editor_toggle);
            gtk_widget_show(priv->legend_toggle);
            gtk_tree_view_set_model (GTK_TREE_VIEW (priv->mask_view),
                                     GTK_TREE_MODEL (priv->rename_masks_model));
            Scan_Rename_File_Generate_Preview (self);
            g_signal_emit_by_name(G_OBJECT(priv->legend_toggle),"toggled");        /* To hide or show legend frame */
            g_signal_emit_by_name(G_OBJECT(priv->mask_editor_toggle),"toggled");    /* To hide or show mask editor frame */
            break;

        case ET_SCAN_MODE_PROCESS_FIELDS:
            gtk_widget_hide(priv->mask_editor_toggle);
            gtk_widget_hide(priv->legend_toggle);
            // Hide directly the frames to don't change state of the buttons!
            gtk_widget_hide (priv->legend_grid);
            gtk_widget_hide (priv->editor_grid);

            gtk_tree_view_set_model (GTK_TREE_VIEW (priv->mask_view), NULL);
            break;
        default:
            g_assert_not_reached ();
    }

    /* TODO: Either duplicate the legend and mask editor, or split the dialog.
     */
    if (mode == ET_SCAN_MODE_FILL_TAG || mode == ET_SCAN_MODE_RENAME_FILE)
    {
        GtkWidget *parent;

        parent = gtk_widget_get_parent (priv->editor_grid);

        if ((mode == ET_SCAN_MODE_RENAME_FILE && parent != priv->rename_grid)
            || (mode == ET_SCAN_MODE_FILL_TAG && parent != priv->fill_grid))
        {
            g_object_ref (priv->editor_grid);
            g_object_ref (priv->legend_grid);
            gtk_container_remove (GTK_CONTAINER (parent),
                                  priv->editor_grid);
            gtk_container_remove (GTK_CONTAINER (parent), priv->legend_grid);

            if (mode == ET_SCAN_MODE_RENAME_FILE)
            {
                gtk_container_add (GTK_CONTAINER (priv->rename_grid),
                                   priv->editor_grid);
                gtk_container_add (GTK_CONTAINER (priv->rename_grid),
                                   priv->legend_grid);
            }
            else
            {
                gtk_container_add (GTK_CONTAINER (priv->fill_grid),
                                   priv->editor_grid);
                gtk_container_add (GTK_CONTAINER (priv->fill_grid),
                                   priv->legend_grid);
            }

            g_object_unref (priv->editor_grid);
            g_object_unref (priv->legend_grid);
        }
    }
}

static void
Mask_Editor_List_Add (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    gint i = 0;
    GtkTreeModel *treemodel;

    priv = et_scan_dialog_get_instance_private (self);

    treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->mask_view));

    if (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)) != ET_SCAN_MODE_FILL_TAG)
    {
        while(Scan_Masks[i])
        {
            gtk_list_store_insert_with_values (GTK_LIST_STORE (treemodel),
                                               NULL, G_MAXINT,
                                               MASK_EDITOR_TEXT, Scan_Masks[i], -1);
            i++;
        }
    } else if (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)) == ET_SCAN_MODE_RENAME_FILE)
    {
        while(Rename_File_Masks[i])
        {
            gtk_list_store_insert_with_values (GTK_LIST_STORE (treemodel),
                                               NULL, G_MAXINT,
                                               MASK_EDITOR_TEXT, Rename_File_Masks[i], -1);
            i++;
        }
    }
}

/*
 * Clean up the currently displayed masks lists, ready for saving
 */
static void
Mask_Editor_Clean_Up_Masks_List (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    gchar *text = NULL;
    gchar *text1 = NULL;
    GtkTreeIter currentIter;
    GtkTreeIter itercopy;
    GtkTreeModel *treemodel;

    priv = et_scan_dialog_get_instance_private (self);

    treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->mask_view));

    /* Remove blank and duplicate items */
    if (gtk_tree_model_get_iter_first(treemodel, &currentIter))
    {

        while(TRUE)
        {
            gtk_tree_model_get(treemodel, &currentIter, MASK_EDITOR_TEXT, &text, -1);

            /* Check for blank entry */
            if (text && *text == '\0')
            {
                g_free(text);

                if (!gtk_list_store_remove(GTK_LIST_STORE(treemodel), &currentIter))
                    break; /* No following entries */
                else
                    continue; /* Go on to next entry, which the remove function already moved onto for us */
            }

            /* Check for duplicate entries */
            itercopy = currentIter;
            if (!gtk_tree_model_iter_next(treemodel, &itercopy))
            {
                g_free(text);
                break;
            }

            while(TRUE)
            {
                gtk_tree_model_get(treemodel, &itercopy, MASK_EDITOR_TEXT, &text1, -1);
                if (text1 && g_utf8_collate(text,text1) == 0)
                {
                    g_free(text1);

                    if (!gtk_list_store_remove(GTK_LIST_STORE(treemodel), &itercopy))
                        break; /* No following entries */
                    else
                        continue; /* Go on to next entry, which the remove function already set iter to for us */

                }
                g_free(text1);
                if (!gtk_tree_model_iter_next(treemodel, &itercopy))
                    break;
            }

            g_free(text);

            if (!gtk_tree_model_iter_next(treemodel, &currentIter))
                break;
        }
    }
}

/*
 * Save the currently displayed mask list in the mask editor
 */
static void
Mask_Editor_List_Save (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;

    priv = et_scan_dialog_get_instance_private (self);

    Mask_Editor_Clean_Up_Masks_List (self);

    if (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)) == ET_SCAN_MODE_FILL_TAG)
    {
        Save_Scan_Tag_Masks_List (priv->fill_masks_model, MASK_EDITOR_TEXT);
    }
    else if (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)) == ET_SCAN_MODE_RENAME_FILE)
    {
        Save_Rename_File_Masks_List(priv->rename_masks_model, MASK_EDITOR_TEXT);
    }
}

static void
Process_Fields_First_Letters_Check_Button_Toggled (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;

    priv = et_scan_dialog_get_instance_private (self);

    gtk_widget_set_sensitive (GTK_WIDGET (priv->capitalize_roman_check),
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->capitalize_first_style_radio)));
}

/*
 * Set sensitive state of the processing check boxes : if no one is selected => all disabled
 */
static void
on_process_fields_changed (EtScanDialog *self,
                           const gchar *key,
                           GSettings *settings)
{
    EtScanDialogPrivate *priv;

    priv = et_scan_dialog_get_instance_private (self);

    if (g_settings_get_flags (settings, key) != 0)
    {
        gtk_widget_set_sensitive (priv->convert_space_radio, TRUE);
        gtk_widget_set_sensitive (priv->convert_underscores_radio, TRUE);
        gtk_widget_set_sensitive (priv->convert_string_radio, TRUE);
        gtk_widget_set_sensitive (GTK_WIDGET (priv->convert_to_label), TRUE);
        // Activate the two entries only if the check box is activated, else keep them disabled
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->convert_string_radio)))
        {
            gtk_widget_set_sensitive (priv->convert_to_entry, TRUE);
            gtk_widget_set_sensitive (priv->convert_from_entry, TRUE);
        }
        gtk_widget_set_sensitive (priv->capitalize_all_radio, TRUE);
        gtk_widget_set_sensitive (priv->capitalize_lower_radio, TRUE);
        gtk_widget_set_sensitive (priv->capitalize_first_radio, TRUE);
        gtk_widget_set_sensitive (priv->capitalize_first_style_radio, TRUE);
        Process_Fields_First_Letters_Check_Button_Toggled (self);
        gtk_widget_set_sensitive (priv->spaces_remove_radio, TRUE);
        gtk_widget_set_sensitive (priv->spaces_insert_radio, TRUE);
        gtk_widget_set_sensitive (priv->spaces_insert_one_radio, TRUE);
    }else
    {
        gtk_widget_set_sensitive (priv->convert_space_radio, FALSE);
        gtk_widget_set_sensitive (priv->convert_underscores_radio, FALSE);
        gtk_widget_set_sensitive (priv->convert_string_radio, FALSE);
        gtk_widget_set_sensitive (priv->convert_to_entry, FALSE);
        gtk_widget_set_sensitive (priv->convert_to_label, FALSE);
        gtk_widget_set_sensitive (priv->convert_from_entry, FALSE);
        gtk_widget_set_sensitive (priv->capitalize_all_radio, FALSE);
        gtk_widget_set_sensitive (priv->capitalize_lower_radio, FALSE);
        gtk_widget_set_sensitive (priv->capitalize_first_radio, FALSE);
        gtk_widget_set_sensitive (priv->capitalize_first_style_radio, FALSE);
        gtk_widget_set_sensitive (priv->capitalize_roman_check,  FALSE);
        gtk_widget_set_sensitive (priv->spaces_remove_radio, FALSE);
        gtk_widget_set_sensitive (priv->spaces_insert_radio, FALSE);
        gtk_widget_set_sensitive (priv->spaces_insert_one_radio, FALSE);
    }
}

static void
Scan_Toggle_Legend_Button (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;

    priv = et_scan_dialog_get_instance_private (self);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->legend_toggle)))
    {
        gtk_widget_show_all (priv->legend_grid);
    }
    else
    {
        gtk_widget_hide (priv->legend_grid);
    }
}

static void
Scan_Toggle_Mask_Editor_Button (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    GtkTreeModel *treemodel;
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    priv = et_scan_dialog_get_instance_private (self);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->mask_editor_toggle)))
    {
        gtk_widget_show_all (priv->editor_grid);

        // Select first row in list
        treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->mask_view));
        if (gtk_tree_model_get_iter_first(treemodel, &iter))
        {
            selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->mask_view));
            gtk_tree_selection_unselect_all(selection);
            gtk_tree_selection_select_iter(selection, &iter);
        }

        // Update status of the icon box cause prev instruction show it for all cases
        g_signal_emit_by_name (G_OBJECT (priv->mask_entry), "changed");
    }else
    {
        gtk_widget_hide (priv->editor_grid);
    }
}

/*
 * Update the Mask List with the new value of the entry box
 */
static void
Mask_Editor_Entry_Changed (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    GtkTreeSelection *selection;
    GtkTreePath *firstSelected;
    GtkTreeModel *treemodel;
    GList *selectedRows;
    GtkTreeIter row;
    const gchar* text;

    priv = et_scan_dialog_get_instance_private (self);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->mask_view));
    treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->mask_view));
    selectedRows = gtk_tree_selection_get_selected_rows(selection, NULL);

    if (!selectedRows)
    {
        return;
    }

    firstSelected = (GtkTreePath *)g_list_first(selectedRows)->data;
    text = gtk_entry_get_text (GTK_ENTRY (priv->mask_entry));

    if (gtk_tree_model_get_iter (treemodel, &row, firstSelected))
    {
        gtk_list_store_set(GTK_LIST_STORE(treemodel), &row, MASK_EDITOR_TEXT, text, -1);
    }

    g_list_free_full (selectedRows, (GDestroyNotify)gtk_tree_path_free);
}

/*
 * Callback from the mask edit list
 * Previously known as Mask_Editor_List_Select_Row
 */
static void
Mask_Editor_List_Row_Selected (GtkTreeSelection* selection, EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    GList *selectedRows;
    gchar *text = NULL;
    GtkTreePath *lastSelected;
    GtkTreeIter lastFile;
    GtkTreeModel *treemodel;
    gboolean valid;

    priv = et_scan_dialog_get_instance_private (self);

    treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->mask_view));

    /* We must block the function, else the previous selected row will be modified */
    g_signal_handlers_block_by_func (G_OBJECT (priv->mask_entry),
                                     (gpointer)Mask_Editor_Entry_Changed,
                                     NULL);

    selectedRows = gtk_tree_selection_get_selected_rows(selection, NULL);

    /*
     * At some point, we might get called when no rows are selected?
     */
    if (!selectedRows)
    {
        g_signal_handlers_unblock_by_func (G_OBJECT (priv->mask_entry),
                                           (gpointer)Mask_Editor_Entry_Changed,
                                           NULL);
        return;
    }

    /* Get the text of the last selected row */
    lastSelected = (GtkTreePath *)g_list_last(selectedRows)->data;

    valid= gtk_tree_model_get_iter(treemodel, &lastFile, lastSelected);
    if (valid)
    {
        gtk_tree_model_get(treemodel, &lastFile, MASK_EDITOR_TEXT, &text, -1);

        if (text)
        {
            gtk_entry_set_text (GTK_ENTRY (priv->mask_entry), text);
            g_free(text);
        }
    }

    g_signal_handlers_unblock_by_func (G_OBJECT (priv->mask_entry),
                                       (gpointer)Mask_Editor_Entry_Changed,
                                       NULL);

    g_list_free_full (selectedRows, (GDestroyNotify)gtk_tree_path_free);
}

/*
 * Remove the selected rows from the mask editor list
 */
static void
Mask_Editor_List_Remove (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreeModel *treemodel;

    priv = et_scan_dialog_get_instance_private (self);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->mask_view));
    treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->mask_view));

    if (gtk_tree_selection_count_selected_rows(selection) == 0) {
        g_critical ("%s", "Remove: No row selected");
        return;
    }

    if (!gtk_tree_model_get_iter_first(treemodel, &iter))
        return;

    while (TRUE)
    {
        if (gtk_tree_selection_iter_is_selected(selection, &iter))
        {
            if (!gtk_list_store_remove(GTK_LIST_STORE(treemodel), &iter))
            {
                break;
            }
        } else
        {
            if (!gtk_tree_model_iter_next(treemodel, &iter))
            {
                break;
            }
        }
    }
}

/*
 * Actions when the a key is pressed into the masks editor clist
 */
static gboolean
Mask_Editor_List_Key_Press (GtkWidget *widget,
                            GdkEvent *event,
                            EtScanDialog *self)
{
    if (event && event->type == GDK_KEY_PRESS)
    {
        GdkEventKey *kevent = (GdkEventKey *)event;

        switch (kevent->keyval)
        {
            case GDK_KEY_Delete:
                Mask_Editor_List_Remove (self);
                return GDK_EVENT_STOP;
                break;
            default:
                /* Ignore all other keypresses. */
                break;
        }
    }

    return GDK_EVENT_PROPAGATE;
}

/*
 * Add a new mask to the list
 */
static void
Mask_Editor_List_New (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    gchar *text;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    GtkTreeModel *treemodel;

    priv = et_scan_dialog_get_instance_private (self);

    text = _("New_mask");
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->mask_view));
    treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->mask_view));

    gtk_list_store_insert(GTK_LIST_STORE(treemodel), &iter, 0);
    gtk_list_store_set(GTK_LIST_STORE(treemodel), &iter, MASK_EDITOR_TEXT, text, -1);

    gtk_tree_selection_unselect_all(selection);
    gtk_tree_selection_select_iter(selection, &iter);
}

/*
 * Move all selected rows up one place in the mask list
 */
static void
Mask_Editor_List_Move_Up (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    GtkTreeSelection *selection;
    GList *selectedRows;
    GList *l;
    GtkTreeIter currentFile;
    GtkTreeIter nextFile;
    GtkTreePath *currentPath;
    GtkTreeModel *treemodel;

    priv = et_scan_dialog_get_instance_private (self);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->mask_view));
    treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->mask_view));
    selectedRows = gtk_tree_selection_get_selected_rows(selection, NULL);

    if (!selectedRows)
    {
        g_critical ("%s", "Move Up: No row selected");
        return;
    }

    for (l = selectedRows; l != NULL; l = g_list_next (l))
    {
        currentPath = (GtkTreePath *)l->data;
        if (gtk_tree_model_get_iter(treemodel, &currentFile, currentPath))
        {
            /* Find the entry above the node... */
            if (gtk_tree_path_prev(currentPath))
            {
                /* ...and if it exists, swap the two rows by iter */
                gtk_tree_model_get_iter(treemodel, &nextFile, currentPath);
                gtk_list_store_swap(GTK_LIST_STORE(treemodel), &currentFile, &nextFile);
            }
        }
    }

    g_list_free_full (selectedRows, (GDestroyNotify)gtk_tree_path_free);
}

/*
 * Move all selected rows down one place in the mask list
 */
static void
Mask_Editor_List_Move_Down (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    GtkTreeSelection *selection;
    GList *selectedRows;
    GList *l;
    GtkTreeIter currentFile;
    GtkTreeIter nextFile;
    GtkTreePath *currentPath;
    GtkTreeModel *treemodel;

    priv = et_scan_dialog_get_instance_private (self);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->mask_view));
    treemodel = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->mask_view));
    selectedRows = gtk_tree_selection_get_selected_rows(selection, NULL);

    if (!selectedRows)
    {
        g_critical ("%s", "Move Down: No row selected");
        return;
    }

    for (l = selectedRows; l != NULL; l = g_list_next (l))
    {
        currentPath = (GtkTreePath *)l->data;

        if (gtk_tree_model_get_iter(treemodel, &currentFile, currentPath))
        {
            /* Find the entry below the node and swap the two nodes by iter */
            gtk_tree_path_next(currentPath);
            if (gtk_tree_model_get_iter(treemodel, &nextFile, currentPath))
                gtk_list_store_swap(GTK_LIST_STORE(treemodel), &currentFile, &nextFile);
        }
    }

    g_list_free_full (selectedRows, (GDestroyNotify)gtk_tree_path_free);
}

/*
 * Set a row visible in the mask editor list (by scrolling the list)
 */
static void
Mask_Editor_List_Set_Row_Visible (GtkTreeView *view,
                                  GtkTreeModel *treeModel,
                                  GtkTreeIter *rowIter)
{
    /*
     * TODO: Make this only scroll to the row if it is not visible
     * (like in easytag GTK1)
     * See function gtk_tree_view_get_visible_rect() ??
     */
    GtkTreePath *rowPath;

    g_return_if_fail (treeModel != NULL);

    rowPath = gtk_tree_model_get_path (treeModel, rowIter);
    gtk_tree_view_scroll_to_cell (view, rowPath, NULL, FALSE, 0, 0);
    gtk_tree_path_free (rowPath);
}

/*
 * Duplicate a mask on the list
 */
static void
Mask_Editor_List_Duplicate (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;
    gchar *text = NULL;
    GList *selectedRows;
    GList *l;
    GList *toInsert = NULL;
    GtkTreeSelection *selection;
    GtkTreeIter rowIter;
    GtkTreeModel *treeModel;

    priv = et_scan_dialog_get_instance_private (self);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->mask_view));
    selectedRows = gtk_tree_selection_get_selected_rows(selection, NULL);
    treeModel = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->mask_view));

    if (!selectedRows)
    {
        g_critical ("%s", "Copy: No row selected");
        return;
    }

    /* Loop through selected rows, duplicating them into a GList
     * We cannot directly insert because the paths in selectedRows
     * get out of date after an insertion */
    for (l = selectedRows; l != NULL; l = g_list_next (l))
    {
        if (gtk_tree_model_get_iter (treeModel, &rowIter,
                                     (GtkTreePath*)l->data))
        {
            gtk_tree_model_get(treeModel, &rowIter, MASK_EDITOR_TEXT, &text, -1);
            toInsert = g_list_prepend (toInsert, text);
        }
    }

    for (l = toInsert; l != NULL; l = g_list_next (l))
    {
        gtk_list_store_insert_with_values (GTK_LIST_STORE(treeModel), &rowIter,
                                           0, MASK_EDITOR_TEXT,
                                           (gchar *)l->data, -1);
    }

    /* Set focus to the last inserted line. */
    if (toInsert)
    {
        Mask_Editor_List_Set_Row_Visible (GTK_TREE_VIEW (priv->mask_view),
                                          treeModel, &rowIter);
    }

    /* Free data no longer needed */
    g_list_free_full (selectedRows, (GDestroyNotify)gtk_tree_path_free);
    g_list_free_full (toInsert, (GDestroyNotify)g_free);
}

static void
Process_Fields_Convert_Check_Button_Toggled (EtScanDialog *self, GtkWidget *object)
{
    EtScanDialogPrivate *priv;

    priv = et_scan_dialog_get_instance_private (self);

    gtk_widget_set_sensitive (priv->convert_to_entry,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->convert_string_radio)));
    gtk_widget_set_sensitive (priv->convert_from_entry,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->convert_string_radio)));
}

/* Make sure that the Show Scanner toggle action is updated. */
static void
et_scan_on_hide (GtkWidget *widget,
                 gpointer user_data)
{
    g_action_group_activate_action (G_ACTION_GROUP (MainWindow), "scanner",
                                    NULL);
}

static void
init_process_field_check (GtkWidget *widget)
{
    g_object_set_data (G_OBJECT (widget), "flags-type",
                       GSIZE_TO_POINTER (ET_TYPE_PROCESS_FIELD));
    g_object_set_data (G_OBJECT (widget), "flags-key",
                       (gpointer) "process-fields");
    g_settings_bind_with_mapping (MainSettings, "process-fields", widget,
                                  "active", G_SETTINGS_BIND_DEFAULT,
                                  et_settings_flags_toggle_get,
                                  et_settings_flags_toggle_set, widget, NULL);
}

static void
create_scan_dialog (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;

    priv = et_scan_dialog_get_instance_private (self);

    g_settings_bind_with_mapping (MainSettings, "scan-mode", priv->notebook,
                                  "page", G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_get, et_settings_enum_set,
                                  GSIZE_TO_POINTER (ET_TYPE_SCAN_MODE), NULL);
    g_signal_connect_swapped (MainSettings, "changed::scan-mode",
                              G_CALLBACK (on_scan_mode_changed),
                              self);

    /* Mask Editor button */
    g_settings_bind (MainSettings, "scan-mask-editor-show",
                     priv->mask_editor_toggle, "active",
                     G_SETTINGS_BIND_DEFAULT);

    g_settings_bind (MainSettings, "scan-legend-show", priv->legend_toggle,
                     "active", G_SETTINGS_BIND_DEFAULT);

    /* Signal to generate preview (preview of the new tag values). */
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (priv->fill_combo)),
                              "changed",
                              G_CALLBACK (Scan_Fill_Tag_Generate_Preview),
                              self);

    /* Load masks into the combobox from a file. */
    Load_Scan_Tag_Masks_List (priv->fill_masks_model, MASK_EDITOR_TEXT,
                              Scan_Masks);
    g_settings_bind (MainSettings, "scan-tag-default-mask",
                     gtk_bin_get_child (GTK_BIN (priv->fill_combo)),
                     "text", G_SETTINGS_BIND_DEFAULT);
    Add_String_To_Combo_List (priv->fill_masks_model,
                              gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->fill_combo)))));

    /* Mask status icon. Signal connection to check if mask is correct in the
     * mask entry. */
    g_signal_connect (gtk_bin_get_child (GTK_BIN (priv->fill_combo)),
                      "changed", G_CALLBACK (entry_check_mask), NULL);

    /* Frame for Rename File. */
    /* Signal to generate preview (preview of the new filename). */
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (priv->rename_combo)),
                              "changed",
                              G_CALLBACK (Scan_Rename_File_Generate_Preview),
                              self);

    /* Load masks into the combobox from a file. */
    Load_Rename_File_Masks_List (priv->rename_masks_model, MASK_EDITOR_TEXT,
                                 Rename_File_Masks);
    g_settings_bind (MainSettings, "rename-file-default-mask",
                     gtk_bin_get_child (GTK_BIN (priv->rename_combo)),
                     "text", G_SETTINGS_BIND_DEFAULT);
    Add_String_To_Combo_List (priv->rename_masks_model,
                              gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->rename_combo)))));

    /* Mask status icon. Signal connection to check if mask is correct to the
     * mask entry. */
    g_signal_connect (gtk_bin_get_child (GTK_BIN (priv->rename_combo)),
                      "changed", G_CALLBACK (entry_check_mask), NULL);

    /* Group: select entry fields to process */
    init_process_field_check (priv->process_filename_check);
    init_process_field_check (priv->process_title_check);
    init_process_field_check (priv->process_artist_check);
    init_process_field_check (priv->process_album_artist_check);
    init_process_field_check (priv->process_album_check);
    init_process_field_check (priv->process_genre_check);
    init_process_field_check (priv->process_comment_check);
    init_process_field_check (priv->process_composer_check);
    init_process_field_check (priv->process_orig_artist_check);
    init_process_field_check (priv->process_copyright_check);
    init_process_field_check (priv->process_url_check);
    init_process_field_check (priv->process_encoded_by_check);

    g_signal_connect_swapped (MainSettings, "changed::process-fields",
                              G_CALLBACK (on_process_fields_changed), self);

    /* Group: character conversion */
    g_settings_bind_with_mapping (MainSettings, "process-convert",
                                  priv->convert_space_radio, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  priv->convert_space_radio, NULL);
    g_settings_bind_with_mapping (MainSettings, "process-convert",
                                  priv->convert_underscores_radio, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  priv->convert_underscores_radio, NULL);
    g_settings_bind_with_mapping (MainSettings, "process-convert",
                                  priv->convert_string_radio, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  priv->convert_string_radio, NULL);
    g_settings_bind_with_mapping (MainSettings, "process-convert",
                                  priv->convert_none_radio, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  priv->convert_none_radio, NULL);
    g_settings_bind (MainSettings, "process-convert-characters-from",
                     priv->convert_from_entry, "text",
                     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "process-convert-characters-to",
                     priv->convert_to_entry, "text", G_SETTINGS_BIND_DEFAULT);

    /* Group: capitalize, ... */
    g_settings_bind (MainSettings, "process-detect-roman-numerals",
                     priv->capitalize_roman_check, "active",
                     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind_with_mapping (MainSettings, "process-capitalize",
                                  priv->capitalize_all_radio, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  priv->capitalize_all_radio, NULL);
    g_settings_bind_with_mapping (MainSettings, "process-capitalize",
                                  priv->capitalize_lower_radio, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  priv->capitalize_lower_radio, NULL);
    g_settings_bind_with_mapping (MainSettings, "process-capitalize",
                                  priv->capitalize_first_radio, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  priv->capitalize_first_radio, NULL);
    g_settings_bind_with_mapping (MainSettings, "process-capitalize",
                                  priv->capitalize_first_style_radio, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  priv->capitalize_first_style_radio, NULL);
    g_settings_bind_with_mapping (MainSettings, "process-capitalize",
                                  priv->capitalize_none_radio, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_radio_get,
                                  et_settings_enum_radio_set,
                                  priv->capitalize_none_radio, NULL);

    /* Group: insert/remove spaces */
    g_settings_bind (MainSettings, "process-remove-spaces",
                     priv->spaces_remove_radio, "active",
                     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "process-insert-capital-spaces",
                     priv->spaces_insert_radio, "active",
                     G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "process-remove-duplicate-spaces",
                     priv->spaces_insert_one_radio, "active",
                     G_SETTINGS_BIND_DEFAULT);
    on_process_fields_changed (self, "process-fields", MainSettings);

    /*
     * Frame to display codes legend
     */
    /* The buttons part */
    /* To initialize the mask status icon and visibility */
    g_signal_emit_by_name (gtk_bin_get_child (GTK_BIN (priv->fill_combo)),
                           "changed");
    g_signal_emit_by_name (gtk_bin_get_child (GTK_BIN (priv->rename_combo)),
                           "changed");
    g_signal_emit_by_name (priv->mask_entry, "changed");
    g_signal_emit_by_name (priv->legend_toggle, "toggled"); /* To hide legend frame */
    g_signal_emit_by_name (priv->mask_editor_toggle, "toggled"); /* To hide mask editor frame */
    g_signal_emit_by_name (priv->convert_string_radio, "toggled"); /* To enable / disable entries */
    g_signal_emit_by_name (priv->capitalize_roman_check, "toggled"); /* To enable / disable entries */

    /* Activate the current menu in the option menu. */
    on_scan_mode_changed (self, "scan-mode", MainSettings);
}

/*
 * Select the scanner to run for the current ETFile
 */
void
Scan_Select_Mode_And_Run_Scanner (EtScanDialog *self, ET_File *ETFile)
{
    EtScanDialogPrivate *priv;

    g_return_if_fail (ET_SCAN_DIALOG (self));
    g_return_if_fail (ETFile != NULL);

    priv = et_scan_dialog_get_instance_private (self);
    switch (gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook)))
    {
        case ET_SCAN_MODE_FILL_TAG:
            Scan_Tag_With_Mask (self, ETFile);
            break;
        case ET_SCAN_MODE_RENAME_FILE:
            Scan_Rename_File_With_Mask (self, ETFile);
            break;
        case ET_SCAN_MODE_PROCESS_FIELDS:
            Scan_Process_Fields (self, ETFile);
            break;
        default:
            g_assert_not_reached ();
    }
}

/*
 * For the configuration file...
 */
void
et_scan_dialog_apply_changes (EtScanDialog *self)
{
    EtScanDialogPrivate *priv;

    g_return_if_fail (ET_SCAN_DIALOG (self));

    priv = et_scan_dialog_get_instance_private (self);

    /* Save default masks. */
    Add_String_To_Combo_List (priv->fill_masks_model,
                              gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->fill_combo)))));
    Save_Rename_File_Masks_List (priv->fill_masks_model, MASK_EDITOR_TEXT);

    Add_String_To_Combo_List (priv->rename_masks_model,
                              gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->rename_combo)))));
    Save_Rename_File_Masks_List (priv->rename_masks_model, MASK_EDITOR_TEXT);
}


/* Callback from Option button */
static void
Scan_Option_Button (void)
{
    et_application_window_show_preferences_dialog_scanner(MainWindow);
}


void
et_scan_dialog_scan_selected_files (EtScanDialog *self)
{
    EtApplicationWindow *window;
    guint progress_bar_index = 0;
    guint selectcount;

    window = MainWindow;
    et_application_window_update_et_file_from_ui (window);

    /* Initialize status bar */
    auto selfilelist = window->browser()->get_selected_files();
    selectcount = selfilelist.size();
    et_application_window_progress_set(window, 0, selectcount);

    /* Set to unsensitive all command buttons (except Quit button) */
    et_application_window_disable_command_actions (window, FALSE);

    for (auto& l : selfilelist)
    {
        /* Run the current scanner. */
        Scan_Select_Mode_And_Run_Scanner(self, l.get());

        et_application_window_progress_set(window, ++progress_bar_index, selectcount);
        /* Needed to refresh status bar */
        while (gtk_events_pending())
            gtk_main_iteration();
    }

    selfilelist.clear();

    /* Refresh the whole list (faster than file by file) to show changes. */
    et_browser_refresh_list(window->browser());

    /* Update the current file */
    et_application_window_update_ui_from_et_file(window);

    /* To update state of command buttons */
    et_application_window_update_actions (window);

    et_application_window_progress_set(window, 0, 0);
    et_application_window_status_bar_message (window,
                                              _("All tags have been scanned"),
                                              TRUE);
}

/*
 * et_scan_on_response:
 * @dialog: the scanner window
 * @response_id: the #GtkResponseType corresponding to the dialog event
 * @user_data: user data set when the signal was connected
 *
 * Handle the response signal of the scanner dialog.
 */
static void
et_scan_on_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
    switch (response_id)
    {
        case GTK_RESPONSE_APPLY:
            et_scan_dialog_scan_selected_files (ET_SCAN_DIALOG (dialog));
            break;
        case GTK_RESPONSE_CLOSE:
            gtk_widget_hide (GTK_WIDGET (dialog));
            break;
        case GTK_RESPONSE_DELETE_EVENT:
            break;
        default:
            g_assert_not_reached ();
            break;
    }
}

static void
et_scan_dialog_init (EtScanDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
    create_scan_dialog (self);
}

static void
et_scan_dialog_class_init (EtScanDialogClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/EasyTAG/scan_dialog.ui");
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  notebook);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  fill_grid);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  fill_combo);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  rename_grid);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  rename_combo);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  mask_editor_toggle);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  fill_masks_model);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  rename_masks_model);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  mask_view);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  mask_entry);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  legend_grid);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  editor_grid);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  legend_toggle);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  mask_editor_toggle);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_filename_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_title_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_artist_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_album_artist_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_album_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_genre_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_comment_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_composer_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_orig_artist_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_copyright_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_url_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  process_encoded_by_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  convert_space_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  convert_underscores_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  convert_string_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  convert_none_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  convert_to_entry);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  convert_from_entry);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  convert_to_label);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  capitalize_all_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  capitalize_lower_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  capitalize_first_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  capitalize_first_style_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  capitalize_none_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  capitalize_roman_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  spaces_remove_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  spaces_insert_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  spaces_insert_one_radio);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  fill_preview_label);
    gtk_widget_class_bind_template_child_private (widget_class, EtScanDialog,
                                                  rename_preview_label);
    gtk_widget_class_bind_template_callback (widget_class,
                                             entry_check_mask);
    gtk_widget_class_bind_template_callback (widget_class, et_scan_on_hide);
    gtk_widget_class_bind_template_callback (widget_class,
                                             et_scan_on_response);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Mask_Editor_Entry_Changed);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Mask_Editor_List_Add);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Mask_Editor_List_Add);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Mask_Editor_List_Duplicate);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Mask_Editor_List_Key_Press);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Mask_Editor_List_Move_Down);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Mask_Editor_List_Move_Up);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Mask_Editor_List_New);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Mask_Editor_List_Remove);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Mask_Editor_List_Row_Selected);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Mask_Editor_List_Save);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Process_Fields_Convert_Check_Button_Toggled);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Process_Fields_First_Letters_Check_Button_Toggled);
    gtk_widget_class_bind_template_callback (widget_class, Scan_Option_Button);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Scan_Rename_File_Prefix_Path);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Scan_Toggle_Legend_Button);
    gtk_widget_class_bind_template_callback (widget_class,
                                             Scan_Toggle_Mask_Editor_Button);
}

/*
 * et_scan_dialog_new:
 *
 * Create a new EtScanDialog instance.
 *
 * Returns: a new #EtScanDialog
 */
EtScanDialog *
et_scan_dialog_new (GtkWindow *parent)
{
    g_return_val_if_fail (GTK_WINDOW (parent), NULL);

    return (EtScanDialog*)g_object_new (ET_TYPE_SCAN_DIALOG, "transient-for", parent, NULL);
}
