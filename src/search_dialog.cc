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

#include "config.h"

#include "search_dialog.h"

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
#include "enums.h"
#include "file_renderer.h"
#include "file_list.h"

using namespace std;

typedef struct
{
    GtkWidget *search_find_button;
    GtkWidget *search_string_combo;
    GtkListStore *search_string_model;
    GtkWidget *search_filename_check;
    GtkWidget *search_tag_check;
    GtkWidget *search_case_check;
    GtkTreeView *search_results_view;
    GtkListStore *search_results_model;
    GtkWidget *status_bar;
    guint status_bar_context;
} EtSearchDialogPrivate;

// learn correct return type for et_browser_get_instance_private
#define et_search_dialog_get_instance_private et_search_dialog_get_instance_private_
G_DEFINE_TYPE_WITH_PRIVATE (EtSearchDialog, et_search_dialog, GTK_TYPE_DIALOG)
#undef et_search_dialog_get_instance_private
#define et_search_dialog_get_instance_private(x) ((EtSearchDialogPrivate*)et_search_dialog_get_instance_private_(x))

enum
{   SEARCH_RESULT_POINTER,
    SEARCH_RESULT_FLAGS
};

/* Number of first columns that belong to the option "search filename".
 */
#define SEACH_RESULT_FILENAME_COLUMNS 2

/*
 * Callback to select-row event
 * Select all results that are selected in the search result list also in the browser list
 */
static void
Search_Result_List_Row_Selected (GtkTreeSelection *selection,
                                 gpointer user_data)
{
    EtSearchDialogPrivate *priv;
    GList       *selectedRows;
    GList *l;
    ET_File     *ETFile;
    GtkTreeIter  currentFile;

    priv = et_search_dialog_get_instance_private (ET_SEARCH_DIALOG (user_data));

    selectedRows = gtk_tree_selection_get_selected_rows(selection, NULL);

    /* We might be called with no rows selected */
    if (!selectedRows)
    {
        return;
    }

    /* Unselect files in the main list before re-selecting them... */
    et_application_window_browser_unselect_all(MainWindow);

    for (l = selectedRows; l != NULL; l = g_list_next (l))
    {
        if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->search_results_model),
                                     &currentFile, (GtkTreePath *)l->data))
        {
            gtk_tree_model_get(GTK_TREE_MODEL(priv->search_results_model), &currentFile, 
                               SEARCH_RESULT_POINTER, &ETFile, -1);
            /* Select the files (but don't display them to increase speed). */
            et_browser_select_file_by_et_file(MainWindow->browser(), ETFile, TRUE);
            /* Display only the last file (to increase speed). */
            if (!selectedRows->next)
            {
                et_application_window_select_file_by_et_file(MainWindow, ETFile);
            }
        }
    }

    g_list_free_full (selectedRows, (GDestroyNotify)gtk_tree_path_free);
}

/*
 * Search_File:
 * @search_button: the search button which was clicked
 * @user_data: the #EtSearchDialog which contains @search_button
 *
 * Search for the search term (in the search entry of @user_data) in the list
 * of open files.
 */
static void
Search_File (GtkWidget *search_button,
             gpointer user_data)
{
    EtSearchDialog *self;
    EtSearchDialogPrivate *priv;
    const gchar *string_to_search = NULL;

    self = ET_SEARCH_DIALOG (user_data);
    priv = et_search_dialog_get_instance_private (self);

    if (!priv->search_string_combo || !priv->search_filename_check || !priv->search_tag_check || !priv->search_results_view)
        return;

    string_to_search = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->search_string_combo))));
    if (!string_to_search || !*string_to_search)
        return;

    gint mincol = g_settings_get_boolean(MainSettings, "search-filename") ? 0 : SEACH_RESULT_FILENAME_COLUMNS;
    gint maxcol = g_settings_get_boolean(MainSettings, "search-tag") ? gtk_tree_view_get_n_columns(priv->search_results_view) : SEACH_RESULT_FILENAME_COLUMNS;
    if (mincol >= maxcol)
        return; // nothing to search

    Add_String_To_Combo_List (priv->search_string_model, string_to_search);

    gtk_widget_set_sensitive (GTK_WIDGET (search_button), FALSE);
    gtk_list_store_clear (priv->search_results_model);
    gtk_statusbar_push (GTK_STATUSBAR (priv->status_bar),
                        priv->status_bar_context, "");

    gchar* (*normalize)(const gchar*) = g_settings_get_boolean(MainSettings, "search-case-sensitive")
        ? [](const gchar* value) { return g_utf8_normalize(value, -1, G_NORMALIZE_DEFAULT); }
        : [](const gchar* value) { return g_utf8_casefold(value, -1); };
    gchar* string_to_search_normalized = normalize(string_to_search);

    GEnumClass *enum_class = (GEnumClass*)g_type_class_ref(ET_TYPE_SORT_MODE);
    for (const ET_File* ETFile : ET_FileList::all_files())
    {
        // check for match
        gint match = 0;
        for (gint i = mincol; i < maxcol; i++)
        {
            GtkTreeViewColumn *column = gtk_tree_view_get_column(priv->search_results_view, i);
            string id = FileColumnRenderer::ColumnName2Nick(GTK_BUILDABLE(column));
            auto rdr = FileColumnRenderer::Get_Renderer(id.c_str());

            string text = rdr->RenderText(ETFile);
            if (!text.length())
                continue;
            gchar* normalized = normalize(text.c_str());
            if (strstr(normalized, string_to_search_normalized) != nullptr)
                match |= 1 << rdr->Column;
            g_free(normalized);
        }

        if (match)
          gtk_list_store_insert_with_values (priv->search_results_model, NULL, G_MAXINT,
                                             SEARCH_RESULT_POINTER, ETFile,
                                             SEARCH_RESULT_FLAGS, match, -1);
    }
    g_type_class_unref(enum_class);
    g_free(string_to_search_normalized);

    gtk_widget_set_sensitive (GTK_WIDGET (search_button), TRUE);

    /* Display the number of matches in the statusbar. */
    gint resultCount = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->search_results_model), NULL);
    gchar *msg = g_strdup_printf (ngettext ("Found one file", "Found %d files", resultCount), resultCount);
    gtk_statusbar_push (GTK_STATUSBAR (priv->status_bar), priv->status_bar_context, msg);
    g_free (msg);

    /* Disable result list if no row inserted. */
    gtk_widget_set_sensitive (GTK_WIDGET (priv->search_results_view), resultCount > 0);
}

static gboolean
on_delete_event (GtkWidget *widget)
{
    et_search_dialog_apply_changes (ET_SEARCH_DIALOG (widget));
    gtk_widget_hide (widget);

    return G_SOURCE_CONTINUE;
}

static void on_visible_columns_changed(EtSearchDialog *self, const gchar *key, GSettings *settings)
{
	FileColumnRenderer::ShowHideColumns(et_search_dialog_get_instance_private(self)->search_results_view, (EtColumn)g_settings_get_flags(settings, key));
}

static void set_cell_data(GtkTreeViewColumn* column, GtkCellRenderer* cell, GtkTreeModel* model, GtkTreeIter* iter, gpointer data)
{
	const ET_File *file;
	gint flags;
	gtk_tree_model_get(model, iter, SEARCH_RESULT_POINTER, &file, SEARCH_RESULT_FLAGS, &flags, -1);
	auto renderer = (const FileColumnRenderer*)data;
	string text = renderer->RenderText(file);
	FileColumnRenderer::SetText(GTK_CELL_RENDERER_TEXT(cell),
		text.c_str(), false, flags & (1 << renderer->Column) ? FileColumnRenderer::HIGHLIGHT : FileColumnRenderer::NORMAL);
}

/*
 * The window to search keywords in the list of files.
 */
static void
create_search_dialog (EtSearchDialog *self)
{
    EtSearchDialogPrivate *priv;

    priv = et_search_dialog_get_instance_private (self);

    /* Words to search. */
    priv->search_string_model = gtk_list_store_new (MISC_COMBO_COUNT,
                                                    G_TYPE_STRING);
    gtk_combo_box_set_model (GTK_COMBO_BOX (priv->search_string_combo),
                             GTK_TREE_MODEL (priv->search_string_model));
    g_object_unref (priv->search_string_model);
    /* History List. */
    Load_Search_File_List (priv->search_string_model, MISC_COMBO_TEXT);
    gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->search_string_combo))),
                        "");

    /* Set content of the clipboard if available. */
    gtk_editable_paste_clipboard (GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (priv->search_string_combo))));

    et_settings_bind_boolean("search-filename", priv->search_filename_check);
    et_settings_bind_boolean("search-tag", priv->search_tag_check);

    /* Property of the search. */
    et_settings_bind_boolean("search-case-sensitive", priv->search_case_check);

    /* Button to run the search. */
    gtk_widget_grab_default (priv->search_find_button);
    g_signal_connect (gtk_bin_get_child (GTK_BIN (priv->search_string_combo)),
                      "activate", G_CALLBACK (Search_File), self);

    /* Status bar. */
    priv->status_bar_context = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->status_bar),
                                                             "Messages");
    gtk_statusbar_push (GTK_STATUSBAR (priv->status_bar),
                        priv->status_bar_context, _("Ready to search…"));

    /* Init columns */
    GEnumClass *enum_class = (GEnumClass*)g_type_class_ref(ET_TYPE_SORT_MODE);
    for (gsize i = 0; i < gtk_tree_view_get_n_columns(priv->search_results_view); i++)
    {
        GtkTreeViewColumn *column = gtk_tree_view_get_column(priv->search_results_view, i);
        string id = FileColumnRenderer::ColumnName2Nick(GTK_BUILDABLE(column));

        // rendering method
        GtkCellRenderer* renderer = GTK_CELL_RENDERER(GTK_CELL_LAYOUT_GET_IFACE(column)->get_cells(GTK_CELL_LAYOUT(column))->data);
        auto rdr = FileColumnRenderer::Get_Renderer(id.c_str());
        g_assert(rdr);
        gtk_tree_view_column_set_cell_data_func(column, renderer, &set_cell_data, (gpointer)rdr, NULL);
    }
    g_type_class_unref(enum_class);

    g_signal_connect_swapped(MainSettings, "changed::visible-columns", G_CALLBACK(on_visible_columns_changed), self);
    on_visible_columns_changed(self, "visible-columns", MainSettings);
}

/*
 * For the configuration file...
 */
void
et_search_dialog_apply_changes (EtSearchDialog *self)
{
    EtSearchDialogPrivate *priv;

    g_return_if_fail (ET_SEARCH_DIALOG (self));

    priv = et_search_dialog_get_instance_private (self);

    Save_Search_File_List (priv->search_string_model, MISC_COMBO_TEXT);
}

void et_search_dialog_clear(EtSearchDialog *self)
{
  g_return_if_fail (ET_SEARCH_DIALOG (self));
  EtSearchDialogPrivate *priv = et_search_dialog_get_instance_private (self);
  gtk_list_store_clear(priv->search_results_model);
}

static void
et_search_dialog_init (EtSearchDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
    create_search_dialog (self);
}

static void
et_search_dialog_class_init (EtSearchDialogClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/EasyTAG/search_dialog.ui");
    gtk_widget_class_bind_template_child_private (widget_class, EtSearchDialog, search_find_button);
    gtk_widget_class_bind_template_child_private (widget_class, EtSearchDialog, search_string_combo);
    gtk_widget_class_bind_template_child_private (widget_class, EtSearchDialog, search_filename_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtSearchDialog, search_tag_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtSearchDialog, search_case_check);
    gtk_widget_class_bind_template_child_private (widget_class, EtSearchDialog, search_results_model);
    gtk_widget_class_bind_template_child_private (widget_class, EtSearchDialog, search_results_view);
    gtk_widget_class_bind_template_child_private (widget_class, EtSearchDialog, status_bar);
    gtk_widget_class_bind_template_callback (widget_class, on_delete_event);
    gtk_widget_class_bind_template_callback (widget_class, Search_File);
    gtk_widget_class_bind_template_callback (widget_class, Search_Result_List_Row_Selected);
}

/*
 * et_search_dialog_new:
 *
 * Create a new EtSearchDialog instance.
 *
 * Returns: a new #EtSearchDialog
 */
EtSearchDialog *
et_search_dialog_new (GtkWindow *parent)
{
    g_return_val_if_fail (GTK_WINDOW (parent), NULL);

    return (EtSearchDialog*)g_object_new (ET_TYPE_SEARCH_DIALOG, "transient-for", parent, NULL);
}
