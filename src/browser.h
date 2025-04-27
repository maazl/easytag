/* EasyTAG - tag editor for audio files
 * Copyright (C) 2014-2015  David King <amigadave@amigadave.com>
 * Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
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

#ifndef ET_BROWSER_H_
#define ET_BROWSER_H_

#include <gtk/gtk.h>

#include "setting.h"
#include "file.h"

#include <vector>


struct File_Name;

#define ET_TYPE_BROWSER (et_browser_get_type ())
#define ET_BROWSER(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), ET_TYPE_BROWSER, EtBrowser))

typedef struct _EtBrowser EtBrowser;
typedef struct _EtBrowserClass EtBrowserClass;

struct _EtBrowser
{
	/*< private >*/
	GtkBin parent_instance;

private:
	bool has_file();
	ET_File* current_file();

public:
	void load_file_list();
	void clear();

	bool has_prev();
	bool has_next();
	ET_File* select_first_file();
	ET_File* select_last_file();
	ET_File* select_prev_file();
	ET_File* select_next_file();
};

struct _EtBrowserClass
{
    /*< private >*/
    GtkBinClass parent_class;
};

GType et_browser_get_type (void);
EtBrowser *et_browser_new (void);
void et_browser_show_open_directory_with_dialog (EtBrowser *self);
void et_browser_show_open_files_with_dialog (EtBrowser *self);
void et_browser_show_rename_directory_dialog (EtBrowser *self);

enum EtBrowserMode : int
{
    ET_BROWSER_MODE_FILE,
    ET_BROWSER_MODE_ARTIST,
    ET_BROWSER_MODE_ARTIST_ALBUM
};

void et_browser_select_dir (EtBrowser *self, GFile *file);
void et_browser_reload (EtBrowser *self);
void et_browser_collapse (EtBrowser *self);
void et_browser_set_sensitive (EtBrowser *self, gboolean sensitive);

void et_browser_refresh_list (EtBrowser *self);
void et_browser_refresh_file_in_list (EtBrowser *self, const ET_File *ETFile);

void et_browser_select_file_by_et_file (EtBrowser *self, const ET_File *ETFile, gboolean select_it);
GtkTreePath * et_browser_select_file_by_et_file2 (EtBrowser *self, const ET_File *searchETFile, gboolean select_it, GtkTreePath *startPath);
void et_browser_select_file_by_iter_string (EtBrowser *self, const gchar* stringiter, gboolean select_it);
ET_File *et_browser_select_file_by_dlm (EtBrowser *self, const gchar* string, gboolean select_it);
void et_browser_select_all (EtBrowser *self);
void et_browser_unselect_all (EtBrowser *self);
void et_browser_invert_selection (EtBrowser *self);
void et_browser_remove_file (EtBrowser *self, const ET_File *ETFile);

void et_browser_entry_set_text (EtBrowser *self, const gchar *text);
void et_browser_label_set_text (EtBrowser *self, const gchar *text);

void et_browser_set_display_mode (EtBrowser *self, EtBrowserMode mode);

void et_browser_go_home (EtBrowser *self);
void et_browser_go_desktop (EtBrowser *self);
void et_browser_go_documents (EtBrowser *self);
void et_browser_go_downloads (EtBrowser *self);
void et_browser_go_music (EtBrowser *self);
void et_browser_go_parent (EtBrowser *self);

void et_browser_run_player_for_album_list (EtBrowser *self);
void et_browser_run_player_for_artist_list (EtBrowser *self);
void et_browser_run_player_for_selection (EtBrowser *self);

void et_browser_load_default_dir (EtBrowser *self);
void et_browser_reload_directory (EtBrowser *self);
void et_browser_set_current_path_default (EtBrowser *self);
GFile * et_browser_get_current_path (EtBrowser *self);
const gchar* et_browser_get_current_path_name (EtBrowser *self);

void et_browser_save_state(EtBrowser *self, GKeyFile* keyfile);
void et_browser_restore_state(EtBrowser *self, GKeyFile* keyfile);

std::vector<xPtr<ET_File>> et_browser_get_selected_files(EtBrowser *self);

#endif /* ET_BROWSER_H_ */
