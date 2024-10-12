/* EasyTAG - Tag editor for audio files
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

#include "easytag.h"

#include <glib/gi18n.h>
#include <unistd.h>
#include <sys/types.h>

#include "application_window.h"
#include "browser.h"
#include "file_description.h"
#include "file_list.h"
#include "id3_tag.h"
#include "log.h"
#include "misc.h"
#include "cddb_dialog.h"
#include "setting.h"
#include "scan_dialog.h"
#include "et_core.h"
#include "charset.h"
#include "replaygain.h"
#include "file_name.h"
#include "file_tag.h"

#include "win32/win32dep.h"

#include <vector>
#include <algorithm>
using namespace std;

static GtkWidget *QuitRecursionWindow = NULL;

/* Referenced in the header. */
gboolean Main_Stop_Button_Pressed;
GtkWidget *MainWindow;
gboolean ReadingDirectory;

/* Used to force to hide the msgbox when saving tag */
static gboolean SF_HideMsgbox_Write_Tag;
/* To remember which button was pressed when saving tag */
static gint SF_ButtonPressed_Write_Tag;
/* Used to force to hide the msgbox when renaming file */
static gboolean SF_HideMsgbox_Rename_File;
/* To remember which button was pressed when renaming file */
static gint SF_ButtonPressed_Rename_File;

static gboolean Write_File_Tag (ET_File *ETFile, gboolean hide_msgbox);
static gint Save_File (ET_File *ETFile, gboolean multiple_files,
                       gboolean force_saving_files);
static gint Save_List_Of_Files (GList *etfilelist,
                                gboolean force_saving_files);

static GList *read_directory_recursively (GList *file_list,
                                          GFileEnumerator *dir_enumerator,
                                          gboolean recurse);
static void Open_Quit_Recursion_Function_Window (void);
static void Destroy_Quit_Recursion_Function_Window (void);
static void et_on_quit_recursion_response (GtkDialog *dialog, gint response_id,
                                           gpointer user_data);


/*
 * Will save the full list of file (not only the selected files in list)
 * and check if we must save also only the changed files or all files
 * (force_saving_files==TRUE)
 */
gint Save_All_Files_With_Answer (gboolean force_saving_files)
{
    g_return_val_if_fail (ETCore != NULL && ETCore->ETFileList != NULL, FALSE);

    return Save_List_Of_Files (ETCore->ETFileList, force_saving_files);
}

/*
 * Will save only the selected files in the file list
 */
gint
Save_Selected_Files_With_Answer (gboolean force_saving_files)
{
    gint toreturn;
    GList *etfilelist = NULL;

    etfilelist = et_application_window_browser_get_selected_files (ET_APPLICATION_WINDOW (MainWindow));
    toreturn = Save_List_Of_Files (etfilelist, force_saving_files);
    g_list_free (etfilelist);

    return toreturn;
}

/*
 * Save_List_Of_Files: Function to save a list of files.
 *  - force_saving_files = TRUE => force saving the file even if it wasn't changed
 *  - force_saving_files = FALSE => force saving only the changed files
 */
static gint
Save_List_Of_Files (GList *etfilelist, gboolean force_saving_files)
{
    EtApplicationWindow *window;
    gint       progress_bar_index;
    gint       saving_answer;
    gint       nb_files_to_save;
    gint       nb_files_changed_by_ext_program;
    gchar     *msg;
    GList *l;
    ET_File   *etfile_save_position = NULL;
    GAction *action;
    GVariant *variant;
    GtkWidget *widget_focused;
    GtkTreePath *currentPath = NULL;

    g_return_val_if_fail (ETCore != NULL, FALSE);

    window = ET_APPLICATION_WINDOW (MainWindow);

    /* Save the current position in the list */
    etfile_save_position = ETCore->ETFileDisplayed;

    et_application_window_update_et_file_from_ui (window);

    /* Save widget that has current focus, to give it again the focus after saving */
    widget_focused = gtk_window_get_focus(GTK_WINDOW(MainWindow));

    /* Count the number of files to save */
    /* Count the number of files changed by an external program */
    nb_files_to_save = 0;
    nb_files_changed_by_ext_program = 0;

    for (l = etfilelist; l != NULL; l = g_list_next (l))
    {
        GFile *file;
        GFileInfo *fileinfo;

        const ET_File *ETFile = (ET_File *)l->data;

        // Count only the changed files or all files if force_saving_files==TRUE
        if (force_saving_files || !ETFile->is_saved())
            nb_files_to_save++;

        file = g_file_new_for_path(ETFile->FilePath);
        fileinfo = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                      G_FILE_QUERY_INFO_NONE, NULL, NULL);
        g_object_unref (file);

        if (fileinfo)
        {
            if (ETFile->FileModificationTime
                != g_file_info_get_attribute_uint64 (fileinfo,
                                                     G_FILE_ATTRIBUTE_TIME_MODIFIED))
            {
                nb_files_changed_by_ext_program++;
            }

            g_object_unref (fileinfo);
        }
    }

    /* Initialize status bar */
    progress_bar_index = 0;
    et_application_window_progress_set(window, 0, nb_files_to_save);

    /* Set to unsensitive all command buttons (except Quit button) */
    et_application_window_disable_command_actions (window, FALSE);
    et_application_window_browser_set_sensitive (window, FALSE);
    et_application_window_tag_area_set_sensitive (window, FALSE);
    et_application_window_file_area_set_sensitive (window, FALSE);

    /* Show msgbox (if needed) to ask confirmation ('SF' for Save File) */
    SF_HideMsgbox_Write_Tag = FALSE;
    SF_HideMsgbox_Rename_File = FALSE;

    Main_Stop_Button_Pressed = FALSE;
    /* Activate the stop button. */
    action = g_action_map_lookup_action (G_ACTION_MAP (MainWindow), "stop");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

    /*
     * Check if file was changed by an external program
     */
    if (nb_files_changed_by_ext_program > 0)
    {
        // Some files were changed by other program than EasyTAG
        GtkWidget *msgdialog = NULL;
        gint response;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_WARNING,
                                           GTK_BUTTONS_NONE,
                                           ngettext ("A file was changed by an external program",
                                                     "%d files were changed by an external program",
                                                     nb_files_changed_by_ext_program),
                                           nb_files_changed_by_ext_program);
        gtk_dialog_add_buttons (GTK_DIALOG (msgdialog), _("_Discard"),
                                GTK_RESPONSE_NO, _("_Save"), GTK_RESPONSE_YES,
                                NULL);
        gtk_dialog_set_default_response (GTK_DIALOG (msgdialog),
                                         GTK_RESPONSE_YES);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",_("Do you want to continue saving the file?"));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Quit"));

        response = gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);

        switch (response)
        {
            case GTK_RESPONSE_YES:
                break;
            case GTK_RESPONSE_NO:
            case GTK_RESPONSE_DELETE_EVENT:
                /* Skip the following loop. */
                Main_Stop_Button_Pressed = TRUE;
                break;
            default:
                g_assert_not_reached ();
                break;
        }
    }

    for (l = etfilelist; l != NULL && !Main_Stop_Button_Pressed;
         l = g_list_next (l))
    {
        ET_File* ETFile = (ET_File *)l->data;

        /* We process only the files changed and not saved, or we force to save all
         * files if force_saving_files==TRUE */
        if (force_saving_files || !ETFile->is_saved())
        {
            /* ET_Display_File_Data_To_UI ((ET_File *)l->data);
             * Use of 'currentPath' to try to increase speed. Indeed, in many
             * cases, the next file to select, is the next in the list. */
            currentPath = et_application_window_browser_select_file_by_et_file2(window, ETFile, FALSE, currentPath);

            et_application_window_progress_set(window, ++progress_bar_index, nb_files_to_save);
            /* Needed to refresh status bar */
            while (gtk_events_pending())
                gtk_main_iteration();

            // Save tag and rename file
            saving_answer = Save_File(ETFile, nb_files_to_save > 1, force_saving_files);

            if (saving_answer == -1)
            {
                /* Stop saving files + reinit progress bar */
                et_application_window_progress_set (window, 0, 0);
                et_application_window_status_bar_message (window, _("Saving files was stopped"), TRUE);
                /* To update state of command buttons */
                et_application_window_update_actions (window);
                et_application_window_browser_set_sensitive (window, TRUE);
                et_application_window_tag_area_set_sensitive (window, TRUE);
                et_application_window_file_area_set_sensitive (window, TRUE);

                if (currentPath)
                {
                    gtk_tree_path_free (currentPath);
                }
                return -1; /* We stop all actions */
            }
        }
    }

    if (currentPath)
        gtk_tree_path_free(currentPath);

    if (Main_Stop_Button_Pressed)
        msg = g_strdup (_("Saving files was stopped"));
    else
        msg = g_strdup (_("All files have been saved"));

    Main_Stop_Button_Pressed = FALSE;
    action = g_action_map_lookup_action (G_ACTION_MAP (MainWindow), "stop");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

    /* Return to the saved position in the list */
    et_application_window_display_et_file (ET_APPLICATION_WINDOW (MainWindow),
                                           etfile_save_position);
    et_application_window_browser_select_file_by_et_file (ET_APPLICATION_WINDOW (MainWindow),
                                                          etfile_save_position,
                                                          TRUE);

    /* FIXME: Find out why this is a special case for the artist/album mode. */
    action = g_action_map_lookup_action (G_ACTION_MAP (MainWindow),
                                         "file-artist-view");
    variant = g_action_get_state (action);

    if (strcmp (g_variant_get_string (variant, NULL), "artist") == 0)
    {
        et_application_window_browser_toggle_display_mode (window);
    }

    g_variant_unref (variant);

    /* To update state of command buttons */
    et_application_window_update_actions (ET_APPLICATION_WINDOW (MainWindow));
    et_application_window_browser_set_sensitive (window, TRUE);
    et_application_window_tag_area_set_sensitive (window, TRUE);
    et_application_window_file_area_set_sensitive (window, TRUE);

    /* Give again focus to the first entry, else the focus is passed to another */
    gtk_widget_grab_focus(GTK_WIDGET(widget_focused));

    et_application_window_progress_set(window, 0, 0);
    et_application_window_status_bar_message (window, msg, TRUE);
    g_free(msg);
    et_application_window_browser_refresh_list (window);
    return TRUE;
}



/*
 * Save changes of the ETFile (write tag and rename file)
 *  - multiple_files = TRUE  : when saving files, a msgbox appears with ability
 *                             to do the same action for all files.
 *  - multiple_files = FALSE : appears only a msgbox to ask confirmation.
 */
static gint
Save_File (ET_File *ETFile, gboolean multiple_files,
           gboolean force_saving_files)
{
    g_return_val_if_fail (ETFile != NULL, 0);

    const File_Name& filename_cur = *ETFile->FileNameCur();
    const File_Name& filename_new = *ETFile->FileNameNew();

    /* Save the current displayed data */

    /*
     * Check if file was changed by an external program
     */
    /*stat(filename_cur,&statbuf);
    if (ETFile->FileModificationTime != statbuf.st_mtime)
    {
        // File was changed
        GtkWidget *msgbox = NULL;
        gint response;

        msg = g_strdup_printf(_("The file '%s' was changed by an external program.\nDo you want to continue?"),basename_cur_utf8);
        msgbox = msg_box_new(_("Write File"),
                             GTK_WINDOW(MainWindow),
                             NULL,
                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                             msg,
                             GTK_STOCK_DIALOG_WARNING,
                             GTK_STOCK_NO,  GTK_RESPONSE_NO,
                             GTK_STOCK_YES, GTK_RESPONSE_YES,
                             NULL);
        g_free(msg);

        response = gtk_dialog_run(GTK_DIALOG(msgbox));
        gtk_widget_destroy(msgbox);

        switch (response)
        {
            case GTK_RESPONSE_YES:
                break;
            case GTK_RESPONSE_NO:
            case GTK_RESPONSE_NONE:
                stop_loop = -1;
                return stop_loop;
                break;
        }
    }*/


    /*
     * First part: write tag information (artist, title,...)
     */
    // Note : the option 'force_saving_files' is only used to save tags
    if (force_saving_files || !ETFile->is_filetag_saved()) // This tag had been already saved ?
    {
        GtkWidget *msgdialog = NULL;
        GtkWidget *msgdialog_check_button = NULL;
        gint response;

        if (g_settings_get_boolean (MainSettings, "confirm-write-tags")
            && !SF_HideMsgbox_Write_Tag)
        {
            // ET_Display_File_Data_To_UI(ETFile);

            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_NONE,
                                               _("Do you want to write the tag of file ‘%s’?"),
                                               filename_cur.file().get());
            gtk_window_set_title(GTK_WINDOW(msgdialog),_("Confirm Tag Writing"));
            if (multiple_files)
            {
                GtkWidget *message_area;
                message_area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(msgdialog));
                msgdialog_check_button = gtk_check_button_new_with_label(_("Repeat action for the remaining files"));
                gtk_container_add(GTK_CONTAINER(message_area),msgdialog_check_button);
                gtk_widget_show (msgdialog_check_button);
                gtk_dialog_add_buttons (GTK_DIALOG (msgdialog),
                                        _("_Discard"), GTK_RESPONSE_NO,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Save"), GTK_RESPONSE_YES, NULL);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(msgdialog_check_button), TRUE); // Checked by default
            }
            else
            {
                gtk_dialog_add_buttons (GTK_DIALOG (msgdialog),
                                        _("_Cancel"), GTK_RESPONSE_NO,
                                        _("_Save"), GTK_RESPONSE_YES, NULL);
            }

            gtk_dialog_set_default_response (GTK_DIALOG (msgdialog),
                                             GTK_RESPONSE_YES);
            SF_ButtonPressed_Write_Tag = response = gtk_dialog_run(GTK_DIALOG(msgdialog));
            // When check button in msgbox was activated : do not display the message again
            if (msgdialog_check_button && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button)))
                SF_HideMsgbox_Write_Tag = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button));
            gtk_widget_destroy(msgdialog);
        }else
        {
            if (SF_HideMsgbox_Write_Tag)
                response = SF_ButtonPressed_Write_Tag;
            else
                response = GTK_RESPONSE_YES;
        }

        switch (response)
        {
            case GTK_RESPONSE_YES:
                // if 'SF_HideMsgbox_Write_Tag is TRUE', then errors are displayed only in log
                if (!Write_File_Tag(ETFile,SF_HideMsgbox_Write_Tag)
                    // if an error occurs when 'SF_HideMsgbox_Write_Tag is TRUE', we don't stop saving...
                    && !SF_HideMsgbox_Write_Tag)
                    return -1;
            case GTK_RESPONSE_NO:
                break;
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
                return -1;
            default:
                g_assert_not_reached ();
                break;
        }
    }


    /*
     * Second part: rename the file
     */
    // Do only if changed! (don't take force_saving_files into account)
    if (!ETFile->is_filename_saved()) // This filename had been already saved ?
    {
        GtkWidget *msgdialog = NULL;
        GtkWidget *msgdialog_check_button = NULL;
        gint response;

        if (g_settings_get_boolean (MainSettings, "confirm-rename-file")
            && !SF_HideMsgbox_Rename_File)
        {
            gchar *msgdialog_title = NULL;
            gchar *msg = NULL;
            gchar *msg1 = NULL;
            // ET_Display_File_Data_To_UI(ETFile);

            // Directories were renamed? or only filename?
            if (filename_cur.path() != filename_new.path())
            {
                if (filename_cur.file() != filename_new.file())
                {
                    // Directories and filename changed
                    msgdialog_title = g_strdup (_("Rename File and Directory"));
                    msg = g_strdup(_("File and directory rename confirmation required"));
                    msg1 = g_strdup_printf (_("Do you want to rename the file and directory ‘%s’ to ‘%s’?"),
                                           filename_cur.full_name().get(), filename_new.full_name().get());
                }else
                {
                    // Only directories changed
                    msgdialog_title = g_strdup (_("Rename Directory"));
                    msg = g_strdup(_("Directory rename confirmation required"));
                    msg1 = g_strdup_printf (_("Do you want to rename the directory ‘%s’ to ‘%s’?"),
                                            filename_cur.path().get(), filename_new.path().get());
                }
            }else
            {
                // Only filename changed
                msgdialog_title = g_strdup (_("Rename File"));
                msg = g_strdup(_("File rename confirmation required"));
                msg1 = g_strdup_printf (_("Do you want to rename the file ‘%s’ to ‘%s’?"),
                                       filename_cur.file().get(), filename_new.file().get());
            }

            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_NONE,
                                               "%s",
                                               msg);
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",msg1);
            gtk_window_set_title(GTK_WINDOW(msgdialog),msgdialog_title);
            if (multiple_files)
            {
                GtkWidget *message_area;
                message_area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(msgdialog));
                msgdialog_check_button = gtk_check_button_new_with_label(_("Repeat action for the remaining files"));
                gtk_container_add(GTK_CONTAINER(message_area),msgdialog_check_button);
                gtk_widget_show (msgdialog_check_button);
                gtk_dialog_add_buttons (GTK_DIALOG (msgdialog), _("_Discard"),
                                        GTK_RESPONSE_NO, _("_Cancel"),
                                        GTK_RESPONSE_CANCEL, _("_Save"),
                                        GTK_RESPONSE_YES, NULL);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(msgdialog_check_button), TRUE); // Checked by default
            }
            else
            {
                gtk_dialog_add_buttons (GTK_DIALOG (msgdialog), _("_Discard"),
                                        GTK_RESPONSE_NO, _("_Save"),
                                        GTK_RESPONSE_YES, NULL);
            }
            g_free(msg);
            g_free(msg1);
            g_free(msgdialog_title);
            gtk_dialog_set_default_response (GTK_DIALOG (msgdialog),
                                             GTK_RESPONSE_YES);
            SF_ButtonPressed_Rename_File = response = gtk_dialog_run(GTK_DIALOG(msgdialog));
            if (msgdialog_check_button && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button)))
                SF_HideMsgbox_Rename_File = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button));
            gtk_widget_destroy(msgdialog);
        }else
        {
            if (SF_HideMsgbox_Rename_File)
                response = SF_ButtonPressed_Rename_File;
            else
                response = GTK_RESPONSE_YES;
        }

        switch(response)
        {
            case GTK_RESPONSE_YES:
            {
                GError *error = NULL;
                gboolean rc = ETFile->rename_file(&error);

                // if 'SF_HideMsgbox_Rename_File is TRUE', then errors are displayed only in log
                if (!rc)
                {
                    if (!SF_HideMsgbox_Rename_File)
                    {
                        msgdialog = gtk_message_dialog_new (GTK_WINDOW (MainWindow),
                                                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                            GTK_MESSAGE_ERROR,
                                                            GTK_BUTTONS_CLOSE,
                                                            _("Cannot rename file ‘%s’ to ‘%s’"),
                                                            filename_cur.full_name().get(),
                                                            filename_new.full_name().get());
                        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msgdialog),
                                                                  "%s",
                                                                  error->message);
                        gtk_window_set_title (GTK_WINDOW (msgdialog),
                                              _("Rename File Error"));

                        gtk_dialog_run (GTK_DIALOG (msgdialog));
                        gtk_widget_destroy (msgdialog);
                    }

                    Log_Print (LOG_ERROR,
                               _("Cannot rename file ‘%s’ to ‘%s’: %s"),
                               filename_cur.full_name().get(),
                               filename_new.full_name().get(),
                               error->message);

                    et_application_window_status_bar_message (ET_APPLICATION_WINDOW (MainWindow),
                                                              _("File(s) not renamed"),
                                                              TRUE);
                    g_error_free (error);
                }

                // if an error occurs when 'SF_HideMsgbox_Rename_File is TRUE', we don't stop saving...
                if (!rc && !SF_HideMsgbox_Rename_File)
                    return -1;
                break;
            }
            case GTK_RESPONSE_NO:
                break;
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
                return -1;
            default:
                g_assert_not_reached ();
                break;
        }
    }

    /* Refresh file into browser list */
    // Browser_List_Refresh_File_In_List(ETFile);

    return 1;
}

/*
 * Write tag of the ETFile
 * Return TRUE => OK
 *        FALSE => error
 */
static gboolean
Write_File_Tag (ET_File *ETFile, gboolean hide_msgbox)
{
    GError *error = NULL;

    const char* basename_utf8 = ETFile->FileNameCur()->file().get();
    et_application_window_status_bar_message (ET_APPLICATION_WINDOW (MainWindow),
        strprintf(_("Writing tag of ‘%s’"), basename_utf8).c_str(), TRUE);

    if (ETFile->save_file_tag(&error))
    {
        et_application_window_status_bar_message (ET_APPLICATION_WINDOW (MainWindow),
            strprintf(_("Wrote tag of ‘%s’"), basename_utf8).c_str(), TRUE);
        return TRUE;
    }

    Log_Print (LOG_ERROR, "%s", error->message);

    if (!hide_msgbox)
    {
        GtkWidget *msgdialog;
#ifdef ENABLE_ID3LIB
        if (g_error_matches (error, ET_ID3_ERROR, ET_ID3_ERROR_BUGGY_ID3LIB))
        {
            msgdialog = gtk_message_dialog_new (GTK_WINDOW (MainWindow),
                                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_CLOSE,
                                                "%s",
                                                _("You have tried to save "
                                                "this tag to Unicode but it "
                                                "was detected that your "
                                                "version of id3lib is buggy"));
            gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msgdialog),
                                                      _("If you reload this "
                                                      "file, some characters "
                                                      "in the tag may not be "
                                                      "displayed correctly. "
                                                      "Please, apply the "
                                                      "patch "
                                                      "src/id3lib/patch_id3lib_3.8.3_UTF16_writing_bug.diff "
                                                      "to id3lib, which is "
                                                      "available in the "
                                                      "EasyTAG package "
                                                      "sources.\nNote that "
                                                      "this message will "
                                                      "appear only "
                                                      "once.\n\nFile: %s"),
                                                      basename_utf8);
        }
        else
#endif
        {
            msgdialog = gtk_message_dialog_new (GTK_WINDOW (MainWindow),
                                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_CLOSE,
                                                _("Cannot write tag in file ‘%s’"),
                                                basename_utf8);
            gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msgdialog),
                                                      "%s", error->message);
            gtk_window_set_title (GTK_WINDOW (msgdialog),
                                  _("Tag Write Error"));
        }

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
    }

    g_clear_error (&error);

    return FALSE;
}

#ifdef ENABLE_REPLAYGAIN
void ReplayGain_For_Selected_Files (void)
{
	EtApplicationWindow* const window = ET_APPLICATION_WINDOW(MainWindow);
	vector<ET_File*> files;
	{	GList *etfilelist = et_application_window_browser_get_selected_files(window);
		for (GList* l = etfilelist; l; l = g_list_next(l))
			files.emplace_back((ET_File*)l->data);
		g_list_free(etfilelist);
	}

	if (files.empty())
		return;

	Main_Stop_Button_Pressed = FALSE;
	et_application_window_disable_command_actions(window, TRUE);
	et_application_window_progress_set(window, 0, files.size());
	/* Needed to refresh status bar */
	while (gtk_events_pending())
		gtk_main_iteration();

	gint (*comp)(const ET_File *ETFile1, const ET_File *ETFile2) = nullptr;
	unsigned level = 1;
	switch ((EtReplayGainGroupBy)g_settings_get_enum(MainSettings, "replaygain-groupby"))
	{	EtSortMode mode;
	case ET_REPLAYGAIN_GROUPBY_DISC:
		level = 2;
	case ET_REPLAYGAIN_GROUPBY_ALBUM:
		mode = ET_SORT_MODE_ASCENDING_ALBUM;
		goto sort;
	case ET_REPLAYGAIN_GROUPBY_FILEPATH:
		mode = ET_SORT_MODE_ASCENDING_FILEPATH;
	sort:
		comp = ET_File::get_comp_func(mode);
		sort(files.begin(), files.end(), [comp](ET_File* l, ET_File* r){ return comp(l, r) < 0; });
	default:;
	}

	ReplayGainAnalyzer analyzer;

	int done = 0;
	bool error = false;
	auto first = files.begin();

	auto finish_album = [&first, &error, &analyzer, window](decltype(first) last)
	{
		if (error)
			return Log_Print(LOG_WARNING, _("Skip album gain because of previous errors."));

		float album_gain = analyzer.GetAggregatedResult().Gain();
		float album_peak = analyzer.GetAggregatedResult().Peak();
		for (auto cur = first; cur != last; ++cur)
		{	ET_File* file = *cur;
			File_Tag* file_tag = new File_Tag(*file->FileTagNew());
			file_tag->album_gain = album_gain;
			file_tag->album_peak = album_peak;
			file->apply_changes(nullptr, file_tag);

			if (ETCore->ETFileDisplayed == file)
				et_application_window_display_et_file(window, file, ET_COLUMN_REPLAYGAIN);
		}
		Log_Print(LOG_OK, _("ReplayGain of album is %.1f dB, peak %.2f"), album_gain, album_peak);
		et_application_window_browser_refresh_list(window);
	};

	for (auto cur = first; cur != files.end(); ++cur)
	{	ET_File* file = *cur;
		// Group processing for album gain
		if (comp && first != cur && (unsigned)(abs(comp(*first, file)) - 1) < level)
		{	finish_album(cur);
			analyzer.Reset();
			error = false;
			first = cur;
		}

		const File_Name& file_name = *file->FileNameCur();

		string err = analyzer.AnalyzeFile(file->FilePath);
		if (!err.empty())
		{	Log_Print(LOG_ERROR, _("Failed to analyze file '%s': %s"), file_name.full_name().get(), err.c_str());
			error = 1;
		} else
		{	File_Tag* file_tag = new File_Tag(*file->FileTagNew());
			file_tag->track_gain = analyzer.GetLastResult().Gain();
			file_tag->track_peak = analyzer.GetLastResult().Peak();
			file->apply_changes(nullptr, file_tag);
			Log_Print(LOG_OK, _("ReplayGain of file '%s' is %.1f dB, peak %.2f"), file_name.full_name().get(), file_tag->track_gain, file_tag->track_peak);

			if (ETCore->ETFileDisplayed == file)
				et_application_window_display_et_file(window, file, ET_COLUMN_REPLAYGAIN);
			et_application_window_browser_refresh_file_in_list(window, file);
		}

		et_application_window_progress_set(window, ++done, files.size());
		/* Needed to refresh status bar */
		while (gtk_events_pending())
			gtk_main_iteration();

		if (Main_Stop_Button_Pressed)
		{	et_application_window_status_bar_message (window, _("ReplayGain calculation stopped"), TRUE);
			goto end;
		}
	}

	if (files.size() > 1) // album gain requires at least 2 files
		finish_album(files.end());

end:
	et_application_window_progress_set(window, 0, 0);
	et_application_window_update_actions(window);
}
#endif

/*
 * Scans the specified directory: and load files into a list.
 * If the path doesn't exist, we free the previous loaded list of files.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
gboolean
Read_Directory (const gchar *path_real)
{
    GFile *dir;
    GFileEnumerator *dir_enumerator;
    GError *error = NULL;
    gchar *msg;
    guint  nbrfile = 0;
    GList *FileList = NULL;
    GList *l;
    gint   progress_bar_index = 0;
    GAction *action;
    EtApplicationWindow *window;

    g_return_val_if_fail (path_real != NULL, FALSE);

    ReadingDirectory = TRUE;    /* A flag to avoid to start another reading */

    window = ET_APPLICATION_WINDOW (MainWindow);

    /* Initialize browser list */
    et_application_window_browser_clear (window);
    et_application_window_search_dialog_clear (window);

    /* Clear entry boxes  */
    et_application_window_file_area_clear (window);
    et_application_window_tag_area_clear (window);

    /* Initialize file list */
    ET_Core_Free ();
    ET_Core_Create ();
    et_application_window_update_actions (ET_APPLICATION_WINDOW (MainWindow));

    // Set to unsensitive the Browser Area, to avoid to select another file while loading the first one
    et_application_window_browser_set_sensitive (window, FALSE);

    /* Placed only here, to empty the previous list of files */
    dir = g_file_new_for_path (path_real);
    dir_enumerator = g_file_enumerate_children (dir,
                                                G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                                G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                                G_FILE_QUERY_INFO_NONE,
                                                NULL, &error);
    if (!dir_enumerator)
    {
        // Message if the directory doesn't exist...
        GtkWidget *msgdialog;
        gchar *display_path = g_filename_display_name (path_real);

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("Cannot read directory ‘%s’"),
                                           display_path);
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msgdialog),
                                                  "%s", error->message);
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Directory Read Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
        g_free (display_path);

        ReadingDirectory = FALSE; //Allow a new reading
        et_application_window_browser_set_sensitive (window, TRUE);
        g_object_unref (dir);
        g_error_free (error);
        return FALSE;
    }

    /* Open the window to quit recursion (since 27/04/2007 : not only into recursion mode) */
    et_application_window_set_busy_cursor (window);
    action = g_action_map_lookup_action (G_ACTION_MAP (MainWindow), "stop");
    g_settings_bind (MainSettings, "browse-subdir", G_SIMPLE_ACTION (action),
                     "enabled", G_SETTINGS_BIND_GET);
    Open_Quit_Recursion_Function_Window();

    /* Read the directory recursively */
    et_application_window_status_bar_message (window, _("Search in progress…"), FALSE);
    /* Search the supported files. */
    FileList = read_directory_recursively (FileList, dir_enumerator,
                                           g_settings_get_boolean (MainSettings,
                                                                   "browse-subdir"));
    g_file_enumerator_close (dir_enumerator, NULL, &error);
    g_object_unref (dir_enumerator);
    g_object_unref (dir);

    nbrfile = g_list_length(FileList);

    et_application_window_progress_set (window, 0, nbrfile);

    // Load the supported files (Extension recognized)
    for (l = FileList; l != NULL && !Main_Stop_Button_Pressed;
         l = g_list_next (l))
    {
        GFile *file = (GFile*)l->data;
        gchar *filename_real = g_file_get_path (file);
        gchar *display_path = g_filename_display_name (filename_real);

        msg = g_strdup_printf (_("File: ‘%s’"), display_path);
        et_application_window_status_bar_message (window, msg, FALSE);
        g_free(msg);
        g_free (filename_real);
        g_free (display_path);

        ETCore->ETFileList = et_file_list_add (ETCore->ETFileList, file, path_real);

        /* Update the progress bar. */
        et_application_window_progress_set(window, ++progress_bar_index, nbrfile);
        while (gtk_events_pending())
            gtk_main_iteration();
    }

    g_list_free_full (FileList, g_object_unref);
    et_application_window_progress_set(window, 0, 0);

    /* Close window to quit recursion */
    Destroy_Quit_Recursion_Function_Window();
    Main_Stop_Button_Pressed = FALSE;
    action = g_action_map_lookup_action (G_ACTION_MAP (MainWindow), "stop");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

    EtPicture::GarbageCollector(); // release orphaned images

    //ET_Debug_Print_File_List(ETCore->ETFileList,__FILE__,__LINE__,__FUNCTION__);

    if (ETCore->ETFileList)
    {
        //GList *etfilelist;
        /* Load the list of file into the browser list widget */
        et_application_window_browser_toggle_display_mode (window);

        /* Display the first file */
        //No need to select first item, because Browser_Display_Tree_Or_Artist_Album_List() does this
        //etfilelist = ET_Displayed_File_List_First();
        //if (etfilelist)
        //{
        //    ET_Display_File_Data_To_UI((ET_File *)etfilelist->data);
        //    Browser_List_Select_File_By_Etfile((ET_File *)etfilelist->data,FALSE);
        //}

        /* Prepare message for the status bar */
        if (g_settings_get_boolean (MainSettings, "browse-subdir"))
        {
            msg = g_strdup_printf (ngettext ("Found one file in this directory and subdirectories",
                                             "Found %u files in this directory and subdirectories",
                                             ETCore->ETFileDisplayedList_Length),
                                   ETCore->ETFileDisplayedList_Length);
        }
        else
        {
            msg = g_strdup_printf (ngettext ("Found one file in this directory",
                                             "Found %u files in this directory",
                                             ETCore->ETFileDisplayedList_Length),
                                   ETCore->ETFileDisplayedList_Length);
        }
    }else
    {
        /* Clear entry boxes */
        et_application_window_file_area_clear (ET_APPLICATION_WINDOW (MainWindow));
        et_application_window_tag_area_clear (ET_APPLICATION_WINDOW (MainWindow));

        et_application_window_browser_label_set_text (ET_APPLICATION_WINDOW (MainWindow),
                                                      /* Translators: No files, as in "0 files". */
                                                      _("No files")); /* See in ET_Display_Filename_To_UI */

        /* Prepare message for the status bar */
        if (g_settings_get_boolean (MainSettings, "browse-subdir"))
            msg = g_strdup(_("No file found in this directory and subdirectories"));
        else
            msg = g_strdup(_("No file found in this directory"));
    }

    /* Update sensitivity of buttons and menus */
    et_application_window_update_actions (window);

    et_application_window_browser_set_sensitive (window, TRUE);

    et_application_window_status_bar_message (window, msg, FALSE);
    g_free (msg);
    et_application_window_set_normal_cursor (window);
    ReadingDirectory = FALSE;

    return TRUE;
}



/*
 * Recurse the path to create a list of files. Return a GList of the files found.
 */
static GList *
read_directory_recursively (GList *file_list, GFileEnumerator *dir_enumerator,
                            gboolean recurse)
{
    GError *error = NULL;
    GFileInfo *info;
    const char *file_name;
    gboolean is_hidden;
    GFileType type;

    g_return_val_if_fail (dir_enumerator != NULL, file_list);

    while ((info = g_file_enumerator_next_file (dir_enumerator, NULL, &error))
           != NULL)
    {
        if (Main_Stop_Button_Pressed)
        {
            g_object_unref (info);
            return file_list;
        }

        file_name = g_file_info_get_name (info);
        is_hidden = g_file_info_get_is_hidden (info);
        type = g_file_info_get_file_type (info);

        /* Hidden directory like '.mydir' will also be browsed if allowed. */
        if (!is_hidden || (g_settings_get_boolean (MainSettings,
                                                   "browse-show-hidden")
                           && is_hidden))
        {
            if (type == G_FILE_TYPE_DIRECTORY)
            {
                if (recurse)
                {
                    /* Searching for files recursively. */
                    GFile *child_dir = g_file_enumerator_get_child (dir_enumerator,
                                                                    info);
                    GFileEnumerator *childdir_enumerator;
                    GError *child_error = NULL;
                    childdir_enumerator = g_file_enumerate_children (child_dir,
                                                                     G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                                     G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                                                     G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                                                     G_FILE_QUERY_INFO_NONE,
                                                                     NULL, &child_error);
                    if (!childdir_enumerator)
                    {
                        gchar *child_path;
                        gchar *display_path;

                        child_path = g_file_get_path (child_dir);
                        display_path = g_filename_display_name (child_path);

                        Log_Print (LOG_ERROR,
                                   _("Error opening directory ‘%s’: %s"),
                                   display_path, child_error->message);

                        g_free (display_path);
                        g_free (child_path);
                        g_error_free (child_error);
                        g_object_unref (child_dir);
                        g_object_unref (info);
                        continue;
                    }
                    file_list = read_directory_recursively (file_list,
                                                            childdir_enumerator,
                                                            recurse);
                    g_object_unref (child_dir);
                    g_file_enumerator_close (childdir_enumerator, NULL,
                                             &error);
                    g_object_unref (childdir_enumerator);
                }
            }
            else if (type == G_FILE_TYPE_REGULAR &&
                ET_File_Description::Get(file_name)->IsSupported())
            {
                GFile *file = g_file_enumerator_get_child (dir_enumerator, info);
                file_list = g_list_append (file_list, file);
            }

            // Just to not block X events
            while (gtk_events_pending())
                gtk_main_iteration();
        }
        g_object_unref (info);
    }

    if (error)
    {
        Log_Print (LOG_ERROR, _("Cannot read directory ‘%s’"), error->message);
        g_error_free (error);
    }

    return file_list;
}

/*
 * Window with the 'STOP' button to stop recursion when reading directories
 */
static void
Open_Quit_Recursion_Function_Window (void)
{
    if (QuitRecursionWindow != NULL)
        return;
    QuitRecursionWindow = gtk_message_dialog_new (GTK_WINDOW (MainWindow),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                                  GTK_MESSAGE_OTHER,
                                                  GTK_BUTTONS_NONE,
                                                  "%s",
                                                  _("Searching for audio files…"));
    gtk_window_set_title (GTK_WINDOW (QuitRecursionWindow), _("Searching"));
    gtk_dialog_add_button (GTK_DIALOG (QuitRecursionWindow), _("_Stop"),
                           GTK_RESPONSE_CANCEL);

    g_signal_connect (G_OBJECT (QuitRecursionWindow),"response",
                      G_CALLBACK (et_on_quit_recursion_response), NULL);

    gtk_widget_show_all(QuitRecursionWindow);
}

static void
Destroy_Quit_Recursion_Function_Window (void)
{
    if (QuitRecursionWindow)
    {
        gtk_widget_destroy(QuitRecursionWindow);
        QuitRecursionWindow = NULL;
        /*Statusbar_Message(_("Recursive file search interrupted."),FALSE);*/
    }
}

static void
et_on_quit_recursion_response (GtkDialog *dialog, gint response_id,
                               gpointer user_data)
{
    switch (response_id)
    {
        case GTK_RESPONSE_CANCEL:
            Action_Main_Stop_Button_Pressed ();
            Destroy_Quit_Recursion_Function_Window ();
            break;
        case GTK_RESPONSE_DELETE_EVENT:
            Destroy_Quit_Recursion_Function_Window ();
            break;
        default:
            g_assert_not_reached ();
            break;
    }
}

/*
 * To stop the recursive search within directories or saving files
 */
void Action_Main_Stop_Button_Pressed (void)
{
    GAction *action;

    action = g_action_map_lookup_action (G_ACTION_MAP (MainWindow), "stop");
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
    Main_Stop_Button_Pressed = TRUE;
}
