/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022-2025  Marcel Müller <github@maazl.de>
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

	GFile* get_current_path();
	const gchar* get_current_path_name();

	/// Return all selected files in the currently visible order.
	std::vector<xPtr<ET_File>> get_selected_files();
	/// Return current files in the visible order.
	/// @remarks This function is recommended for context menu actions.
	/// @remarks The current files are the same as the selected files
	/// unless an emphasis for a context menu source has been set to a file that is NOT selected.
	/// Otherwise only the file with the emphasis is returned.\n
	/// The emphasis currently uses drag target emphasis because GTK TreeView
	/// does not support setting the cursor without discarding the selection.
	std::vector<xPtr<ET_File>> get_current_files();
	/// Return all files in the currently visible order.
	std::vector<ET_File*> get_all_files();

	/// Return the focused file if a popup menu action is currently pending in the file view.
	ET_File* popup_file();

	/// Get previous and next file that matches a predicate.
	/// @return previous and next matching file or \c nullptr if no such file exists.
	/// @remarks The current implementation needs to scan the entire model.
	std::pair<ET_File*, ET_File*> prev_next_if(ET_File* file, bool (*predicate)(const ET_File*));

	// Actions ...

	/// Select the directory corresponding to the 'path' in the tree browser,
	/// but it doesn't read it! Check if path is correct before selecting it.
	void select_dir(gString&& path);
	void select_dir(const char* path) { select_dir(gString(g_strdup(path))); }
	void select_dir(GFile *file) { select_dir(gString(g_file_get_path(file))); }
	void go_home() { select_dir(g_get_home_dir()); }
	/// Load predefined user directory
	void go_special(GUserDirectory dir) { select_dir(g_get_user_special_dir(dir)); }
	void go_parent();
	void go_directory();
	void load_default_dir();
	void set_current_path_default();

	void reload_directory();

	void set_display_mode(EtBrowserMode mode);

	void show_open_directory_with_dialog();
	void show_rename_directory_dialog();

	void show_open_files_with_dialog();

	void run_player_for_album_list();
	void run_player_for_artist_list();
	void run_player_for_selection();

	void remove_file(const ET_File *ETFile);

	void select_all();
	void unselect_all();
	void invert_selection();

	void collapse();

	void reload();
};

struct _EtBrowserClass
{
    /*< private >*/
    GtkBinClass parent_class;
};

GType et_browser_get_type (void);
EtBrowser *et_browser_new (void);

enum EtBrowserMode : int
{
    ET_BROWSER_MODE_FILE,
    ET_BROWSER_MODE_ARTIST,
    ET_BROWSER_MODE_ARTIST_ALBUM
};

void et_browser_set_sensitive (EtBrowser *self, gboolean sensitive);

void et_browser_refresh_list (EtBrowser *self);
void et_browser_refresh_file_in_list (EtBrowser *self, const ET_File *ETFile);

void et_browser_select_file_by_et_file (EtBrowser *self, const ET_File *ETFile, gboolean select_it);
GtkTreePath * et_browser_select_file_by_et_file2 (EtBrowser *self, const ET_File *searchETFile, gboolean select_it, GtkTreePath *startPath);
void et_browser_select_file_by_iter_string (EtBrowser *self, const gchar* stringiter, gboolean select_it);
ET_File *et_browser_select_file_by_dlm (EtBrowser *self, const gchar* string, gboolean select_it);

void et_browser_entry_set_text (EtBrowser *self, const gchar *text);
void et_browser_label_set_text (EtBrowser *self, const gchar *text);

void et_browser_save_state(EtBrowser *self, GKeyFile* keyfile);
void et_browser_restore_state(EtBrowser *self, GKeyFile* keyfile);

#endif /* ET_BROWSER_H_ */
