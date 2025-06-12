/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022-2025  Marcel Müller <github@maazl.de>
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
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

/* Some parts of this browser are taken from:
 * XMMS - Cross-platform multimedia player
 * Copyright (C) 1998-1999  Peter Alm, Mikael Alm, Olle Hallnas,
 * Thomas Nilsson and 4Front Technologies
 */

#include "config.h"

#include "browser.h"
#include "file_renderer.h"

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "application_window.h"
#include "charset.h"
#include "dlm.h"
#include "easytag.h"
#include "file_list.h"
#include "mask.h"
#include "log.h"
#include "misc.h"
#include "setting.h"
#include "enums.h"
#include "file_name.h"

#include "win32/win32dep.h"

#include <functional>
using namespace std;

typedef struct
{
    GtkWidget *files_label;
    GtkWidget *open_button;
    GtkPaned *browser_paned;

    GtkWidget *entry_combo;
    GtkListStore *entry_model;

    GtkWidget *directory_album_artist_notebook;

    GtkListStore *file_model;
    GtkTreeView *file_view;
    GtkWidget *file_menu;
    guint file_selected_handler;
    EtSortMode file_sort_mode;

    GtkWidget *album_view;
    GtkWidget *album_menu;
    GtkListStore *album_model;
    guint album_selected_handler;

    GtkWidget *artist_view;
    GtkWidget *artist_menu;
    GtkListStore *artist_model;
    guint artist_selected_handler;

    GtkWidget *directory_view; /* Tree of directories. */
    GtkWidget *directory_view_menu;
    GtkTreeStore *directory_model;

    /* directory icons */
    GIcon *folder_icon;
    GIcon *folder_open_icon;
    GIcon *folder_readonly_icon;
    GIcon *folder_open_readonly_icon;
    GIcon *folder_unreadable_icon;

    GtkListStore *run_program_model;

    GtkWidget *open_directory_with_dialog;
    GtkWidget *open_directory_with_combobox;

    GtkWidget *open_files_with_dialog;
    GtkWidget *open_files_with_combobox;

    /* The Rename Directory window. */
    GtkWidget *rename_directory_dialog;
    GtkWidget *rename_directory_entry;
    GtkWidget *rename_directory_mask_toggle;
    GtkWidget *rename_directory_mask_entry;
    GtkWidget *rename_directory_preview_label;

    GFile *current_path;
    gchar *current_path_name; ///< name of current_path in system encoding
    GtkTreeIter current_file; ///< The file currently visible in the file and tag area. Binary zero if none.

} EtBrowserPrivate;

// learn correct return type for et_browser_get_instance_private
#define et_browser_get_instance_private et_browser_get_instance_private_
G_DEFINE_TYPE_WITH_PRIVATE (EtBrowser, et_browser, GTK_TYPE_BIN)
#undef et_browser_get_instance_private
#define et_browser_get_instance_private(x) ((EtBrowserPrivate*)et_browser_get_instance_private_(x))

/*
 * EtPathState:
 * @ET_PATH_STATE_OPEN: the path is open or has been read
 * @ET_PATH_STATE_CLOSED: the path is closed or could not be read
 *
 * Whether to generate an icon with an indicaton that the directory is open
 * (being viewed) or closed (not yet viewed or read).
 */
enum EtPathState : char
{
    ET_PATH_STATE_OPEN,
    ET_PATH_STATE_CLOSED
};

enum // file_model
{
    LIST_FILE_POINTER
};

enum // album_model
{
    ALBUM_GICON,
    ALBUM_NAME,
    ALBUM_NUM_FILES,
    ALBUM_FONT_WEIGHT,
    ALBUM_ROW_FOREGROUND,
    ALBUM_STATE, // ALBUM_STATE_...
    ALBUM_COLUMN_COUNT
};

enum
{	ALBUM_STATE_NONE,
	ALBUM_STATE_ALL_ALBUMS,
	ALBUM_STATE_SEPARATOR,
};

enum // artist_model
{
    ARTIST_PIXBUF,
    ARTIST_NAME,
    ARTIST_NUM_ALBUMS,
    ARTIST_NUM_FILES,
    ARTIST_FONT_WEIGHT,
    ARTIST_ROW_FOREGROUND,
    ARTIST_COLUMN_COUNT
};

enum // directory_model
{
    TREE_COLUMN_DIR_NAME,
    TREE_COLUMN_FULL_PATH,
    TREE_COLUMN_SCANNED,
    TREE_COLUMN_HAS_SUBDIR,
    TREE_COLUMN_ICON,
    TREE_COLUMN_COUNT
};

static const constexpr GdkRGBA RED = {1.0, 0.0, 0.0, 1.0 };
static const constexpr GtkTreeIter invalid_iter = { 0 };


/**************
 * Prototypes *
 **************/

static void et_browser_clear_album_model(EtBrowser *self);
static void et_browser_clear_artist_model(EtBrowser *self);

static void Browser_Tree_Handle_Rename (EtBrowser *self,
                                        GtkTreeIter *parentnode,
                                        const gchar *old_path,
                                        const gchar *new_path);

static void Browser_List_Select_File_By_Iter (EtBrowser *self,
                                              GtkTreeIter *iter,
                                              gboolean select_it);

static void Browser_Artist_List_Row_Selected (EtBrowser *self,
                                              GtkTreeSelection *selection);
static void Browser_Artist_List_Set_Row_Appearance(EtBrowser *self, GtkTreeIter& iter, const xStringD0& artist);

static void Browser_Album_List_Load_Files (EtBrowser *self, ET_FileList::index_range_type range);
static void Browser_Album_List_Row_Selected (EtBrowser *self,
                                             GtkTreeSelection *selection);
static void Browser_Album_List_Set_Row_Appearance(EtBrowser *self, GtkTreeIter& iter, const xStringD0& artist);

static gboolean check_for_subdir(GFile *dir);

static GtkTreePath *Find_Child_Node (EtBrowser *self, GtkTreeIter *parent, gchar *searchtext);

static const GIcon* get_gicon_for_path(EtBrowser *self, const gchar *path, EtPathState path_state);

static GtkTreeViewColumn * et_browser_get_column_for_sort_mode (EtBrowser *self, EtSortMode sort_mode);

/* For window to rename a directory */
static void Destroy_Rename_Directory_Window (EtBrowser *self);
static void Rename_Directory_With_Mask_Toggled (EtBrowser *self);

/* For window to run a program with the directory */
static void Destroy_Run_Program_Tree_Window (EtBrowser *self);
static void Run_Program_With_Directory (EtBrowser *self);

/* For window to run a program with the file */
static void Destroy_Run_Program_List_Window (EtBrowser *self);

static void empty_entry_disable_widget (GtkWidget *widget, GtkEntry *entry);

static void et_browser_refresh_sort (EtBrowser *self);

static void et_rename_directory_on_response (GtkDialog *dialog,
                                             gint response_id,
                                             gpointer user_data);
static void et_run_program_tree_on_response (GtkDialog *dialog,
                                             gint response_id,
                                             gpointer user_data);
static void et_run_program_list_on_response (GtkDialog *dialog,
                                             gint response_id,
                                             gpointer user_data);

static void et_browser_on_column_clicked (GtkTreeViewColumn *column,
                                          gpointer data);


/*************
 * Functions *
 *************/

/*
 * Load home directory
 */
void
et_browser_go_home (EtBrowser *self)
{
    GFile *file;

    file = g_file_new_for_path (g_get_home_dir ());
    et_browser_select_dir (self, file);
    g_object_unref (file);
}

/*
 * Load desktop directory
 */
void
et_browser_go_desktop (EtBrowser *self)
{
    GFile *file;

    file = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));
    et_browser_select_dir (self, file);
    g_object_unref (file);
}

/*
 * Load documents directory
 */
void
et_browser_go_documents (EtBrowser *self)
{
    GFile *file;

    file = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS));
    et_browser_select_dir (self, file);
    g_object_unref (file);
}

/*
 * Load downloads directory
 */
void
et_browser_go_downloads (EtBrowser *self)
{
    GFile *file;

    file = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD));
    et_browser_select_dir (self, file);
    g_object_unref (file);
}

/*
 * Load music directory
 */
void
et_browser_go_music (EtBrowser *self)
{
    GFile *file;

    file = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_MUSIC));
    et_browser_select_dir (self, file);
    g_object_unref (file);
}


/*
 * Load default directory
 */
void
et_browser_load_default_dir (EtBrowser *self)
{
    GFile **files;
    GVariant *default_path;
    const gchar *path;

    default_path = g_settings_get_value (MainSettings, "default-path");
    path = g_variant_get_bytestring (default_path);

    files = g_new (GFile *, 1);
    files[0] = g_file_new_for_path (path);
    g_application_open (g_application_get_default (), files, 1, "");

    g_object_unref (files[0]);
    g_variant_unref (default_path);
    g_free (files);
}

void
et_browser_run_player_for_album_list (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    GtkTreeIter iter;
    GtkTreeSelection *selection;

    priv = et_browser_get_instance_private (self);
    g_return_if_fail (priv->album_view != NULL);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->album_view));
    if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
        return;

    gchar* album;
    gtk_tree_model_get(GTK_TREE_MODEL(priv->album_model), &iter, ALBUM_NAME, &album, -1);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->artist_view));
    if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
    {   g_free(album);
        return;
    }

    gchar* artist;
    gtk_tree_model_get(GTK_TREE_MODEL(priv->artist_model), &iter, ARTIST_NAME, &artist, -1);
    auto range = ET_FileList::to_file_range(ET_FileList::matching_range(xStringD0(artist), xStringD0(album)));
    g_free(artist);

    et_run_audio_player(range.first, range.second);
}

void
et_browser_run_player_for_artist_list (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    GtkTreeIter iter;
    GtkTreeSelection *selection;

    priv = et_browser_get_instance_private (self);
    g_return_if_fail (priv->artist_view != NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->artist_view));
    if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
        return;

    gchar* artist;
    gtk_tree_model_get(GTK_TREE_MODEL(priv->artist_model), &iter, ARTIST_NAME, &artist, -1);
    auto range = ET_FileList::to_file_range(ET_FileList::matching_range(xStringD0(artist)));
    g_free(artist);

    et_run_audio_player(range.first, range.second);
}

void
et_browser_run_player_for_selection (EtBrowser *self)
{
    auto files = et_browser_get_selected_files(self);
    et_run_audio_player(files.begin(), files.end());
}

/*
 * Get the path from the selected node (row) in the browser
 * Warning: return NULL if no row selected int the tree.
 * Remember to free the value returned from this function!
 */
static gchar *
Browser_Tree_Get_Path_Of_Selected_Node (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    GtkTreeSelection *selection;
    GtkTreeIter selectedIter;
    gchar *path;

    priv = et_browser_get_instance_private (self);

    g_return_val_if_fail (priv->directory_view != NULL, NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->directory_view));
    if (selection
    && gtk_tree_selection_get_selected(selection, NULL, &selectedIter))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(priv->directory_model), &selectedIter,
                           TREE_COLUMN_FULL_PATH, &path, -1);
        return path;
    }else
    {
        return NULL;
    }
}


/*
 * Set the current path to be shown in the browser.
 */
static void
et_browser_set_current_path (EtBrowser *self,
                             GFile *file)
{
    EtBrowserPrivate *priv;

    g_return_if_fail (file != NULL);

    priv = et_browser_get_instance_private (self);

    /* Ref the new file first, in case the current file is passed in. */
    g_object_ref (file);

    if (priv->current_path)
        g_object_unref (priv->current_path);
    g_free(priv->current_path_name);

    priv->current_path = file;
    priv->current_path_name = g_file_get_path(file);
}


/*
 * Return the current path
 */
GFile *
et_browser_get_current_path (EtBrowser *self)
{
    EtBrowserPrivate *priv;

    g_return_val_if_fail (ET_BROWSER (self), NULL);

    priv = et_browser_get_instance_private (self);

    return priv->current_path;
}

/*
 * Return the current path
 */
const gchar*
et_browser_get_current_path_name (EtBrowser *self)
{
    EtBrowserPrivate *priv;

    g_return_val_if_fail (ET_BROWSER (self), NULL);

    priv = et_browser_get_instance_private (self);

    return priv->current_path_name;
}

void et_browser_save_state(EtBrowser *self, GKeyFile* keyfile)
{
	EtBrowserPrivate *priv = et_browser_get_instance_private(self);
	// paned position
	g_key_file_set_integer(keyfile, "EtBrowser", "paned_position", gtk_paned_get_position(priv->browser_paned));
}

void et_browser_restore_state(EtBrowser *self, GKeyFile* keyfile)
{
	EtBrowserPrivate *priv = et_browser_get_instance_private(self);
	// paned position
	gint value = g_key_file_get_integer(keyfile, "EtBrowser", "paned_position", NULL);
	if (value)
		gtk_paned_set_position(priv->browser_paned, value);
}

/*
 * et_browser_get_selected_files:
 * @self: an #EtBrowser from which to get a list of selected files
 *
 * Get a list of #ET_File of the current selection of the #EtBrowser @self.
 *
 * Returns: list of the selected files in the browser
 */
vector<xPtr<ET_File>> et_browser_get_selected_files(EtBrowser *self)
{
    EtBrowserPrivate *priv;
    GtkTreeSelection *selection;
    GList *selfilelist;
    vector<xPtr<ET_File>> files;

    g_return_val_if_fail (ET_BROWSER (self), files);

    priv = et_browser_get_instance_private (self);
    selection = gtk_tree_view_get_selection (priv->file_view);
    selfilelist = gtk_tree_selection_get_selected_rows (selection, NULL);

    for (GList* l = selfilelist; l != NULL; l = g_list_next (l))
    {
        GtkTreeIter iter;
        if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->file_model), &iter, (GtkTreePath*)l->data))
            continue; // invalid selected path ???

        ET_File *etfile;
        gtk_tree_model_get(GTK_TREE_MODEL(priv->file_model), &iter, LIST_FILE_POINTER, &etfile, -1);
        files.emplace_back(etfile);
    }

    g_list_free_full (selfilelist, (GDestroyNotify)gtk_tree_path_free);

    return files;
}

vector<ET_File*> et_browser_get_all_files(EtBrowser *self)
{
	EtBrowserPrivate* priv = et_browser_get_instance_private (self);

	vector<ET_File*> files;
	files.reserve(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->file_model), NULL));

	gtk_tree_model_foreach(GTK_TREE_MODEL(priv->file_model),
		[](GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, gpointer data)
		{	ET_File* etfile;
			gtk_tree_model_get(model, iter, LIST_FILE_POINTER, &etfile, -1);
			((vector<xPtr<ET_File>>*)data)->emplace_back(etfile);
			return FALSE;
		}, &files);

	return files;
}

/*
 * Reload the current directory.
 */
void
et_browser_reload_directory (EtBrowser *self)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    if (priv->directory_view && priv->current_path != NULL)
    {
        /* Unselect files, to automatically reload the file of the directory. */
        GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->directory_view));

        if (selection)
        {
            gtk_tree_selection_unselect_all(selection);
        }

        et_browser_select_dir (self, priv->current_path);
    }
}

/*
 * Set the current path (selected node) in browser as default path (within config variable).
 */
void
et_browser_set_current_path_default (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    gchar *path;

    g_return_if_fail (ET_BROWSER (self));

    priv = et_browser_get_instance_private (self);

    path = g_file_get_path (priv->current_path);
    g_settings_set_value (MainSettings, "default-path",
                          g_variant_new_bytestring (path));
    g_free (path);

    et_application_window_status_bar_message(MainWindow,
        _("New default directory selected for browser"), TRUE);
}

/*
 * When you press the key 'enter' in the BrowserEntry to validate the text (browse the directory)
 */
static void
Browser_Entry_Activated (EtBrowser *self,
                         GtkEntry *entry)
{
    EtBrowserPrivate *priv;
    const gchar *parse_name;
    GFile *file;

    priv = et_browser_get_instance_private (self);

    parse_name = gtk_entry_get_text (entry);
    Add_String_To_Combo_List (GTK_LIST_STORE (priv->entry_model), parse_name);

    file = g_file_parse_name (parse_name);

    et_browser_select_dir (self, file);

    g_object_unref (file);
}

/*
 * Set a text into the BrowserEntry (and don't activate it)
 */
void
et_browser_entry_set_text (EtBrowser *self, const gchar *text)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    if (!text || !priv->entry_combo)
        return;

    gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->entry_combo))),
                        text);
}

/*
 * Button to go to parent directory
 */
void
et_browser_go_parent (EtBrowser *self)
{
    GFile *parent;

    parent = g_file_get_parent (et_browser_get_current_path (self));

    if (parent)
    {
        et_browser_select_dir (self, parent);
        g_object_unref (parent);
    }
    else
    {
        g_debug ("%s", "No parent found for current browser path");
    }
}

/*
 * Set a text into the priv->files_label
 */
void
et_browser_label_set_text (EtBrowser *self, const gchar *text)
{
    EtBrowserPrivate *priv;

    g_return_if_fail (ET_BROWSER (self));
    g_return_if_fail (text != NULL);

    priv = et_browser_get_instance_private (self);

    gtk_label_set_text (GTK_LABEL (priv->files_label), text);
}

/*
 * Key Press events into browser tree
 */
static gboolean
Browser_Tree_Key_Press (GtkWidget *tree, GdkEvent *event, gpointer data)
{
    GdkEventKey *kevent;
    GtkTreeIter SelectedNode;
    GtkTreeModel *treeModel;
    GtkTreeSelection *treeSelection;
    GtkTreePath *treePath;

    g_return_val_if_fail (tree != NULL, FALSE);

    treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));

    if (event && event->type==GDK_KEY_PRESS)
    {
        if (!gtk_tree_selection_get_selected(treeSelection, &treeModel, &SelectedNode))
            return FALSE;

        kevent = (GdkEventKey *)event;
        treePath = gtk_tree_model_get_path(GTK_TREE_MODEL(treeModel), &SelectedNode);

        switch(kevent->keyval)
        {
            case GDK_KEY_KP_Enter:    /* Enter key in Num Pad */
            case GDK_KEY_Return:      /* 'Normal' Enter key */
            case GDK_KEY_t:           /* Expand/Collapse node */
            case GDK_KEY_T:           /* Expand/Collapse node */
                if(gtk_tree_view_row_expanded(GTK_TREE_VIEW(tree), treePath))
                    gtk_tree_view_collapse_row(GTK_TREE_VIEW(tree), treePath);
                else
                    gtk_tree_view_expand_row(GTK_TREE_VIEW(tree), treePath, FALSE);

                gtk_tree_path_free(treePath);
                return TRUE;
                break;

            case GDK_KEY_e:           /* Expand node */
            case GDK_KEY_E:           /* Expand node */
                gtk_tree_view_expand_row(GTK_TREE_VIEW(tree), treePath, FALSE);
                gtk_tree_path_free(treePath);
                return TRUE;
                break;

            case GDK_KEY_c:           /* Collapse node */
            case GDK_KEY_C:           /* Collapse node */
                gtk_tree_view_collapse_row(GTK_TREE_VIEW(tree), treePath);
                gtk_tree_path_free(treePath);
                return TRUE;
                break;
            /* Ignore all other keypresses. */
            default:
                break;
        }
        gtk_tree_path_free(treePath);
    }
    return FALSE;
}

/*
 * Key press into browser list
 *   - Delete = delete file
 * Also tries to capture text input and relate it to files
 */
static gboolean
Browser_List_Key_Press (GtkWidget *list, GdkEvent *event, gpointer data)
{
    GdkEventKey *kevent;
    GtkTreeSelection *fileSelection;

    g_return_val_if_fail (list != NULL, FALSE);

    fileSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));

    kevent = (GdkEventKey *)event;
    if (event && event->type==GDK_KEY_PRESS)
    {
        if (gtk_tree_selection_count_selected_rows(fileSelection))
        {
            switch(kevent->keyval)
            {
                case GDK_KEY_Delete:
                    g_action_group_activate_action (G_ACTION_GROUP (MainWindow),
                                                    "delete", NULL);
                    return TRUE;
                /* Ignore all other keypresses. */
                default:
                    break;
            }
        }
    }

    return FALSE;
}

/*
 * Collapse (close) tree recursively up to the root node.
 */
void
et_browser_collapse (EtBrowser *self)
{
    EtBrowserPrivate *priv;
#ifndef G_OS_WIN32
    GtkTreePath *rootPath;
#endif /* !G_OS_WIN32 */

    priv = et_browser_get_instance_private (self);

    g_return_if_fail (priv->directory_view != NULL);

    gtk_tree_view_collapse_all (GTK_TREE_VIEW (priv->directory_view));

#ifndef G_OS_WIN32
    /* But keep the main directory opened */
    rootPath = gtk_tree_path_new_first();
    gtk_tree_view_expand_to_path (GTK_TREE_VIEW (priv->directory_view),
                                  rootPath);
    gtk_tree_path_free(rootPath);
#endif /* !G_OS_WIN32 */
}


/*
 * Set a row (or node) visible in the TreeView (by scrolling the tree)
 */
static void
Browser_Tree_Set_Node_Visible (GtkWidget *directoryView, GtkTreePath *path)
{
    g_return_if_fail (directoryView != NULL || path != NULL);

    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(directoryView), path, NULL, TRUE, 0.5, 0.0);
}


/*
 * Set a row visible in the file list (by scrolling the list)
 */
static void
et_browser_set_row_visible (EtBrowser *self, GtkTreeIter *rowIter)
{
    EtBrowserPrivate *priv;

    /*
     * TODO: Make this only scroll to the row if it is not visible
     * (like in easytag GTK1)
     * See function gtk_tree_view_get_visible_rect() ??
     */
    GtkTreePath *rowPath;

    priv = et_browser_get_instance_private (self);

    g_return_if_fail (rowIter != NULL);

    rowPath = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->file_model),
                                       rowIter);
    gtk_tree_view_scroll_to_cell (priv->file_view, rowPath, NULL, FALSE, 0, 0);
    gtk_tree_path_free (rowPath);
}

/*
 * Triggers when a new node in the browser tree is selected
 * Do file-save confirmation, and then prompt the new dir to be loaded
 */
static gboolean
Browser_Tree_Node_Selected (EtBrowser *self, GtkTreeSelection *selection)
{
    EtBrowserPrivate *priv;
    gchar *pathName;
    GFile *file;
    gchar *parse_name;
    static gboolean first_read = TRUE;
    GtkTreeIter selectedIter;
    GtkTreePath *selectedPath;

    priv = et_browser_get_instance_private (self);

    if (!gtk_tree_selection_get_selected(selection, NULL, &selectedIter))
        return TRUE;
    selectedPath = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->directory_model), &selectedIter);

    /* Open the node */
    if (g_settings_get_boolean (MainSettings, "browse-expand-children"))
    {
        gtk_tree_view_expand_row (GTK_TREE_VIEW (priv->directory_view),
                                  selectedPath, FALSE);
    }
    gtk_tree_path_free(selectedPath);

    /* Don't start a new reading, if another one is running... */
    if (IsReadingDirectory())
        return TRUE;

    /* Browser_Tree_Set_Node_Visible (priv->directory_view, selectedPath); */
    gtk_tree_model_get(GTK_TREE_MODEL(priv->directory_model), &selectedIter,
                       TREE_COLUMN_FULL_PATH, &pathName, -1);
    if (!pathName)
        return FALSE;

    et_application_window_update_et_file_from_ui(MainWindow);

    /* FIXME: Not clean to put this here. */
    et_application_window_update_actions(MainWindow);

    /* Check if all files have been saved before changing the directory */
    if (g_settings_get_boolean (MainSettings, "confirm-when-unsaved-files")
        && !ET_FileList::check_all_saved())
    {
        GtkWidget *msgdialog;
        gint response;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_QUESTION,
                                           GTK_BUTTONS_NONE,
                                           "%s",
                                           _("Some files have been modified but not saved"));
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msgdialog),
                                                  "%s",
                                                  _("Do you want to save them before changing directory?"));
        gtk_dialog_add_buttons (GTK_DIALOG (msgdialog), _("_Discard"),
                                GTK_RESPONSE_NO, _("_Cancel"),
                                GTK_RESPONSE_CANCEL, _("_Save"),
                                GTK_RESPONSE_YES, NULL);
        gtk_dialog_set_default_response (GTK_DIALOG (msgdialog),
                                         GTK_RESPONSE_YES);
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Confirm Directory Change"));

        response = gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
        switch (response)
        {
            case GTK_RESPONSE_YES:
                if (Save_All_Files_With_Answer(FALSE)==-1)
                {
                    g_free (pathName);
                    return TRUE;
                }
                break;
            case GTK_RESPONSE_NO:
                break;
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
                g_free (pathName);
                return TRUE;
                break;
            default:
                g_assert_not_reached ();
                break;
        }
    }

    /* Memorize the current path */
    file = g_file_new_for_path (pathName);
    et_browser_set_current_path (self, file);

    /* Display the selected path into the BrowserEntry */
    parse_name = g_file_get_parse_name (file);
    gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->entry_combo))),
                        parse_name);
    g_free (parse_name);

    /* Start to read the directory */
    /* Skip loading the file list the first time that it is shown, if the user
     * has requested the read to be skipped. */
    if (!first_read
        || g_settings_get_boolean (MainSettings, "load-on-startup"))
    {
        gboolean dir_loaded;
        GtkTreeIter parentIter;

        dir_loaded = Read_Directory(gString(pathName));
        pathName = NULL;

        // If the directory can't be loaded, the directory musn't exist.
        // So we load the parent node and refresh the children
        if (dir_loaded == FALSE)
        {
            if (gtk_tree_selection_get_selected(selection, NULL, &selectedIter))
            {
                /* If the path could not be read, then it is possible that it
                 * has a subdirectory with readable permissions. In that case
                 * do not refresh the children. */
                if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(priv->directory_model),&parentIter,&selectedIter) )
                {
                    selectedPath = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->directory_model), &parentIter);
                    gtk_tree_selection_select_iter (selection, &parentIter);
                    if (gtk_tree_model_iter_has_child (GTK_TREE_MODEL (priv->directory_model),
                                                       &selectedIter) == FALSE
                                                       && !g_file_query_exists (file, NULL))
                    {
                        gtk_tree_view_collapse_row (GTK_TREE_VIEW (priv->directory_view),
                                                    selectedPath);
                        if (g_settings_get_boolean (MainSettings,
                                                    "browse-expand-children"))
                        {
                            gtk_tree_view_expand_row (GTK_TREE_VIEW (priv->directory_view),
                                                      selectedPath, FALSE);
                        }
                        gtk_tree_path_free (selectedPath);
                    }
                }
            }
        }

    }else
    {
        /* As we don't use the function 'Read_Directory' we must add this function here */
        et_application_window_update_actions(MainWindow);
    }

    first_read = FALSE;

    g_object_unref (file);
    g_free(pathName);
    return FALSE;
}


#ifdef G_OS_WIN32
static gboolean
et_browser_win32_get_drive_root (EtBrowser *self,
                                 gchar *drive,
                                 GtkTreeIter *rootNode,
                                 GtkTreePath **rootPath)
{
    EtBrowserPrivate *priv;
    gint root_index;
    gboolean found = FALSE;
    GtkTreeIter parentNode;
    gchar *nodeName;

    priv = et_browser_get_instance_private (self);

    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->directory_model), &parentNode);

    // Find root of path, ie: the drive letter
    root_index = 0;

    do
    {
        gtk_tree_model_get(GTK_TREE_MODEL(priv->directory_model), &parentNode,
                           TREE_COLUMN_FULL_PATH, &nodeName, -1);
        /* FIXME: Use something other than strncasecmp()! */
        if (strncasecmp(drive,nodeName, strlen(drive)) == 0)
        {
            g_free(nodeName);
            found = TRUE;
            break;
        }
        root_index++;
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->directory_model), &parentNode));

    if (!found) return FALSE;

    *rootNode = parentNode;
    *rootPath = gtk_tree_path_new_from_indices(root_index, -1);

    return TRUE;
}
#endif /* G_OS_WIN32 */


/*
 * et_browser_select_dir:
 *
 * Select the directory corresponding to the 'path' in the tree browser, but it
 * doesn't read it! Check if path is correct before selecting it.
 */
void
et_browser_select_dir (EtBrowser *self,
                       GFile *file)
{
    EtBrowserPrivate *priv;
    gchar *current_path;
    GtkTreePath *rootPath = NULL;
    GtkTreeIter parentNode, currentNode;
    gint index = 1; // Skip the first token as it is NULL due to leading /
    gchar **parts;
    gchar *nodeName;
    gchar *temp;

    priv = et_browser_get_instance_private (self);

    g_return_if_fail (priv->directory_view != NULL);

    /* Don't check here if the path is valid. It will be done later when
     * selecting a node in the tree */

    et_browser_set_current_path (self, file);
    current_path = g_file_get_path (file);

    parts = g_strsplit(current_path, G_DIR_SEPARATOR_S, 0);
    g_free (current_path);

    // Expand root node (fill parentNode and rootPath)
#ifdef G_OS_WIN32
    if (!et_browser_win32_get_drive_root (self, parts[0], &parentNode,
                                          &rootPath))
    {
        return;
    }
#else /* !G_OS_WIN32 */
    if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->directory_model),
                                        &parentNode))
    {
        g_message ("%s", "priv->directory_model is empty");
        return;
    }

    rootPath = gtk_tree_path_new_first();
#endif /* !G_OS_WIN32 */
    if (rootPath)
    {
        gtk_tree_view_expand_to_path (GTK_TREE_VIEW (priv->directory_view),
                                      rootPath);
        gtk_tree_path_free(rootPath);
    }

    while (parts[index]) // it is NULL-terminated
    {
        if (*parts[index] == '\0')
        {
            index++;
            continue;
        }

        if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(priv->directory_model), &currentNode, &parentNode))
        {
            gchar *path, *parent_path;
            GFile *directory;

            gtk_tree_model_get (GTK_TREE_MODEL (priv->directory_model),
                                &parentNode, TREE_COLUMN_FULL_PATH,
                                &parent_path, -1);
            path = g_build_filename (parent_path, parts[index], NULL);
            g_free (parent_path);

            directory = g_file_new_for_path (path);

            /* As dir name was not found in any node, check whether it exists
             * or not. */
            if (g_file_query_file_type (directory, G_FILE_QUERY_INFO_NONE, NULL)
                == G_FILE_TYPE_DIRECTORY)
            {
                /* It exists and is readable permission of parent directory is executable */
                GtkTreeIter iter;

                /* Create a new node for this directory name. */
                const GIcon* icon = get_gicon_for_path (self, path, ET_PATH_STATE_CLOSED);

                gtk_tree_store_insert_with_values (GTK_TREE_STORE (priv->directory_model),
                                                   &iter, &parentNode, 0,
                                                   TREE_COLUMN_DIR_NAME, parts[index],
                                                   TREE_COLUMN_FULL_PATH, path,
                                                   TREE_COLUMN_HAS_SUBDIR, check_for_subdir(directory),
                                                   TREE_COLUMN_SCANNED, TRUE,
                                                   TREE_COLUMN_ICON, icon, -1);

                currentNode = iter;
            }
            else
            {
                g_object_unref (directory);
                g_free (path);
                break;
            }

            g_object_unref (directory);
            g_free (path);
        }

        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(priv->directory_model), &currentNode,
                               TREE_COLUMN_FULL_PATH, &temp, -1);
            nodeName = g_path_get_basename(temp);
            g_free(temp);
#ifdef G_OS_WIN32
            if (strcasecmp(parts[index],nodeName) == 0)
#else /* !G_OS_WIN32 */
            if (strcmp(parts[index],nodeName) == 0)
#endif /* !G_OS_WIN32 */
            {
                g_free(nodeName);
                break;
            }
            g_free(nodeName);

            if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->directory_model),
                                           &currentNode))
            {
                /* Path was not found in tree, such as when a hidden path was
                 * passed in, but hidden paths are set to not be displayed. */
                g_strfreev (parts);
                return;
            }
        } while (1);

        parentNode = currentNode;
        rootPath = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->directory_model), &parentNode);
        if (rootPath)
        {
            gtk_tree_view_expand_to_path (GTK_TREE_VIEW (priv->directory_view),
                                          rootPath);
            gtk_tree_path_free(rootPath);
        }
        index++;
    }

    rootPath = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->directory_model), &parentNode);
    if (rootPath)
    {
        gtk_tree_view_expand_to_path (GTK_TREE_VIEW (priv->directory_view),
                                      rootPath);
        Browser_Tree_Set_Node_Visible (priv->directory_view, rootPath);
        /* Select the node to load the corresponding directory. */
        gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->directory_view)),
                                        rootPath);
        gtk_tree_path_free(rootPath);
    }

    g_strfreev(parts);
    return;
}

/*
 * Callback to select-row event
 * Displays the file info of the lowest selected file in the right-hand pane
 */
static void
Browser_List_Row_Selected (EtBrowser *self, GtkTreeSelection *selection)
{
    EtBrowserPrivate *priv;
    gint n_selected;
    GtkTreePath *cursor_path;
    GtkTreeIter cursor_iter;

    priv = et_browser_get_instance_private (self);

    n_selected = gtk_tree_selection_count_selected_rows (selection);

    /*
     * After a file is deleted, this function is called :
     * So we must handle the situation if no rows are selected
     */
    if (n_selected == 0)
    {
        MainWindow->change_displayed_file(nullptr);
        return;
    }

    gtk_tree_view_get_cursor (priv->file_view, &cursor_path, NULL);

    if (!cursor_path)
    {
        return;
    }

    if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->file_model),
                                 &cursor_iter, cursor_path))
    {
        ET_File *cursor_et_file = nullptr;
        if (gtk_tree_selection_iter_is_selected(selection, &cursor_iter))
        {   gtk_tree_model_get(GTK_TREE_MODEL(priv->file_model), &cursor_iter, LIST_FILE_POINTER, &cursor_et_file, -1);
            priv->current_file = cursor_iter;
        } else
            priv->current_file = invalid_iter;

        /* Clears the tag/file area if the cursor row was unselected, such
         * as by inverting the selection or Ctrl-clicking. */
        MainWindow->change_displayed_file(cursor_et_file);
    }
    else
    {
        g_warning ("%s", "Error getting iter from cursor path");
    }

    gtk_tree_path_free (cursor_path);
}

/*
 * Empty model, disabling Browser_List_Row_Selected () during clear because it
 * is called and causes crashes otherwise.
 */
static void
et_browser_clear_file_model (EtBrowser *self)
{
	EtBrowserPrivate* priv = et_browser_get_instance_private(self);

	// release ET_File references
	gtk_tree_model_foreach(GTK_TREE_MODEL(priv->file_model),
		[](GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, gpointer data)
		{	ET_File* etfile;
			gtk_tree_model_get(model, iter, LIST_FILE_POINTER, &etfile, -1);
			xPtr<ET_File>::fromCptr(etfile);
			return FALSE;
		}, NULL);

	GtkTreeSelection* selection = gtk_tree_view_get_selection(priv->file_view);
	g_signal_handler_block(selection, priv->file_selected_handler);

	priv->current_file = invalid_iter;
	gtk_list_store_clear (priv->file_model);
	gtk_tree_view_columns_autosize (priv->file_view);

	g_signal_handler_unblock (selection, priv->file_selected_handler);
}

/** Change background color of item depending of equality according to the current sort order. */
static void set_zebra(GtkTreeModel* model)
{
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;
	ET_File* last = nullptr;
	auto cmp = ET_File::get_comp_func((EtSortMode)g_settings_get_enum(MainSettings, "sort-mode"));
	bool activate_bg_color = false;
	do
	{	ET_File *file;
		gtk_tree_model_get(model, &iter, LIST_FILE_POINTER, &file, -1);
		file->activate_bg_color = activate_bg_color ^= last && abs(cmp(last, file)) == 1;
		last = file;
	} while (gtk_tree_model_iter_next(model, &iter));
}

/*
 * Loads the current visible range of ET_FileList into the browser list.
 */
void EtBrowser::load_file_list()
{
	EtBrowserPrivate* priv = et_browser_get_instance_private(this);

	et_browser_clear_file_model(this);

	auto range = ET_FileList::visible_range();
	ET_File* etfile_to_select = MainWindow->get_displayed_file();
	bool selected = false;
	GtkTreeIter selected_iter;
	for (auto i = range.first; i != range.second; ++i)
	{
		GtkTreeIter iter;
		gtk_list_store_insert_with_values(priv->file_model, &iter, G_MAXINT, LIST_FILE_POINTER, xPtr<ET_File>::toCptr(*i), -1);

		if (etfile_to_select == i->get())
		{	selected_iter = iter;
			selected = true;
			// update header
			et_application_window_update_ui_from_et_file(MainWindow, (EtColumn)0);
		}
	}

	// If no file to select, use the first one in browser sort order.
	if (!selected)
	{	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->file_model), &selected_iter))
			return; // no files
		// change current file
		gtk_tree_model_get(GTK_TREE_MODEL(priv->file_model), &selected_iter, LIST_FILE_POINTER, &etfile_to_select, -1);
			MainWindow->change_displayed_file(etfile_to_select);
	}
	Browser_List_Select_File_By_Iter(this, &selected_iter, TRUE);

	set_zebra(GTK_TREE_MODEL(priv->file_model));
}


/*
 * Update state of files in the list after changes (without clearing the list model!)
 *  - Refresh 'filename' is file saved,
 *  - Change color is something changed on the file
 */
void et_browser_refresh_list(EtBrowser *self)
{
	g_return_if_fail(ET_BROWSER(self));
	EtBrowserPrivate* priv = et_browser_get_instance_private(self);

	if (ET_FileList::empty() || !priv->file_view
		|| gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->file_model), NULL) == 0)
		return;

	// When displaying Artist + Album lists => refresh also rows color
	GVariant* variant = g_action_group_get_action_state(G_ACTION_GROUP(MainWindow), "file-artist-view");
	if (strcmp(g_variant_get_string(variant, NULL), "artist") == 0)
	{
		xStringD0 selected_artist;
		GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->artist_view));
		GtkTreeIter iter;
		gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->artist_model), &iter);
		while (valid)
		{
			gchar* artist;
			gtk_tree_model_get(GTK_TREE_MODEL(priv->artist_model), &iter, ARTIST_NAME, &artist, -1);

			if (gtk_tree_selection_iter_is_selected(selection, &iter))
				Browser_Artist_List_Set_Row_Appearance(self, iter, selected_artist = xStringD0(artist));
			else
				Browser_Artist_List_Set_Row_Appearance(self, iter, xStringD0(artist));
			g_free(artist);

			valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->artist_model), &iter);
		}

		valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->album_model), &iter);
		while (valid)
		{	Browser_Album_List_Set_Row_Appearance(self, iter, selected_artist);
			valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->album_model), &iter);
		}
	}

	g_variant_unref(variant);
}


/*
 * Update state of one file in the list after changes (without clearing the clist!)
 *  - Refresh filename is file saved,
 *  - Change color is something change on the file
 */
void
et_browser_refresh_file_in_list (EtBrowser *self,
                                 const ET_File *ETFile)
{
    EtBrowserPrivate *priv;
    GList *selectedRow = NULL;
    GVariant *variant;
    GtkTreeSelection *selection;
    GtkTreeIter selectedIter;
    const ET_File *etfile;
    gboolean row_found = FALSE;
    gboolean valid;

    g_return_if_fail (ET_BROWSER (self));

    priv = et_browser_get_instance_private (self);

    if (ET_FileList::empty() || !priv->file_view || !ETFile ||
        gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->file_model), NULL) == 0)
    {
        return;
    }

    // Search the row of the modified file to update it (when found: row_found=TRUE)
    // 1/3. Get position of ETFile in ETFileList
    if (row_found == FALSE)
    {
        valid = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(priv->file_model), &selectedIter, NULL, ET_FileList::visible_index(ETFile));
        if (valid)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(priv->file_model), &selectedIter,
                           LIST_FILE_POINTER, &etfile, -1);
            if (ETFile == etfile)
            {
                row_found = TRUE;
            }
        }
    }

    // 2/3. Try with the selected file in list (works only if we select the same file)
    if (row_found == FALSE)
    {
        selection = gtk_tree_view_get_selection (priv->file_view);
        selectedRow = gtk_tree_selection_get_selected_rows(selection, NULL);
        if (selectedRow && selectedRow->data != NULL)
        {
            valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->file_model), &selectedIter,
                                    (GtkTreePath*) selectedRow->data);
            if (valid)
            {
                gtk_tree_model_get(GTK_TREE_MODEL(priv->file_model), &selectedIter,
                                   LIST_FILE_POINTER, &etfile, -1);
                if (ETFile == etfile)
                {
                    row_found = TRUE;
                }
            }
        }

        g_list_free_full (selectedRow, (GDestroyNotify)gtk_tree_path_free);
    }

    // 3/3. Fails, now we browse the full list to find it
    if (row_found == FALSE)
    {
        valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->file_model), &selectedIter);
        while (valid)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(priv->file_model), &selectedIter,
                               LIST_FILE_POINTER, &etfile, -1);
            if (ETFile == etfile)
            {
                row_found = TRUE;
                break;
            } else
            {
                valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->file_model), &selectedIter);
            }
        }
    }

    // Error somewhere...
    if (row_found == FALSE)
        return;

    // Displayed the filename and refresh other fields
    GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->file_model), &selectedIter);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(priv->file_model), path, &selectedIter);
    gtk_tree_path_free(path);

	/* When displaying Artist + Album lists => refresh also rows color. */
	variant = g_action_group_get_action_state(G_ACTION_GROUP(MainWindow), "file-artist-view");
	if (strcmp(g_variant_get_string(variant, NULL), "artist") != 0)
	{	g_variant_unref(variant);
		return;
	}
	g_variant_unref(variant);

	auto album_range = ET_FileList::artist_album_index_find(ETFile);
	if (album_range.first == album_range.second)
		return;

	xStringD0 matchingArtist;
	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->artist_model), &selectedIter);
	while (valid)
	{	gchar *artist;
		gtk_tree_model_get(GTK_TREE_MODEL(priv->artist_model), &selectedIter, ARTIST_NAME, &artist, -1);

		if (artist == album_range.first->Artist)
		{	// Set color of the row.
			Browser_Artist_List_Set_Row_Appearance (self, selectedIter, matchingArtist = xStringD0(artist));
			g_free(artist);
			break;
		}

		g_free(artist);
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->artist_model), &selectedIter);
	}

	if (!matchingArtist)
		return;

	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->album_model), &selectedIter);
	while (valid)
	{
		gchar* album;
		gtk_tree_model_get(GTK_TREE_MODEL (priv->album_model), &selectedIter, ALBUM_NAME, &album, -1);

		if (album == album_range.first->Album)
		{	// Set color of the row.
			Browser_Album_List_Set_Row_Appearance(self, selectedIter, matchingArtist);
			g_free(album);
			break;
		}

		g_free(album);
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->album_model), &selectedIter);
	}
}


/*
 * Remove a file from the list, by ETFile
 */
void
et_browser_remove_file (EtBrowser *self,
                        const ET_File *searchETFile)
{
    EtBrowserPrivate *priv;
    gint row;
    GtkTreePath *currentPath = NULL;
    GtkTreeIter currentIter;
    ET_File *currentETFile;
    gboolean valid;

    if (searchETFile == NULL)
        return;

    priv = et_browser_get_instance_private (self);

    // Go through the file list until it is found
    for (row=0; row < gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->file_model), NULL); row++)
    {
        if (row == 0)
            currentPath = gtk_tree_path_new_first();
        else
            gtk_tree_path_next(currentPath);

        valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->file_model), &currentIter, currentPath);
        if (valid)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(priv->file_model), &currentIter,
                               LIST_FILE_POINTER, &currentETFile, -1);

            if (currentETFile == searchETFile)
            {
                // release file
                xPtr<ET_File>::fromCptr(currentETFile);
                gtk_list_store_remove(priv->file_model, &currentIter);
                break;
            }
        }
    }

    gtk_tree_path_free (currentPath);
}

/*
 * Select the specified file in the list, by its ETFile
 */
void
et_browser_select_file_by_et_file (EtBrowser *self,
                                   const ET_File *file,
                                   gboolean select_it)
{
	GtkTreePath* currentPath = et_browser_select_file_by_et_file2(self, file, select_it, NULL);
	if (currentPath)
		gtk_tree_path_free(currentPath);
}
/*
 * Select the specified file in the list, by its ETFile
 *  - startPath : if set : starting path to try increase speed
 *  - returns allocated "currentPath" to free
 */
GtkTreePath *
et_browser_select_file_by_et_file2 (EtBrowser *self,
                                    const ET_File *searchETFile,
                                    gboolean select_it,
                                    GtkTreePath *startPath)
{
    EtBrowserPrivate *priv;
    gint row;
    GtkTreePath *currentPath = NULL;
    GtkTreeIter currentIter;
    ET_File *currentETFile;
    gboolean valid;

    g_return_val_if_fail (searchETFile != NULL, NULL);

    priv = et_browser_get_instance_private (self);

    // If the path is used, we try the next item (to increase speed), as it is correct in many cases...
    if (startPath)
    {
        // Try the next path
        gtk_tree_path_next(startPath);
        valid = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->file_model),
                                         &currentIter, startPath);
        if (valid)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(priv->file_model), &currentIter,
                               LIST_FILE_POINTER, &currentETFile, -1);
            // It is the good file?
            if (currentETFile == searchETFile)
            {
                Browser_List_Select_File_By_Iter (self, &currentIter,
                                                  select_it);
                return startPath;
            }
        }
    }

    // Else, we try the whole list...
    // Go through the file list until it is found
    currentPath = gtk_tree_path_new_first();
    for (row=0; row < gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->file_model), NULL); row++)
    {
        valid = gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->file_model), &currentIter, currentPath);
        if (valid)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(priv->file_model), &currentIter,
                               LIST_FILE_POINTER, &currentETFile, -1);

            if (currentETFile == searchETFile)
            {
                Browser_List_Select_File_By_Iter (self, &currentIter,
                                                  select_it);
                return currentPath;
                //break;
            }
        }
        gtk_tree_path_next(currentPath);
    }
    gtk_tree_path_free(currentPath);

    return NULL;
}


/*
 * Select the specified file in the list, by an iter
 */
static void
Browser_List_Select_File_By_Iter (EtBrowser *self,
                                  GtkTreeIter *rowIter,
                                  gboolean select_it)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    if (select_it)
    {
        GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->file_view);
        if (selection)
            gtk_tree_selection_select_iter(selection, rowIter);
    }

    et_browser_set_row_visible (self, rowIter);
}

/*
 * Select the specified file in the list, by a string representation of an iter
 * e.g. output of gtk_tree_model_get_string_from_iter()
 */
void
et_browser_select_file_by_iter_string (EtBrowser *self,
                                       const gchar* stringIter,
                                       gboolean select_it)
{
    EtBrowserPrivate *priv;
    GtkTreeIter iter;

    priv = et_browser_get_instance_private (self);

    g_return_if_fail (priv->file_model != NULL || priv->file_view != NULL);

    if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(priv->file_model), &iter, stringIter))
    {
        if (select_it)
        {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(priv->file_view);

            // FIX ME : Why signal was blocked if selected? Don't remember...
            if (selection)
            {
                //g_signal_handlers_block_by_func(G_OBJECT(selection),G_CALLBACK(Browser_List_Row_Selected),NULL);
                gtk_tree_selection_select_iter(selection, &iter);
                //g_signal_handlers_unblock_by_func(G_OBJECT(selection),G_CALLBACK(Browser_List_Row_Selected),NULL);
            }
        }
        et_browser_set_row_visible (self, &iter);
    }
}

/*
 * Select the specified file in the list, by fuzzy string matching based on
 * the Damerau-Levenshtein Metric (patch from Santtu Lakkala - 23/08/2004)
 */
ET_File *
et_browser_select_file_by_dlm (EtBrowser *self,
                               const gchar* string,
                               gboolean select_it)
{
    EtBrowserPrivate *priv;
    GtkTreeIter iter;
    GtkTreeIter iter2;
    GtkTreeSelection *selection;
    ET_File *current_etfile = NULL, *retval = NULL;
    int max = 0, cur;

    priv = et_browser_get_instance_private (self);

    g_return_val_if_fail (priv->file_model != NULL || priv->file_view != NULL, NULL);

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->file_model),
                                       &iter))
    {
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(priv->file_model), &iter,
                               LIST_FILE_POINTER, &current_etfile, -1);
            const xStringD0& current_title = current_etfile->FileTagNew()->title;

            if ((cur = dlm((current_title ? current_title : current_etfile->FileNameNew()->file().get()), string)) > max) // See "dlm.c"
            {
                max = cur;
                iter2 = iter;
                retval = current_etfile;
            }

        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->file_model), &iter));

        if (select_it)
        {
            selection = gtk_tree_view_get_selection(priv->file_view);
            if (selection)
            {
                g_signal_handler_block(selection, priv->file_selected_handler);
                gtk_tree_selection_select_iter(selection, &iter2);
                g_signal_handler_unblock(selection, priv->file_selected_handler);
            }
        }
        et_browser_set_row_visible (self, &iter2);
    }
    return retval;
}

bool EtBrowser::has_file()
{
	return !!et_browser_get_instance_private(this)->current_file.stamp;
}

ET_File* EtBrowser::current_file()
{
	if (!has_file())
		return nullptr;
	EtBrowserPrivate* priv = et_browser_get_instance_private(this);
	ET_File* etfile;
	gtk_tree_model_get(GTK_TREE_MODEL(priv->file_model), &priv->current_file, LIST_FILE_POINTER, &etfile, -1);
	return etfile;
}

bool EtBrowser::has_prev()
{
	if (!has_file())
		return false;
	EtBrowserPrivate* priv = et_browser_get_instance_private(this);
	GtkTreeIter iter = priv->current_file;
	return !!gtk_tree_model_iter_previous(GTK_TREE_MODEL(priv->file_model), &iter);
}

bool EtBrowser::has_next()
{
	if (!has_file())
		return false;
	EtBrowserPrivate* priv = et_browser_get_instance_private(this);
	GtkTreeIter iter = priv->current_file;
	return !!gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->file_model), &iter);
}

ET_File* EtBrowser::select_first_file()
{
	EtBrowserPrivate* priv = et_browser_get_instance_private(this);
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->file_model), &iter))
		return nullptr;

	Browser_List_Select_File_By_Iter(this, &iter, true);
	return current_file();
}

ET_File* EtBrowser::select_last_file()
{
	EtBrowserPrivate* priv = et_browser_get_instance_private(this);
	GtkTreeIter iter;

	// get last iter
	gint rows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->file_model), NULL);
	if (rows == 0)
		return nullptr;
	GtkTreePath* path = gtk_tree_path_new_from_indices(rows - 1, -1);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->file_model), &iter, path);
	gtk_tree_path_free(path);

	Browser_List_Select_File_By_Iter(this, &iter, true);
	return current_file();
}

ET_File* EtBrowser::select_prev_file()
{
	if (!has_file())
		return nullptr;
	EtBrowserPrivate* priv = et_browser_get_instance_private(this);
	GtkTreeIter iter = priv->current_file;
	if (!gtk_tree_model_iter_previous(GTK_TREE_MODEL(priv->file_model), &iter))
		return nullptr;

	Browser_List_Select_File_By_Iter(this, &iter, true);
	return current_file();
}

ET_File* EtBrowser::select_next_file()
{
	if (!has_file())
		return nullptr;
	EtBrowserPrivate* priv = et_browser_get_instance_private(this);
	GtkTreeIter iter = priv->current_file;
	if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->file_model), &iter))
		return nullptr;

	Browser_List_Select_File_By_Iter(this, &iter, true);
	return current_file();
}

/*
 * Clear all entries on the file list
 */
void EtBrowser::clear()
{
	et_browser_clear_file_model(this);
	et_browser_clear_artist_model(this);
	et_browser_clear_album_model(this);
}

/*
 * Intelligently sort the file list based on the current sorting method
 * see also 'ET_Sort_File_List'
 */
static gint
Browser_List_Sort_Func (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
                        gpointer data)
{
    ET_File *ETFile1;
    ET_File *ETFile2;

    gtk_tree_model_get(model, a, LIST_FILE_POINTER, &ETFile1, -1);
    gtk_tree_model_get(model, b, LIST_FILE_POINTER, &ETFile2, -1);

    return ((gint (*)(const ET_File*, const ET_File*))data)(ETFile1, ETFile2);
}

/*
 * Refresh the list sorting (call me after sort-mode has changed)
 */
static void
et_browser_refresh_sort (EtBrowser *self)
{
	g_return_if_fail (ET_BROWSER (self));
	EtBrowserPrivate* priv = et_browser_get_instance_private (self);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(priv->file_model), 0, Browser_List_Sort_Func,
		(gpointer)ET_File::get_comp_func((EtSortMode)g_settings_get_enum(MainSettings, "sort-mode")), NULL);

	set_zebra(GTK_TREE_MODEL(priv->file_model));
}

/*
 * Select all files on the file list
 */
void
et_browser_select_all (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    GtkTreeSelection *selection;

    g_return_if_fail (ET_BROWSER (self));

    priv = et_browser_get_instance_private (self);
    selection = gtk_tree_view_get_selection(priv->file_view);

    if (selection)
    {
        /* Must block the select signal to avoid the selecting, one by one, of
         * all files in the main files list. */
        g_signal_handler_block (selection, priv->file_selected_handler);
        gtk_tree_selection_select_all(selection);
        g_signal_handler_unblock (selection, priv->file_selected_handler);
    }
}

/*
 * Unselect all files on the file list
 */
void
et_browser_unselect_all (EtBrowser *self)
{
    GtkTreeSelection* selection = gtk_tree_view_get_selection(et_browser_get_instance_private(self)->file_view);
    if (selection)
        gtk_tree_selection_unselect_all (selection);
}

/*
 * Invert the selection of the file list
 */
void
et_browser_invert_selection (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    gboolean valid;

    priv = et_browser_get_instance_private (self);

    g_return_if_fail (priv->file_model != NULL || priv->file_view != NULL);

    selection = gtk_tree_view_get_selection(priv->file_view);
    if (selection)
    {
        /* Must block the select signal to avoid selecting all files (one by
         * one) in the main files list. */
        g_signal_handler_block (selection, priv->file_selected_handler);
        valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->file_model), &iter);
        while (valid)
        {
            if (gtk_tree_selection_iter_is_selected(selection, &iter))
            {
                gtk_tree_selection_unselect_iter(selection, &iter);
            } else
            {
                gtk_tree_selection_select_iter(selection, &iter);
            }
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->file_model), &iter);
        }
        g_signal_handler_unblock (selection, priv->file_selected_handler);
    }
}

// all file in range saved?
static bool any_unsaved(const ET_FileList::index_range_type& range)
{	auto file_range = ET_FileList::to_file_range(range);
	for (auto i = file_range.first; i < file_range.second; ++i)
		if (!i->get()->is_saved())
			return true;
	return false;
}

static void et_browser_clear_artist_model(EtBrowser *self)
{
	EtBrowserPrivate* priv = et_browser_get_instance_private(self);

	GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->artist_view));
	gtk_tree_selection_unselect_all(selection);

	g_signal_handler_block(selection, priv->artist_selected_handler);
	gtk_list_store_clear(priv->artist_model);
	g_signal_handler_unblock(selection, priv->artist_selected_handler);
}

static void Browser_Artist_List_Load_Files(EtBrowser *self)
{
	EtBrowserPrivate* priv = et_browser_get_instance_private(self);
	g_return_if_fail (priv->artist_view != NULL);

	et_browser_clear_artist_model(self);

	GdkPixbuf* pixbuf = gdk_pixbuf_new_from_resource("/org/gnome/EasyTAG/images/artist.png", NULL);
	gboolean bold = g_settings_get_boolean(MainSettings, "file-changed-bold");
	GtkTreePath* path = NULL;
	const ET_File* etfile = MainWindow->get_displayed_file();
	GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->artist_view));

	// Iterate over blocks of the same artist
	g_signal_handler_block(selection, priv->artist_selected_handler);

	for (auto range = ET_FileList::index_range_type(ET_FileList::artist_album_index().begin(), ET_FileList::artist_album_index().begin());
		range.first != ET_FileList::artist_album_index().end(); range.first = range.second)
	{
		// propagate end to end of artist and aggregate some data
		while (++range.second != ET_FileList::artist_album_index().end() && range.second->Artist == range.first->Artist);
		bool unsaved = any_unsaved(range);

		// Insert a line for each artist
		GtkTreeIter iter;
		gtk_list_store_insert_with_values(priv->artist_model, &iter, G_MAXINT,
			ARTIST_PIXBUF, pixbuf,
			ARTIST_NAME, range.first->Artist.get(),
			ARTIST_NUM_ALBUMS, (unsigned)(range.second - range.first),
			ARTIST_NUM_FILES, ET_FileList::files_in_range(range),
			ARTIST_FONT_WEIGHT, (unsaved && bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL),
			ARTIST_ROW_FOREGROUND, (unsaved && !bold ? &RED : NULL),
			-1);

		// Select the first line if we weren't asked to select anything
		if (!path && (!etfile || ET_FileList::is_in_range(range, etfile)))
		{
			gtk_tree_selection_select_iter(selection, &iter);

			path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->artist_model), &iter);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(priv->artist_view), path, NULL, FALSE, 0, 0);
			gtk_tree_path_free(path);

			Browser_Album_List_Load_Files(self, range);
		}
	}

	g_signal_handler_unblock(selection, priv->artist_selected_handler);

	g_object_unref(pixbuf);
}

/*
 * Callback to select-row event
 */
static void
Browser_Artist_List_Row_Selected (EtBrowser *self, GtkTreeSelection* selection)
{
	EtBrowserPrivate* priv = et_browser_get_instance_private (self);

	// Display the relevant albums
	GtkTreeIter iter;
	if(!gtk_tree_selection_get_selected(selection, NULL, &iter))
			return; // We might be called with no row selected

	gchar* artist;
	gtk_tree_model_get(GTK_TREE_MODEL(priv->artist_model), &iter, ARTIST_NAME, &artist, -1);
	auto range = ET_FileList::matching_range(xStringD0(artist));
	g_free(artist);

	Browser_Album_List_Load_Files(self, range);
}

/*
 * Set the color of the row of priv->artist_view
 */
static void Browser_Artist_List_Set_Row_Appearance(EtBrowser *self, GtkTreeIter& iter, const xStringD0& artist)
{
	EtBrowserPrivate* priv = et_browser_get_instance_private(self);

	// Change the style (red/bold) of the row if one of the files was changed
	auto range = ET_FileList::matching_range(artist);

	bool unsaved = any_unsaved(range);
	gboolean bold = g_settings_get_boolean(MainSettings, "file-changed-bold");

	gtk_list_store_set(priv->artist_model, &iter,
		ARTIST_FONT_WEIGHT, (unsaved && bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL),
		ARTIST_ROW_FOREGROUND, (unsaved && !bold ? &RED : NULL), -1);
}

static void et_browser_clear_album_model(EtBrowser *self)
{
	EtBrowserPrivate* priv = et_browser_get_instance_private(self);

	GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->album_view));
	// unselect first to get exactly one selection event
	gtk_tree_selection_unselect_all(selection);

	g_signal_handler_block(selection, priv->album_selected_handler);
	gtk_list_store_clear(priv->album_model);
	g_signal_handler_unblock(selection, priv->album_selected_handler);
}

/*
 * Load the list of albums for an artist
 */
static void Browser_Album_List_Load_Files(EtBrowser *self, ET_FileList::index_range_type range)
{
	EtBrowserPrivate *priv;
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	priv = et_browser_get_instance_private (self);

	g_return_if_fail (priv->album_view != NULL);

	et_browser_clear_album_model(self);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->album_view));

	// Create a first row to select all albums of the artist
	gtk_list_store_insert_with_values(priv->album_model, &iter, G_MAXINT,
		ALBUM_NAME, _("All albums"),
		ALBUM_NUM_FILES, ET_FileList::files_in_range(range),
		ALBUM_STATE, ALBUM_STATE_ALL_ALBUMS,
		-1);

	gtk_list_store_insert_with_values(priv->album_model, &iter, G_MAXINT,
		ALBUM_STATE, ALBUM_STATE_SEPARATOR, -1);

	/* TODO: Make the icon use the symbolic variant. */
	GIcon* icon = g_themed_icon_new_with_default_fallbacks ("media-optical-cd-audio");
	gboolean bold = g_settings_get_boolean(MainSettings, "file-changed-bold");
	GtkTreePath *path = NULL;
	const ET_File* etfile = MainWindow->get_displayed_file();
	if (etfile && !ET_FileList::is_in_range(range, etfile))
		etfile = nullptr; // no match within this artist => select first

	// Create a line for each album of the artist
	g_signal_handler_block(selection, priv->album_selected_handler);

	for (auto end = range.second; range.first != end; range.first = range.second)
	{	range.second = range.first + 1;
		bool unsaved = any_unsaved(range);
		/* Add the new row. */
		gtk_list_store_insert_with_values(priv->album_model, &iter, G_MAXINT,
			ALBUM_GICON, icon,
			ALBUM_NAME, range.first->Album.get(),
			ALBUM_NUM_FILES, ET_FileList::files_in_range(range),
			ALBUM_FONT_WEIGHT, (unsaved && bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL),
			ALBUM_ROW_FOREGROUND, (unsaved && !bold ? &RED : NULL),
			-1);

		// Select the first line if we weren't asked to select anything
		if (!path && (!etfile || ET_FileList::is_in_range(range, etfile)))
		{	path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->album_model), &iter);

			gtk_tree_selection_select_iter(selection, &iter);

			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(priv->album_view), path, NULL, FALSE, 0, 0);
			gtk_tree_path_free(path);
		}
	}

	g_signal_handler_unblock(selection, priv->album_selected_handler);

	Browser_Album_List_Row_Selected(self, selection);

	g_object_unref(icon);
}

/*
 * Callback to select-row event
 */
static void
Browser_Album_List_Row_Selected (EtBrowser *self, GtkTreeSelection *selection)
{
	EtBrowserPrivate* priv = et_browser_get_instance_private (self);

	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
		return; // We might be called with no rows selected
	gchar* album;
	gint state;
	gtk_tree_model_get(GTK_TREE_MODEL(priv->album_model), &iter, ALBUM_NAME, &album, ALBUM_STATE, &state, -1);

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->artist_view));
	if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
	{	g_free(album);
		return; // We might be called with no row selected
	}

	gchar* artist;
	gtk_tree_model_get(GTK_TREE_MODEL(priv->artist_model), &iter, ARTIST_NAME, &artist, -1);

	xStringD0 Artist(artist);
	if (state & ALBUM_STATE_ALL_ALBUMS)
		ET_FileList::set_visible_range(&Artist);
	else
	{	xStringD0 Album(album);
		ET_FileList::set_visible_range(&Artist, &Album);
	}

	g_free(artist);
	g_free(album);

	self->load_file_list();
}

/*
 * Set the color of the row of priv->album_view
 */
static void Browser_Album_List_Set_Row_Appearance(EtBrowser *self, GtkTreeIter& iter, const xStringD0& artist)
{
	EtBrowserPrivate *priv = et_browser_get_instance_private(self);

	gchar* album;
	gint state;
	gtk_tree_model_get(GTK_TREE_MODEL(priv->album_model), &iter, ALBUM_NAME, &album, ALBUM_STATE, &state, -1);
	auto range = state & ALBUM_STATE_ALL_ALBUMS ? ET_FileList::matching_range(artist) : ET_FileList::matching_range(artist, xStringD0(album));
	g_free(album);

	bool unsaved = any_unsaved(range);
	gboolean bold = g_settings_get_boolean(MainSettings, "file-changed-bold");

	gtk_list_store_set(priv->album_model, &iter,
		ALBUM_FONT_WEIGHT, (unsaved && bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL),
		ALBUM_ROW_FOREGROUND, (unsaved && !bold ? &RED : NULL), -1);
}

void et_browser_set_display_mode(EtBrowser *self, EtBrowserMode mode)
{
	g_return_if_fail (ET_BROWSER (self));

	EtBrowserPrivate* priv = et_browser_get_instance_private(self);

	et_application_window_update_et_file_from_ui(MainWindow);

	switch (mode)
	{
	default:
		g_assert_not_reached();

	case ET_BROWSER_MODE_FILE:
		/* Set the whole list as "Displayed list". */
		ET_FileList::set_display_mode(ET_BROWSER_MODE_FILE);
		ET_FileList::set_visible_range();

		/* Display Tree Browser. */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->directory_album_artist_notebook), 0);
		self->load_file_list();
		break;

	case ET_BROWSER_MODE_ARTIST:
		ET_FileList::set_display_mode(ET_BROWSER_MODE_ARTIST_ALBUM);
		Browser_Artist_List_Load_Files(self);

		/* Display Artist + Album lists. */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->directory_album_artist_notebook), 1);
		break;
	}
}

/*
 * Disable (FALSE) / Enable (TRUE) all user widgets in the browser area (Tree + List + Entry)
 */
void
et_browser_set_sensitive (EtBrowser *self, gboolean sensitive)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    gtk_widget_set_sensitive (GTK_WIDGET (priv->entry_combo), sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (priv->directory_view), sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (priv->file_view), sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (priv->artist_view), sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (priv->album_view), sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (priv->open_button), sensitive);
    gtk_widget_set_sensitive (GTK_WIDGET (priv->files_label), sensitive);
}

static void
do_popup_menu (EtBrowser *self,
               GdkEventButton *event,
               GtkTreeView *view,
               GtkWidget *menu)
{
    gint button;
    gint event_time;

    if (event)
    {
        button = event->button;
        event_time = event->time;
    }
    else
    {
        button = 0;
        event_time = gtk_get_current_event_time ();
    }

    /* TODO: Add popup positioning function. */
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button,
                    event_time);
}

static void
select_row_for_button_press_event (GtkTreeView *treeview,
                                   GdkEventButton *event)
{
    if (event->window == gtk_tree_view_get_bin_window (treeview))
    {
        GtkTreePath *tree_path;

        if (gtk_tree_view_get_path_at_pos (treeview, event->x,
                                           event->y, &tree_path, NULL,
                                           NULL,NULL))
        {
            gtk_tree_selection_select_path (gtk_tree_view_get_selection (treeview),
                                            tree_path);
            gtk_tree_path_free (tree_path);
        }
    }
}

static gboolean
on_album_tree_popup_menu (GtkWidget *treeview,
                          EtBrowser *self)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    do_popup_menu (self, NULL, GTK_TREE_VIEW (treeview), priv->album_menu);

    return GDK_EVENT_STOP;
}

static gboolean
on_artist_tree_popup_menu (GtkWidget *treeview,
                           EtBrowser *self)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    do_popup_menu (self, NULL, GTK_TREE_VIEW (treeview), priv->artist_menu);

    return GDK_EVENT_STOP;
}

static gboolean
on_directory_tree_popup_menu (GtkWidget *treeview,
                              EtBrowser *self)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    do_popup_menu (self, NULL, GTK_TREE_VIEW (treeview), priv->directory_view_menu);

    return GDK_EVENT_STOP;
}

static gboolean
on_file_tree_popup_menu (GtkWidget *treeview,
                         EtBrowser *self)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    do_popup_menu (self, NULL, GTK_TREE_VIEW (treeview), priv->file_menu);

    return GDK_EVENT_STOP;
}

static gboolean
on_album_tree_button_press_event (GtkWidget *widget,
                                  GdkEventButton *event,
                                  EtBrowser *self)
{
    if (gdk_event_triggers_context_menu ((GdkEvent *)event))
    {
        EtBrowserPrivate *priv;

        priv = et_browser_get_instance_private (self);

        if (GTK_IS_TREE_VIEW (widget))
        {
            select_row_for_button_press_event (GTK_TREE_VIEW (widget), event);
        }

        do_popup_menu (self, event, GTK_TREE_VIEW (priv->album_view),
                       priv->album_menu);

        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
on_artist_tree_button_press_event (GtkWidget *widget,
                                   GdkEventButton *event,
                                   EtBrowser *self)
{
    if (gdk_event_triggers_context_menu ((GdkEvent *)event))
    {
        EtBrowserPrivate *priv;

        priv = et_browser_get_instance_private (self);

        if (GTK_IS_TREE_VIEW (widget))
        {
            select_row_for_button_press_event (GTK_TREE_VIEW (widget), event);
        }

        do_popup_menu (self, event, GTK_TREE_VIEW (priv->artist_view),
                       priv->artist_menu);

        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
on_directory_tree_button_press_event (GtkWidget *widget,
                                      GdkEventButton *event,
                                      EtBrowser *self)
{
    if (gdk_event_triggers_context_menu ((GdkEvent *)event))
    {
        EtBrowserPrivate *priv;

        priv = et_browser_get_instance_private (self);

        if (GTK_IS_TREE_VIEW (widget))
        {
            select_row_for_button_press_event (GTK_TREE_VIEW (widget), event);
        }

        do_popup_menu (self, event, GTK_TREE_VIEW (priv->directory_view),
                       priv->directory_view_menu);

        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

/*
 * Browser_Popup_Menu_Handler : displays the corresponding menu
 */
static gboolean
on_file_tree_button_press_event (GtkWidget *widget,
                                 GdkEventButton *event,
                                 EtBrowser *self)
{
    if (gdk_event_triggers_context_menu ((GdkEvent *)event))
    {
        EtBrowserPrivate *priv;

        priv = et_browser_get_instance_private (self);

        if (GTK_IS_TREE_VIEW (widget))
        {
            select_row_for_button_press_event (GTK_TREE_VIEW (widget), event);
        }

        do_popup_menu (self, event, priv->file_view, priv->file_menu);

        return GDK_EVENT_STOP;
    }
    else if (event->type == GDK_2BUTTON_PRESS
        && event->button == GDK_BUTTON_PRIMARY)
    {
        /* Double left mouse click. Select files of the same directory (useful
         * when browsing sub-directories). */
        if (gtk_tree_view_get_bin_window(GTK_TREE_VIEW(widget)) != event->window)
            /* If the double-click is not on a tree view row, for example when
             * resizing a header column, ignore it. */
            return GDK_EVENT_PROPAGATE;

        GtkTreePath* tree_path;
        GtkTreeViewColumn* column;
        if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &tree_path, &column, NULL,NULL))
            return GDK_EVENT_PROPAGATE;

        EtBrowserPrivate* priv = et_browser_get_instance_private(self);
        GtkTreeModel* model = GTK_TREE_MODEL(priv->file_model);
        GtkTreeIter iter;
        gtk_tree_model_get_iter(model, &iter, tree_path);
        gtk_tree_path_free(tree_path);

        if (!column)
            return GDK_EVENT_PROPAGATE;

        ET_File* selected;
        gtk_tree_model_get(model, &iter, LIST_FILE_POINTER, &selected, -1);

        // matching compare function
        const gchar* id = gtk_buildable_get_name(GTK_BUILDABLE(column));
        gchar* nick = g_strconcat("ascending-", id, NULL);
        nick[strlen(nick) - 7] = 0; // strip "-column"
        // Replace '_' by '-'
        for (char* cp = nick; *cp; ++cp)
        	if (*cp == '_')
        		*cp = '-';
        GEnumClass *enum_class = (GEnumClass*)g_type_class_ref(ET_TYPE_SORT_MODE);
        GEnumValue* enum_value = g_enum_get_value_by_nick(enum_class, nick);
        g_type_class_unref(enum_class);
        g_free(nick);
        if (!enum_value)
            return GDK_EVENT_PROPAGATE;
        auto cmp = ET_File::get_comp_func((EtSortMode)enum_value->value);
        if (!cmp)
            return GDK_EVENT_PROPAGATE;

        /* Search and select files of the property. */
        gtk_tree_model_get_iter_first(model, &iter);
        GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->file_view));
        g_signal_handler_block(selection, priv->file_selected_handler);
        do
        {   ET_File *file;
            gtk_tree_model_get(model, &iter, LIST_FILE_POINTER, &file, -1);
            if (abs(cmp(selected, file)) != 1)
                Browser_List_Select_File_By_Iter(self, &iter, TRUE);
        } while (gtk_tree_model_iter_next(model, &iter));
        g_signal_handler_unblock(selection, priv->file_selected_handler);

        return GDK_EVENT_STOP;
    }
    else if (event->type == GDK_3BUTTON_PRESS
             && event->button == GDK_BUTTON_PRIMARY)
    {
        /* Triple left mouse click, select all files of the list. */
        g_action_group_activate_action (G_ACTION_GROUP (MainWindow),
                                        "select-all", NULL);
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

/*
 * Destroy the whole tree up to the root node
 */
static void
Browser_Tree_Initialize (EtBrowser *self)
{
    EtBrowserPrivate *priv;
#ifdef G_OS_WIN32
    GVolumeMonitor *monitor;
    GList *mounts;
    GList *l;
#endif
    GtkTreeIter parent_iter;
    GtkTreeIter dummy_iter;

    priv = et_browser_get_instance_private (self);

    g_return_if_fail (priv->directory_model != NULL);

    gtk_tree_store_clear (priv->directory_model);

#ifdef G_OS_WIN32
    /* TODO: Connect to the monitor changed signals. */
    monitor = g_volume_monitor_get ();
    mounts = g_volume_monitor_get_mounts (monitor);

    for (l = mounts; l != NULL; l = g_list_next (l))
    {
        GMount *mount;
        gchar *name;
        GFile *root;
        gchar *path;

        mount = l->data;
        GIcon* drive_icon = g_mount_get_icon (mount);
        name = g_mount_get_name (mount);
        root = g_mount_get_root (mount);
        path = g_file_get_path (root);

        gtk_tree_store_insert_with_values (priv->directory_model,
                                           &parent_iter, NULL, G_MAXINT,
                                           TREE_COLUMN_DIR_NAME,
                                           name,
                                           TREE_COLUMN_FULL_PATH,
                                           path,
                                           TREE_COLUMN_HAS_SUBDIR, TRUE,
                                           TREE_COLUMN_SCANNED, FALSE,
                                           TREE_COLUMN_ICON, drive_icon,
                                           -1);
        /* Insert dummy node. */
        gtk_tree_store_append (priv->directory_model, &dummy_iter,
                               &parent_iter);

        g_free (path);
        g_free (name);
        g_object_unref (root);
        g_object_unref (drive_icon);
    }

    g_list_free_full (mounts, g_object_unref);
    g_object_unref (monitor);
#else /* !G_OS_WIN32 */
    const GIcon* drive_icon = get_gicon_for_path(self, G_DIR_SEPARATOR_S, ET_PATH_STATE_CLOSED);
    gtk_tree_store_insert_with_values (priv->directory_model, &parent_iter, NULL,
                                       G_MAXINT, TREE_COLUMN_DIR_NAME,
                                       G_DIR_SEPARATOR_S,
                                       TREE_COLUMN_FULL_PATH,
                                       G_DIR_SEPARATOR_S,
                                       TREE_COLUMN_HAS_SUBDIR, TRUE,
                                       TREE_COLUMN_SCANNED, FALSE,
                                       TREE_COLUMN_ICON, drive_icon, -1);
    /* Insert dummy node. */
    gtk_tree_store_append (priv->directory_model, &dummy_iter, &parent_iter);
#endif /* !G_OS_WIN32 */
}

/*
 * et_browser_reload: Refresh the tree browser by destroying it and rebuilding it.
 * Opens tree nodes corresponding to the current path.
 */
void
et_browser_reload (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    gchar *current_path = NULL;
    GtkTreeSelection *selection;

    priv = et_browser_get_instance_private (self);

    /* Memorize the current path to load it again at the end */
    current_path = Browser_Tree_Get_Path_Of_Selected_Node (self);

    /* Select again the memorized path without loading files */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->directory_view));

    if (selection)
    {
        GFile *file;

        g_signal_handlers_block_by_func (selection, (gpointer)Browser_Tree_Node_Selected, self);
        Browser_Tree_Initialize (self);
        file = g_file_new_for_path (current_path);
        et_browser_select_dir (self, file);
        g_object_unref (file);
        g_signal_handlers_unblock_by_func (selection, (gpointer)(Browser_Tree_Node_Selected), self);
    }
    g_free(current_path);

    et_application_window_update_actions(MainWindow);
}

/*
 * Renames a directory
 * last_path:
 * new_path:
 * Parameters are non-utf8!
 */
static void
Browser_Tree_Rename_Directory (EtBrowser *self,
                               const gchar *last_path,
                               const gchar *new_path)
{
    EtBrowserPrivate *priv;
    gchar **textsplit;
    gint i;
    GtkTreeIter  iter;
    GtkTreePath *childpath;
    GtkTreePath *parentpath;
    gchar *new_basename_utf8;
    gchar *path;
    GFile *file;

    if (!last_path || !new_path)
        return;

    priv = et_browser_get_instance_private (self);

    /*
     * Find the existing tree entry
     */
    textsplit = g_strsplit(last_path, G_DIR_SEPARATOR_S, 0);

#ifdef G_OS_WIN32
    if (!et_browser_win32_get_drive_root (self, textsplit[0], &iter,
                                          &parentpath))
    {
        return;
    }
#else /* !G_OS_WIN32 */
    parentpath = gtk_tree_path_new_first();
#endif /* !G_OS_WIN32 */

    for (i = 1; textsplit[i] != NULL; i++)
    {
        gboolean valid = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->directory_model),
                                                  &iter, parentpath);
        if (valid)
        {
            childpath = Find_Child_Node (self, &iter, textsplit[i]);
        }
        else
        {
            childpath = NULL;
        }

        if (childpath == NULL)
        {
            // ERROR! Could not find it!
            gchar *text_utf8 = g_filename_display_name (textsplit[i]);
            g_critical ("Error: Searching for %s, could not find node %s in tree.",
                        last_path, text_utf8);
            g_strfreev(textsplit);
            g_free(text_utf8);
            return;
        }
        gtk_tree_path_free(parentpath);
        parentpath = childpath;
    }

    gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->directory_model), &iter, parentpath);
    gtk_tree_path_free(parentpath);

    /* Rename the on-screen node */
    new_basename_utf8 = g_filename_display_basename (new_path);
    gtk_tree_store_set(priv->directory_model, &iter,
                       TREE_COLUMN_DIR_NAME,  new_basename_utf8,
                       TREE_COLUMN_FULL_PATH, new_path,
                       -1);

    /* Update fullpath of child nodes */
    Browser_Tree_Handle_Rename (self, &iter, last_path, new_path);

    /* Update the variable of the current path */
    path = Browser_Tree_Get_Path_Of_Selected_Node (self);
    file = g_file_new_for_path (path);
    et_browser_set_current_path (self, file);
    g_object_unref (file);
    g_free(path);

    g_strfreev(textsplit);
    g_free(new_basename_utf8);
}

/*
 * Browser_Tree_Handle_Rename:
 * @self: an #EtBrowser
 * @parentnode: the parent node in a tree model containing directory paths
 * @old_path: (type filename): the old path, in GLib filename encoding
 * @new_path: (type filename): the new path, in GLib filename encoding
 *
 * Recursive function to update paths of all child nodes.
 */
static void
Browser_Tree_Handle_Rename (EtBrowser *self,
                            GtkTreeIter *parentnode,
                            const gchar *old_path,
                            const gchar *new_path)
{
    EtBrowserPrivate *priv;
    GtkTreeIter iter;

    priv = et_browser_get_instance_private (self);

    /* If there are no children then nothing needs to be done! */
    if (!gtk_tree_model_iter_children (GTK_TREE_MODEL (priv->directory_model),
                                       &iter, parentnode))
    {
        return;
    }

    do
    {
        gchar *path;
        gsize path_shift;
        gchar *path_new;

        gtk_tree_model_get(GTK_TREE_MODEL(priv->directory_model), &iter,
                           TREE_COLUMN_FULL_PATH, &path, -1);
        if(path == NULL)
            continue;

        /* Graft the new path onto the old path. */
        path_shift = strlen (old_path);
        path_new = g_strconcat (new_path, path + path_shift, NULL);

        gtk_tree_store_set(priv->directory_model, &iter,
                           TREE_COLUMN_FULL_PATH, path_new, -1);

        g_free(path_new);
        g_free(path);

        /* Recurse if necessary. */
        if (gtk_tree_model_iter_has_child (GTK_TREE_MODEL (priv->directory_model),
                                           &iter))
        {
            Browser_Tree_Handle_Rename (self, &iter, old_path, new_path);
        }

    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->directory_model), &iter));

}

/*
 * Find the child node of "parentnode" that has text of "childtext
 * Returns NULL on failure
 */
static GtkTreePath *
Find_Child_Node (EtBrowser *self, GtkTreeIter *parentnode, gchar *childtext)
{
    EtBrowserPrivate *priv;
    gint row;
    GtkTreeIter iter;
    gchar *text;
    gchar *temp;

    priv = et_browser_get_instance_private (self);

    for (row=0; row < gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->directory_model), parentnode); row++)
    {
        if (row == 0)
        {
            if (gtk_tree_model_iter_children(GTK_TREE_MODEL(priv->directory_model), &iter, parentnode) == FALSE) return NULL;
        } else
        {
            if (gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->directory_model), &iter) == FALSE)
                return NULL;
        }
        gtk_tree_model_get(GTK_TREE_MODEL(priv->directory_model), &iter,
                           TREE_COLUMN_FULL_PATH, &temp, -1);
        text = g_path_get_basename(temp);
        g_free(temp);
        if(strcmp(childtext,text) == 0)
        {
            g_free(text);
            return gtk_tree_model_get_path(GTK_TREE_MODEL(priv->directory_model), &iter);
        }
        g_free(text);

    }

    return NULL;
}

/*
 * check_for_subdir:
 * @path: (type filename): the path to test
 *
 * Check if @path has any subdirectories.
 *
 * Returns: %TRUE if subdirectories exist, %FALSE otherwise
 */
static gboolean check_for_subdir(GFile *dir)
{
    gboolean show_hidden = g_settings_get_boolean (MainSettings, "browse-show-hidden");

    GFileEnumerator *enumerator = g_file_enumerate_children (dir,
        show_hidden ? G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN : G_FILE_ATTRIBUTE_STANDARD_TYPE,
        G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if (enumerator)
    {
        GFileInfo *childinfo;

        while ((childinfo = g_file_enumerator_next_file (enumerator,
                                                         NULL, NULL)))
        {
            if (g_file_info_get_file_type(childinfo) == G_FILE_TYPE_DIRECTORY
                && (show_hidden || !g_file_info_get_is_hidden(childinfo)))
            {
                g_object_unref (childinfo);
                g_file_enumerator_close (enumerator, NULL, NULL);
                g_object_unref (enumerator);
                return TRUE;
            }

            g_object_unref (childinfo);
        }

        g_file_enumerator_close (enumerator, NULL, NULL);
        g_object_unref (enumerator);
    }

    return FALSE;
}

/*
 * get_gicon_for_path:
 * @path: (type filename): path to create icon for
 * @path_state: whether the icon should be shown open or closed
 *
 * Check the permissions for the supplied @path (authorized?, readonly?,
 * unreadable?) and return an appropriate icon.
 *
 * Returns: an icon corresponding to the @path
 */
static const GIcon* get_gicon_for_path(EtBrowser *self, const gchar *path, EtPathState path_state)
{
	GFile *file;
	GFileInfo *info;
	GError *error = NULL;

	bool can_read = false;
	bool can_write = true;

	file = g_file_new_for_path (path);
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_ACCESS_CAN_READ "," G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
		G_FILE_QUERY_INFO_NONE, NULL, &error);
	if (info == NULL)
	{	g_warning(_("Error while querying path information: %s"), error->message);
		g_clear_error(&error);
	} else
	{	can_read = g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ);
		can_write = g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
		g_object_unref (info);
	}
	g_object_unref (file);

	EtBrowserPrivate* priv = et_browser_get_instance_private(self);

	switch (path_state)
	{
	case ET_PATH_STATE_CLOSED:
		if (!can_read)
			return priv->folder_unreadable_icon;
		if (!can_write)
			return priv->folder_readonly_icon;
		return priv->folder_icon;
	case ET_PATH_STATE_OPEN:
		if (!can_write)
			return priv->folder_open_readonly_icon;
		return priv->folder_open_icon;
	default:
		g_assert_not_reached();
		return NULL;
	}
}

/*
 * Open up a node on the browser tree
 * Scanning and showing all subdirectories
 */
static void
expand_cb (EtBrowser *self, GtkTreeIter *iter, GtkTreePath *gtreePath, GtkTreeView *tree)
{
    EtBrowserPrivate *priv;
    GFile *dir;
    GFileEnumerator *enumerator;
    gchar *fullpath_file;
    gchar *parentPath;
    gboolean treeScanned;
    gboolean has_subdir = FALSE;
    GtkTreeIter currentIter;
    GtkTreeIter subNodeIter;

    priv = et_browser_get_instance_private (self);

    g_return_if_fail (priv->directory_model != NULL);

    gtk_tree_model_get(GTK_TREE_MODEL(priv->directory_model), iter,
                       TREE_COLUMN_FULL_PATH, &parentPath,
                       TREE_COLUMN_SCANNED,   &treeScanned, -1);

    if (treeScanned)
        return;

    dir = g_file_new_for_path (parentPath);
    enumerator = g_file_enumerate_children (dir,
                                            G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                            G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                            G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                            G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN,
                                            G_FILE_QUERY_INFO_NONE,
                                            NULL, NULL);

    if (enumerator)
    {
        GFileInfo *childinfo;

        while ((childinfo = g_file_enumerator_next_file (enumerator,
                                                         NULL, NULL))
               != NULL)
        {
            GFile *child;
            gboolean isdir;

            child = g_file_enumerator_get_child (enumerator, childinfo);
            fullpath_file = g_file_get_path (child);
            isdir = g_file_info_get_file_type (childinfo) == G_FILE_TYPE_DIRECTORY;

            if (isdir &&
                (g_settings_get_boolean (MainSettings, "browse-show-hidden")
                 || !g_file_info_get_is_hidden (childinfo)))
            {
                const gchar *dirname_utf8;
                dirname_utf8 = g_file_info_get_display_name (childinfo);

                has_subdir = check_for_subdir(child);

                /* Select pixmap according permissions for the directory. */
                const GIcon* icon = get_gicon_for_path(self, fullpath_file, ET_PATH_STATE_CLOSED);

                gtk_tree_store_insert_with_values (priv->directory_model,
                                                   &currentIter, iter,
                                                   G_MAXINT,
                                                   TREE_COLUMN_DIR_NAME,
                                                   dirname_utf8,
                                                   TREE_COLUMN_FULL_PATH,
                                                   fullpath_file,
                                                   TREE_COLUMN_HAS_SUBDIR,
                                                   !has_subdir,
                                                   TREE_COLUMN_SCANNED, FALSE,
                                                   TREE_COLUMN_ICON, icon, -1);

                if (has_subdir)
                {
                    /* Insert a dummy node. */
                    gtk_tree_store_append(priv->directory_model, &subNodeIter, &currentIter);
                }
            }

            g_free (fullpath_file);
            g_object_unref (childinfo);
            g_object_unref (child);
        }

        g_file_enumerator_close (enumerator, NULL, NULL);
        g_object_unref (enumerator);

        /* Remove dummy node. */
        gtk_tree_model_iter_children (GTK_TREE_MODEL (priv->directory_model),
                                      &subNodeIter, iter);
        gtk_tree_store_remove (priv->directory_model, &subNodeIter);
    }

    g_object_unref (dir);
    const GIcon* icon = get_gicon_for_path(self, parentPath, ET_PATH_STATE_OPEN);

#ifdef G_OS_WIN32
    // set open folder pixmap except on drive (depth == 0)
    if (gtk_tree_path_get_depth(gtreePath) > 1)
    {
        // update the icon of the node to opened folder :-)
        gtk_tree_store_set(priv->directory_model, iter,
                           TREE_COLUMN_SCANNED, TRUE,
                           TREE_COLUMN_ICON, icon,
                           -1);
    }
#else /* !G_OS_WIN32 */
    // update the icon of the node to opened folder :-)
    gtk_tree_store_set(priv->directory_model, iter,
                       TREE_COLUMN_SCANNED, TRUE,
                       TREE_COLUMN_ICON, icon,
                       -1);
#endif /* !G_OS_WIN32 */

    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(priv->directory_model),
                                         TREE_COLUMN_DIR_NAME, GTK_SORT_ASCENDING);

    g_free(parentPath);
}

static void
collapse_cb (EtBrowser *self, GtkTreeIter *iter, GtkTreePath *treePath, GtkTreeView *tree)
{
    EtBrowserPrivate *priv;
    GtkTreeIter subNodeIter;
    gchar *path;
    GFile *file;
    GFileInfo *fileinfo;
    GError *error = NULL;

    priv = et_browser_get_instance_private (self);

    g_return_if_fail (priv->directory_model != NULL);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->directory_model), iter,
                        TREE_COLUMN_FULL_PATH, &path, -1);

    /* If the directory is not readable, do not delete its children. */
    file = g_file_new_for_path (path);
    g_free (path);
    fileinfo = g_file_query_info (file, G_FILE_ATTRIBUTE_ACCESS_CAN_READ,
                                  G_FILE_QUERY_INFO_NONE, NULL, &error);
    g_object_unref (file);

    if (fileinfo)
    {
        if (!g_file_info_get_attribute_boolean (fileinfo,
                                                G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
        {
            g_object_unref (fileinfo);
            return;
        }

        g_object_unref (fileinfo);
    }

    gtk_tree_model_iter_children(GTK_TREE_MODEL(priv->directory_model),
                                 &subNodeIter, iter);
    while (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(priv->directory_model), iter))
    {
        gtk_tree_model_iter_children(GTK_TREE_MODEL(priv->directory_model), &subNodeIter, iter);
        gtk_tree_store_remove(priv->directory_model, &subNodeIter);
    }

    gtk_tree_model_get (GTK_TREE_MODEL (priv->directory_model), iter,
                        TREE_COLUMN_FULL_PATH, &path, -1);
    const GIcon* icon = get_gicon_for_path(self, path, ET_PATH_STATE_OPEN);
    g_free (path);
#ifdef G_OS_WIN32
    // set closed folder pixmap except on drive (depth == 0)
    if(gtk_tree_path_get_depth(treePath) > 1)
    {
        // update the icon of the node to closed folder :-)
        gtk_tree_store_set(priv->directory_model, iter,
                           TREE_COLUMN_SCANNED, FALSE,
                           TREE_COLUMN_ICON, icon, -1);
    }
#else /* !G_OS_WIN32 */
    // update the icon of the node to closed folder :-)
    gtk_tree_store_set(priv->directory_model, iter,
                       TREE_COLUMN_SCANNED, FALSE,
                       TREE_COLUMN_ICON, icon, -1);
#endif /* !G_OS_WIN32 */

    /* Insert dummy node only if directory exists. */
    if (error)
    {
        /* Remove the parent (missing) directory from the tree. */
        if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
            gtk_tree_store_remove (priv->directory_model, iter);
        }

        g_error_free (error);
    }
    else
    {
        gtk_tree_store_append (priv->directory_model, &subNodeIter, iter);
    }
}

static void on_visible_columns_changed(EtBrowser *self, const gchar *key, GSettings *settings)
{
	FileColumnRenderer::ShowHideColumns(et_browser_get_instance_private(self)->file_view, (EtColumn)g_settings_get_flags(settings, key));
}

static void
on_sort_mode_changed (EtBrowser *self, const gchar *key, GSettings *settings)
{
    EtBrowserPrivate *priv;
    EtSortMode sort_mode;
    GtkTreeViewColumn *column;

    priv = et_browser_get_instance_private (self);

    sort_mode = (EtSortMode)g_settings_get_enum (settings, key);

    /* If the column to sort is different than the old sorted column. */
    if (sort_mode / 2 != priv->file_sort_mode / 2)
    {
        column = et_browser_get_column_for_sort_mode(self, priv->file_sort_mode);
        if (column != NULL)
            gtk_tree_view_column_set_sort_indicator(column, FALSE);
    }

    /* New sort mode is for a column with a visible counterpart. */
    column = et_browser_get_column_for_sort_mode(self, sort_mode);
    if (column != NULL)
    {
        /* Even is GTK_SORT_ASCENDING, odd is GTK_SORT_DESCENDING. */
        gtk_tree_view_column_set_sort_order (column, sort_mode & 1 ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING);
        gtk_tree_view_column_set_sort_indicator (column, TRUE);
    }

    /* Store the new sort mode. */
    priv->file_sort_mode = sort_mode;

    et_browser_refresh_sort (self);
}

/*
 * Open the file selection window and saves the selected file path into entry
 */
static void
open_file_selection_dialog (GtkWidget *entry,
                            const gchar *title,
                            GtkFileChooserAction action)
{
    const gchar *tmp;
    gchar *filename, *filename_utf8;
    GtkWidget *dialog;
    GtkWindow *parent_window = NULL;
    gint response;

    parent_window = (GtkWindow*) gtk_widget_get_toplevel (entry);
    if (!gtk_widget_is_toplevel (GTK_WIDGET (parent_window)))
    {
        g_warning ("%s", "Could not get parent window");
        return;
    }

    dialog = gtk_file_chooser_dialog_new (title, parent_window, action,
                                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                                          _("_Open"), GTK_RESPONSE_ACCEPT,
                                          NULL);

    /* Set initial directory. */
    tmp = gtk_entry_get_text (GTK_ENTRY (entry));

    if (tmp && *tmp)
    {
        if (!gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), tmp))
        {
            gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                                 tmp);
        }
    }

    response = gtk_dialog_run (GTK_DIALOG (dialog));

    if (response == GTK_RESPONSE_ACCEPT)
    {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        filename_utf8 = g_filename_display_name (filename);
        gtk_entry_set_text (GTK_ENTRY (entry), filename_utf8);
        g_free (filename);
        g_free (filename_utf8);
        /* Useful for the button on the main window. */
        gtk_widget_grab_focus (GTK_WIDGET (entry));
        g_signal_emit_by_name (entry, "activate");
    }

    gtk_widget_destroy (dialog);
}

/* File selection window */
static void
File_Selection_Window_For_File (GtkWidget *entry)
{
    open_file_selection_dialog (entry, _("Select File"),
                                GTK_FILE_CHOOSER_ACTION_OPEN);
}

static void
File_Selection_Window_For_Directory (GtkWidget *entry)
{
    open_file_selection_dialog (entry, _("Select Directory"),
                                GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
}

static gboolean
album_list_separator_func (GtkTreeModel *model,
                           GtkTreeIter *iter,
                           gpointer user_data)
{
    gint state;
    gtk_tree_model_get (model, iter, ALBUM_STATE, &state, -1);
    return (state & ALBUM_STATE_SEPARATOR) != 0;
}

static void set_cell_data(GtkTreeViewColumn* column, GtkCellRenderer* cell, GtkTreeModel* model, GtkTreeIter* iter, gpointer data)
{
	const ET_File *file;
	gtk_tree_model_get(model, iter, LIST_FILE_POINTER, &file, -1);
	auto renderer = (const FileColumnRenderer*)data;
	string text = renderer->RenderText(file);
	bool saved = file->is_saved();
	bool changed = !saved
		&& (renderer->Column < ET_SORT_MODE_ASCENDING_CREATION_DATE || renderer->Column >= ET_SORT_MODE_ASCENDING_REPLAYGAIN)
		&& text != renderer->RenderText(file, true);
	if (changed && text.length() == 0)
		text = "\xe2\x90\xa0"; // ␠ Symbol for Space
	FileColumnRenderer::SetText(GTK_CELL_RENDERER_TEXT(cell),
		text.c_str(), file->activate_bg_color, saved ? FileColumnRenderer::NORMAL : changed ? FileColumnRenderer::STRONGHIGHLIGHT : FileColumnRenderer::HIGHLIGHT);
}

/*
 * Create item of the browser (Entry + Tree + List).
 */
static void
create_browser (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    gsize i;
    GtkBuilder *builder;
    GMenuModel *menu_model;
    GFile *file;

    priv = et_browser_get_instance_private (self);

    /* History list */
    Load_Path_Entry_List (priv->entry_model, MISC_COMBO_TEXT);

    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (priv->entry_combo)),
                              "activate", G_CALLBACK (Browser_Entry_Activated),
                              self);
    /* The button to select a directory to browse. */
    g_signal_connect_swapped (priv->open_button, "clicked",
                              G_CALLBACK (File_Selection_Window_For_Directory),
                              gtk_bin_get_child (GTK_BIN (priv->entry_combo)));

    /* Icons */
    {   priv->folder_icon = g_themed_icon_new("folder");
        priv->folder_open_icon = g_themed_icon_new("folder-open");
        GIcon* emblem_icon = g_themed_icon_new("emblem-readonly");
        GEmblem* emblem = g_emblem_new_with_origin(emblem_icon, G_EMBLEM_ORIGIN_LIVEMETADATA);
        priv->folder_readonly_icon = g_emblemed_icon_new(priv->folder_icon, emblem);
        priv->folder_open_readonly_icon = g_emblemed_icon_new(priv->folder_open_icon, emblem);
        g_object_unref(emblem);
        g_object_unref(emblem_icon);
        emblem_icon = g_themed_icon_new("emblem-unreadable");
        emblem = g_emblem_new_with_origin(emblem_icon, G_EMBLEM_ORIGIN_LIVEMETADATA);
        priv->folder_unreadable_icon = g_emblemed_icon_new(priv->folder_icon, emblem);
        g_object_unref(emblem);
        g_object_unref(emblem_icon);
    }

    /* The tree view */
    Browser_Tree_Initialize (self);

    /* Create popup menu on browser tree view. */
    builder = gtk_builder_new_from_resource ("/org/gnome/EasyTAG/menus.ui");

    menu_model = G_MENU_MODEL (gtk_builder_get_object (builder,
                                                       "directory-menu"));
    priv->directory_view_menu = gtk_menu_new_from_model (menu_model);
    gtk_menu_attach_to_widget (GTK_MENU (priv->directory_view_menu), priv->directory_view, NULL);

    /* The ScrollWindows with the Artist and Album Lists. */
    priv->artist_selected_handler = g_signal_connect_swapped(gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->artist_view)),
        "changed", G_CALLBACK(Browser_Artist_List_Row_Selected), self);

    /* Create popup menu on browser artist list. */
    menu_model = G_MENU_MODEL (gtk_builder_get_object (builder,
                                                       "directory-artist-menu"));
    priv->artist_menu = gtk_menu_new_from_model (menu_model);
    gtk_menu_attach_to_widget (GTK_MENU (priv->artist_menu), priv->artist_view,
                               NULL);

    gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (priv->album_view),
                                          album_list_separator_func, NULL,
                                          NULL);

    priv->album_selected_handler = g_signal_connect_swapped(gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->album_view)),
        "changed", G_CALLBACK(Browser_Album_List_Row_Selected), self);

    /* Create Popup Menu on browser album list. */
    menu_model = G_MENU_MODEL (gtk_builder_get_object (builder,
                                                       "directory-album-menu"));
    priv->album_menu = gtk_menu_new_from_model (menu_model);
    gtk_menu_attach_to_widget (GTK_MENU (priv->album_menu), priv->album_view,
                               NULL);

    /* The file list */
    /* Add columns to tree view. See ET_FILE_LIST_COLUMN. */
    GEnumClass *enum_class = (GEnumClass*)g_type_class_ref(ET_TYPE_SORT_MODE);
    for (i = 0; i < gtk_tree_view_get_n_columns(priv->file_view); i++)
    {
        GtkTreeViewColumn *column = gtk_tree_view_get_column (priv->file_view, i);
        string id = FileColumnRenderer::ColumnName2Nick(GTK_BUILDABLE(column));

        g_object_set_data (G_OBJECT (column), "browser", self);

        // rendering method
        GtkCellRenderer* renderer = GTK_CELL_RENDERER(GTK_CELL_LAYOUT_GET_IFACE(column)->get_cells(GTK_CELL_LAYOUT(column))->data);
        auto rdr = FileColumnRenderer::Get_Renderer(id.c_str());
        if (!rdr)
        	g_error("No renderer with name %s found.", id.c_str());
        gtk_tree_view_column_set_cell_data_func(column, renderer, &set_cell_data, (gpointer)rdr, NULL);

        // sort action
        id.insert(0, "ascending-");
        GEnumValue* enum_value = g_enum_get_value_by_nick(enum_class, id.c_str());
        if (enum_value == NULL)
        	g_error("No sort mode with name %s found.", id.c_str());

        g_signal_connect (column, "clicked", G_CALLBACK (et_browser_on_column_clicked), enum_value);
    }
    g_type_class_unref(enum_class);

    g_signal_connect_swapped(MainSettings, "changed::visible-columns", G_CALLBACK(on_visible_columns_changed), self);
    on_visible_columns_changed(self, "visible-columns", MainSettings);

    g_signal_connect_swapped (MainSettings, "changed::sort-mode", G_CALLBACK (on_sort_mode_changed), self);
    // To sort list
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->file_model), 0, GTK_SORT_ASCENDING);
    on_sort_mode_changed(self, "sort-mode", MainSettings);

    priv->file_selected_handler = g_signal_connect_swapped (gtk_tree_view_get_selection (priv->file_view),
                                                            "changed",
                                                            G_CALLBACK (Browser_List_Row_Selected),
                                                            self);

    /* Create popup menu on file list. */
    menu_model = G_MENU_MODEL (gtk_builder_get_object (builder, "file-menu"));
    priv->file_menu = gtk_menu_new_from_model (menu_model);
    gtk_menu_attach_to_widget (GTK_MENU (priv->file_menu), GTK_WIDGET(priv->file_view),
                               NULL);

    g_object_unref (builder);

    /* The list store for run program combos. */
    priv->run_program_model = gtk_list_store_new(MISC_COMBO_COUNT, G_TYPE_STRING);

    /* TODO: Give the browser area a sensible default size. */

    /* Set home variable as current path */
    file = g_file_new_for_path (g_get_home_dir ());
    et_browser_set_current_path (self, file);
    g_object_unref (file);
}

/*
 * et_browser_on_column_clicked:
 * @column: the tree view column to sort
 * @data: the (required) #ET_Sorting_Type, converted to a pointer with
 * #GINT_TO_POINTER
 *
 * Set the sort mode and display appropriate sort indicator when
 * column is clicked.
 */
static void
et_browser_on_column_clicked (GtkTreeViewColumn *column, gpointer data)
{
    GEnumValue* ev = (GEnumValue*)data;
    if (ev == NULL)
        return;

    EtBrowser *self = ET_BROWSER(g_object_get_data(G_OBJECT(column), "browser"));
    EtBrowserPrivate *priv = et_browser_get_instance_private (self);
    gint value = ev->value + ((priv->file_sort_mode ^ ev->value) == 0);

    g_settings_set_enum (MainSettings, "sort-mode", value);
}

/*******************************
 * Scanner To Rename Directory *
 *******************************/
static void
rename_directory_generate_preview (EtBrowser *self)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    ET_File *etfile = MainWindow->get_displayed_file();
    if (!etfile
        || !priv->rename_directory_dialog || !priv->rename_directory_mask_entry || !priv->rename_directory_preview_label)
        return;

    gString mask(g_settings_get_string (MainSettings, "rename-directory-default-mask"));
    if (!mask)
        return;

    string preview_text = et_evaluate_mask(etfile, mask, FALSE);

    if (GTK_IS_LABEL(priv->rename_directory_preview_label))
    {
        if (!preview_text.empty())
        {
            //gtk_label_set_text(GTK_LABEL(priv->rename_file_preview_label),preview_text);
            gchar *tmp_string = g_markup_printf_escaped("%s",preview_text.c_str()); // To avoid problem with strings containing characters like '&'
            gchar *str = g_strdup_printf("<i>%s</i>",tmp_string);
            gtk_label_set_markup(GTK_LABEL(priv->rename_directory_preview_label),str);
            g_free(tmp_string);
            g_free(str);
        } else
        {
            gtk_label_set_text(GTK_LABEL(priv->rename_directory_preview_label),"");
        }

        // Force the window to be redrawed else the preview label may be not placed correctly
        gtk_widget_queue_resize(priv->rename_directory_dialog);
    }
}

/*
 * The window to Rename a directory into the browser.
 */
void
et_browser_show_rename_directory_dialog (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    GtkBuilder *builder;
    GtkWidget *label;
    GtkWidget *button;
    GFile *parent;
    gchar *parent_path;
    gchar *basename;
    gchar *display_basename;
    gchar *string;

    priv = et_browser_get_instance_private (self);

    if (priv->rename_directory_dialog != NULL)
    {
        gtk_window_present(GTK_WINDOW(priv->rename_directory_dialog));
        return;
    }

    /* We get the full path but we musn't display the parent directories */
    parent = g_file_get_parent (priv->current_path);

    if (!parent)
    {
        return;
    }

    parent_path = g_file_get_path (parent);
    g_object_unref (parent);

    if (!parent_path)
    {
        return;
    }

    basename = g_file_get_basename (priv->current_path);

    if (!basename)
    {
        return;
    }

    display_basename = g_filename_display_name (basename);

    builder = gtk_builder_new_from_resource ("/org/gnome/EasyTAG/browser_dialogs.ui");

    priv->rename_directory_dialog = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                        "rename_directory_dialog"));

    gtk_window_set_transient_for (GTK_WINDOW (priv->rename_directory_dialog),
                                  GTK_WINDOW (MainWindow));

    /* We attach useful data to the combobox */
    g_object_set_data_full (G_OBJECT (priv->rename_directory_dialog),
                            "Parent_Directory", parent_path, g_free);
    g_object_set_data_full (G_OBJECT (priv->rename_directory_dialog),
                            "Current_Directory", basename, g_free);
    g_signal_connect (priv->rename_directory_dialog, "response",
                      G_CALLBACK (et_rename_directory_on_response), self);

    string = g_strdup_printf (_("Rename the directory ‘%s’ to:"),
                              display_basename);
    label = GTK_WIDGET (gtk_builder_get_object (builder, "rename_label"));
    gtk_label_set_label (GTK_LABEL (label), string);
    g_free (string);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);

    /* The entry to rename the directory. */
    priv->rename_directory_entry = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                       "rename_entry"));
    /* Set the directory into the combobox */
    gtk_entry_set_text (GTK_ENTRY (priv->rename_directory_entry),
                        display_basename);

    /* Rename directory : check box + entry + Status icon */
    priv->rename_directory_mask_toggle = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                             "rename_mask_check"));
    g_settings_bind (MainSettings, "rename-directory-with-mask",
                     priv->rename_directory_mask_toggle, "active",
                     G_SETTINGS_BIND_DEFAULT);
    g_signal_connect_swapped (priv->rename_directory_mask_toggle, "toggled",
                              G_CALLBACK (Rename_Directory_With_Mask_Toggled),
                              self);

    /* The entry to enter the mask to apply. */
    priv->rename_directory_mask_entry = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                            "rename_mask_entry"));
    gtk_widget_set_size_request(priv->rename_directory_mask_entry, 80, -1);

    /* Signal to generate preview (preview of the new directory). */
    g_signal_connect_swapped (priv->rename_directory_mask_entry,
                              "changed",
                              G_CALLBACK (rename_directory_generate_preview),
                              self);

    g_settings_bind (MainSettings, "rename-directory-default-mask",
                     priv->rename_directory_mask_entry, "text",
                     G_SETTINGS_BIND_DEFAULT);

    /* Mask status icon. Signal connection to check if mask is correct to the
     * mask entry. */
    g_signal_connect (priv->rename_directory_mask_entry, "changed",
                      G_CALLBACK (entry_check_mask), NULL);

    /* Preview label. */
    priv->rename_directory_preview_label = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                               "rename_preview_label"));
    /* Button to save: to rename directory */
    button = gtk_dialog_get_widget_for_response (GTK_DIALOG (priv->rename_directory_dialog),
                                                 GTK_RESPONSE_APPLY);
    g_signal_connect_swapped (priv->rename_directory_entry,
                              "changed",
                              G_CALLBACK (empty_entry_disable_widget),
                              G_OBJECT (button));

    g_object_unref (builder);

    gtk_widget_show_all (priv->rename_directory_dialog);

    /* To initialize the 'Use mask' check button state. */
    g_signal_emit_by_name (G_OBJECT (priv->rename_directory_mask_toggle),
                           "toggled");

    /* To initialize PreviewLabel + MaskStatusIconBox. */
    g_signal_emit_by_name (priv->rename_directory_mask_entry, "changed");

    g_free (display_basename);
}

static void
Destroy_Rename_Directory_Window (EtBrowser *self)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    if (priv->rename_directory_dialog)
    {
        gtk_widget_destroy(priv->rename_directory_dialog);
        priv->rename_directory_preview_label = NULL;
        priv->rename_directory_dialog = NULL;
    }
}

static void
Rename_Directory (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    gchar *directory_parent;
    gchar *directory_last_name;
    string directory_new_name;
    gchar *directory_new_name_file;
    gchar *last_path;
    gchar *last_path_utf8;
    gchar *new_path;
    gchar *new_path_utf8;
    gchar *tmp_path;
    gchar *tmp_path_utf8;
    gint   fd_tmp;

    priv = et_browser_get_instance_private (self);

    g_return_if_fail (priv->rename_directory_dialog != NULL);

    directory_parent    = (gchar*)g_object_get_data(G_OBJECT(priv->rename_directory_dialog),"Parent_Directory");
    directory_last_name = (gchar*)g_object_get_data(G_OBJECT(priv->rename_directory_dialog),"Current_Directory");

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->rename_directory_mask_toggle)))
    {
        /* Renamed from mask. */
        gString mask(g_settings_get_string(MainSettings, "rename-directory-default-mask"));
        // TODO the current file may not even be part of the directory to rename.
        directory_new_name = et_evaluate_mask(MainWindow->get_displayed_file(), mask, FALSE);
    }
    else
    {
        /* Renamed 'manually'. */
        directory_new_name = gtk_entry_get_text(GTK_ENTRY(priv->rename_directory_entry));
    }

    /* Check if a name for the directory have been supplied */
    if (directory_new_name.empty())
    {
        GtkWidget *msgdialog;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "%s",
                                           _("You must type a directory name"));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Directory Name Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
        return;
    }

    /* Check that we can write the new directory name */
    directory_new_name_file = filename_from_display(directory_new_name.c_str());
    if (!directory_new_name_file)
    {
        GtkWidget *msgdialog;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL  | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           _("Could not convert ‘%s’ into filename encoding"),
                                           directory_new_name.c_str());
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msgdialog),
                                                  _("Please use another name."));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Directory Name Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);
        g_free(directory_new_name_file);
    }

    /* If the directory name haven't been changed, we do nothing! */
    if (directory_last_name && directory_new_name_file
    && strcmp(directory_last_name,directory_new_name_file)==0)
    {
        Destroy_Rename_Directory_Window (self);
        g_free(directory_new_name_file);
        return;
    }

    /* Build the current and new absolute paths */
    last_path = g_build_filename (directory_parent, directory_last_name, NULL);
    last_path_utf8 = g_filename_display_name (last_path);
    new_path = g_build_filename (directory_parent, directory_new_name_file,
                                 NULL);
    new_path_utf8 = g_filename_display_name (new_path);

    /* TODO: Replace with g_file_move(). */
    /* Check if the new directory name doesn't already exists, and detect if
     * it's only a case change (needed for vfat) */
    if (g_file_test (new_path, G_FILE_TEST_IS_DIR))
    {
        GtkWidget *msgdialog;
        //gint response;

        if (strcasecmp(last_path,new_path) != 0)
        {
    // TODO
    //        // The same directory already exists. So we ask if we want to move the files
    //        msg = g_strdup_printf(_("The directory already exists!\n(%s)\nDo you want "
    //            "to move the files?"),new_path_utf8);
    //        msgbox = msg_box_new(_("Confirm"),
    //                             GTK_WINDOW(MainWindow),
    //                             NULL,
    //                             GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
    //                             msg,
    //                             GTK_STOCK_DIALOG_QUESTION,
    //                             GTK_STOCK_NO,  GTK_RESPONSE_NO,
	//                             GTK_STOCK_YES, GTK_RESPONSE_YES,
    //                             NULL);
    //        g_free(msg);
    //        response = gtk_dialog_run(GTK_DIALOG(msgbox));
    //        gtk_widget_destroy(msgbox);
    //
    //        switch (response)
    //        {
    //            case GTK_STOCK_YES:
    //                // Here we must rename all files with the new location, and remove the directory
    //
    //                Rename_File ()
    //
    //                break;
    //            case BUTTON_NO:
    //                break;
    //        }

            msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s",
                                               "Cannot rename file");
            gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msgdialog),
                                                      _("The directory name ‘%s’ already exists."),new_path_utf8);
            gtk_window_set_title(GTK_WINDOW(msgdialog),_("Rename File Error"));

            gtk_dialog_run(GTK_DIALOG(msgdialog));
            gtk_widget_destroy(msgdialog);

            g_free(directory_new_name_file);
            g_free(last_path);
            g_free(last_path_utf8);
            g_free(new_path);
            g_free(new_path_utf8);

            return;
        }
    }

    /* Temporary path (useful when changing only string case) */
    tmp_path = g_strdup_printf("%s.XXXXXX",last_path);
    tmp_path_utf8 = g_filename_display_name (tmp_path);

    if ((fd_tmp = g_mkstemp (tmp_path)) >= 0)
    {
        /* TODO: handle error. */
        g_close (fd_tmp, NULL);
        g_unlink (tmp_path);
    }

    /* Rename the directory from 'last name' to 'tmp name' */
    if (g_rename (last_path, tmp_path) != 0)
    {
        GtkWidget *msgdialog;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Cannot rename directory '%s' to '%s'",
                                           last_path_utf8,
                                           tmp_path_utf8);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Rename Directory Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);

        g_free(directory_new_name_file);
        g_free(last_path);
        g_free(last_path_utf8);
        g_free(new_path);
        g_free(new_path_utf8);
        g_free(tmp_path);
        g_free(tmp_path_utf8);

        return;
    }

    /* Rename the directory from 'tmp name' to 'new name' (final name) */
    if (g_rename (tmp_path, new_path) != 0)
    {
        GtkWidget *msgdialog;

        msgdialog = gtk_message_dialog_new(GTK_WINDOW(MainWindow),
                                           GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Cannot rename directory '%s' to '%s",
                                           tmp_path_utf8,
                                           new_path_utf8);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(msgdialog),"%s",g_strerror(errno));
        gtk_window_set_title(GTK_WINDOW(msgdialog),_("Rename Directory Error"));

        gtk_dialog_run(GTK_DIALOG(msgdialog));
        gtk_widget_destroy(msgdialog);

        g_free(directory_new_name_file);
        g_free(last_path);
        g_free(last_path_utf8);
        g_free(new_path);
        g_free(new_path_utf8);
        g_free(tmp_path);
        g_free(tmp_path_utf8);

        return;
    }

    ET_FileList::update_directory_name(last_path, new_path);
    Browser_Tree_Rename_Directory (self, last_path, new_path);

    // To update file path in the browser entry
    ET_File* file = MainWindow->get_displayed_file();
    if (file)
    {
        et_application_window_update_ui_from_et_file(MainWindow, ET_COLUMN_FILEPATH);
    }else
    {
        gchar *tmp = g_file_get_parse_name (et_browser_get_current_path (self));
        et_browser_entry_set_text (self, tmp);
        g_free (tmp);
    }

    Destroy_Rename_Directory_Window (self);
    g_free(last_path);
    g_free(last_path_utf8);
    g_free(new_path);
    g_free(new_path_utf8);
    g_free(tmp_path);
    g_free(tmp_path_utf8);
    g_free(directory_new_name_file);
    et_application_window_status_bar_message(MainWindow, _("Directory renamed"), TRUE);
}

static void
Rename_Directory_With_Mask_Toggled (EtBrowser *self)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    gtk_widget_set_sensitive (priv->rename_directory_entry,
                              !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->rename_directory_mask_toggle)));
    gtk_widget_set_sensitive (priv->rename_directory_mask_entry,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->rename_directory_mask_toggle)));
    gtk_widget_set_sensitive (priv->rename_directory_preview_label,
                              gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->rename_directory_mask_toggle)));
}


/*
 * Window where is typed the name of the program to run, which
 * receives the current directory as parameter.
 */
void
et_browser_show_open_directory_with_dialog (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    GtkBuilder *builder;
    GtkWidget *button;
    gchar *current_directory = NULL;

    g_return_if_fail (ET_BROWSER (self));

    priv = et_browser_get_instance_private (self);

    if (priv->open_directory_with_dialog != NULL)
    {
        gtk_window_present(GTK_WINDOW(priv->open_directory_with_dialog));
        return;
    }

    /* Current directory. */
    if (!priv->current_path)
    {
        return;
    }

    current_directory = g_file_get_path (priv->current_path);

    builder = gtk_builder_new_from_resource ("/org/gnome/EasyTAG/browser_dialogs.ui");

    priv->open_directory_with_dialog = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                           "open_directory_dialog"));

    gtk_window_set_transient_for (GTK_WINDOW (priv->open_directory_with_dialog),
                                  GTK_WINDOW (MainWindow));
    g_signal_connect (priv->open_directory_with_dialog, "response",
                      G_CALLBACK (et_run_program_tree_on_response), self);

    /* The combobox to enter the program to run */
    priv->open_directory_with_combobox = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                             "open_directory_combo"));
    gtk_combo_box_set_model (GTK_COMBO_BOX (priv->open_directory_with_combobox),
                             GTK_TREE_MODEL (priv->run_program_model));

    /* History list */
    gtk_list_store_clear (priv->run_program_model);
    Load_Run_Program_With_Directory_List (priv->run_program_model,
                                          MISC_COMBO_TEXT);
    g_signal_connect_swapped (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->open_directory_with_combobox))),
                              "activate",
                              G_CALLBACK (Run_Program_With_Directory),
                              self);

    /* The button to Browse */
    button = GTK_WIDGET (gtk_builder_get_object (builder,
                                                 "open_directory_button"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (File_Selection_Window_For_File),
                              G_OBJECT (gtk_bin_get_child (GTK_BIN (priv->open_directory_with_combobox))));

    /* We attach useful data to the combobox (into Run_Program_With_Directory) */
    g_object_set_data_full (G_OBJECT (priv->open_directory_with_combobox),
                            "Current_Directory", current_directory, g_free);

    /* Button to execute */
    button = gtk_dialog_get_widget_for_response (GTK_DIALOG (priv->open_directory_with_dialog),
                                                 GTK_RESPONSE_OK);
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Run_Program_With_Directory),
                              self);
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (priv->open_directory_with_combobox)),
                              "changed",
                              G_CALLBACK (empty_entry_disable_widget),
                              G_OBJECT (button));
    g_signal_emit_by_name (G_OBJECT (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->open_directory_with_combobox)))),
                           "changed", NULL);

    gtk_widget_show_all (priv->open_directory_with_dialog);
}

static void
Destroy_Run_Program_Tree_Window (EtBrowser *self)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    if (priv->open_directory_with_dialog)
    {
        gtk_widget_hide (priv->open_directory_with_dialog);
    }
}

void
Run_Program_With_Directory (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    gchar *program_name;
    gchar *current_directory;
    GList *args_list = NULL;
    gboolean program_ran;
    GError *error = NULL;

    priv = et_browser_get_instance_private (self);

    program_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->open_directory_with_combobox)))));
    current_directory = (gchar*)g_object_get_data (G_OBJECT (priv->open_directory_with_combobox), "Current_Directory");

    // List of parameters (here only one! : the current directory)
    args_list = g_list_append(args_list,current_directory);

    program_ran = et_run_program (program_name, args_list, &error);
    g_list_free(args_list);

    if (program_ran)
    {
        gchar *msg;

        // Append newest choice to the drop down list
        Add_String_To_Combo_List(priv->run_program_model, program_name);

        // Save list attached to the combobox
        Save_Run_Program_With_Directory_List(priv->run_program_model, MISC_COMBO_TEXT);

        Destroy_Run_Program_Tree_Window (self);

        msg = g_strdup_printf (_("Executed command ‘%s’"), program_name);
        et_application_window_status_bar_message(MainWindow, msg, TRUE);

        g_free (msg);
    }
    else
    {
        Log_Print (LOG_ERROR, _("Failed to launch program ‘%s’"),
                   error->message);
        g_clear_error (&error);
    }

    g_free(program_name);
}

static void
Run_Program_With_Selected_Files (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    gchar   *program_name;
    GList   *selected_paths;
    GList *l;
    gListP<const char*> args_list;
    GtkTreeIter iter;
    gboolean program_ran;
    GError *error = NULL;

    priv = et_browser_get_instance_private (self);

    if (!GTK_IS_COMBO_BOX (priv->open_files_with_combobox) || ET_FileList::empty())
        return;

    // Program name to run
    program_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->open_files_with_combobox)))));

    // List of files to pass as parameters
    selected_paths = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(priv->file_view), NULL);

    for (l = selected_paths; l != NULL; l = g_list_next (l))
    {
        if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->file_model), &iter,
                                     (GtkTreePath *)l->data))
        {
            const ET_File *ETFile;

            gtk_tree_model_get (GTK_TREE_MODEL (priv->file_model), &iter,
                                LIST_FILE_POINTER, &ETFile, -1);

            args_list = args_list.prepend(ETFile->FilePath.get());
            //args_list = g_list_append(args_list,((File_Name *)ETFile->FileNameCur->data)->value_utf8);
        }
    }

    args_list = args_list.reverse();
    program_ran = et_run_program (program_name, args_list, &error);

    g_list_free_full (selected_paths, (GDestroyNotify)gtk_tree_path_free);
    g_list_free(args_list);

    if (program_ran)
    {
        gchar *msg;

        // Append newest choice to the drop down list
        //gtk_list_store_prepend(GTK_LIST_STORE(priv->run_program_model), &iter);
        //gtk_list_store_set(priv->run_program_model, &iter, MISC_COMBO_TEXT, program_name, -1);
        Add_String_To_Combo_List(GTK_LIST_STORE(priv->run_program_model), program_name);

        // Save list attached to the combobox
        Save_Run_Program_With_File_List(priv->run_program_model, MISC_COMBO_TEXT);

        Destroy_Run_Program_List_Window (self);

        msg = g_strdup_printf (_("Executed command ‘%s’"), program_name);
        et_application_window_status_bar_message(MainWindow, msg, TRUE);

        g_free (msg);
    }
    else
    {
        Log_Print (LOG_ERROR, _("Failed to launch program ‘%s’"),
                   error->message);
        g_clear_error (&error);
    }

    g_free(program_name);
}

/*
 * Window where is typed the name of the program to run, which
 * receives the current file as parameter.
 */
void
et_browser_show_open_files_with_dialog (EtBrowser *self)
{
    EtBrowserPrivate *priv;
    GtkBuilder *builder;
    GtkWidget *button;

    g_return_if_fail (ET_BROWSER (self));

    priv = et_browser_get_instance_private (self);

    if (priv->open_files_with_dialog != NULL)
    {
        gtk_window_present(GTK_WINDOW(priv->open_files_with_dialog));
        return;
    }

    builder = gtk_builder_new_from_resource ("/org/gnome/EasyTAG/browser_dialogs.ui");

    priv->open_files_with_dialog = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                       "open_files_dialog"));
    gtk_window_set_transient_for (GTK_WINDOW (priv->open_files_with_dialog),
                                  GTK_WINDOW (MainWindow));
    g_signal_connect ((priv->open_files_with_dialog), "response",
                      G_CALLBACK (et_run_program_list_on_response), self);

    /* The combobox to enter the program to run */
    priv->open_files_with_combobox = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                         "open_files_combo"));
    gtk_combo_box_set_model (GTK_COMBO_BOX (priv->open_files_with_combobox),
                             GTK_TREE_MODEL (priv->run_program_model));
    gtk_widget_set_size_request (GTK_WIDGET (priv->open_files_with_combobox),
                                 250, -1);

    /* History list */
    gtk_list_store_clear (priv->run_program_model);
    Load_Run_Program_With_File_List (priv->run_program_model, MISC_COMBO_TEXT);
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (priv->open_files_with_combobox)),
                              "activate",
                              G_CALLBACK (Run_Program_With_Selected_Files),
			                  self);

    /* The button to Browse */
    button = GTK_WIDGET (gtk_builder_get_object (builder,
                                                 "open_files_button"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (File_Selection_Window_For_File),
                              G_OBJECT (gtk_bin_get_child (GTK_BIN (priv->open_files_with_combobox))));

    g_object_unref (builder);

    /* Button to execute */
    button = gtk_dialog_get_widget_for_response (GTK_DIALOG (priv->open_files_with_dialog),
                                                 GTK_RESPONSE_OK);
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Run_Program_With_Selected_Files),
			                  self);
    g_signal_connect_swapped (gtk_bin_get_child (GTK_BIN (priv->open_files_with_combobox)),
                              "changed",
                              G_CALLBACK (empty_entry_disable_widget),
                              G_OBJECT (button));
    g_signal_emit_by_name (gtk_bin_get_child (GTK_BIN (priv->open_files_with_combobox)),
                           "changed", NULL);

    gtk_widget_show_all (priv->open_files_with_dialog);
}

static void
Destroy_Run_Program_List_Window (EtBrowser *self)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (self);

    if (priv->open_files_with_dialog)
    {
        gtk_widget_hide (priv->open_files_with_dialog);
    }
}

/*
 * empty_entry_disable_widget:
 * @widget: a widget to set sensitive if @entry contains text
 * @entry: the entry for which to test the text
 *
 * Make @widget insensitive if @entry contains no text, or sensitive otherwise.
 */
static void
empty_entry_disable_widget (GtkWidget *widget, GtkEntry *entry)
{
    const gchar *text;

    g_return_if_fail (widget != NULL && entry != NULL);

    text = gtk_entry_get_text (GTK_ENTRY (entry));

    gtk_widget_set_sensitive (widget, text && *text);
}

/*
 * et_rename_directory_on_response:
 * @dialog: the dialog which emitted the response signal
 * @response_id: the response ID
 * @user_data: user data set when the signal was connected
 *
 * Signal handler for the rename directory dialog.
 */
static void
et_rename_directory_on_response (GtkDialog *dialog, gint response_id,
                                 gpointer user_data)
{
    EtBrowser *self;

    self = ET_BROWSER (user_data);

    switch (response_id)
    {
        case GTK_RESPONSE_APPLY:
            Rename_Directory (self);
            break;
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
            Destroy_Rename_Directory_Window (self);
            break;
        default:
            g_assert_not_reached ();
    }
}

/*
 * et_run_program_tree_on_response:
 * @dialog: the dialog which emitted the response signal
 * @response_id: the response ID
 * @user_data: user data set when the signal was connected
 *
 * Signal handler for the run program on directory tree dialog.
 */
static void
et_run_program_tree_on_response (GtkDialog *dialog, gint response_id,
                                 gpointer user_data)
{
    EtBrowser *self;

    self = ET_BROWSER (user_data);

    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            /* FIXME: Ignored for now. */
            break;
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
            Destroy_Run_Program_Tree_Window (self);
            break;
        default:
            g_assert_not_reached ();
    }
}
/*
 * et_run_program_list_on_response:
 * @dialog: the dialog which emitted the response signal
 * @response_id: the response ID
 * @user_data: user data set when the signal was connected
 *
 * Signal handler for the run program on selected file dialog.
 */
static void
et_run_program_list_on_response (GtkDialog *dialog, gint response_id,
                                 gpointer user_data)
{
    EtBrowser *self;

    self = ET_BROWSER (user_data);

    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            /* FIXME: Ignored for now. */
            break;
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
            Destroy_Run_Program_List_Window (self);
            break;
        default:
            g_assert_not_reached ();
    }
}

/*
 * et_browser_get_column_for_sort_mode:
 * @sort_mode: the sort mode of the #GtkTreeViewColumn to fetch
 *
 * Gets the browser list treeview column for the given column ID.
 *
 * Returns: (transfer none): the tree view column corresponding to @sort_mode
 */
static GtkTreeViewColumn *
et_browser_get_column_for_sort_mode(EtBrowser *self, EtSortMode sort_mode)
{
    EtBrowserPrivate *priv = et_browser_get_instance_private(self);

    GEnumValue* ev = g_enum_get_value((GEnumClass*)g_type_class_ref(ET_TYPE_SORT_MODE), sort_mode);
    /* remove ascending/descending, append "_column", replace '-' by '_' */
    gchar* column_id = g_strconcat(strchr(ev->value_nick, '-') + 1, "_column", NULL);
    for (char* cp = column_id; *cp != '_'; ++cp)
    	if (*cp == '-')
    		*cp = '_';

    GtkTreeViewColumn *column;
    gint i = 0;
    while ((column = gtk_tree_view_get_column(priv->file_view, i++)) != NULL)
    	if (strcmp(gtk_buildable_get_name(GTK_BUILDABLE(column)), column_id) == 0)
    		break;

    g_free(column_id);
    return column;
}

static void
et_browser_destroy (GtkWidget *widget)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (ET_BROWSER (widget));

    /* Save combobox history list before exit. */
    if (priv->entry_model)
    {
        Save_Path_Entry_List (priv->entry_model, MISC_COMBO_TEXT);
        priv->entry_model = NULL;
        /* The model is disposed when the combo box is disposed. */
    }

    if (priv->file_model)
        ET_BROWSER(widget)->clear();

    GTK_WIDGET_CLASS (et_browser_parent_class)->destroy (widget);
}

static void
et_browser_finalize (GObject *object)
{
    EtBrowserPrivate *priv;

    priv = et_browser_get_instance_private (ET_BROWSER (object));

    g_clear_object (&priv->current_path);
    g_free(priv->current_path_name);
    priv->current_path_name = NULL;
    g_clear_object (&priv->run_program_model);

    g_object_unref(priv->folder_icon);
    g_object_unref(priv->folder_open_icon);
    g_object_unref(priv->folder_readonly_icon);
    g_object_unref(priv->folder_open_readonly_icon);
    g_object_unref(priv->folder_unreadable_icon);

    G_OBJECT_CLASS (et_browser_parent_class)->finalize (object);
}

static void
et_browser_init (EtBrowser *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
    create_browser (self);
}

static
void et_browser_class_init (EtBrowserClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    G_OBJECT_CLASS (klass)->finalize = et_browser_finalize;
    widget_class->destroy = et_browser_destroy;

    gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/EasyTAG/browser.ui");
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, files_label);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, open_button);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, browser_paned);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, entry_model);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, entry_combo);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, directory_album_artist_notebook);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, file_model);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, file_view);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, album_model);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, album_view);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, artist_model);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, artist_view);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, directory_model);
    gtk_widget_class_bind_template_child_private(widget_class, EtBrowser, directory_view);
    gtk_widget_class_bind_template_callback(widget_class, collapse_cb);
    gtk_widget_class_bind_template_callback(widget_class, expand_cb);
    gtk_widget_class_bind_template_callback(widget_class, on_album_tree_button_press_event);
    gtk_widget_class_bind_template_callback(widget_class, on_artist_tree_button_press_event);
    gtk_widget_class_bind_template_callback(widget_class, on_directory_tree_button_press_event);
    gtk_widget_class_bind_template_callback(widget_class, on_file_tree_button_press_event);
    gtk_widget_class_bind_template_callback(widget_class, on_album_tree_popup_menu);
    gtk_widget_class_bind_template_callback(widget_class, on_artist_tree_popup_menu);
    gtk_widget_class_bind_template_callback(widget_class, on_directory_tree_popup_menu);
    gtk_widget_class_bind_template_callback(widget_class, on_file_tree_popup_menu);
    gtk_widget_class_bind_template_callback(widget_class, Browser_Entry_Activated);
    gtk_widget_class_bind_template_callback(widget_class, Browser_List_Key_Press);
    gtk_widget_class_bind_template_callback(widget_class, Browser_Tree_Key_Press);
    gtk_widget_class_bind_template_callback(widget_class, Browser_Tree_Node_Selected);
}

/*
 * et_browser_new:
 *
 * Create a new EtBrowser instance.
 *
 * Returns: a new #EtBrowser
 */
EtBrowser *
et_browser_new (void)
{
    return (EtBrowser*)g_object_new (ET_TYPE_BROWSER, NULL);
}
