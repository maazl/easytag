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
#include "charset.h"
#include "replaygain.h"
#include "file_name.h"
#include "file_tag.h"
#include "xptr.h"

#include "win32/win32dep.h"

#include <vector>
#include <deque>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
using namespace std;

/* Referenced in the header. */
atomic<bool> Main_Stop_Button_Pressed(false);
struct _EtApplicationWindow *MainWindow;

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


/*
 * Save_List_Of_Files: Function to save a list of files.
 *  - force_saving_files = TRUE => force saving the file even if it wasn't changed
 *  - force_saving_files = FALSE => force saving only the changed files
 */
static gint
Save_List_Of_Files(const vector<xPtr<ET_File>>& etfilelist, gboolean force_saving_files)
{
    EtApplicationWindow *window;
    gint       progress_bar_index;
    gint       saving_answer;
    gint       nb_files_to_save;
    gint       nb_files_changed_by_ext_program;
    GtkWidget *widget_focused;
    GtkTreePath *currentPath = NULL;

    window = MainWindow;

    et_application_window_update_et_file_from_ui (window);

    /* Save widget that has current focus, to give it again the focus after saving */
    widget_focused = gtk_window_get_focus(GTK_WINDOW(MainWindow));

    /* Count the number of files to save */
    /* Count the number of files changed by an external program */
    nb_files_to_save = 0;
    nb_files_changed_by_ext_program = 0;

    for (const ET_File *ETFile : etfilelist)
    {
        GFile *file;
        GFileInfo *fileinfo;

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
    et_browser_set_sensitive(window->browser(), FALSE);
    window->displayed_file_sensitive(false);

    /* Show msgbox (if needed) to ask confirmation ('SF' for Save File) */
    SF_HideMsgbox_Write_Tag = FALSE;
    SF_HideMsgbox_Rename_File = FALSE;

    Main_Stop_Button_Pressed = false;
    /* Activate the stop button. */
    et_application_set_action_state(window, "stop", FALSE);

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
                Main_Stop_Button_Pressed = true;
                break;
            default:
                g_assert_not_reached ();
                break;
        }
    }

    for (auto& ETFile : etfilelist)
    {
        if (Main_Stop_Button_Pressed)
            break;

        /* We process only the files changed and not saved, or we force to save all
         * files if force_saving_files==TRUE */
        if (force_saving_files || !ETFile->is_saved())
        {
            /* ET_Display_File_Data_To_UI ((ET_File *)l->data);
             * Use of 'currentPath' to try to increase speed. Indeed, in many
             * cases, the next file to select, is the next in the list. */
            currentPath = et_browser_select_file_by_et_file2(window->browser(), ETFile, FALSE, currentPath);

            et_application_window_progress_set(window, ++progress_bar_index, nb_files_to_save);
            /* Needed to refresh status bar */
            while (gtk_events_pending())
                gtk_main_iteration();

            // Save tag and rename file
            saving_answer = Save_File(ETFile.get(), nb_files_to_save > 1, force_saving_files);

            if (saving_answer == -1)
            {
                /* Stop saving files + reinit progress bar */
                et_application_window_progress_set (window, 0, 0);
                et_application_window_status_bar_message (window, _("Saving files was stopped"), TRUE);
                /* To update state of command buttons */
                et_application_window_update_actions (window);
                et_browser_set_sensitive(window->browser(), TRUE);
                window->displayed_file_sensitive(true);

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

    const gchar* msg = Main_Stop_Button_Pressed ? _("Saving files was stopped") : _("All files have been saved");

    Main_Stop_Button_Pressed = false;
    et_application_set_action_state(window, "stop", FALSE);

    /* Return to the saved position in the list */
    et_browser_select_file_by_et_file(MainWindow->browser(), MainWindow->get_displayed_file(), FALSE);
    /* To update state of command buttons */
    et_application_window_update_actions(window);
    et_browser_set_sensitive(window->browser(), TRUE);
    window->displayed_file_sensitive(true);

    /* Give again focus to the first entry, else the focus is passed to another */
    gtk_widget_grab_focus(GTK_WIDGET(widget_focused));

    et_application_window_progress_set(window, 0, 0);
    et_application_window_status_bar_message (window, msg, TRUE);
    et_browser_refresh_list(window->browser());
    return TRUE;
}

/*
 * Will save the full list of file (not only the selected files in list)
 * and check if we must save also only the changed files or all files
 * (force_saving_files==TRUE)
 */
gint Save_All_Files_With_Answer(gboolean force_saving_files)
{
    return Save_List_Of_Files(ET_FileList::all_files(), force_saving_files);
}

/*
 * Will save only the selected files in the file list
 */
gint Save_Selected_Files_With_Answer(gboolean force_saving_files)
{
    return Save_List_Of_Files(MainWindow->browser()->get_selected_files(), force_saving_files);
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

                    et_application_window_status_bar_message(MainWindow, _("File(s) not renamed"), TRUE);
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
    et_application_window_status_bar_message(MainWindow,
        strprintf(_("Writing tag of ‘%s’"), basename_utf8).c_str(), TRUE);

    if (ETFile->save_file_tag(&error))
    {
        et_application_window_status_bar_message(MainWindow,
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
class ReplayGainWorker : public xObj
{
	typedef vector<xPtr<ET_File>> FileList;
	typedef FileList::const_iterator FileListIterator;

	/// The currently running instance if any.
	static xPtr<ReplayGainWorker> Instance;

	ReplayGainAnalyzer Analyzer;

	gint (*AlbumComparer)(const ET_File *ETFile1, const ET_File *ETFile2) = nullptr;
	unsigned CompareLevel = 1;

	/// List of files to process.
	const FileList Files;
	double TotalDuration = 0.;
	double CurrentDuration = 0.; // this field belongs to the main thread.

	FileList::iterator AlbumFirst;

private:
	double GetFileDuration(const ET_File* file)
	{	double duration = file->ETFileInfo.duration;
		return duration > 0 ? duration : file->FileSize / 16000.; // no length? => use some estimate
	}

	void OnFileCompleted(FileListIterator cur, string err, float track_gain, float track_peak);
	void OnAlbumCompleted(FileListIterator first, FileListIterator last, bool error, float album_gain, float album_peak);
	void OnFinished(bool cancelled);

	ReplayGainWorker(vector<xPtr<ET_File>>&& files);
	void FinishAlbum(FileListIterator first, FileListIterator last, bool error);
	/// Background thread
	void Run();
public:
	/// Start a new worker instance.
	/// @return Instance pointer or \c nullptr if there is another instance still running.
	static ReplayGainWorker* Start(vector<xPtr<ET_File>>&& files);
};

xPtr<ReplayGainWorker> ReplayGainWorker::Instance;

ReplayGainWorker::ReplayGainWorker(vector<xPtr<ET_File>>&& files)
:	Analyzer((EtReplayGainModel)g_settings_get_enum(MainSettings, "replaygain-model"))
,	Files(files)
{
	switch ((EtReplayGainGroupBy)g_settings_get_enum(MainSettings, "replaygain-groupby"))
	{	EtSortMode mode;
	case ET_REPLAYGAIN_GROUPBY_DISC:
		CompareLevel = 2;
	case ET_REPLAYGAIN_GROUPBY_ALBUM:
		mode = ET_SORT_MODE_ASCENDING_ALBUM;
		goto sort;
	case ET_REPLAYGAIN_GROUPBY_FILEPATH:
		mode = ET_SORT_MODE_ASCENDING_FILEPATH;
	sort:
		AlbumComparer = ET_File::get_comp_func(mode);
	default:;
	}

	if (AlbumComparer)
		sort(const_cast<FileList&>(Files).begin(), const_cast<FileList&>(Files).end(),
			[comp = AlbumComparer](const xPtr<ET_File>& l, const xPtr<ET_File>& r){ return comp(l, r) < 0; });

	for (const ET_File* file : Files)
		TotalDuration += GetFileDuration(file);
}

void ReplayGainWorker::OnAlbumCompleted(FileListIterator first, FileListIterator last, bool error, float album_gain, float album_peak)
{
	if (error)
		return Log_Print(LOG_WARNING, _("Skip album gain because of previous errors."));

	EtApplicationWindow* const window = MainWindow;

	for (auto cur = first; cur != last; ++cur)
	{	ET_File* file = cur->get();
		File_Tag* file_tag = new File_Tag(*file->FileTagNew());
		file_tag->album_gain = album_gain;
		file_tag->album_peak = album_peak;
		file->apply_changes(nullptr, file_tag);

		if (MainWindow->get_displayed_file() == file)
			et_application_window_update_ui_from_et_file(window, ET_COLUMN_REPLAYGAIN);
	}
	Log_Print(LOG_OK, _("ReplayGain of album is %.1f dB, peak %.2f"), album_gain, album_peak);
	et_browser_refresh_list(window->browser()); // hmm, maybe a bit too much
}

void ReplayGainWorker::OnFileCompleted(FileListIterator cur, string err, float track_gain, float track_peak)
{
	ET_File* const file = cur->get();
	const File_Name& file_name = *file->FileNameCur();
	EtApplicationWindow* const window = MainWindow;

	if (!err.empty())
	{	Log_Print(LOG_ERROR, _("Failed to analyze file '%s': %s"), file_name.full_name().get(), err.c_str());
	} else
	{	File_Tag* file_tag = new File_Tag(*file->FileTagNew());
		file_tag->track_gain = track_gain;
		file_tag->track_peak = track_peak;
		file->apply_changes(nullptr, file_tag);
		Log_Print(LOG_OK, _("ReplayGain of file '%s' is %.1f dB, peak %.2f"), file_name.full_name().get(), file_tag->track_gain, file_tag->track_peak);

		if (MainWindow->get_displayed_file() == file)
			et_application_window_update_ui_from_et_file(window, ET_COLUMN_REPLAYGAIN);
		et_browser_refresh_file_in_list(window->browser(), file);
	}

	CurrentDuration += GetFileDuration(*cur);
	et_application_window_progress_set(window, cur - Files.begin() + 1, Files.size(), CurrentDuration / TotalDuration);
}

void ReplayGainWorker::FinishAlbum(FileListIterator first, FileListIterator last, bool error)
{
	float album_gain = Analyzer.GetAggregatedResult().Gain();
	float album_peak = Analyzer.GetAggregatedResult().Peak();
	gIdleAdd(new function<void()>([that = xPtr<ReplayGainWorker>(this), first, last, error, album_gain, album_peak]()
		{ that->OnAlbumCompleted(first, last, error, album_gain, album_peak); }));
}

ReplayGainWorker* ReplayGainWorker::Start(vector<xPtr<ET_File>>&& files)
{
	if (Instance)
		return nullptr;
	Main_Stop_Button_Pressed = false;
	EtApplicationWindow* const window = MainWindow;
	et_application_window_disable_command_actions(window, TRUE);
	et_application_window_progress_set(window, 0, files.size(), 0.);

	Instance = new ReplayGainWorker(move(files));
	std::thread(&ReplayGainWorker::Run, ref(*Instance)).detach();
	return Instance.get();
}

void ReplayGainWorker::Run()
{
	bool error = false;
	auto first = Files.begin();

	for (auto cur = first; cur != Files.end(); ++cur)
	{	ET_File* file = cur->get();
		// Group processing for album gain
		if (AlbumComparer && first != cur && (unsigned)(abs(AlbumComparer(*first, file)) - 1) < CompareLevel)
		{	FinishAlbum(first, cur, error);
			Analyzer.Reset();
			error = false;
			first = cur;
		}

		string err = Analyzer.AnalyzeFile(file->FilePath);
		if (err == "$Aborted")
		{abort:
			error = true;
			goto end;
		}
		if (!err.empty())
			error = true;

		float track_gain = Analyzer.GetLastResult().Gain();
		float track_peak = Analyzer.GetLastResult().Peak();
		gIdleAdd(new function<void()>([that = xPtr<ReplayGainWorker>(this), cur, err, track_gain, track_peak]()
			{ that->OnFileCompleted(cur, err, track_gain, track_peak); }));

		if (Main_Stop_Button_Pressed)
			goto abort;
	}

	if (Files.size() > 1) // album gain requires at least 2 files
		FinishAlbum(first, Files.end(), error);

	error = false;

end:
	gIdleAdd(new function<void()>([that = xPtr<ReplayGainWorker>(this), error]()
		{ that->OnFinished(error); }));

	Instance.reset();
}

void ReplayGainWorker::OnFinished(bool cancelled)
{
	EtApplicationWindow* const window = MainWindow;

	if (cancelled)
		et_application_window_status_bar_message (window, _("ReplayGain calculation stopped"), TRUE);

	et_application_window_progress_set(window, 0, 0);
	et_application_window_update_actions(window);
}

void ReplayGain_For_Selected_Files (void)
{
	auto etfilelist = MainWindow->browser()->get_selected_files();
	if (!etfilelist.size())
		return; // nothing to do

	ReplayGainWorker::Start(move(etfilelist));
}
#endif

class ReadDirectoryWorker : public xObj
{
	/// The currently running instance if any.
	static xPtr<ReadDirectoryWorker> Instance;

	// captured settings
	const bool Recursive;
	const bool BrowseHidden;
	const size_t NumWorkers;

	const gString RootPath;

	/// worker queue
	/// @details
	/// GFile, false => audio file,
	/// GFile, true => directory,
	/// nullptr, false => end marker,
	/// nullptr, true => last end marker
	deque<pair<gObject<GFile>, bool>> Files;
	ET_FileList::list_type ResultList;
	/// worker threads
	vector<thread> Worker;
	/// Synchronize access to the above data
	mutex Sync;
	condition_variable Cond;

	atomic<int> DirCount;
	// data for the UI
	atomic<int> FilesTotal;
	int FilesCompleted;

private:
	static void OnDirCompleted(GFile* child_dir, const char* error);
	static void OnFileCompleted(xPtr<ET_File> ETFile, const char* error);
	static void OnFinished();

	ReadDirectoryWorker(gString&& path);
	void DirScan(gObject<GFileEnumerator> dir_enumerator);
	void ItemWorker();
public:
	static bool Start(gString&& path);
	static bool IsReadingDirectory() { return !!Instance; }
	~ReadDirectoryWorker();
};

ReadDirectoryWorker::ReadDirectoryWorker(gString&& path)
:	Recursive(g_settings_get_boolean(MainSettings, "browse-subdir"))
,	BrowseHidden(g_settings_get_boolean(MainSettings, "browse-show-hidden"))
,	NumWorkers(g_settings_get_uint(MainSettings, "background-threads"))
,	RootPath(move(path))
,	Worker()
,	DirCount(1)
,	FilesTotal(0)
,	FilesCompleted(0)
{	Worker.reserve(NumWorkers);
	// kick off root dir worker
	lock_guard<mutex> lock(Sync);
	Files.emplace_front(move(gObject<GFile>(g_file_new_for_path(RootPath))), true);
	// from here continue in background thread
	Worker.emplace_back(&ReadDirectoryWorker::ItemWorker, ref(*this));
}

xPtr<ReadDirectoryWorker> ReadDirectoryWorker::Instance;

ReadDirectoryWorker::~ReadDirectoryWorker()
{	for (auto& w : Worker)
		if (w.joinable())
			w.join();
}

bool ReadDirectoryWorker::Start(gString&& path)
{
	if (Instance)
		return false;

	Instance = xPtr<ReadDirectoryWorker>(new ReadDirectoryWorker(move(path)));

	// Set to unsensitive the Browser Area, to avoid to select another file while loading the first one
	EtApplicationWindow* window = MainWindow;

	et_application_window_disable_command_actions(window, TRUE);
	et_application_window_status_bar_message(window, _("Searching for audio files…"), FALSE);

	return true;
}

void ReadDirectoryWorker::OnDirCompleted(GFile* child_dir, const char* error)
{	if (!child_dir)
		Log_Print(LOG_ERROR, _("Cannot read directory ‘%s’"), error);
	else
	{	gString child_path(g_file_get_path(child_dir));
		gString display_path(g_filename_display_name(child_path));
		if (Instance->DirCount)
			Log_Print(LOG_ERROR, _("Error opening directory ‘%s’: %s"), display_path.get(), error);
		else
		{	// Message if the root directory doesn't exist...
			GtkWidget *msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Cannot read directory ‘%s’"),
				display_path.get());
			gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog), "%s", error);
			gtk_window_set_title(GTK_WINDOW(msgdialog), _("Directory Read Error"));

			gtk_dialog_run(GTK_DIALOG(msgdialog));
			gtk_widget_destroy(msgdialog);
		}
	}
}

void ReadDirectoryWorker::OnFileCompleted(xPtr<ET_File> ETFile, const char* error)
{	if (error)
		Log_Print(LOG_ERROR, _("Error reading tag from %s ‘%s’: %s"),
			ETFile->ETFileDescription->FileType, ETFile->FileNameNew()->full_name().get(), error);
	else if (ETFile->autofix())
		Log_Print(LOG_INFO, _("Automatic corrections applied for file ‘%s’"), ETFile->FileNameNew()->full_name().get());

  /* Update the progress bar. */
  et_application_window_progress_set(MainWindow, ++Instance->FilesCompleted, Instance->FilesTotal);
}

void ReadDirectoryWorker::OnFinished()
{	EtApplicationWindow* window = MainWindow;

	et_application_window_progress_set(window, 0, 0);

	const gchar* msg;
	gString msgBuffer;
	unsigned count = Instance->ResultList.size();

	if (Main_Stop_Button_Pressed)
		msg = _("Directory scan aborted.");
	else
	{
		ET_File::reset_undo_history();
		ET_FileList::set_file_list(move(Instance->ResultList));

		if (count)
		{	/* Load the list of file into the browser list widget */
			et_application_window_browser_update_display_mode(window);
			et_application_window_update_actions(window);

			/* Prepare message for the status bar */
			msg = msgBuffer = g_strdup_printf(g_settings_get_boolean(MainSettings, "browse-subdir")
					? ngettext("Found one file in this directory and subdirectories", "Found %u files in this directory and subdirectories", count)
					: ngettext("Found one file in this directory", "Found %u files in this directory", count),
				count);
		} else
		{	/* Clear entry boxes */
			window->change_displayed_file(nullptr);

			et_browser_label_set_text(window->browser(),
				/* Translators: No files, as in "0 files". */ _("No files")); /* See in ET_Display_Filename_To_UI */

			/* Prepare message for the status bar */
			if (g_settings_get_boolean (MainSettings, "browse-subdir"))
				msg = _("No file found in this directory and subdirectories");
			else
				msg = _("No file found in this directory");
		}
	}

	/* Update sensitivity of buttons and menus */
	Main_Stop_Button_Pressed = false;
	et_application_window_status_bar_message(window, msg, FALSE);

	Instance.reset();
}

void ReadDirectoryWorker::DirScan(gObject<GFileEnumerator> dir_enumerator)
{
	GError *error = NULL;
	gObject<GFileInfo> info;

	while ((info = gObject<GFileInfo>(g_file_enumerator_next_file(dir_enumerator.get(), NULL, &error))))
	{
		if (Main_Stop_Button_Pressed)
			return;

		/* Hidden directory like '.mydir' will also be browsed if allowed. */
		if (!BrowseHidden && g_file_info_get_is_hidden(info.get()))
			continue;

		const char *file_name = g_file_info_get_name(info.get());
		GFileType type = g_file_info_get_file_type(info.get());
		switch (type)
		{case G_FILE_TYPE_REGULAR:
			if (ET_File_Description::Get(file_name)->IsSupported())
				break;
			continue;
		 case G_FILE_TYPE_DIRECTORY:
			if (Recursive)
				break;
		 default:
			continue;
		}

		GFile* file = g_file_enumerator_get_child(dir_enumerator.get(), info.get());
		lock_guard<mutex> lock(Sync);
		if (type == G_FILE_TYPE_REGULAR)
		{	Files.emplace_back(gObject<GFile>(file), false);
			++FilesTotal;
		} else
		{	// place directories to the front to get total count ASAP
			Files.emplace_front(gObject<GFile>(file), true);
			++DirCount;
		}

		// notification required?
		if (Files.size() < Worker.size())
			Cond.notify_one();
		// start more workers?
		if (Worker.size() < min(NumWorkers, Files.size()))
			Worker.emplace_back(&ReadDirectoryWorker::ItemWorker, ref(*this));
	}

	if (error)
	{	gIdleAdd(new function<void()>([msg = xString(error->message)]()
			{	OnDirCompleted(nullptr, msg); }));
		g_error_free(error);
	}
}

void ReadDirectoryWorker::ItemWorker()
{	while (true)
	{	// fetch next item
		pair<gObject<GFile>, bool> item;
		{	unique_lock<mutex> lock(Sync);
			while (!Files.size()) Cond.wait(lock);

			item = move(Files.front());

			if (item.first && Main_Stop_Button_Pressed)
			{	// cancel immediately
				Files.clear();
				// force termination of all workers
				for (size_t i = Worker.size(); --i; )
					Files.emplace_back();
				Files.emplace_back(nullptr, true);
				Cond.notify_all();
				item = move(Files.front());
			}

			Files.pop_front();
		}

		if (!item.first) // deadly termination pill
		{	if (item.second)
			{	gIdleAdd(new function<void()>([]() { OnFinished(); }));
				EtPicture::GarbageCollector(); // release orphaned images
			}
			return;
		}

		GError *error = NULL;

		if (!item.second)
		{	/* Get description of the file */
			xPtr<ET_File> ETFile(new ET_File(gString(g_file_get_path(item.first.get()))));

			ETFile->read_file(item.first.get(), RootPath, &error);

			/* Add the item to the "result list" */
			{	lock_guard<mutex> lock(Sync);
				ResultList.emplace_back(ETFile);
			}

			gIdleAdd(new function<void()>([ETFile = move(ETFile), msg = xString(error ? error->message : nullptr)]()
				{	OnFileCompleted(move(ETFile), msg); }));
		} else
		{	// Searching for files recursively.
			gObject<GFileEnumerator> childdir_enumerator(g_file_enumerate_children(item.first.get(),
				G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
				G_FILE_QUERY_INFO_NONE, NULL, &error));
			if (childdir_enumerator)
				DirScan(move(childdir_enumerator));
			else
				gIdleAdd(new function<void()>([child_dir = move(item.first), msg = xString(error->message)]()
					{	OnDirCompleted(child_dir.get(), msg); }));

			if (--DirCount == 0)
			{	// last directory processed => put termination markers in the queue
				lock_guard<mutex> lock(Sync);
				for (size_t i = Worker.size(); --i; )
					Files.emplace_back();
				Files.emplace_back(nullptr, true);
				Cond.notify_all();
			}
		}

		if (error)
			g_error_free(error);
	}
}

bool IsReadingDirectory()
{	return ReadDirectoryWorker::IsReadingDirectory();
}

/*
 * Scans the specified directory: and load files into a list.
 * If the path doesn't exist, we free the previous loaded list of files.
 */
gboolean Read_Directory(gString path_real)
{
    g_return_val_if_fail (path_real != NULL, FALSE);

    EtApplicationWindow *window = MainWindow;

    /* Initialize browser list */
    window->browser()->clear();
    et_application_window_search_dialog_clear (window);

    /* Clear entry boxes  */
    window->change_displayed_file(nullptr);

    /* Initialize file list */
    ET_File::reset_undo_history();
    ET_FileList::clear();
    et_application_window_update_actions(MainWindow);

    return ReadDirectoryWorker::Start(move(path_real));
}


/*
 * To stop the recursive search within directories or saving files
 */
void Action_Main_Stop_Button_Pressed (void)
{
    et_application_set_action_state(MainWindow, "stop", FALSE);
    Main_Stop_Button_Pressed = true;
}

bool et_run_audio_player(std::vector<xPtr<ET_File>>::const_iterator from, std::vector<xPtr<ET_File>>::const_iterator to)
{
	if (from == to)
		return true; // no files, no error

	// generate GFile list
	gListP<GFile*> files;
	do
		files = files.prepend(g_file_new_for_path((--to)->get()->FilePath));
	while (from != to);

  GError *error = NULL;
	GFileInfo* info = g_file_query_info(files->data, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, &error);
	bool ret = false;

  if (info)
	{
		const gchar *content_type = g_file_info_get_content_type(info);
		gObject<GAppInfo> app_info(g_app_info_get_default_for_type(content_type, FALSE));
		g_object_unref (info);

		gObject<GdkAppLaunchContext> context(gdk_display_get_app_launch_context(gdk_display_get_default()));

		ret = g_app_info_launch(app_info.get(), files, G_APP_LAUNCH_CONTEXT(context.get()), &error);
	}

	g_list_free_full(files, g_object_unref);

	if (!ret)
	{	Log_Print (LOG_ERROR, _("Failed to launch program ‘%s’"), error->message);
		g_error_free (error);
	}
	return ret;
}

/*
 * Run a program with a list of parameters
 *  - args_list : list of filename (with path)
 */
gboolean
et_run_program (const gchar *program_name,
                GList *args_list,
                GError **error)
{
    gchar *program_tmp;
    const gchar *program_args;
    gchar **program_args_argv = NULL;
    guint n_program_args = 0;
    gsize i;
    gchar **argv;
    GSubprocess *subprocess;
    GList *l;
    gchar *program_path;
    gboolean res = FALSE;

    g_return_val_if_fail (program_name != NULL && args_list != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    /* Check if a name for the program has been supplied */
    if (!*program_name)
    {
        GtkWidget *msgdialog;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_OK,
                                           "%s",
                                           _("You must type a program name"));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Program Name Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
        return res;
    }

    /* If user arguments are included, try to skip them. FIXME: This works
     * poorly when there are spaces in the absolute path to the binary. */
    program_tmp = g_strdup (program_name);

    /* Skip the binary name and a delimiter. */
#ifdef G_OS_WIN32
    /* FIXME: Should also consider .com, .bat, .sys. See
     * g_find_program_in_path(). */
    if ((program_args = strstr (program_tmp, ".exe")))
    {
        /* Skip ".exe". */
        program_args += 4;
    }
#else /* !G_OS_WIN32 */
    /* Remove arguments if found. */
    program_args = strchr (program_tmp, ' ');
#endif /* !G_OS_WIN32 */

    if (program_args && *program_args)
    {
        size_t len;

        len = program_args - program_tmp;
        program_path = g_strndup (program_name, len);

        /* FIXME: Splitting arguments based on a delimiting space is bogus
         * if the arguments have been quoted. */
        program_args_argv = g_strsplit (program_args, " ", 0);
        n_program_args = g_strv_length (program_args_argv);
    }
    else
    {
        n_program_args = 1;
        program_path = g_strdup (program_name);
    }

    g_free (program_tmp);

    /* +1 for NULL, program_name is already included in n_program_args. */
    argv = g_new0 (gchar *, n_program_args + g_list_length (args_list) + 1);

    argv[0] = program_path;

    if (program_args_argv)
    {
        /* Skip program_args_argv[0], which is " ". */
        for (i = 1; program_args_argv[i] != NULL; i++)
        {
            argv[i] = program_args_argv[i];
        }
    }
    else
    {
        i = 1;
    }

    /* Load arguments from 'args_list'. */
    for (l = args_list; l != NULL; l = g_list_next (l), i++)
    {
        argv[i] = (gchar *)l->data;
    }

    argv[i] = NULL;

    /* Execution ... */
    if ((subprocess = g_subprocess_newv ((const gchar * const *)argv,
                                         G_SUBPROCESS_FLAGS_NONE, error)))
    {
        res = TRUE;
        /* There is no need to watch to see if the child process exited. */
        g_object_unref (subprocess);
    }

    g_strfreev (program_args_argv);
    g_free (program_path);
    g_free (argv);

    return res;
}
