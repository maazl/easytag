/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022-2025  Marcel Müller <github@maazl.de>
 * Copyright (C) 2014-2015  David King <amigadave@amigadave.com>
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

#include "application_window.h"

#include <glib/gi18n.h>

#include "etflagsaction.h"
#include "browser.h"
#include "cddb_dialog.h"
#include "charset.h"
#include "easytag.h"
#include "file_area.h"
#include "file_list.h"
#include "load_files_dialog.h"
#include "log.h"
#include "misc.h"
#include "picture.h"
#include "playlist_dialog.h"
#include "preferences_dialog.h"
#include "progress_bar.h"
#include "search_dialog.h"
#include "scan.h"
#include "scan_dialog.h"
#include "setting.h"
#include "status_bar.h"
#include "tag_area.h"
#include "file_name.h"
#include "file_tag.h"

using namespace std;

typedef struct
{
    EtBrowser *browser;

    GtkWidget *file_area;
    GtkWidget *log_area;
    GtkWidget *tag_area;
    GtkWidget *progress_bar;
    GtkWidget *status_bar;

    GtkWidget *cddb_dialog;
    GtkWidget *load_files_dialog;
    GtkWidget *playlist_dialog;
    GtkWidget *preferences_dialog;
    GtkWidget *scan_dialog;
    GtkWidget *search_dialog;

    GtkPaned *hpaned;
    GtkPaned *vpaned;

    GdkCursor *cursor;

    gboolean is_maximized;
    gint height;
    gint width;
    gint paned_position;

    ET_File* displayed_file; ///< file currently visible in file and tag area. This is an active xPtr instance!
} EtApplicationWindowPrivate;

// learn correct return type for et_browser_get_instance_private
#define et_application_window_get_instance_private et_application_window_get_instance_private_
G_DEFINE_TYPE_WITH_PRIVATE (EtApplicationWindow, et_application_window, GTK_TYPE_APPLICATION_WINDOW)
#undef et_application_window_get_instance_private
#define et_application_window_get_instance_private(x) ((EtApplicationWindowPrivate*)et_application_window_get_instance_private_(x))


EtBrowser* EtApplicationWindow::browser()
{	return et_application_window_get_instance_private(this)->browser;
}

void EtApplicationWindow::displayed_file_sensitive(bool sensitive)
{
	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(this);
	gtk_widget_set_sensitive(gtk_bin_get_child(GTK_BIN(priv->tag_area)), sensitive);
	gtk_widget_set_sensitive(gtk_bin_get_child(GTK_BIN(priv->file_area)), sensitive);
}

ET_File* EtApplicationWindow::get_displayed_file()
{
	return et_application_window_get_instance_private(this)->displayed_file;
}


static const gchar* et_application_window_file_area_get_filename(EtApplicationWindow *self);
static void et_application_window_file_area_set_file_fields(EtApplicationWindow *self, const ET_File *ETFile);

/* Used to force to hide the msgbox when deleting file */
static gboolean SF_HideMsgbox_Delete_File;
/* To remember which button was pressed when deleting file */
static gint SF_ButtonPressed_Delete_File;

static gboolean
on_main_window_delete_event (GtkWidget *window,
                             GdkEvent *event,
                             gpointer user_data)
{
    et_application_window_quit (ET_APPLICATION_WINDOW (window));

    /* Handled the event, so stop propagation. */
    return GDK_EVENT_STOP;
}

static void
save_state (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;
    gchar *path;
    GKeyFile *keyfile;
    GError *error = NULL;

    priv = et_application_window_get_instance_private (self);
    keyfile = g_key_file_new ();
    path = g_build_filename (g_get_user_cache_dir (), PACKAGE_TARNAME,
                             "state", NULL);

    /* Try to preserve comments by loading an existing keyfile. */
    if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_KEEP_COMMENTS,
                                    &error))
    {
        g_debug ("Error loading window state during saving: %s",
                 error->message);
        g_clear_error (&error);
    }

    g_key_file_set_integer (keyfile, "EtApplicationWindow", "width",
                            priv->width);
    g_key_file_set_integer (keyfile, "EtApplicationWindow", "height",
                            priv->height);
    g_key_file_set_boolean (keyfile, "EtApplicationWindow", "is_maximized",
                            priv->is_maximized);

    if (priv->is_maximized)
    {
        gint width;

        /* Calculate the paned position based on the unmaximized window state,
         * as that is the state at which the window starts. */
        gtk_window_get_size (GTK_WINDOW (self), &width, NULL);
        priv->paned_position -= (width - priv->width);
    }

    g_key_file_set_integer (keyfile, "EtApplicationWindow", "paned_position",
                            priv->paned_position);

    et_browser_save_state(priv->browser, keyfile);

    if (!g_key_file_save_to_file (keyfile, path, &error))
    {
        g_warning ("Error saving window state: %s", error->message);
        g_error_free (error);
    }

    g_free (path);
    g_key_file_free (keyfile);
}

static void
restore_state (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;
    GtkWindow *window;
    GKeyFile *keyfile;
    gchar *path;
    GError *error = NULL;

    priv = et_application_window_get_instance_private (self);
    window = GTK_WINDOW (self);

    keyfile = g_key_file_new ();
    path = g_build_filename (g_get_user_cache_dir (), PACKAGE_TARNAME,
                             "state", NULL);

    if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_KEEP_COMMENTS,
                                    &error))
    {
        g_debug ("Error loading window state: %s", error->message);
        g_error_free (error);
        g_key_file_free (keyfile);
        g_free (path);
        return;
    }

    g_free (path);

    /* Ignore errors, as the default values are fine. */
    priv->width = g_key_file_get_integer (keyfile, "EtApplicationWindow",
                                          "width", NULL);
    priv->height = g_key_file_get_integer (keyfile, "EtApplicationWindow",
                                           "height", NULL);
    priv->is_maximized = g_key_file_get_boolean (keyfile,
                                                 "EtApplicationWindow",
                                                 "is_maximized", NULL);
    priv->paned_position = g_key_file_get_integer (keyfile,
                                                   "EtApplicationWindow",
                                                   "paned_position", NULL);

    gtk_window_set_default_size (window, priv->width, priv->height);

    /* Only set the unmaximized position, as the maximized position should only
     * be set after the window manager has maximized the window. */
    gtk_paned_set_position (priv->hpaned, priv->paned_position);

    if (priv->is_maximized)
    {
        gtk_window_maximize (window);
    }

    et_browser_restore_state(priv->browser, keyfile);

    g_key_file_free (keyfile);
}

static gboolean
on_configure_event (GtkWidget *window,
                    GdkEvent *event,
                    gpointer user_data)
{
    EtApplicationWindow *self;
    EtApplicationWindowPrivate *priv;
    GdkEventConfigure *configure_event;

    self = ET_APPLICATION_WINDOW (window);
    priv = et_application_window_get_instance_private (self);
    configure_event = (GdkEventConfigure *)event;

    if (!priv->is_maximized)
    {
        priv->width = configure_event->width;
        priv->height = configure_event->height;
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
on_window_state_event (GtkWidget *window,
                       GdkEvent *event,
                       gpointer user_data)
{
    EtApplicationWindow *self;
    EtApplicationWindowPrivate *priv;
    GdkEventWindowState *state_event;

    self = ET_APPLICATION_WINDOW (window);
    priv = et_application_window_get_instance_private (self);
    state_event = (GdkEventWindowState *)event;

    if (state_event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED)
    {
        priv->is_maximized = (state_event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
    }

    return GDK_EVENT_PROPAGATE;
}

static void
on_paned_notify_position (EtApplicationWindow *self,
                          GParamSpec *pspec,
                          gpointer user_data)
{
    EtApplicationWindowPrivate *priv;

    priv = et_application_window_get_instance_private (self);

    priv->paned_position = gtk_paned_get_position (priv->hpaned);
}

static gboolean
et_application_window_tag_area_display_et_file (EtApplicationWindow *self,
                                                const ET_File *ETFile, EtColumn columns)
{
    EtApplicationWindowPrivate *priv;

    g_return_val_if_fail (ET_APPLICATION_WINDOW (self), FALSE);

    priv = et_application_window_get_instance_private (self);

    return et_tag_area_display_et_file (ET_TAG_AREA (priv->tag_area), ETFile, columns);
}

static void
et_application_window_show_cddb_dialog (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;

    priv = et_application_window_get_instance_private (self);

    if (priv->cddb_dialog)
    {
        gtk_widget_show (priv->cddb_dialog);
    }
    else
    {
        priv->cddb_dialog = GTK_WIDGET (et_cddb_dialog_new ());
        gtk_widget_show_all (priv->cddb_dialog);
    }
}

/*
 * Delete the file ETFile
 */
static gint
delete_file (ET_File *ETFile, gboolean multiple_files, GError **error)
{
    GtkWidget *msgdialog;
    GtkWidget *msgdialog_check_button = NULL;
    gint response;

    g_return_val_if_fail (ETFile != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    /* Filename of the file to delete. */
    const char* basename_utf8 = ETFile->FileNameCur()->file().get();

    /*
     * Remove the file
     */
    if (g_settings_get_boolean (MainSettings, "confirm-delete-file")
        && !SF_HideMsgbox_Delete_File)
    {
        if (multiple_files)
        {
            GtkWidget *message_area;
            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_NONE,
                                               _("Do you really want to delete the file ‘%s’?"),
                                               basename_utf8);
            message_area = gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(msgdialog));
            msgdialog_check_button = gtk_check_button_new_with_label(_("Repeat action for the remaining files"));
            gtk_widget_show(GTK_WIDGET(msgdialog_check_button));
            gtk_container_add(GTK_CONTAINER(message_area),msgdialog_check_button);
            gtk_dialog_add_buttons (GTK_DIALOG (msgdialog), _("_Skip"),
                                    GTK_RESPONSE_NO, _("_Cancel"),
                                    GTK_RESPONSE_CANCEL, _("_Delete"),
                                    GTK_RESPONSE_YES, NULL);
            gtk_window_set_title(GTK_WINDOW(msgdialog),_("Delete File"));
            //GTK_TOGGLE_BUTTON(msgbox_check_button)->active = TRUE; // Checked by default
        }else
        {
            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_NONE,
                                               _("Do you really want to delete the file ‘%s’?"),
                                               basename_utf8);
            gtk_window_set_title(GTK_WINDOW(msgdialog),_("Delete File"));
            gtk_dialog_add_buttons (GTK_DIALOG (msgdialog), _("_Cancel"),
                                    GTK_RESPONSE_NO, _("_Delete"),
                                    GTK_RESPONSE_YES, NULL);
        }
        gtk_dialog_set_default_response (GTK_DIALOG (msgdialog),
                                         GTK_RESPONSE_YES);
        SF_ButtonPressed_Delete_File = response = gtk_dialog_run(GTK_DIALOG(msgdialog));
        if (msgdialog_check_button && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button)))
            SF_HideMsgbox_Delete_File = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(msgdialog_check_button));
        gtk_widget_destroy(msgdialog);
    }else
    {
        if (SF_HideMsgbox_Delete_File)
            response = SF_ButtonPressed_Delete_File;
        else
            response = GTK_RESPONSE_YES;
    }

    switch (response)
    {
        case GTK_RESPONSE_YES:
        {
            GFile *cur_file = g_file_new_for_path (ETFile->FilePath);

            if (g_file_delete (cur_file, NULL, error))
            {
                et_application_window_status_bar_message (MainWindow,
                    strprintf(_("File ‘%s’ deleted"), basename_utf8).c_str(), FALSE);
                g_object_unref (cur_file);
                g_assert (error == NULL || *error == NULL);
                return 1;
            }

            /* Error in deleting file. */
            g_assert (error == NULL || *error != NULL);
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

    return 0;
}

static void
on_delete (GSimpleAction *action,
           GVariant *variant,
           gpointer user_data)
{
    EtApplicationWindow *self;
    EtApplicationWindowPrivate *priv;
    gint   progress_bar_index = 0;
    gint   saving_answer;
    gint   nb_files_to_delete;
    gint   nb_files_deleted = 0;
    GError *error = NULL;

    g_return_if_fail(!ET_FileList::empty());

    self = ET_APPLICATION_WINDOW (user_data);
    priv = et_application_window_get_instance_private (self);

    /* Number of files to save */
    auto selection = priv->browser->get_current_files();
    nb_files_to_delete = selection.size();

    /* Initialize status bar */
    et_application_window_progress_set(self, 0, nb_files_to_delete);

    /* Set to unsensitive all command buttons (except Quit button) */
    et_application_window_disable_command_actions (self, FALSE);
    et_browser_set_sensitive(priv->browser, FALSE);
    self->displayed_file_sensitive(false);

    /* Show msgbox (if needed) to ask confirmation */
    SF_HideMsgbox_Delete_File = 0;

    for (auto& ETFile : selection)
    {
        et_browser_select_file_by_et_file(priv->browser, ETFile, FALSE);
        self->change_displayed_file(ETFile.get());

        et_application_window_progress_set(self, ++progress_bar_index, nb_files_to_delete);
        /* FIXME: Needed to refresh status bar and displayed file */
        while (gtk_events_pending ())
        {
            gtk_main_iteration ();
        }

        saving_answer = delete_file (ETFile.get(),
                                     nb_files_to_delete > 1 ? TRUE : FALSE,
                                     &error);

        switch (saving_answer)
        {
            case 1:
                nb_files_deleted += saving_answer;
                /* Remove file in the browser (corresponding line in the
                 * clist). */
                priv->browser->remove_file(ETFile);
                /* Remove file from file list. */
                ET_FileList::remove_file(ETFile.get());
                break;
            case 0:
                /* Distinguish between the file being skipped, and there being
                 * an error during deletion. */
                if (error)
                {
                    Log_Print (LOG_ERROR, _("Cannot delete file ‘%s’"),
                               error->message);
                    g_clear_error (&error);
                }
                break;
            case -1:
                /* Stop deleting files. */
                goto done;
            default:
                g_assert_not_reached ();
                break;
        }
    }

    {   const gchar *msg = nb_files_deleted < nb_files_to_delete
            ? _("Some files were not deleted")
            : _("All files have been deleted");
        et_application_window_status_bar_message (self, msg, TRUE);
    }

    /* It's important to displayed the new item, as it'll check the changes in et_browser_toggle_display_mode. */
    et_application_window_update_ui_from_et_file(self);

done:
    /* To update state of command buttons */
    et_application_window_update_actions (self);
    et_browser_set_sensitive(priv->browser, TRUE);
    self->displayed_file_sensitive(true);

    et_application_window_progress_set (self, 0, 0);
}

static void
on_undo_file_changes (GSimpleAction *action,
                      GVariant *variant,
                      gpointer user_data)
{
    EtApplicationWindow *self;

    g_return_if_fail(!ET_FileList::empty());

    self = ET_APPLICATION_WINDOW (user_data);
    EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);

    et_application_window_update_et_file_from_ui (self);

    bool state = false;
    for (auto& file : priv->browser->get_selected_files())
        state |= file->undo();

    /* Refresh the whole list (faster than file by file) to show changes. */
    et_browser_refresh_list(priv->browser);

    /* Display the current file */
    et_application_window_update_ui_from_et_file(self);
    et_application_window_update_actions (self);
}

static void
on_redo_file_changes (GSimpleAction *action,
                      GVariant *variant,
                      gpointer user_data)
{
    EtApplicationWindow *self;

    g_return_if_fail(!ET_FileList::empty());

    self = ET_APPLICATION_WINDOW (user_data);
    EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);

    et_application_window_update_et_file_from_ui (self);

    bool state = false;
    for (auto& file : priv->browser->get_selected_files())
        state |= file->redo();

    /* Refresh the whole list (faster than file by file) to show changes. */
    et_browser_refresh_list(priv->browser);

    /* Display the current file */
    et_application_window_update_ui_from_et_file(self);
    et_application_window_update_actions (self);
}

static void
on_save (GSimpleAction *action,
         GVariant *variant,
         gpointer user_data)
{
	Save_Selected_Files_With_Answer (FALSE);
}

static void
on_save_force (GSimpleAction *action,
               GVariant *variant,
               gpointer user_data)
{
	Save_Selected_Files_With_Answer (TRUE);
}

static void on_replaygain (GSimpleAction *action, GVariant *variant, gpointer user_data)
{
#ifdef ENABLE_REPLAYGAIN
	ReplayGain_For_Selected_Files();
#endif
}

static void
on_find (GSimpleAction *action,
         GVariant *variant,
         gpointer user_data)
{
    EtApplicationWindow *self;
    EtApplicationWindowPrivate *priv;

    self = ET_APPLICATION_WINDOW (user_data);
    priv = et_application_window_get_instance_private (self);

    if (priv->search_dialog)
    {
        gtk_widget_show (priv->search_dialog);
    }
    else
    {
        priv->search_dialog = GTK_WIDGET (et_search_dialog_new (GTK_WINDOW (self)));
        gtk_widget_show_all (priv->search_dialog);
    }
}

static void on_select_all(GSimpleAction* action, GVariant* variant, gpointer user_data)
{
	EtApplicationWindow* self = ET_APPLICATION_WINDOW(user_data);
	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);

	if (!priv->browser->popup_file())
	{
		/* Use the currently-focused widget and "select all" as appropriate.
		 * https://bugzilla.gnome.org/show_bug.cgi?id=697515 */
		GtkWidget* focused = gtk_window_get_focus(GTK_WINDOW(self));

		if (GTK_IS_EDITABLE(focused))
		{	gtk_editable_select_region (GTK_EDITABLE (focused), 0, -1);
			return;
		}
		else if (et_tag_area_select_all_if_focused(ET_TAG_AREA(priv->tag_area), focused))
			return;
		/* Assume that other widgets should select all in the file view. */
	}

	et_application_window_update_et_file_from_ui(self);
	priv->browser->select_all();
	et_application_window_update_actions(self);
}

static void on_unselect_all(GSimpleAction* action, GVariant* variant, gpointer user_data)
{
	EtApplicationWindow* self = ET_APPLICATION_WINDOW(user_data);
	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);

	if (!priv->browser->popup_file())
	{
		GtkWidget* focused = gtk_window_get_focus(GTK_WINDOW(self));

		if (GTK_IS_EDITABLE (focused))
		{
			GtkEditable* editable = GTK_EDITABLE(focused);
			gint pos = gtk_editable_get_position (editable);
			gtk_editable_select_region (editable, 0, 0);
			gtk_editable_set_position (editable, pos);
			return;
		}
		else if (et_tag_area_unselect_all_if_focused(ET_TAG_AREA(priv->tag_area), focused))
			return;
		/* Assume that other widgets should unselect all in the file view. */
	}

	self->change_displayed_file(nullptr);
	priv->browser->unselect_all();
}

static void
on_undo_last_changes (GSimpleAction *action,
                      GVariant *variant,
                      gpointer user_data)
{
    EtApplicationWindow *self;
    ET_File *ETFile;

    self = ET_APPLICATION_WINDOW (user_data);
    EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);

    ETFile = ET_File::global_undo();
    if (ETFile)
    {
        et_browser_select_file_by_et_file(priv->browser, ETFile, TRUE);
        et_browser_refresh_file_in_list(priv->browser, ETFile);
    }
}

static void
on_redo_last_changes (GSimpleAction *action,
                      GVariant *variant,
                      gpointer user_data)
{
    EtApplicationWindow *self;
    ET_File *ETFile;

    self = ET_APPLICATION_WINDOW (user_data);
    EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);

    ETFile = ET_File::global_redo();
    if (ETFile)
    {
        et_browser_select_file_by_et_file(priv->browser, ETFile, TRUE);
        et_browser_refresh_file_in_list(priv->browser, ETFile);
    }
}

static void
on_remove_tags (GSimpleAction *action,
                GVariant *variant,
                gpointer user_data)
{
    EtApplicationWindow *self;
    File_Tag *FileTag;
    gint progress_bar_index;
    gint selectcount;

    g_return_if_fail(!ET_FileList::empty());

    self = ET_APPLICATION_WINDOW (user_data);
    EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);

    et_application_window_update_et_file_from_ui (self);

    /* Initialize status bar */
    auto etfilelist = priv->browser->get_selected_files();
    selectcount = etfilelist.size();
    progress_bar_index = 0;
    et_application_window_progress_set(self, 0, selectcount);

    for (const xPtr<ET_File>& etfile : etfilelist)
    {
        FileTag = new File_Tag();
        etfile->apply_changes(nullptr, FileTag);

        et_application_window_progress_set(self, ++progress_bar_index, selectcount);
        /* Needed to refresh status bar */
        while (gtk_events_pending ())
            gtk_main_iteration ();
    }

    /* Refresh the whole list (faster than file by file) to show changes. */
    et_browser_refresh_list(priv->browser);

    /* Display the current file */
    et_application_window_update_ui_from_et_file(self);
    et_application_window_update_actions (self);

    et_application_window_progress_set(self, 0, 0);
    et_application_window_status_bar_message (self, _("All tags have been removed"), TRUE);
}

static void
on_preferences (GSimpleAction *action,
                GVariant *variant,
                gpointer user_data)
{
    EtApplicationWindow *self;
    EtApplicationWindowPrivate *priv;

    self = ET_APPLICATION_WINDOW (user_data);
    priv = et_application_window_get_instance_private (self);

    if (!priv->preferences_dialog)
        priv->preferences_dialog = GTK_WIDGET (et_preferences_dialog_new (GTK_WINDOW (self)));

    gtk_widget_show (priv->preferences_dialog);
}

static void
on_scanner_change (GSimpleAction *action,
                   GVariant *variant,
                   gpointer user_data)
{
    EtApplicationWindow *self;
    EtApplicationWindowPrivate *priv;
    gboolean active;

    self = ET_APPLICATION_WINDOW (user_data);
    priv = et_application_window_get_instance_private (self);
    active = g_variant_get_boolean (variant);

    if (!active)
    {
        if (priv->scan_dialog)
        {
            gtk_widget_hide (priv->scan_dialog);
        }
        else
        {
            return;
        }
    }
    else
    {
        if (priv->scan_dialog)
        {
            gtk_widget_show (priv->scan_dialog);
        }
        else
        {
            priv->scan_dialog = GTK_WIDGET (et_scan_dialog_new (GTK_WINDOW (self)));
            gtk_widget_show (priv->scan_dialog);
        }
    }

    g_simple_action_set_state (action, variant);
}

static void
on_file_artist_view_change (GSimpleAction *action,
                            GVariant *variant,
                            gpointer user_data)
{
    EtApplicationWindow* self = ET_APPLICATION_WINDOW(user_data);

    g_return_if_fail(!ET_FileList::empty());

    et_application_window_update_et_file_from_ui (self);

    g_simple_action_set_state (action, variant);

    et_application_window_browser_update_display_mode(self);

    et_application_window_update_actions (ET_APPLICATION_WINDOW (user_data));
}

static void
on_show_cddb (GSimpleAction *action,
              GVariant *variant,
              gpointer user_data)
{
    EtApplicationWindow *self;

    self = ET_APPLICATION_WINDOW (user_data);

    et_application_window_show_cddb_dialog (self);
}

static void
on_show_load_filenames (GSimpleAction *action,
                        GVariant *variant,
                        gpointer user_data)
{
    EtApplicationWindowPrivate *priv;
    EtApplicationWindow *self;

    self = ET_APPLICATION_WINDOW (user_data);
    priv = et_application_window_get_instance_private (self);

    if (priv->load_files_dialog)
    {
        gtk_widget_show (priv->load_files_dialog);
    }
    else
    {
        priv->load_files_dialog = GTK_WIDGET (et_load_files_dialog_new (GTK_WINDOW (self)));
        gtk_widget_show_all (priv->load_files_dialog);
    }
}

static void
on_show_playlist (GSimpleAction *action,
                  GVariant *variant,
                  gpointer user_data)
{
    EtApplicationWindowPrivate *priv;
    EtApplicationWindow *self;

    self = ET_APPLICATION_WINDOW (user_data);
    priv = et_application_window_get_instance_private (self);

    if (priv->playlist_dialog)
    {
        gtk_widget_show (priv->playlist_dialog);
    }
    else
    {
        priv->playlist_dialog = GTK_WIDGET (et_playlist_dialog_new (GTK_WINDOW (self)));
        gtk_widget_show_all (priv->playlist_dialog);
    }
}

void EtApplicationWindow::change_displayed_file(ET_File *etfile)
{
	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(this);

	// save changes to the previously visible file.
	if (priv->displayed_file)
	{	et_application_window_update_et_file_from_ui(this);
		xPtr<ET_File>::fromCptr(priv->displayed_file); // release instance
	}

	priv->displayed_file = xPtr<ET_File>::toCptr(etfile); // create reference

	// Display the item
	et_application_window_update_ui_from_et_file(this);

	et_application_window_update_actions(this);
	et_application_window_scan_dialog_update_previews(this);

	if (!g_settings_get_boolean(MainSettings, "tag-preserve-focus"))
		et_tag_area_title_grab_focus(ET_TAG_AREA(priv->tag_area));
}

/* Go to the first item of the list */
static void on_go_first(GSimpleAction *action, GVariant *variant, gpointer user_data)
{	EtApplicationWindow* self = ET_APPLICATION_WINDOW(user_data);
	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);
	self->change_displayed_file(priv->browser->select_first_file());
}

/* Go to the prev item of the list */
static void on_go_previous(GSimpleAction *action, GVariant *variant, gpointer user_data)
{	EtApplicationWindow* self = ET_APPLICATION_WINDOW(user_data);
	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);
	self->change_displayed_file(priv->browser->select_prev_file());
}

/* Go to the next item of the list */
static void on_go_next(GSimpleAction *action, GVariant *variant, gpointer user_data)
{	EtApplicationWindow* self = ET_APPLICATION_WINDOW(user_data);
	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);
	self->change_displayed_file(priv->browser->select_next_file());
}

/* Go to the last item of the list */
static void on_go_last(GSimpleAction *action, GVariant *variant, gpointer user_data)
{	EtApplicationWindow* self = ET_APPLICATION_WINDOW(user_data);
	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);
	self->change_displayed_file(priv->browser->select_last_file());
}

static void
on_show_cddb_selection (GSimpleAction *action,
                        GVariant *variant,
                        gpointer user_data)
{
    EtApplicationWindowPrivate *priv;
    EtApplicationWindow *self = ET_APPLICATION_WINDOW (user_data);

    priv = et_application_window_get_instance_private (self);

    et_application_window_show_cddb_dialog (self);
    et_cddb_dialog_search_from_selection (ET_CDDB_DIALOG (priv->cddb_dialog));
}

static void
on_clear_log (GSimpleAction *action,
              GVariant *variant,
              gpointer user_data)
{
    EtApplicationWindowPrivate *priv;
    EtApplicationWindow *self = ET_APPLICATION_WINDOW (user_data);

    priv = et_application_window_get_instance_private (self);

    et_log_area_clear (ET_LOG_AREA (priv->log_area));
}

static void
on_run_player_directory (GSimpleAction *action,
                         GVariant *variant,
                         gpointer user_data)
{
    auto range = ET_FileList::visible_range();
    et_run_audio_player(range.first, range.second);
}

static void
on_stop (GSimpleAction *action,
         GVariant *variant,
         gpointer user_data)
{
    Action_Main_Stop_Button_Pressed ();
}

static void on_fields_changed(EtApplicationWindow *self, const gchar *key, GSettings *settings)
{
	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);
	et_tag_area_update_controls(ET_TAG_AREA(priv->tag_area), priv->displayed_file);
}

template<void (EtBrowser::*F)()>
static void on_browser(GSimpleAction* action, GVariant* variant, gpointer user_data)
{	(et_application_window_get_instance_private(ET_APPLICATION_WINDOW(user_data))->browser->*F)();
}

template<GUserDirectory D>
static void on_browser_go_special(GSimpleAction* action, GVariant* variant, gpointer user_data)
{	et_application_window_get_instance_private(ET_APPLICATION_WINDOW(user_data))->browser->go_special(D);
}

static const GActionEntry actions[] =
{
	/* File menu. */
	{ "open-with", on_browser<&EtBrowser::show_open_files_with_dialog> },
	{ "run-player", on_browser<&EtBrowser::run_player_for_selection> },
	{ "delete", on_delete },
	{ "undo-file-changes", on_undo_file_changes },
	{ "redo-file-changes", on_redo_file_changes },
	{ "save", on_save },
	{ "save-force", on_save_force },
	/* Edit menu. */
	{ "find", on_find },
	{ "select-all", on_select_all },
	{ "unselect-all", on_unselect_all },
	{ "invert-selection", on_browser<&EtBrowser::invert_selection> },
	{ "undo-last-changes", on_undo_last_changes },
	{ "redo-last-changes", on_redo_last_changes },
	{ "remove-tags", on_remove_tags },
	{ "preferences", on_preferences },
	/* View menu. */
	{ "scanner", NULL, NULL, "false", on_scanner_change },
	/* { "scan-mode", on_action_radio, NULL, "false", on_scan_mode_change },
	 * Created from GSetting. */
	/* { "sort-order", on_action_radio, "s", "'filepath'",
	 * on_sort_order_change }, Created from GSetting */
	{ "file-artist-view", NULL, "s", "'file'", on_file_artist_view_change },
	{ "collapse-tree", on_browser<&EtBrowser::collapse> },
	/* { "show-log", on_show_log }, Created from GSetting */
	{ "reload-tree", on_browser<&EtBrowser::reload> },
	{ "reload-directory", on_browser<&EtBrowser::reload_directory> },
	/* { "browse-show-hidden", on_action_toggle, NULL, "true",
		on_browse_show_hidden_change }, Created from GSetting. */
	/* Browser menu. */
	{ "set-default-path", on_browser<&EtBrowser::set_current_path_default> },
	{ "rename-directory", on_browser<&EtBrowser::show_rename_directory_dialog> },
	{ "browse-directory", on_browser<&EtBrowser::show_open_directory_with_dialog> },
	/* { "browse-subdir", on_browse_subdir }, Created from GSetting. */
	/* Miscellaneous menu. */
	{ "show-cddb", on_show_cddb },
	{ "show-load-filenames", on_show_load_filenames },
	{ "show-playlist", on_show_playlist },
	{ "replaygain", on_replaygain },
	/* Go menu. */
	{ "go-home", on_browser<&EtBrowser::go_home> },
	{ "go-desktop", on_browser_go_special<G_USER_DIRECTORY_DESKTOP> },
	{ "go-documents", on_browser_go_special<G_USER_DIRECTORY_DOCUMENTS> },
	{ "go-downloads", on_browser_go_special<G_USER_DIRECTORY_DOWNLOAD> },
	{ "go-music", on_browser_go_special<G_USER_DIRECTORY_MUSIC> },
	{ "go-parent", on_browser<&EtBrowser::go_parent> },
	{ "go-default", on_browser<&EtBrowser::load_default_dir> },
	{ "go-first", on_go_first },
	{ "go-previous", on_go_previous },
	{ "go-next", on_go_next },
	{ "go-last", on_go_last },
	/* Popup menus. */
	{ "show-cddb-selection", on_show_cddb_selection },
	{ "clear-log", on_clear_log },
	{ "go-directory", on_browser<&EtBrowser::go_directory> },
	{ "run-player-album", on_browser<&EtBrowser::run_player_for_album_list> },
	{ "run-player-artist", on_browser<&EtBrowser::run_player_for_artist_list> },
	{ "run-player-directory", on_run_player_directory },
	/* Toolbar. */
	{ "stop", on_stop }
};

static void
et_application_window_destroy(GtkWidget *object)
{
    EtApplicationWindow *self;
    EtApplicationWindowPrivate *priv;

    self = ET_APPLICATION_WINDOW (object);
    priv = et_application_window_get_instance_private (self);

    // For some magic reason this event always fires twice.
    // => Ignore the 2nd call.
    if (!priv->browser)
        return;

    save_state (self);

    if (priv->cddb_dialog)
    {
        gtk_widget_destroy (priv->cddb_dialog);
        priv->cddb_dialog = NULL;
    }

    if (priv->load_files_dialog)
    {
        gtk_widget_destroy (priv->load_files_dialog);
        priv->load_files_dialog = NULL;
    }

    if (priv->playlist_dialog)
    {
        gtk_widget_destroy (priv->playlist_dialog);
        priv->playlist_dialog = NULL;
    }

    if (priv->preferences_dialog)
    {
        gtk_widget_destroy (priv->preferences_dialog);
        priv->preferences_dialog = NULL;
    }

    if (priv->scan_dialog)
    {
        gtk_widget_destroy (priv->scan_dialog);
        priv->scan_dialog = NULL;
    }

    if (priv->search_dialog)
    {
        gtk_widget_destroy (priv->search_dialog);
        priv->search_dialog = NULL;
    }

    if (priv->cursor)
    {
        g_object_unref (priv->cursor);
        priv->cursor = NULL;
    }

    if (priv->displayed_file)
    {
        xPtr<ET_File>::fromCptr(priv->displayed_file); // release instance
        priv->displayed_file = nullptr;
    }

    GTK_WIDGET_CLASS (et_application_window_parent_class)->destroy (object);

    priv->browser = NULL; // mark as destroyed
}

static void
et_application_window_init (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;
    GAction *action;
    GtkWindow *window;
    GtkWidget *main_vbox;
    GtkWidget *hbox;
    GtkWidget *grid;

    priv = et_application_window_get_instance_private (self);

    g_action_map_add_action_entries (G_ACTION_MAP (self), actions,
                                     G_N_ELEMENTS (actions), self);

    action = g_settings_create_action (MainSettings, "log-show");
    g_action_map_add_action (G_ACTION_MAP (self), action);
    g_object_unref (action);
    action = g_settings_create_action (MainSettings, "browse-show-hidden");
    g_action_map_add_action (G_ACTION_MAP (self), action);
    g_object_unref (action);
    action = g_settings_create_action (MainSettings, "browse-subdir");
    g_action_map_add_action (G_ACTION_MAP (self), action);
    g_object_unref (action);
    action = g_settings_create_action (MainSettings, "scan-mode");
    g_action_map_add_action (G_ACTION_MAP (self), action);
    g_object_unref (action);
    action = g_settings_create_action (MainSettings, "sort-order");
    g_action_map_add_action (G_ACTION_MAP (self), action);
    g_object_unref (action);
    action = g_settings_create_action (MainSettings, "sort-descending");
    g_action_map_add_action (G_ACTION_MAP (self), action);
    g_object_unref (action);

    action = g_settings_create_action (MainSettings, "visible-columns");
    et_flags_action_add_all_values(G_ACTION_MAP (self), action);
    g_object_unref (action);

    window = GTK_WINDOW (self);

    gtk_window_set_icon_name (window, "org.gnome.EasyTAG");
    gtk_window_set_title (window, _(PACKAGE_NAME));

    g_signal_connect (self, "configure-event",
                      G_CALLBACK (on_configure_event), NULL);
    g_signal_connect (self, "delete-event",
                      G_CALLBACK (on_main_window_delete_event), NULL);
    g_signal_connect (self, "window-state-event",
                      G_CALLBACK (on_window_state_event), NULL);

    /* Mainvbox for Menu bar + Tool bar + "Browser Area & FileArea & TagArea" + Log Area + "Status bar & Progress bar" */
    main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (self), main_vbox);

    /* Menu bar and tool bar. */
    GtkBuilder *builder = gtk_builder_new_from_resource ("/org/gnome/EasyTAG/toolbar.ui");
    GtkWidget *toolbar = GTK_WIDGET (gtk_builder_get_object (builder, "main_toolbar"));

    gtk_box_pack_start (GTK_BOX (main_vbox), toolbar, FALSE, FALSE, 0);

    /* The two panes: BrowserArea on the left, FileArea+TagArea on the right */
    priv->hpaned = GTK_PANED(gtk_paned_new (GTK_ORIENTATION_HORIZONTAL));

    /* Browser (Tree + File list + Entry) */
    priv->browser = et_browser_new ();
    gtk_paned_pack1 (priv->hpaned, GTK_WIDGET(priv->browser), TRUE, TRUE);

    /* Vertical box for FileArea + TagArea */
    grid = gtk_grid_new ();
    gtk_orientable_set_orientation (GTK_ORIENTABLE (grid),
                                    GTK_ORIENTATION_VERTICAL);
    gtk_paned_pack2 (priv->hpaned, grid, FALSE, FALSE);

    /* File */
    priv->file_area = et_file_area_new ();
    gtk_container_add (GTK_CONTAINER (grid), priv->file_area);

    /* Tag */
    priv->tag_area = et_tag_area_new ();
    gtk_container_add (GTK_CONTAINER (grid), priv->tag_area);

    restore_state (self);
    /* Update the stored paned position whenever it is changed (after configure
     * and windows state events have been processed). */
    g_signal_connect_swapped (priv->hpaned, "notify::position",
                              G_CALLBACK (on_paned_notify_position), self);

    /* Vertical pane for Browser Area + FileArea + TagArea */
    priv->vpaned = GTK_PANED (gtk_paned_new (GTK_ORIENTATION_VERTICAL));
    gtk_box_pack_start (GTK_BOX (main_vbox), GTK_WIDGET(priv->vpaned), TRUE, TRUE, 0);
    gtk_paned_pack1 (priv->vpaned, GTK_WIDGET(priv->hpaned), TRUE, FALSE);

    /* Log */
    priv->log_area = et_log_area_new ();
    gtk_paned_pack2 (priv->vpaned, priv->log_area, FALSE, FALSE);

    /* Horizontal box for Status bar + Progress bar */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show (hbox);

    /* Status bar */
    priv->status_bar = et_status_bar_new ();
    gtk_box_pack_start (GTK_BOX (hbox), priv->status_bar, TRUE, TRUE, 0);

    /* Progress bar */
    priv->progress_bar = et_progress_bar_new ();
    gtk_widget_hide(priv->progress_bar);
    gtk_box_pack_end (GTK_BOX (hbox), priv->progress_bar, FALSE, FALSE, 0);

    gtk_widget_show_all (GTK_WIDGET (main_vbox));

#ifndef ENABLE_REPLAYGAIN
    gtk_widget_hide(GTK_WIDGET (gtk_builder_get_object (builder, "replaygain_button")));
#endif
    g_object_unref (builder);

    /* Bind the setting after the log area has been shown, to avoid
     * force-enabling the visibility on startup. */
    g_settings_bind (MainSettings, "log-show", priv->log_area, "visible",
                     G_SETTINGS_BIND_DEFAULT);

    g_signal_connect_swapped(MainSettings, "changed::hide-fields", G_CALLBACK(on_fields_changed), self);
    g_signal_connect_swapped(MainSettings, "changed::id3v2-enabled", G_CALLBACK(on_fields_changed), self);
    g_signal_connect_swapped(MainSettings, "changed::id3v2-version-4", G_CALLBACK(on_fields_changed), self);
}

static void
et_application_window_class_init (EtApplicationWindowClass *klass)
{
	GTK_WIDGET_CLASS (klass)->destroy = et_application_window_destroy;
}

/*
 * et_application_window_new:
 *
 * Create a new EtApplicationWindow instance.
 *
 * Returns: a new #EtApplicationWindow
 */
EtApplicationWindow *
et_application_window_new (GtkApplication *application)
{
    g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

    return (EtApplicationWindow*)g_object_new (ET_TYPE_APPLICATION_WINDOW, "application", application, NULL);
}

void
et_application_window_scan_dialog_update_previews (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;

    g_return_if_fail (self != NULL);

    priv = et_application_window_get_instance_private (self);

    if (priv->scan_dialog)
    {
        et_scan_dialog_update_previews (ET_SCAN_DIALOG (priv->scan_dialog));
    }
}

void et_application_window_progress_set(EtApplicationWindow *self, gint current, gint total, double fraction)
{
	g_return_if_fail(ET_APPLICATION_WINDOW(self));
	EtApplicationWindowPrivate *priv = et_application_window_get_instance_private(self);
	GtkProgressBar* progress_bar = GTK_PROGRESS_BAR(priv->progress_bar);

	if (fraction < 0)
		fraction = (double)current/total;
	if (fraction >= 0 && fraction <= 1) // valid progress
	{	gtk_progress_bar_set_fraction(progress_bar, fraction);
		gtk_progress_bar_set_text(progress_bar, strprintf("%d/%d", current, total).c_str());
		gtk_widget_show(priv->progress_bar);
	} else
		gtk_widget_hide(priv->progress_bar);
}

void
et_application_window_status_bar_message (EtApplicationWindow *self,
                                          const gchar *message,
                                          gboolean with_timer)
{
    EtApplicationWindowPrivate *priv;

    g_return_if_fail (ET_APPLICATION_WINDOW (self));

    priv = et_application_window_get_instance_private (self);

    et_status_bar_message (ET_STATUS_BAR (priv->status_bar), message,
                           with_timer);
}

GtkWidget *
et_application_window_get_log_area (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;

    g_return_val_if_fail (self != NULL, NULL);

    priv = et_application_window_get_instance_private (self);

    return priv->log_area;
}

void
et_application_window_show_preferences_dialog_scanner (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;

    g_return_if_fail (ET_APPLICATION_WINDOW (self));

    priv = et_application_window_get_instance_private (self);

    if (!priv->preferences_dialog)
    {
        priv->preferences_dialog = GTK_WIDGET (et_preferences_dialog_new (GTK_WINDOW (self)));
    }

    et_preferences_dialog_show_scanner (ET_PREFERENCES_DIALOG (priv->preferences_dialog));
}

void
et_application_window_browser_update_display_mode (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;
    GVariant *variant;

    priv = et_application_window_get_instance_private (self);

    variant = g_action_group_get_action_state (G_ACTION_GROUP (self),
                                               "file-artist-view");

    if (strcmp (g_variant_get_string (variant, NULL), "file") == 0)
    {
        priv->browser->set_display_mode(ET_BROWSER_MODE_FILE);
    }
    else if (strcmp (g_variant_get_string (variant, NULL), "artist") == 0)
    {
        priv->browser->set_display_mode(ET_BROWSER_MODE_ARTIST);
    }
    else
    {
        g_assert_not_reached ();
    }

    g_variant_unref (variant);
}

void
et_application_window_search_dialog_clear (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;

    g_return_if_fail (ET_APPLICATION_WINDOW (self));

    priv = et_application_window_get_instance_private (self);

    g_return_if_fail (priv->browser != NULL);

    if (priv->search_dialog)
        et_search_dialog_clear (ET_SEARCH_DIALOG(priv->search_dialog));
}

/*
 * Select a file in the "main list" using the ETFile address of each item.
 */
void
et_application_window_select_file_by_et_file (EtApplicationWindow *self,
                                              ET_File *ETFile)
{
	if (ET_FileList::empty())
		return;

	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);
	/* Display the item */
	et_browser_select_file_by_et_file(priv->browser, ETFile, TRUE);
	/* select the file */
	self->change_displayed_file(ETFile);
}

/*
 * Save displayed filename into list if it had been changed. Generates also an history list for undo/redo.
 *  - ETFile : the current etfile that we want to save,
 *  - return : 'temporary' with the new filename or NULL if no change.
 *
 * Note : it builds new filename (with path) from strings encoded into file system
 *        encoding, not UTF-8 (it preserves file system encoding of parent directories).
 */
static File_Name*
et_application_window_create_file_name_from_ui (EtApplicationWindow *self,
                                                const ET_File *ETFile)
{
    g_return_val_if_fail(ETFile != NULL, nullptr);

    const gchar *filename_utf8 = et_application_window_file_area_get_filename (self);
    if (et_str_empty(filename_utf8))
    {
        /* Keep the 'last' filename (if a 'blank' filename was entered in the
         * fileentry for example). */
        return nullptr;
    }

    gString filename(filename_from_display(filename_utf8));
    if (!filename)
    {
        /* If conversion fails... */
        GtkWidget *msgdialog;
        gchar *filename_escaped_utf8 = g_strescape(filename_utf8, NULL);
        msgdialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            _("Could not convert filename ‘%s’ to system filename encoding"),
                                            filename_escaped_utf8);
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msgdialog),
                                                  _("Try setting the environment variable G_FILENAME_ENCODING."));
        gtk_window_set_title (GTK_WINDOW (msgdialog),
                              _("Filename translation"));

        gtk_dialog_run (GTK_DIALOG (msgdialog));
        gtk_widget_destroy (msgdialog);
        g_free (filename_escaped_utf8);
        return nullptr;
    }

    /* Regenerate the new filename (without path). */
    string filename_new(filename_utf8);
    File_Name::prepare_func((EtFilenameReplaceMode)g_settings_get_enum(MainSettings, "rename-replace-illegal-chars"),
        (EtConvertSpaces)g_settings_get_enum(MainSettings, "rename-convert-spaces"))(filename_new, 0);

    return new File_Name(ETFile->FileNameNew()->generate_name(filename_new.c_str(), true));
}

void
et_application_window_update_et_file_from_ui (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);

    ET_File* et_file = priv->displayed_file;
    if (!et_file)
        return;

    g_return_if_fail (et_file != NULL && et_file->FileNameCur() != NULL
                      && et_file->FileTagCur() != NULL);

    /* Save filename and generate undo for filename. */
    File_Name* FileName = et_application_window_create_file_name_from_ui(self, et_file);

    File_Tag* fileTag = new File_Tag(*et_file->FileTagNew()); // clone
    et_tag_area_store_file_tag(ET_TAG_AREA(priv->tag_area), fileTag);

    /*
     * Generate undo for the file and the main undo list.
     * If no changes detected, FileName and FileTag item are deleted.
     */
    if (et_file->apply_changes(FileName, fileTag))
        /* Refresh file into browser list */
        et_browser_refresh_file_in_list(priv->browser, et_file);
}

static void
et_application_window_display_file_name (EtApplicationWindow *self,
                                         const ET_File *ETFile)
{
	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);
	g_return_if_fail (ETFile != NULL);

	// Set the path to the file into BrowserEntry (dirbrowser)
	const xStringD0& dirname_utf8 = ETFile->FileNameNew()->path();
	et_browser_entry_set_text(priv->browser, dirname_utf8);

	// And refresh the number of files in this directory
	unsigned n_files = 0;
	for (const ET_File* file : ET_FileList::all_files())
		if (file->FileNameNew()->path() == dirname_utf8)
			++n_files;
	string text = strprintf(ngettext("One file", "%u files", n_files), n_files);
	et_browser_label_set_text(priv->browser, text.c_str());
}

/*
 * "Default" way to display File Info to the user interface.
 */
static void
et_header_fields_new_default (EtFileHeaderFields *fields, const ET_File *ETFile)
{
    const ET_File_Info *info = &ETFile->ETFileInfo;

    fields->description = ETFile->ETFileDescription->FileType;

    /* Bitrate */
    fields->bitrate = strprintf(info->variable_bitrate ? _("~%d kb/s") : _("%d kb/s"), (info->bitrate + 500) / 1000);

    /* Samplerate */
    fields->samplerate = strprintf(_("%d Hz"), info->samplerate);

    /* Mode */
    fields->mode = strprintf("%d", info->mode);

    /* Size */
    fields->size = strprintf("%s (%s)",
        gString(g_format_size(ETFile->FileSize)).get(),
        gString(g_format_size(ET_FileList::visible_total_bytes())).get());

    /* Duration */
    fields->duration = strprintf("%s (%s)",
        Convert_Duration(info->duration).c_str(),
        Convert_Duration(ET_FileList::visible_total_duration()).c_str());
}

/*
 * Display information of the file (Position + Header + Tag) to the user interface.
 */
void et_application_window_update_ui_from_et_file(EtApplicationWindow *self, EtColumn columns)
{
		EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);

    if (!priv->displayed_file || !priv->displayed_file->FileNameCur()) // For the case where ETFile is an "empty" structure.
    {	// Reinit the tag and file area
    	et_file_area_clear(ET_FILE_AREA(priv->file_area));
    	et_tag_area_clear(ET_TAG_AREA(priv->tag_area));
    	return;
    }

    const ET_File_Description *description = priv->displayed_file->ETFileDescription;

    /* Display position in list + show/hide icon if file writable/read_only (cur_filename) */
    et_application_window_file_area_set_file_fields (self, priv->displayed_file);

    /* Display filename (and his path) (value in FileNameNew) */
    if (columns & ET_COLUMN_FILENAME)
        et_application_window_display_file_name (self, priv->displayed_file);

    /* Display tag data */
    et_application_window_tag_area_display_et_file (self, priv->displayed_file, columns);

    /* Display controls in tag area */
    et_tag_area_update_controls(ET_TAG_AREA(priv->tag_area), priv->displayed_file);

    /* Display file data, header data and file type */
    EtFileHeaderFields fields;
    et_header_fields_new_default(&fields, priv->displayed_file); // some defaults...

    if (description->display_file_info_to_ui)
        (*description->display_file_info_to_ui)(&fields, priv->displayed_file);

    et_file_area_set_header_fields(ET_FILE_AREA(priv->file_area), &fields);

    et_application_window_status_bar_message(self, strprintf(_("File: ‘%s’"), priv->displayed_file->FileNameCur()->full_name().get()).c_str(), FALSE);
}

GFile *
et_application_window_get_current_path (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;

    g_return_val_if_fail (ET_APPLICATION_WINDOW (self), NULL);

    priv = et_application_window_get_instance_private (self);

    return priv->browser->get_current_path();
}

const gchar*
et_application_window_get_current_path_name (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;

    g_return_val_if_fail (ET_APPLICATION_WINDOW (self), NULL);

    priv = et_application_window_get_instance_private (self);

    return priv->browser->get_current_path_name();
}

GtkWidget *
et_application_window_get_scan_dialog (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;

    g_return_val_if_fail (self != NULL, NULL);

    priv = et_application_window_get_instance_private (self);

    return priv->scan_dialog;
}

void
et_application_window_apply_changes (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;

    g_return_if_fail (ET_APPLICATION_WINDOW (self));

    priv = et_application_window_get_instance_private (self);

    if (priv->scan_dialog)
    {
        et_scan_dialog_apply_changes (ET_SCAN_DIALOG (priv->scan_dialog));
    }

    if (priv->search_dialog)
    {
        et_search_dialog_apply_changes (ET_SEARCH_DIALOG (priv->search_dialog));
    }
}

static const gchar *
et_application_window_file_area_get_filename (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;

    g_return_val_if_fail (ET_APPLICATION_WINDOW (self), NULL);

    priv = et_application_window_get_instance_private (self);

    return et_file_area_get_filename (ET_FILE_AREA (priv->file_area));
}

static void
et_application_window_file_area_set_file_fields (EtApplicationWindow *self,
                                                 const ET_File *ETFile)
{
    EtApplicationWindowPrivate *priv;

    g_return_if_fail (ET_APPLICATION_WINDOW (self));

    priv = et_application_window_get_instance_private (self);

    et_file_area_set_file_fields (ET_FILE_AREA (priv->file_area), ETFile);
}

void
et_application_set_action_state (EtApplicationWindow *self,
                  const gchar *action_name,
                  gboolean enabled)
{
    GSimpleAction *action;

    action = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (self),
                                                          action_name));

    if (action == NULL)
    {
        g_error ("Unable to find action '%s' in application window",
                 action_name);
    }

    g_simple_action_set_enabled (action, enabled);
}

void
et_application_window_disable_command_actions (EtApplicationWindow *self, gboolean allowStop)
{
    GtkDialog *dialog = GTK_DIALOG (et_application_window_get_scan_dialog (self));

    /* Scanner Window */
    if (dialog)
        gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_APPLY, FALSE);

    /* Tool bar buttons (the others are covered by the menu) */
    et_application_set_action_state (self, "stop", allowStop);

    /* "File" menu commands */
    et_application_set_action_state (self, "open-with", FALSE);
    et_application_set_action_state (self, "invert-selection", FALSE);
    et_application_set_action_state (self, "delete", FALSE);
    et_application_set_action_state (self, "go-first", FALSE);
    et_application_set_action_state (self, "go-previous", FALSE);
    et_application_set_action_state (self, "go-next", FALSE);
    et_application_set_action_state (self, "go-last", FALSE);
    et_application_set_action_state (self, "remove-tags", FALSE);
    et_application_set_action_state (self, "undo-file-changes", FALSE);
    et_application_set_action_state (self, "redo-file-changes", FALSE);
    et_application_set_action_state (self, "save", FALSE);
    et_application_set_action_state (self, "save-force", FALSE);
    et_application_set_action_state (self, "undo-last-changes", FALSE);
    et_application_set_action_state (self, "redo-last-changes", FALSE);
    et_application_set_action_state (self, "replaygain", FALSE);

    /* FIXME: "Scanner" menu commands */
    /*set_action_state (self, "scan-mode", FALSE);*/
}

void
et_application_window_update_actions (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);

    GtkDialog *dialog = GTK_DIALOG (et_application_window_get_scan_dialog (self));

    /* Tool bar buttons (the others are covered by the menu) */
    et_application_set_action_state (self, "stop", FALSE);

    if (ET_FileList::empty())
    {
        /* No file found */

        /* File and Tag frames */
        self->displayed_file_sensitive(false);

        /* Scanner Window */
        if (dialog)
            gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_APPLY, FALSE);

        /* Menu commands */
        et_application_set_action_state (self, "open-with", FALSE);
        et_application_set_action_state (self, "invert-selection", FALSE);
        et_application_set_action_state (self, "delete", FALSE);
        /* FIXME: set_action_state (self, "sort-mode", FALSE); */
        et_application_set_action_state (self, "go-previous", FALSE);
        et_application_set_action_state (self, "go-next", FALSE);
        et_application_set_action_state (self, "go-first", FALSE);
        et_application_set_action_state (self, "go-last", FALSE);
        et_application_set_action_state (self, "remove-tags", FALSE);
        et_application_set_action_state (self, "undo-file-changes", FALSE);
        et_application_set_action_state (self, "redo-file-changes", FALSE);
        et_application_set_action_state (self, "save", FALSE);
        et_application_set_action_state (self, "save-force", FALSE);
        et_application_set_action_state (self, "undo-last-changes", FALSE);
        et_application_set_action_state (self, "redo-last-changes", FALSE);
        et_application_set_action_state (self, "find", FALSE);
        et_application_set_action_state (self, "show-load-filenames", FALSE);
        et_application_set_action_state (self, "show-playlist", FALSE);
        et_application_set_action_state (self, "run-player", FALSE);
        et_application_set_action_state (self, "replaygain", FALSE);
        /* FIXME set_action_state (self, "scan-mode", FALSE);*/
        et_application_set_action_state (self, "file-artist-view", FALSE);

        return;
    }else
    {
        gboolean has_undo = FALSE;
        gboolean has_redo = FALSE;
        //gboolean has_to_save = FALSE;

        /* File and Tag frames */
        self->displayed_file_sensitive(true);

        /* Scanner Window */
        if (dialog)
            gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_APPLY, TRUE);

        /* Commands into menu */
        et_application_set_action_state (self, "open-with", TRUE);
        et_application_set_action_state (self, "invert-selection", TRUE);
        et_application_set_action_state (self, "delete", TRUE);
        /* FIXME set_action_state (self, "sort-mode", TRUE); */
        et_application_set_action_state (self, "remove-tags", TRUE);
        et_application_set_action_state (self, "find", TRUE);
        et_application_set_action_state (self, "show-load-filenames", TRUE);
        et_application_set_action_state (self, "show-playlist", TRUE);
        et_application_set_action_state (self, "run-player", TRUE);
        et_application_set_action_state (self, "replaygain", TRUE);
        /* FIXME set_action_state (self, "scan-mode", TRUE); */
        et_application_set_action_state (self, "file-artist-view", TRUE);

        /* Check if one of the selected files has undo or redo data */
        for (const xPtr<ET_File>& etfile : priv->browser->get_selected_files())
        {
            has_undo |= etfile->has_undo_data();
            has_redo |= etfile->has_redo_data();
            /* has_to_save |= et_file_check_saved (etfile); */
            if (has_undo && has_redo) // Useless to check the other files
                break;
        }

        /* Enable undo commands if there are undo data */
        et_application_set_action_state(self, "undo-file-changes", has_undo);

        /* Enable redo commands if there are redo data */
        et_application_set_action_state (self, "redo-file-changes", has_redo);

        /* Enable save file command if file has been changed */
        // Desactivated because problem with only one file in the list, as we can't change the selected file => can't mark file as changed
        /*if (has_to_save)
            ui_widget_set_sensitive(MENU_FILE, AM_SAVE, FALSE);
        else*/
            et_application_set_action_state (self, "save", TRUE);
        
        et_application_set_action_state (self, "save-force", TRUE);

        /* Enable undo command if there are data into main undo list (history list) */
        et_application_set_action_state (self, "undo-last-changes", ET_File::has_global_undo());

        /* Enable redo commands if there are data into main redo list (history list) */
        et_application_set_action_state (self, "redo-last-changes", ET_File::has_global_redo());

        {
            GVariant *variant;
            const gchar *state;

            variant = g_action_group_get_action_state (G_ACTION_GROUP (self),
                                                     "file-artist-view");

            state = g_variant_get_string (variant, NULL);

            if (strcmp (state, "artist") == 0)
            {
                et_application_set_action_state (self, "collapse-tree", FALSE);
                et_application_set_action_state (self, "reload-tree", FALSE);
            }
            else if (strcmp (state, "file") == 0)
            {
                et_application_set_action_state (self, "collapse-tree", TRUE);
                et_application_set_action_state (self, "reload-tree", TRUE);
            }
            else
            {
                g_assert_not_reached ();
            }

            g_variant_unref (variant);
        }
    }

    gboolean flag = priv->browser->has_prev();
    et_application_set_action_state(self, "go-previous", flag);
    et_application_set_action_state(self, "go-first", flag);

    flag = priv->browser->has_next();
    et_application_set_action_state(self, "go-next", flag);
    et_application_set_action_state(self, "go-last", flag);
}

void
et_application_window_set_busy_cursor (EtApplicationWindow *self)
{
    EtApplicationWindowPrivate *priv;

    g_return_if_fail (ET_APPLICATION_WINDOW (self));

    priv = et_application_window_get_instance_private (self);

    if (!priv->cursor)
    {
        priv->cursor = gdk_cursor_new_for_display (gdk_window_get_display (gtk_widget_get_window (GTK_WIDGET (self))),
                                                   GDK_WATCH);

    }

    gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (self)),
                           priv->cursor);
}

void
et_application_window_set_normal_cursor (EtApplicationWindow *self)
{
    g_return_if_fail (ET_APPLICATION_WINDOW (self));

    /* Back to standard cursor */
    gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (self)), NULL);
}

void
et_application_window_browser_unselect_all (EtApplicationWindow *self)
{
	EtApplicationWindowPrivate* priv = et_application_window_get_instance_private(self);

	priv->browser->unselect_all();
	self->change_displayed_file(nullptr);
}

static void
quit_confirmed (EtApplicationWindow *self)
{
    /* Save the configuration when exiting. */
    et_application_window_apply_changes (self);
    
    /* Quit EasyTAG. */
    Log_Print (LOG_OK, _("Normal exit"));

    gtk_widget_destroy (GTK_WIDGET (self));
}

static void
save_and_quit (EtApplicationWindow *self)
{
    /* Save modified tags */
    if (Save_All_Files_With_Answer (FALSE) == -1)
        return;

    quit_confirmed (self);
}

void
et_application_window_quit (EtApplicationWindow *self)
{
    GtkWidget *msgbox;
    gint response;

    /* If you change the displayed data and quit immediately */
    et_application_window_update_et_file_from_ui (self);

    /* Check if all files have been saved before exit */
    if (g_settings_get_boolean (MainSettings, "confirm-when-unsaved-files")
        && !ET_FileList::check_all_saved())
    {
        /* Some files haven't been saved */
        msgbox = gtk_message_dialog_new (GTK_WINDOW (self),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_QUESTION,
                                         GTK_BUTTONS_NONE,
                                         "%s",
                                         _("Some files have been modified but not saved"));
        gtk_dialog_add_buttons (GTK_DIALOG (msgbox), _("_Discard"),
                                GTK_RESPONSE_NO, _("_Cancel"),
                                GTK_RESPONSE_CANCEL, _("_Save"),
                                GTK_RESPONSE_YES, NULL);
        gtk_dialog_set_default_response (GTK_DIALOG (msgbox),
                                         GTK_RESPONSE_YES);
        gtk_window_set_title (GTK_WINDOW (msgbox), _("Quit"));
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msgbox),
                                                  "%s",
                                                  _("Do you want to save them before quitting?"));
        response = gtk_dialog_run (GTK_DIALOG (msgbox));
        gtk_widget_destroy (msgbox);

        switch (response)
        {
            case GTK_RESPONSE_YES:
                save_and_quit (self);
                break;
            case GTK_RESPONSE_NO:
                quit_confirmed (self);
                break;
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
                return;
                break;
            default:
                g_assert_not_reached ();
                break;
        }

    }
    else
    {
        quit_confirmed (self);
    }
}
