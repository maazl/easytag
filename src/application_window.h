/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022-2025  Marcel Müller <github@maazl.de>
 * Copyright (C) 2013-2015  David King <amigadave@amigadave.com>
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

#ifndef ET_APPLICATION_WINDOW_H_
#define ET_APPLICATION_WINDOW_H_

#include <gtk/gtk.h>

#include "file.h"

struct _EtBrowser;

#define ET_TYPE_APPLICATION_WINDOW (et_application_window_get_type ())
#define ET_APPLICATION_WINDOW(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), ET_TYPE_APPLICATION_WINDOW, EtApplicationWindow))

typedef struct _EtApplicationWindow EtApplicationWindow;
typedef struct _EtApplicationWindowClass EtApplicationWindowClass;

struct _EtApplicationWindow
{
    /*< private >*/
    GtkApplicationWindow parent_instance;

    /// Access the browser instance.
    struct _EtBrowser* browser();

    /// Get the file currently displayed in tag file/area or `nullptr` if none.
    ET_File* get_displayed_file();
    /// Replaces the currently edited file.
    /// @param etfile New file to show. `nullptr` to clear the controls.
    /// @details Any changes to the previous file are applied first.
    /// @remarks This function is dedicated to the browser.
    /// It does not change the browser's selection.
    void change_displayed_file(ET_File* etfile);

    /// Disable (`false`) / enable (`true`) all user widgets related to the currently displayed file.
    void displayed_file_sensitive(bool sensitive);
};

struct _EtApplicationWindowClass
{
    /*< private >*/
    GtkApplicationWindowClass parent_class;
};

GType et_application_window_get_type (void);
EtApplicationWindow *et_application_window_new (GtkApplication *application);

void et_application_set_action_state(EtApplicationWindow *self, const gchar *action_name, gboolean enabled);
/** Disable buttons when saving files (do not disable Quit button). */
void et_application_window_disable_command_actions (EtApplicationWindow *self, gboolean allowStop);
/** Set to sensitive/unsensitive the state of each button into
 * the commands area and menu items in function of state of the "main list". */
void et_application_window_update_actions (EtApplicationWindow *self);

// TODO: unused?
void et_application_window_set_busy_cursor (EtApplicationWindow *self);
void et_application_window_set_normal_cursor (EtApplicationWindow *self);

GtkWidget * et_application_window_get_log_area (EtApplicationWindow *self);
void et_application_window_show_preferences_dialog_scanner (EtApplicationWindow *self);
void et_application_window_browser_update_display_mode (EtApplicationWindow *self);
void et_application_window_search_dialog_clear (EtApplicationWindow *self);
void et_application_window_select_file_by_et_file (EtApplicationWindow *self, ET_File *ETFile);
GFile * et_application_window_get_current_path (EtApplicationWindow *self);
const gchar* et_application_window_get_current_path_name (EtApplicationWindow *self);
GtkWidget * et_application_window_get_scan_dialog (EtApplicationWindow *self);
void et_application_window_apply_changes (EtApplicationWindow *self);
/// Ssve changes in tag area to global file list.
void et_application_window_update_et_file_from_ui (EtApplicationWindow *self);
/// Update tag area from current file.
/// @param columns Only update this fields.
void et_application_window_update_ui_from_et_file (EtApplicationWindow *self, EtColumn columns = (EtColumn)~0);
void et_application_window_browser_unselect_all (EtApplicationWindow *self);

void et_application_window_scan_dialog_update_previews (EtApplicationWindow *self);
/** Set progress bar value.
 * @param current current progress
 * @param total total prograss (for 100%)
 * @details If current/total is non not a number the progress bar becomes inactive,
 * e.g. if you pass -1 for total. */
void et_application_window_progress_set (EtApplicationWindow *self, gint current, gint total, double fraction = -1);
void et_application_window_status_bar_message (EtApplicationWindow *self, const gchar *message, gboolean with_timer);
void et_application_window_quit (EtApplicationWindow *self);

#endif /* !ET_APPLICATION_WINDOW_H_ */
