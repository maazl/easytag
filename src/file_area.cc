/* EasyTAG - tag editor for audio files
 * Copyright (C) 2024-2026  Marcel Müller <github@maazl.de>
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

#include "file_area.h"
#include "file_name.h"
#include "file_description.h"
#include "file_list.h"

#include <glib/gi18n.h>

#include "charset.h"
#include "log.h"
#include "setting.h"
#include "tag_area.h"

#include <string>
using namespace std;

typedef struct
{
	GtkLabel* file_label;
	GtkLabel* index_label;
	GtkLabel* files_label;

	GtkEntry* name_entry;
	GtkEntry* path_entry; ///< Relative path of the currently selected file.

	GtkWidget *header_grid;

	GtkLabel* version_label;
	GtkLabel* version_value_label;
	GtkLabel* bitrate_label;
	GtkLabel* bitrate_value_label;
	GtkLabel* samplerate_label;
	GtkLabel* samplerate_value_label;
	GtkLabel* mode_label;
	GtkLabel* mode_value_label;
	GtkLabel* size_label;
	GtkLabel* size_value_label;
	GtkLabel* duration_label;
	GtkLabel* duration_value_label;
} EtFileAreaPrivate;

// learn correct return type for et_file_area_get_instance_private
#define et_file_area_get_instance_private et_file_area_get_instance_private_
G_DEFINE_TYPE_WITH_PRIVATE (EtFileArea, et_file_area, GTK_TYPE_BIN)
#undef et_file_area_get_instance_private
#define et_file_area_get_instance_private(x) ((EtFileAreaPrivate*)et_file_area_get_instance_private_(x))

static void on_file_show_header_changed(EtFileArea *self, const gchar *key, GSettings *settings)
{
	gtk_widget_set_visible(et_file_area_get_instance_private(self)->header_grid, g_settings_get_boolean(settings, key));
}

static void et_file_area_init(EtFileArea *self)
{
	gtk_widget_init_template(GTK_WIDGET(self));
	g_signal_connect_swapped(MainSettings, "changed::file-show-header", G_CALLBACK(on_file_show_header_changed), self);
	on_file_show_header_changed(self, "file-show-header", MainSettings);

	EtFileAreaPrivate* priv = et_file_area_get_instance_private(self);
	// Attach context menu functions to path entry
	et_tag_field_connect_signals(priv->path_entry, NULL);

}

static void et_file_area_class_init(EtFileAreaClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/EasyTAG/file_area.ui");
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, file_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, index_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, files_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, name_entry);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, path_entry);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, header_grid);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, version_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, version_value_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, bitrate_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, bitrate_value_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, samplerate_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, samplerate_value_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, mode_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, mode_value_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, size_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, size_value_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, duration_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtFileArea, duration_value_label);
	gtk_widget_class_bind_template_callback(widget_class, on_entry_populate_popup);
}

void EtFileArea::clear()
{
	EtFileAreaPrivate* priv = et_file_area_get_instance_private(this);
	gtk_entry_set_text(priv->name_entry, "");
	gtk_entry_set_icon_from_gicon(priv->name_entry, GTK_ENTRY_ICON_SECONDARY, NULL);
	gtk_label_set_text(priv->index_label, "");
	gtk_label_set_text(priv->files_label, /* Translators: No files, as in "0 files". */ _("No files"));

	EtFileHeaderFields fields;
	fields.description = _("File");
	set_header_fields(fields);
}

void EtFileArea::set_header_fields(const EtFileHeaderFields& fields)
{
	EtFileAreaPrivate* priv = et_file_area_get_instance_private(this);
	gtk_label_set_text(priv->file_label, fields.description.c_str());
	gtk_label_set_text(priv->version_label, fields.version_label.c_str());
	gtk_label_set_text(priv->version_value_label, fields.version.c_str());
	gtk_label_set_text(priv->bitrate_value_label, fields.bitrate.c_str());
	gtk_label_set_text(priv->samplerate_value_label, fields.samplerate.c_str());
	gtk_label_set_text(priv->mode_label, fields.mode_label.c_str());
	gtk_label_set_text(priv->mode_value_label, fields.mode.c_str());
	gtk_label_set_text(priv->size_value_label, fields.size.c_str());
	gtk_label_set_text(priv->duration_value_label, fields.duration.c_str());
}

/*
 * "Default" way to display File Info to the user interface.
 */
void EtFileArea::default_header_fields(EtFileHeaderFields& fields, const ET_File& ETFile)
{
	const ET_File_Info *info = &ETFile.ETFileInfo;

	fields.description = ETFile.ETFileDescription->FileType;

	/* Bitrate */
	fields.bitrate = strprintf(info->variable_bitrate ? _("~%d kb/s") : _("%d kb/s"), (info->bitrate + 500) / 1000);

	/* Samplerate */
	fields.samplerate = strprintf(_("%d Hz"), info->samplerate);

	/* Mode */
	fields.mode = strprintf("%d", info->mode);

	/* Size */
	fields.size = strprintf("%s (%s)",
		gString(g_format_size(ETFile.FileSize)).get(),
		gString(g_format_size(ET_FileList::visible_total_bytes())).get());

	/* Duration */
	fields.duration = strprintf("%s (%s)",
		et_file_duration_to_string(info->duration).c_str(),
		et_file_duration_to_string(ET_FileList::visible_total_duration()).c_str());
}

/* Toggle visibility of the small status icon if filename is read-only or not
 * found. Show the position of the current file in the list, by using the index
 * and list length. */
void EtFileArea::display_et_file(const ET_File *ETFile, EtColumn columns)
{
	g_return_if_fail (ETFile != NULL);
	EtFileAreaPrivate* priv = et_file_area_get_instance_private(this);

	/* Set new filename into name_entry/name_path. */
	if (columns & ET_COLUMN_FILENAME)
		gtk_entry_set_text(priv->name_entry, ET_Remove_File_Extension(ETFile->FileNameNew()->file()).c_str());

	if (columns & ET_COLUMN_FILEPATH)
	{	const xStringD0& dirname_utf8 = ETFile->FileNameNew()->path();
		gtk_entry_set_text(priv->path_entry, dirname_utf8);

		// And refresh the number of files in this directory
		unsigned n_files = 0;
		for (const ET_File* file : ET_FileList::all_files())
			if (file->FileNameNew()->path() == dirname_utf8)
				++n_files;
		gtk_label_set_text(priv->files_label, strprintf(ngettext("One file", "%u files", n_files), n_files).c_str());
	}

	/* Show position of current file in list */
	gtk_label_set_text(priv->index_label, strprintf("%u/%u", ET_FileList::visible_index(ETFile), ET_FileList::visible_size()).c_str());

	/* Display file data, header data and file type */
	EtFileHeaderFields fields;
	default_header_fields(fields, *ETFile); // some defaults...

	if (ETFile->ETFileDescription->display_file_info_to_ui)
		(*ETFile->ETFileDescription->display_file_info_to_ui)(&fields, ETFile);

	set_header_fields(fields);

	/* Show/hide 'AccessStatusIcon' */
	gObject<GIcon> emblem_icon;
	const char* tooltip = NULL;

	GError *error = NULL;
	gObject<GFileInfo> info(g_file_query_info(gObject<GFile>(g_file_new_for_path(ETFile->FilePath)).get(),
		G_FILE_ATTRIBUTE_ACCESS_CAN_READ "," G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
		G_FILE_QUERY_INFO_NONE, NULL, &error));

	if (!info)
	{	emblem_icon.reset(g_themed_icon_new("emblem-unreadable"));
		tooltip = error->message;

		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
			/* No such file or directory. */
			tooltip = _("File not found");
		else
			Log_Print (LOG_ERROR, _("Cannot query file information ‘%s’"), error->message);

		g_error_free(error);
	}
	else
	{
		if (!g_file_info_get_attribute_boolean(info.get(), G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
		{	/* Otherwise unreadable. */
			emblem_icon.reset(g_themed_icon_new("emblem-unreadable"));
			tooltip = _("File not found");
		}
		else if (!g_file_info_get_attribute_boolean(info.get(), G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
		{	/* Read only file or permission denied. */
			emblem_icon.reset(g_themed_icon_new("emblem-readonly"));
			tooltip = _("Read-only file");
		}
	}

	gtk_entry_set_icon_from_gicon(priv->name_entry, GTK_ENTRY_ICON_SECONDARY, emblem_icon.get());
	gtk_entry_set_icon_tooltip_text(priv->name_entry, GTK_ENTRY_ICON_SECONDARY, tooltip);
}

const gchar* EtFileArea::get_file_name()
{	return gtk_entry_get_text(et_file_area_get_instance_private(this)->name_entry);
}

const gchar* EtFileArea::get_file_path()
{	return gtk_entry_get_text(et_file_area_get_instance_private(this)->path_entry);
}
