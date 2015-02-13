/* EasyTAG - tag editor for audio files
 * Copyright (C) 2013,2014  David King <amigadave@amigadave.com>
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

#include "load_files_dialog.h"

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "application_window.h"
#include "browser.h"
#include "charset.h"
#include "easytag.h"
#include "log.h"
#include "misc.h"
#include "picture.h"
#include "scan_dialog.h"
#include "setting.h"

/* TODO: Use G_DEFINE_TYPE_WITH_PRIVATE. */
G_DEFINE_TYPE (EtLoadFilesDialog, et_load_files_dialog, GTK_TYPE_DIALOG)

#define et_load_files_dialog_get_instance_private(dialog) (dialog->priv)

static const guint BOX_SPACING = 6;

enum
{
    LOAD_FILE_CONTENT_TEXT,
    LOAD_FILE_CONTENT_COUNT
};

enum
{
    LOAD_FILE_NAME_TEXT,
    LOAD_FILE_NAME_POINTER,
    LOAD_FILE_NAME_COUNT
};

struct _EtLoadFilesDialogPrivate
{
    GtkWidget *file_chooser;
    GtkWidget *load_file_content_view;
    GtkListStore *load_file_content_model;
    GtkWidget *load_file_content_menu;
    GtkWidget *load_file_name_view;
    GtkListStore *load_file_name_model;
    GtkWidget *load_file_name_menu;

    GtkWidget *selected_line_entry;
};


/*
 * Set the new filename of each file.
 * Associate lines from priv->load_file_content_view with priv->load_file_name_view
 */
static void
Load_Filename_Set_Filenames (EtLoadFilesDialog *self)
{
    EtLoadFilesDialogPrivate *priv;
    gint row;
    ET_File *ETFile = NULL;
    File_Name *FileName;
    gchar *list_text = NULL;
    gint rowcount;
    gboolean found;

    GtkTreePath *currentPath = NULL;
    GtkTreeIter iter_name;
    GtkTreeIter iter_content;

    priv = et_load_files_dialog_get_instance_private (self);

    if ( !ETCore->ETFileList || !priv->load_file_content_view || !priv->load_file_name_view)
        return;

    /* Save current file */
    ET_Save_File_Data_From_UI(ETCore->ETFileDisplayed);

    rowcount = MIN(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->load_file_name_model), NULL),
                   gtk_tree_model_iter_n_children(GTK_TREE_MODEL(priv->load_file_content_model), NULL));

    for (row=0; row < rowcount; row++)
    {
        if (row == 0)
            currentPath = gtk_tree_path_new_first();
        else
            gtk_tree_path_next(currentPath);

        found = gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->load_file_name_model), &iter_name, currentPath);
        if (found)
            gtk_tree_model_get(GTK_TREE_MODEL(priv->load_file_name_model), &iter_name, 
                               LOAD_FILE_NAME_POINTER, &ETFile, -1);

        found = gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->load_file_content_model), &iter_content, currentPath);
        if (found)
            gtk_tree_model_get(GTK_TREE_MODEL(priv->load_file_content_model), &iter_content, 
                               LOAD_FILE_CONTENT_TEXT, &list_text, -1);

        if (ETFile && list_text && (g_utf8_strlen (list_text, -1) > 0))
        {
            gchar *list_text_tmp;
            gchar *filename_new_utf8;

            list_text_tmp = g_strdup(list_text);
            et_filename_prepare (list_text_tmp,
                                 g_settings_get_boolean (MainSettings,
                                                         "rename-replace-illegal-chars"));

            /* Build the filename with the path */
            filename_new_utf8 = ET_File_Name_Generate(ETFile,list_text_tmp);
            g_free(list_text_tmp);

            /* Set the new filename */
            // Create a new 'File_Name' item
            FileName = et_file_name_new ();
            // Save changes of the 'File_Name' item
            ET_Set_Filename_File_Name_Item(FileName,filename_new_utf8,NULL);
            ET_Manage_Changes_Of_File_Data(ETFile,FileName,NULL);

            g_free(filename_new_utf8);

            /* Then run current scanner if requested. */
            if (g_settings_get_boolean (MainSettings,
                                        "load-filenames-run-scanner"))
            {
                EtScanDialog *dialog;

                dialog = ET_SCAN_DIALOG (et_application_window_get_scan_dialog (ET_APPLICATION_WINDOW (MainWindow)));

                if (dialog)
                {
                    Scan_Select_Mode_And_Run_Scanner (dialog, ETFile);
                }
            }
        }
        g_free(list_text);
    }

    gtk_tree_path_free(currentPath);

    et_application_window_browser_refresh_list (ET_APPLICATION_WINDOW (MainWindow));
    ET_Display_File_Data_To_UI(ETCore->ETFileDisplayed);
}

/*
 * on_response:
 * @dialog: the dialog which emitted the response signal
 * @response_id: the response ID
 * @user_data: user data set when the signal was connected
 *
 * Signal handler for the load filenames from text file dialog.
 */
static void
on_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
    switch (response_id)
    {
        case GTK_RESPONSE_APPLY:
            Load_Filename_Set_Filenames (ET_LOAD_FILES_DIALOG (dialog));
            break;
        case GTK_RESPONSE_CANCEL:
            gtk_widget_hide (GTK_WIDGET (dialog));
            break;
        case GTK_RESPONSE_DELETE_EVENT:
            break;
        default:
            g_assert_not_reached ();
    }
}

/*
 * To enable/disable sensitivity of the button 'Load'
 */
static void
set_load_button_sensitivity (GtkWidget *button, GtkWidget *chooser)
{
    GFile *file;
    GFileInfo *info;
    GError *error = NULL;

    g_return_if_fail (button != NULL && chooser != NULL);

    file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (chooser));

    if (!file)
    {
        gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
        return;
    }

    info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE,
                              G_FILE_QUERY_INFO_NONE, NULL, &error);
    g_object_unref (file);

    if (info && G_FILE_TYPE_REGULAR == g_file_info_get_file_type (info))
    {
        gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
    }
    else
    {
        gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

        if (!info)
        {
            Log_Print (LOG_ERROR, _("Cannot query file information ‘%s’"),
                       error->message);
            g_error_free (error);
            return;
        }
    }

    g_object_unref (info);
}

/*
 * Load content of the file into the priv->load_file_content_view list
 */
static void
Load_File_Content (G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
    EtLoadFilesDialogPrivate *priv;
    GFile *file;
    GFileInputStream *istream;
    GDataInputStream *data;
    GError *error = NULL;
    gsize size_read;
    gchar *path;
    gchar *filename_utf8;
    gchar *line;
    gchar *valid;

    priv = et_load_files_dialog_get_instance_private (ET_LOAD_FILES_DIALOG (user_data));
    file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (priv->file_chooser));

    /* The file to read. */
    path = g_file_get_path (file);
    filename_utf8 = filename_to_display (path);
    g_free (path);

    istream = g_file_read (file, NULL, &error);
    g_object_unref (file);

    if (!istream)
    {
        Log_Print (LOG_ERROR, _("Cannot open file ‘%s’: %s"), filename_utf8,
                   error->message);
        g_error_free (error);
        g_object_unref (file);
        g_free (filename_utf8);
        return;
    }

    g_free (filename_utf8);
    data = g_data_input_stream_new (G_INPUT_STREAM (istream));
    /* TODO: Find a safer alternative to _ANY. */
    g_data_input_stream_set_newline_type (data,
                                          G_DATA_STREAM_NEWLINE_TYPE_ANY);
    gtk_list_store_clear (priv->load_file_content_model);

    while ((line = g_data_input_stream_read_line (data, &size_read, NULL,
                                                  &error)))
    {
        /* FIXME: This should use the GLib filename encoding, not UTF-8. */
        valid = Try_To_Validate_Utf8_String (line);
        g_free (line);

        gtk_list_store_insert_with_values (priv->load_file_content_model, NULL,
                                           G_MAXINT, LOAD_FILE_CONTENT_TEXT,
                                           valid, -1);
        g_free (valid);
    }

    if (error)
    {
        Log_Print (LOG_ERROR, _("Error reading file ‘%s’"), error->message);
        g_error_free (error);
    }

    g_object_unref (data);
    g_object_unref (istream);
}

/*
 * Delete the selected line in the treeview passed as parameter
 */
static void
Load_Filename_List_Delete_Line (GtkWidget *treeview)
{
    GtkTreeSelection *selection;
    GtkTreeIter selectedIter, itercopy;
    GtkTreeModel *model;
    gboolean rowafter;

    g_return_if_fail (treeview != NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

    if (gtk_tree_selection_get_selected(selection, &model, &selectedIter) != TRUE)
    {
        return;
    }

    // If there is a line following the one about to be removed, select it for convenience
    itercopy = selectedIter;
    rowafter = gtk_tree_model_iter_next(model, &itercopy);

    // Remove the line to be deleted
    gtk_list_store_remove(GTK_LIST_STORE(model), &selectedIter);

    if (rowafter)
        gtk_tree_selection_select_iter(selection, &itercopy);
}

/*
 * Insert a blank line before the selected line in the treeview passed as parameter
 */
static void
Load_Filename_List_Insert_Blank_Line (GtkWidget *treeview)
{
    GtkTreeSelection *selection;
    GtkTreeIter selectedIter;
    GtkTreeIter *temp;
    GtkTreeModel *model;

    g_return_if_fail (treeview != NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

    if (gtk_tree_selection_get_selected(selection, &model, &selectedIter) != TRUE)
        return;

    temp = &selectedIter; /* Not used here, but it must be non-NULL to keep GTK+ happy! */
    gtk_list_store_insert_before(GTK_LIST_STORE(model), temp, &selectedIter);
}

static gboolean
Load_Filename_List_Key_Press (GtkWidget *treeview, GdkEvent *event)
{
    if (event && event->type == GDK_KEY_PRESS)
    {
        GdkEventKey *kevent = (GdkEventKey *)event;

        switch(kevent->keyval)
        {
            case GDK_KEY_Delete:
                Load_Filename_List_Delete_Line(treeview);
                return GDK_EVENT_STOP;
                break;
            case GDK_KEY_I:
            case GDK_KEY_i:
                Load_Filename_List_Insert_Blank_Line(treeview);
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
 * Delete all blank lines in the treeview passed as parameter
 */
static void
Load_Filename_List_Delete_All_Blank_Lines (GtkWidget *treeview)
{
    gchar *text = NULL;
    GtkTreeIter iter;
    GtkTreeModel *model;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    while (TRUE)
    {
        gtk_tree_model_get(model, &iter, LOAD_FILE_NAME_TEXT, &text, -1);

        /* Check for blank entry */
        if (!text || g_utf8_strlen(text, -1) == 0)
        {
            g_free(text);

            if (!gtk_list_store_remove(GTK_LIST_STORE(model), &iter))
                break;
            else
                continue;
        }
        g_free(text);

        if (!gtk_tree_model_iter_next(model, &iter))
            break;
    }
}

/*
 * Move up the selected line in the treeview passed as parameter
 */
static void
Load_Filename_List_Move_Up (GtkWidget *treeview)
{
    GtkTreeSelection *selection;
    GList *selectedRows;
    GList *l;
    GtkTreeIter currentFile;
    GtkTreeIter nextFile;
    GtkTreePath *currentPath;
    GtkTreeModel *treemodel;

    g_return_if_fail (treeview != NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    treemodel = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    selectedRows = gtk_tree_selection_get_selected_rows(selection, NULL);

    if (!selectedRows)
    {
        return;
    }

    for (l = selectedRows; l != NULL; l = g_list_next (l))
    {
        currentPath = (GtkTreePath *)l->data;

        if (gtk_tree_model_get_iter (treemodel, &currentFile, currentPath))
        {
            // Find the entry above the node...
            if (gtk_tree_path_prev(currentPath))
            {
                // ...and if it exists, swap the two rows by iter
                gtk_tree_model_get_iter(treemodel, &nextFile, currentPath);
                gtk_list_store_swap(GTK_LIST_STORE(treemodel), &currentFile, &nextFile);
            }
        }
    }

    g_list_free_full (selectedRows, (GDestroyNotify)gtk_tree_path_free);
}

/*
 * Move down the selected line in the treeview passed as parameter
 */
static void
Load_Filename_List_Move_Down (GtkWidget *treeview)
{
    GtkTreeSelection *selection;
    GList *selectedRows;
    GList *l;
    GtkTreeIter currentFile;
    GtkTreeIter nextFile;
    GtkTreePath *currentPath;
    GtkTreeModel *treemodel;

    g_return_if_fail (treeview != NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    treemodel = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    selectedRows = gtk_tree_selection_get_selected_rows(selection, NULL);

    if (!selectedRows)
    {
        return;
    }

    for (l = selectedRows; l != NULL; l = g_list_next (l))
    {
        currentPath = (GtkTreePath *)l->data;

        if (gtk_tree_model_get_iter (treemodel, &currentFile, currentPath))
        {
            // Find the entry below the node and swap the two nodes by iter
            gtk_tree_path_next(currentPath);
            if (gtk_tree_model_get_iter(treemodel, &nextFile, currentPath))
                gtk_list_store_swap(GTK_LIST_STORE(treemodel), &currentFile, &nextFile);
        }
    }

    g_list_free_full (selectedRows, (GDestroyNotify)gtk_tree_path_free);
}

/*
 * Load the names of the current list of files
 */
static void
Load_File_List (EtLoadFilesDialog *self)
{
    EtLoadFilesDialogPrivate *priv;
    GList *l;
    ET_File *etfile;
    gchar *filename_utf8;
    gchar *pos;

    priv = et_load_files_dialog_get_instance_private (self);

    gtk_list_store_clear(priv->load_file_name_model);

    for (l = g_list_first (ETCore->ETFileList); l != NULL; l = g_list_next (l))
    {
        etfile = (ET_File *)l->data;
        filename_utf8 = g_path_get_basename(((File_Name *)etfile->FileNameNew->data)->value_utf8);
        // Remove the extension ('filename' must be allocated to don't affect the initial value)
        if ((pos=strrchr(filename_utf8,'.'))!=NULL)
            *pos = 0;
        gtk_list_store_insert_with_values (priv->load_file_name_model, NULL,
                                           G_MAXINT, LOAD_FILE_NAME_TEXT,
                                           filename_utf8,
                                           LOAD_FILE_NAME_POINTER, l->data,
                                           -1);
        g_free(filename_utf8);
    }
}

static void
on_load_file_name_view_reload_clicked (G_GNUC_UNUSED GtkButton *button,
                                       gpointer user_data)
{
    Load_File_List (ET_LOAD_FILES_DIALOG (user_data));
}

static void
on_load_file_content_view_reload_clicked (G_GNUC_UNUSED GtkButton *button,
                                          gpointer user_data)
{
    Load_File_Content (NULL, ET_LOAD_FILES_DIALOG (user_data));
}

/*
 * To select the corresponding row in the other list
 */
static void
Load_Filename_Select_Row_In_Other_List (GtkWidget* treeview_target,
                                        GtkTreeSelection *selection_orig)
{
    GtkAdjustment *ct_adj, *ce_adj;
    GtkTreeSelection *selection_target;
    GtkTreeView *treeview_orig;
    GtkTreeModel *treemodel_orig;
    GtkTreeModel *treemodel_target;
    GtkTreeIter iter_orig;
    GtkTreeIter iter_target;
    GtkTreePath *path_orig;
    gint *indices_orig;
    gchar *stringiter;

    g_return_if_fail (treeview_target != NULL && selection_orig != NULL);

    selection_target = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview_target));
    treemodel_target = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview_target));

    if (!gtk_tree_selection_get_selected(selection_orig, &treemodel_orig, &iter_orig))
        return; /* Might be called with no selection */

    treeview_orig = gtk_tree_selection_get_tree_view(selection_orig);
    path_orig = gtk_tree_model_get_path(treemodel_orig, &iter_orig);
    gtk_tree_selection_unselect_all(selection_target);

    /* Synchronize the two lists. */
    ce_adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (treeview_orig));
    ct_adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(treeview_target));

    if (gtk_adjustment_get_upper(GTK_ADJUSTMENT(ct_adj)) >= gtk_adjustment_get_page_size(GTK_ADJUSTMENT(ct_adj))
    &&  gtk_adjustment_get_upper(GTK_ADJUSTMENT(ce_adj)) >= gtk_adjustment_get_page_size(GTK_ADJUSTMENT(ce_adj)))
    {
        // Rules are displayed in the both clist
        if (gtk_adjustment_get_value(GTK_ADJUSTMENT(ce_adj)) <= gtk_adjustment_get_upper(GTK_ADJUSTMENT(ct_adj)) - gtk_adjustment_get_page_size(GTK_ADJUSTMENT(ct_adj)))
        {
            gtk_adjustment_set_value(GTK_ADJUSTMENT(ct_adj),gtk_adjustment_get_value(GTK_ADJUSTMENT(ce_adj)));
        } else
        {
            gtk_adjustment_set_value(GTK_ADJUSTMENT(ct_adj),gtk_adjustment_get_upper(GTK_ADJUSTMENT(ct_adj)) - gtk_adjustment_get_page_size(GTK_ADJUSTMENT(ct_adj)));
            indices_orig = gtk_tree_path_get_indices(path_orig);

            if (indices_orig[0] <= (gtk_tree_model_iter_n_children(treemodel_target, NULL) - 1))
                gtk_adjustment_set_value(GTK_ADJUSTMENT(ce_adj),gtk_adjustment_get_value(GTK_ADJUSTMENT(ct_adj)));

        }
    }else if (gtk_adjustment_get_upper(GTK_ADJUSTMENT(ct_adj)) < gtk_adjustment_get_page_size(GTK_ADJUSTMENT(ct_adj))) // Target Clist rule not visible
    {
        indices_orig = gtk_tree_path_get_indices(path_orig);

        if (indices_orig[0] <= (gtk_tree_model_iter_n_children(treemodel_target, NULL) - 1))
            gtk_adjustment_set_value(GTK_ADJUSTMENT(ce_adj),gtk_adjustment_get_value(GTK_ADJUSTMENT(ct_adj)));
    }

    // Must block the select signal of the target to avoid looping
    g_signal_handlers_block_by_func(G_OBJECT(selection_target), G_CALLBACK(Load_Filename_Select_Row_In_Other_List), G_OBJECT(treeview_orig));

    stringiter = gtk_tree_model_get_string_from_iter(treemodel_orig, &iter_orig);
    if (gtk_tree_model_get_iter_from_string(treemodel_target, &iter_target, stringiter))
    {
        gtk_tree_selection_select_iter(selection_target, &iter_target);
    }

    g_free(stringiter);
    g_signal_handlers_unblock_by_func(G_OBJECT(selection_target), G_CALLBACK(Load_Filename_Select_Row_In_Other_List), G_OBJECT(treeview_orig));
}

static void
create_load_file_content_view_popup (EtLoadFilesDialog *self)
{
    EtLoadFilesDialogPrivate *priv;
    GtkWidget *list;
    GtkWidget *BrowserPopupMenu;
    GtkWidget *MenuItem;

    priv = et_load_files_dialog_get_instance_private (self);
    list = priv->load_file_name_view;

    BrowserPopupMenu = gtk_menu_new();
    gtk_menu_attach_to_widget (GTK_MENU (BrowserPopupMenu), list, NULL);
    
    MenuItem = gtk_menu_item_new_with_label(_("Insert a blank line"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu), MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate", G_CALLBACK(Load_Filename_List_Insert_Blank_Line), G_OBJECT(list));

    MenuItem = gtk_menu_item_new_with_label(_("Delete this line"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate", G_CALLBACK(Load_Filename_List_Delete_Line), G_OBJECT(list));

    MenuItem = gtk_menu_item_new_with_label(_("Delete all blank lines"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate", G_CALLBACK(Load_Filename_List_Delete_All_Blank_Lines),G_OBJECT(list));

    MenuItem = gtk_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);

    MenuItem = gtk_menu_item_new_with_label (_("Move this line up"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate", G_CALLBACK(Load_Filename_List_Move_Up),G_OBJECT(list));

    MenuItem = gtk_menu_item_new_with_label (_("Move this line down"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate", G_CALLBACK(Load_Filename_List_Move_Down),G_OBJECT(list));

    MenuItem = gtk_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);

    MenuItem = gtk_menu_item_new_with_label(_("Reload"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);
    g_signal_connect (MenuItem, "activate",
                      G_CALLBACK (on_load_file_name_view_reload_clicked),
                      self);

    gtk_widget_show_all(BrowserPopupMenu);

    priv->load_file_content_menu = BrowserPopupMenu;
}

static void
do_name_view_popup_menu (EtLoadFilesDialog *self,
                         GdkEventButton *event)
{
    EtLoadFilesDialogPrivate *priv;
    gint button;
    gint event_time;

    priv = et_load_files_dialog_get_instance_private (self);

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
    gtk_menu_popup (GTK_MENU (priv->load_file_content_menu), NULL, NULL, NULL,
                    NULL, button, event_time);
}

static gboolean
on_name_view_popup_menu (GtkWidget *treeview,
                         EtLoadFilesDialog *self)
{
    do_name_view_popup_menu (self, NULL);

    return GDK_EVENT_STOP;
}

/*
 * Handle button press events from the file name view. */
static gboolean
on_name_view_button_press_event (GtkWidget *treeview,
                                 GdkEventButton *event,
                                 EtLoadFilesDialog *self)
{
    if (gdk_event_triggers_context_menu ((GdkEvent *)event))
    {
        do_name_view_popup_menu (self, event);

        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static void
create_load_file_name_view_popup (EtLoadFilesDialog *self)
{
    EtLoadFilesDialogPrivate *priv;
    GtkWidget *list;
    GtkWidget *BrowserPopupMenu;
    GtkWidget *MenuItem;

    priv = et_load_files_dialog_get_instance_private (self);
    list = priv->load_file_content_view;

    BrowserPopupMenu = gtk_menu_new();
    gtk_menu_attach_to_widget (GTK_MENU (BrowserPopupMenu), list, NULL);
    
    MenuItem = gtk_menu_item_new_with_label (_("Insert a blank line"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu), MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate", G_CALLBACK(Load_Filename_List_Insert_Blank_Line), G_OBJECT(list));

    MenuItem = gtk_menu_item_new_with_label (_("Delete this line"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate", G_CALLBACK(Load_Filename_List_Delete_Line), G_OBJECT(list));

    MenuItem = gtk_menu_item_new_with_label (_("Delete all blank lines"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate", G_CALLBACK(Load_Filename_List_Delete_All_Blank_Lines),G_OBJECT(list));

    MenuItem = gtk_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);

    MenuItem = gtk_menu_item_new_with_label (_("Move this line up"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate", G_CALLBACK(Load_Filename_List_Move_Up),G_OBJECT(list));

    MenuItem = gtk_menu_item_new_with_label (_("Move this line down"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);
    g_signal_connect_swapped(G_OBJECT(MenuItem),"activate", G_CALLBACK(Load_Filename_List_Move_Down),G_OBJECT(list));

    MenuItem = gtk_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);

    MenuItem = gtk_menu_item_new_with_label (_("Reload"));
    gtk_menu_shell_append(GTK_MENU_SHELL(BrowserPopupMenu),MenuItem);
    g_signal_connect (MenuItem, "activate",
                      G_CALLBACK (on_load_file_content_view_reload_clicked),
                      self);

    gtk_widget_show_all(BrowserPopupMenu);

    priv->load_file_name_menu = BrowserPopupMenu;
}

static void
do_content_view_popup_menu (EtLoadFilesDialog *self,
                            GdkEventButton *event)
{
    EtLoadFilesDialogPrivate *priv;
    gint button;
    gint event_time;

    priv = et_load_files_dialog_get_instance_private (self);

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
    gtk_menu_popup (GTK_MENU (priv->load_file_name_menu), NULL, NULL, NULL,
                    NULL, button, event_time);
}

static gboolean
on_content_view_popup_menu (GtkWidget *treeview,
                            EtLoadFilesDialog *self)
{
    do_content_view_popup_menu (self, NULL);

    return GDK_EVENT_STOP;
}

/*
 * Handle button press events from the file content view. */
static gboolean
on_content_view_button_press_event (GtkWidget *treeview,
                                    GdkEventButton *event,
                                    EtLoadFilesDialog *self)
{
    if (gdk_event_triggers_context_menu ((GdkEvent *)event))
    {
        do_content_view_popup_menu (self, event);

        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

/*
 * Update the text of the selected line into the list, with the text entered into the entry
 */
static void
Load_Filename_Update_Text_Line(GtkWidget *entry, GtkWidget *list)
{
    GtkTreeIter SelectedRow;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    gboolean hasSelectedRows;

    g_return_if_fail (entry != NULL || list != NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    hasSelectedRows = gtk_tree_selection_get_selected(selection, &model, &SelectedRow);
    if (hasSelectedRows)
    {
        const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
        gtk_list_store_set(GTK_LIST_STORE(model), &SelectedRow, LOAD_FILE_CONTENT_TEXT, text, -1);
    }
}

/*
 * Set the text of the selected line of the list into the entry
 */
static void
Load_Filename_Edit_Text_Line (GtkTreeSelection *selection, gpointer user_data)
{
    EtLoadFilesDialogPrivate *priv;
    gchar *text;
    GtkTreeIter selectedIter;
    GtkEntry *entry;
    gulong handler;

    priv = et_load_files_dialog_get_instance_private (ET_LOAD_FILES_DIALOG (user_data));

    entry = GTK_ENTRY (priv->selected_line_entry);

    if (gtk_tree_selection_get_selected(selection, NULL, &selectedIter) != TRUE)
        return;

    gtk_tree_model_get(GTK_TREE_MODEL(priv->load_file_content_model), &selectedIter, LOAD_FILE_NAME_TEXT, &text, -1);

    handler = g_signal_handler_find(entry, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, Load_Filename_Update_Text_Line, NULL);
    g_signal_handler_block(entry, handler);
    if (text)
    {
        gtk_entry_set_text(entry, text);
        g_free(text);
    } else
        gtk_entry_set_text(entry, "");

    g_signal_handler_unblock(entry, handler);
}

/*
 * The window to load the filenames from a txt.
 */
static void
create_load_files_dialog (EtLoadFilesDialog *self)
{
    EtLoadFilesDialogPrivate *priv;
    GtkWidget *content_area;
    GtkBuilder *builder;
    GError *error = NULL;
    GtkWidget *grid;
    GtkWidget *button;
    GtkWidget *load_file_run_scanner;
    const gchar *path;

    priv = et_load_files_dialog_get_instance_private (self);

    gtk_window_set_title (GTK_WINDOW (self),
                          _("Load Filenames From a Text File"));
    gtk_window_set_destroy_with_parent (GTK_WINDOW (self), TRUE);
    gtk_dialog_add_buttons (GTK_DIALOG (self), _("_Close"),
                            GTK_RESPONSE_CANCEL, _("_Apply"),
                            GTK_RESPONSE_APPLY, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_APPLY);
    g_signal_connect (self, "response", G_CALLBACK (on_response), NULL);
    g_signal_connect (self, "delete-event",
                      G_CALLBACK (gtk_widget_hide_on_delete), NULL);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (self));
    gtk_container_set_border_width (GTK_CONTAINER (self), BOX_SPACING);
    gtk_box_set_spacing (GTK_BOX (content_area), BOX_SPACING);

    builder = gtk_builder_new ();
    gtk_builder_add_from_resource (builder,
                                   "/org/gnome/EasyTAG/load_files_dialog.ui",
                                   &error);

    if (error != NULL)
    {
        g_error ("Unable to get scanner page from resource: %s",
                 error->message);
    }

    grid = GTK_WIDGET (gtk_builder_get_object (builder, "file_grid"));
    gtk_container_add (GTK_CONTAINER (content_area), grid);

    /* File to load. */
    priv->file_chooser = GTK_WIDGET (gtk_builder_get_object (builder,
                                                             "file_chooser"));
    /* Initial value. */
    if ((path = et_application_window_get_current_path (ET_APPLICATION_WINDOW (MainWindow))) != NULL)
    {
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (priv->file_chooser),
                                             path);
    }
    // the 'changed' signal is attached below to enable/disable the button to load
    /* Button 'load'
     * the signal attached to this button, to load the file, is placed after
     * the priv->load_file_content_view definition. */
    button = GTK_WIDGET (gtk_builder_get_object (builder, "file_load_button"));
    g_signal_connect_swapped (priv->file_chooser, "file-set",
                              G_CALLBACK (set_load_button_sensitivity),
                              button);

    /* Content of the loaded file. */
    priv->load_file_content_model = GTK_LIST_STORE (gtk_builder_get_object (builder,
                                                                            "file_content_model"));
    priv->load_file_content_view = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                       "file_content_view"));
    
    /* Signal to automatically load the file. */
    g_signal_connect (button, "clicked", G_CALLBACK (Load_File_Content), self);
    g_signal_connect (priv->load_file_content_view, "key-press-event",
                      G_CALLBACK (Load_Filename_List_Key_Press), NULL);

    /* Commands (like the popup menu). */
    button = GTK_WIDGET (gtk_builder_get_object (builder,
                                                 "content_insert_blank"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Load_Filename_List_Insert_Blank_Line),
                              G_OBJECT (priv->load_file_content_view));

    button = GTK_WIDGET (gtk_builder_get_object (builder, "content_remove"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Load_Filename_List_Delete_Line),
                              G_OBJECT (priv->load_file_content_view));
    
    button = GTK_WIDGET (gtk_builder_get_object (builder,
                                                 "content_remove_blank"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Load_Filename_List_Delete_All_Blank_Lines),
                              G_OBJECT (priv->load_file_content_view));
    
    button = GTK_WIDGET (gtk_builder_get_object (builder, "content_up"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Load_Filename_List_Move_Up),
                              G_OBJECT (priv->load_file_content_view));

    button = GTK_WIDGET (gtk_builder_get_object (builder, "content_down"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Load_Filename_List_Move_Down),
                              G_OBJECT (priv->load_file_content_view));

    button = GTK_WIDGET (gtk_builder_get_object (builder, "content_reload"));
    g_signal_connect (button, "clicked",
                      G_CALLBACK (on_load_file_content_view_reload_clicked),
                      self);
    
    /* List of current filenames. */
    priv->load_file_name_model = GTK_LIST_STORE (gtk_builder_get_object (builder,
                                                                         "file_name_model"));
    priv->load_file_name_view = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                    "file_name_view"));
    
    g_signal_connect (priv->load_file_name_view, "key-press-event",
                      G_CALLBACK (Load_Filename_List_Key_Press), NULL);

    /* Signals to 'select' the same row into the other list (to show the
     * corresponding filenames). */
    g_signal_connect_swapped (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->load_file_content_view)),
                              "changed",
                              G_CALLBACK (Load_Filename_Select_Row_In_Other_List),
                              priv->load_file_name_view);
    g_signal_connect_swapped (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->load_file_name_view)),
                              "changed",
                              G_CALLBACK (Load_Filename_Select_Row_In_Other_List),
                              priv->load_file_content_view);

    /* Commands (like the popup menu). */
    button = GTK_WIDGET (gtk_builder_get_object (builder,
                                                 "name_insert_blank"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Load_Filename_List_Insert_Blank_Line),
                              priv->load_file_name_view);

    button = GTK_WIDGET (gtk_builder_get_object (builder, "name_remove"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Load_Filename_List_Delete_Line),
                              priv->load_file_name_view);

    button = GTK_WIDGET (gtk_builder_get_object (builder,
                                                 "name_remove_blank"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Load_Filename_List_Delete_All_Blank_Lines),
                              priv->load_file_name_view);
    
    button = GTK_WIDGET (gtk_builder_get_object (builder, "name_up"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Load_Filename_List_Move_Up),
                              priv->load_file_name_view);

    button = GTK_WIDGET (gtk_builder_get_object (builder, "name_down"));
    g_signal_connect_swapped (button, "clicked",
                              G_CALLBACK (Load_Filename_List_Move_Down),
                              priv->load_file_name_view);

    button = GTK_WIDGET (gtk_builder_get_object (builder, "name_reload"));
    g_signal_connect (button, "clicked",
                      G_CALLBACK (on_load_file_name_view_reload_clicked),
                      self);
    
    /* Load the list of files in the list widget. */
    Load_File_List (self);

    /* Create popup menus. */
    create_load_file_content_view_popup (self);
    create_load_file_name_view_popup (self);
    g_signal_connect (priv->load_file_content_view, "button-press-event",
                      G_CALLBACK (on_content_view_button_press_event), self);
    g_signal_connect (priv->load_file_content_view, "popup-menu",
                      G_CALLBACK (on_content_view_popup_menu), self);
    g_signal_connect (priv->load_file_name_view, "button-press-event",
                      G_CALLBACK (on_name_view_button_press_event), self);
    g_signal_connect (priv->load_file_name_view, "popup-menu",
                      G_CALLBACK (on_name_view_popup_menu), self);

    /* Entry to edit a line in the list. */
    priv->selected_line_entry = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                    "file_entry"));
    g_signal_connect (priv->selected_line_entry, "changed",
                      G_CALLBACK (Load_Filename_Update_Text_Line),
                      G_OBJECT (priv->load_file_content_view));

    /* Signal to load the line text in the editing entry. */
    g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->load_file_content_view)),
                      "changed", G_CALLBACK (Load_Filename_Edit_Text_Line),
                      self);

    load_file_run_scanner = GTK_WIDGET (gtk_builder_get_object (builder,
                                                                "file_scanner_check"));
    g_settings_bind (MainSettings, "load-filenames-run-scanner",
                     load_file_run_scanner, "active", G_SETTINGS_BIND_DEFAULT);

    g_object_unref (builder);

    /* To initialize load button sensitivity. */
    g_signal_emit_by_name (G_OBJECT (priv->file_chooser), "file-set");
}

static void
et_load_files_dialog_finalize (GObject *object)
{
    G_OBJECT_CLASS (et_load_files_dialog_parent_class)->finalize (object);
}

static void
et_load_files_dialog_init (EtLoadFilesDialog *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ET_TYPE_LOAD_FILES_DIALOG,
                                              EtLoadFilesDialogPrivate);

    create_load_files_dialog (self);
}

static void
et_load_files_dialog_class_init (EtLoadFilesDialogClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = et_load_files_dialog_finalize;

    g_type_class_add_private (klass, sizeof (EtLoadFilesDialogPrivate));
}

/*
 * et_load_files_dialog_new:
 *
 * Create a new EtLoadFilesDialog instance.
 *
 * Returns: a new #EtLoadFilesDialog
 */
EtLoadFilesDialog *
et_load_files_dialog_new (GtkWindow *parent)
{
    g_return_val_if_fail (GTK_WINDOW (parent), NULL);

    return g_object_new (ET_TYPE_LOAD_FILES_DIALOG, "transient-for", parent,
                         NULL);
}
