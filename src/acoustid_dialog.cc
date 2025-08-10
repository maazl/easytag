/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2025  Marcel Mueller <github@maazl.de>
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
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "acoustid_dialog.h"

#ifdef ENABLE_ACOUSTID

#include "xptr.h"
#include "file.h"
#include "acoustid.h"
#include "easytag.h"
#include "application_window.h"
#include "browser.h"
#include "log.h"

#include <glib/gi18n.h>

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>

using namespace std;


typedef struct
{
	GtkWidget *file_path_label;
	GtkWidget *duration_label;
	GtkLabel* remaining_label;
	GtkWidget *stop_button;

	GtkWidget *results_view;
	GtkListStore *results_list_model;

	GtkWidget *fill_title_check;
	GtkWidget *fill_artist_check;
	GtkWidget *fill_year_check;
	GtkWidget *fill_track_check;
	GtkWidget *fill_disc_check;
	GtkWidget *fill_album_check;
	GtkWidget *fill_album_artist_check;
	GtkWidget *fill_release_year_check;
	GtkWidget *fill_track_total_check;
	GtkWidget *fill_disc_total_check;

	GtkWidget *first_year_check;
	GtkWidget *discard_date_check;
	GtkWidget *no_empty_fields;
	GtkWidget *no_disc_total_01_check;

	GtkWidget *status_bar;

	GtkWidget *prev_button;
	GtkWidget *apply_button;
	GtkWidget *apply_next_button;
	GtkWidget *next_button;

	ET_File* current_file;

} EtAcoustIDDialogPrivate;

// learn correct return type for et_browser_get_instance_private
#define et_acoustid_dialog_get_instance_private et_acoustid_dialog_get_instance_private_
G_DEFINE_TYPE_WITH_PRIVATE(EtAcoustIDDialog, et_acoustid_dialog, GTK_TYPE_DIALOG)
#undef et_acoustid_dialog_get_instance_private
#define et_acoustid_dialog_get_instance_private(x) ((EtAcoustIDDialogPrivate*)et_acoustid_dialog_get_instance_private_(x))

enum class ResultsList
{	ARTIST,
	TITLE,
	ALBUM,
	ALBUM_ARTIST,
	YEAR,
	RELEASE_YEAR,
	TRACK,
	DISC,
	DURATION,
	COUNTRY,
	FORMAT,
	SCORE
};

static void update_apply_button_sensitivity(EtAcoustIDDialog *self)
{
	auto priv = et_acoustid_dialog_get_instance_private(self);
	gboolean active = g_settings_get_flags(MainSettings, "acoustid-set-fields")
		&& gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->results_view)), NULL, NULL);
	gtk_widget_set_sensitive(priv->apply_button, active);
	active &= gtk_widget_get_sensitive(priv->next_button);
	gtk_widget_set_sensitive(priv->apply_next_button, active);
}

static string acoustid_format_track_disc(unsigned position, unsigned count, string (*pad)(unsigned number))
{
	string result;
	if (position)
		result += pad(position);
	if (count)
		(result += '/') += pad(count);
	return result;
}

static void acoustid_refresh_dialog(EtAcoustIDDialog *self)
{
	auto priv = et_acoustid_dialog_get_instance_private(self);

	gtk_list_store_clear(priv->results_list_model);

	if (!priv->current_file)
	{	gtk_label_set_text(GTK_LABEL(priv->file_path_label), "");
		gtk_label_set_text(GTK_LABEL(priv->duration_label), "");
		gtk_label_set_text(GTK_LABEL(priv->status_bar), _("No file"));
		return;
	}

	gtk_label_set_text(GTK_LABEL(priv->file_path_label), priv->current_file->FileNameNew()->full_name());
	gtk_label_set_text(GTK_LABEL(priv->duration_label), et_file_duration_to_string(priv->current_file->ETFileInfo.duration).c_str());

	const AcoustID::Matches* matches = priv->current_file->AcoustIDMatches.get();

	const char* msg;
	if (!matches)
		msg = _("Use fingerprint action to start.");
	else switch (matches->get_state())
	{case AcoustID::PENDING:
		msg = _("Fingerprint results pending…");
		break;
	 case AcoustID::ABORTED:
		msg = _("Fingerprinting aborted.");
		break;
	 case AcoustID::ERROR:
		msg = matches->get_error();
		break;
	 default:
		unsigned recording_count = matches->get_recording_count();
		switch (recording_count)
		{case 0:
			msg = _("No matches found.");
			break;
		 case 1:
			msg = _("Hit 'Apply' to take the match.");
			break;
		 default:
			msg = _("Select a match.");
		}

		// populate result list
		for (auto recording = matches->get_first_recording(); recording_count--; ++recording)
		{	const char* title = recording->title.get();
			unsigned count = recording->release_count;
			if (count)
			{	string rel_year = recording->first_release().toString();
				string duration = et_file_duration_to_string(recording->duration);
				for (const AcoustID::Release* release = recording->releases.get(); count--; ++release)
					gtk_list_store_insert_with_values(priv->results_list_model, NULL, -1,
						ResultsList::ARTIST, recording->artist.get(),
						ResultsList::TITLE, title,
						ResultsList::ALBUM, release->title.get(),
						ResultsList::ALBUM_ARTIST, release->artist.get(),
						ResultsList::YEAR, rel_year.c_str(),
						ResultsList::RELEASE_YEAR, release->date.toString().c_str(),
						ResultsList::TRACK, acoustid_format_track_disc(release->track, release->track_count, &File_Tag::track_number_to_string).c_str(),
						ResultsList::DISC, acoustid_format_track_disc(release->medium, release->medium_count, &File_Tag::disc_number_to_string).c_str(),
						ResultsList::DURATION, duration.c_str(),
						ResultsList::COUNTRY, release->country,
						ResultsList::FORMAT, release->format.get(),
						ResultsList::SCORE, (gint)(recording->score * 100 + .5), -1);
			} else if (!et_str_empty(title)) // Sometimes empty results are returned => ignore.
				gtk_list_store_insert_with_values(priv->results_list_model, NULL, -1,
					ResultsList::ARTIST, recording->artist.get(),
					ResultsList::TITLE, title,
					ResultsList::DURATION, et_file_duration_to_string(recording->duration).c_str(),
					ResultsList::SCORE, (gint)(recording->score * 100 + .5), -1);

		}

		GtkTreeIter iter;
		if (recording_count == 1 && gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->results_list_model), &iter))
			gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->results_view)), &iter);
	}
	gtk_label_set_text(GTK_LABEL(priv->status_bar), msg);

	update_apply_button_sensitivity(self);
}

static void acoustid_stop(EtAcoustIDDialog *self)
{
	AcoustIDWorker::Stop();
	auto priv = et_acoustid_dialog_get_instance_private(self);
	gtk_widget_set_sensitive(priv->stop_button, FALSE);
}

static bool apply_row(EtAcoustIDDialog *self, GtkTreeIter* iter)
{	auto priv = et_acoustid_dialog_get_instance_private(self);
	if (!priv->current_file)
		return false;

	EtCddbSetField fields = (EtCddbSetField)g_settings_get_flags(MainSettings, "acoustid-set-fields");
	if (!fields)
		return false;

	gchar *artist, *title, *album, *album_artist, *year, *rel_year, *track, *disc;
	gtk_tree_model_get(GTK_TREE_MODEL(priv->results_list_model), iter,
		ResultsList::ARTIST, &artist,
		ResultsList::TITLE, &title,
		ResultsList::ALBUM, &album,
		ResultsList::ALBUM_ARTIST, &album_artist,
		ResultsList::YEAR, &year,
		ResultsList::RELEASE_YEAR, &rel_year,
		ResultsList::TRACK, &track,
		ResultsList::DISC, &disc, -1);

	/* Allocation of a new FileTag. */
	File_Tag* tag = new File_Tag(*priv->current_file->FileTagNew());

	gboolean no_empty = g_settings_get_boolean(MainSettings, "acoustid-no-empty-fields");

	EtColumn toUpdate = (EtColumn)0;
	auto assign = [&toUpdate](xStringD0& target, const char* value, EtColumn col)
	{	if (target != value)
		{	target = value;
			toUpdate = toUpdate | col;
	}	};

	if ((fields & ET_CDDB_SET_FIELD_ARTIST) && (!no_empty || *artist))
		assign(tag->artist, artist, ET_COLUMN_ARTIST);
	if ((fields & ET_CDDB_SET_FIELD_TITLE) && (!no_empty || *title))
		assign(tag->title, title, ET_COLUMN_TITLE);
	if ((fields & ET_CDDB_SET_FIELD_ALBUM) && (!no_empty || *album))
		assign(tag->album, album, ET_COLUMN_ALBUM);
	if ((fields & ET_CDDB_SET_FIELD_ALBUM_ARTIST) && (!no_empty || *album_artist))
		assign(tag->album_artist, album_artist, ET_COLUMN_ALBUM_ARTIST);
	if (fields & ET_CDDB_SET_FIELD_YEAR)
	{	char* y = g_settings_get_boolean(MainSettings, "acoustid-use-first-year") ? year : rel_year;
		if (*y)
		{	if (y[1] && y[2] && y[3] && y[4] && g_settings_get_boolean(MainSettings, "acoustid-discard-date"))
				y[4] = 0;
			assign(tag->year, y, ET_COLUMN_YEAR);
		} else if (!no_empty)
			assign(tag->year, nullptr, ET_COLUMN_YEAR);
	}
	if (fields & ET_CDDB_SET_FIELD_RELEASE_YEAR)
	{	if (*rel_year)
		{	if (rel_year[1] && rel_year[2] && rel_year[3] && rel_year[4] && g_settings_get_boolean(MainSettings, "acoustid-discard-date"))
				rel_year[4] = 0;
			assign(tag->release_year, rel_year, ET_COLUMN_RELEASE_YEAR);
		} else if (!no_empty)
			assign(tag->release_year, nullptr, ET_COLUMN_RELEASE_YEAR);
	}
	char* total = strchr(track, '/');
	if (total)
		*total++ = 0;
	if ((fields & ET_CDDB_SET_FIELD_TRACK) && (!no_empty || *track))
		assign(tag->track, track, ET_COLUMN_TRACK_NUMBER);
	if ((fields & ET_CDDB_SET_FIELD_TRACK_TOTAL) && (!no_empty || total))
		assign(tag->track_total, total, ET_COLUMN_TRACK_NUMBER);
	total = strchr(disc, '/');
	if (total)
	{	*total++ = 0;
		if (atoi(total) == 1 && g_settings_get_boolean(MainSettings, "acoustid-no-disc-total-01"))
			*disc = 0, total = nullptr;
	}
	if ((fields & ET_CDDB_SET_FIELD_DISC) && (!no_empty || *disc))
		assign(tag->disc_number, disc, ET_COLUMN_DISC_NUMBER);
	if ((fields & ET_CDDB_SET_FIELD_DISC_TOTAL) && (!no_empty || total))
		assign(tag->disc_total, total, ET_COLUMN_DISC_NUMBER);

	bool changed = priv->current_file->apply_changes(nullptr, tag);

	g_free(artist);
	g_free(title);
	g_free(album);
	g_free(album_artist);
	g_free(year);
	g_free(rel_year);
	g_free(track);
	g_free(disc);

	if (!changed)
		return false;

	if (MainWindow->get_displayed_file() == priv->current_file)
		et_application_window_update_ui_from_et_file(MainWindow, toUpdate);
	et_browser_refresh_file_in_list(MainWindow->browser(), priv->current_file);
	return true;
}

static bool has_acoustid(const ET_File* file)
{	return !!file->AcoustIDMatches;
}

static void acoustid_previous(EtAcoustIDDialog *self)
{	auto priv = et_acoustid_dialog_get_instance_private(self);
	self->current_file(MainWindow->browser()->prev_next_if(priv->current_file, has_acoustid).first);
}

static void acoustid_next(EtAcoustIDDialog *self)
{	auto priv = et_acoustid_dialog_get_instance_private(self);
	self->current_file(MainWindow->browser()->prev_next_if(priv->current_file, has_acoustid).second);
}

static void acoustid_apply(EtAcoustIDDialog *self)
{
	auto priv = et_acoustid_dialog_get_instance_private(self);

	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->results_view)), NULL, &iter))
		return;

	apply_row(self, &iter);
}

static void acoustid_apply_next(EtAcoustIDDialog *self)
{	acoustid_apply(self);
	acoustid_next(self);
}

static gboolean on_results_button_press_event(GtkWidget *widget, GdkEventButton *event, EtAcoustIDDialog *self)
{
	if (event->type == GDK_2BUTTON_PRESS && event->button == GDK_BUTTON_PRIMARY)
	{	// Double left mouse click. => apply & next
		if (gtk_tree_view_get_bin_window(GTK_TREE_VIEW(widget)) != event->window)
				// If the double-click is not on a tree view row, for example when resizing a header column, ignore it.
				return GDK_EVENT_PROPAGATE;

		auto priv = et_acoustid_dialog_get_instance_private(self);

		GtkTreePath* tree_path;
		GtkTreeViewColumn* column;
		if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &tree_path, &column, NULL,NULL))
				return GDK_EVENT_PROPAGATE;
		GtkTreeIter iter;
		gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->results_list_model), &iter, tree_path);
		gtk_tree_path_free(tree_path);

		apply_row(self, &iter);
		acoustid_next(self);
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static void onFileUpdated(EtAcoustIDDialog *self, ET_File* file, unsigned remaining)
{	auto error = file->AcoustIDMatches->get_error();
	if (error)
		Log_Print(LOG_ERROR, "AcoustID fingerprinting failed for file '%s': %s", file->FilePath.get(), error);

	if (self->current_file() == file)
		acoustid_refresh_dialog(self); // update
	self->set_remaining_files(remaining);
}

static void onFinished(EtAcoustIDDialog *self, bool cancelled)
{	if (cancelled)
		Log_Print(LOG_INFO, _("Audio fingerprinting stopped"));

	self->set_remaining_files(0);
}

static void et_acoustid_dialog_init(EtAcoustIDDialog *self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

	auto priv = et_acoustid_dialog_get_instance_private(self);

	/* Apply results to fields.  */
	auto init_set_field_check([](GtkWidget *widget)
	{	et_settings_bind_flags("acoustid-set-fields", widget); });
	init_set_field_check(priv->fill_title_check);
	init_set_field_check(priv->fill_artist_check);
	init_set_field_check(priv->fill_year_check);
	init_set_field_check(priv->fill_track_check);
	init_set_field_check(priv->fill_disc_check);
	init_set_field_check(priv->fill_album_check);
	init_set_field_check(priv->fill_album_artist_check);
	init_set_field_check(priv->fill_track_total_check);
	init_set_field_check(priv->fill_disc_total_check);
	init_set_field_check(priv->fill_release_year_check);

	// Options
	et_settings_bind_boolean("acoustid-use-first-year", priv->first_year_check);
	et_settings_bind_boolean("acoustid-discard-date", priv->discard_date_check);
	et_settings_bind_boolean("acoustid-no-disc-total-01", priv->no_disc_total_01_check);
	et_settings_bind_boolean("acoustid-no-empty-fields", priv->no_empty_fields);

	AcoustIDWorker::RegisterEvents(onFileUpdated, onFinished, self);
}

static void et_acoustid_dialog_dispose(GObject *self)
{
	auto priv = et_acoustid_dialog_get_instance_private(ET_ACOUSTID_DIALOG(self));
	xPtr<ET_File>::fromCptr(priv->current_file);
	priv->current_file = nullptr;
}

static void et_acoustid_dialog_class_init(EtAcoustIDDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	G_OBJECT_CLASS (klass)->dispose = et_acoustid_dialog_dispose;

	gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/EasyTAG/acoustid_dialog.ui");

	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, file_path_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, duration_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, remaining_label);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, stop_button);

	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, results_view);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, results_list_model);

	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, fill_title_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, fill_artist_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, fill_year_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, fill_track_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, fill_disc_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, fill_album_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, fill_album_artist_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, fill_release_year_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, fill_track_total_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, fill_disc_total_check);

	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, first_year_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, discard_date_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, no_disc_total_01_check);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, no_empty_fields);

	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, status_bar);

	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, prev_button);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, apply_button);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, apply_next_button);
	gtk_widget_class_bind_template_child_private(widget_class, EtAcoustIDDialog, next_button);

	gtk_widget_class_bind_template_callback(widget_class, update_apply_button_sensitivity);
  gtk_widget_class_bind_template_callback(widget_class, on_results_button_press_event);
	gtk_widget_class_bind_template_callback(widget_class, acoustid_stop);
	gtk_widget_class_bind_template_callback(widget_class, acoustid_previous);
	gtk_widget_class_bind_template_callback(widget_class, acoustid_apply);
	gtk_widget_class_bind_template_callback(widget_class, acoustid_apply_next);
	gtk_widget_class_bind_template_callback(widget_class, acoustid_next);
}

EtAcoustIDDialog* et_acoustid_dialog_new()
{
	gboolean use_header_bar = FALSE;
	GtkSettings* settings = gtk_settings_get_default();
	if (settings)
		g_object_get(settings, "gtk-dialogs-use-header", &use_header_bar, NULL);

	return (EtAcoustIDDialog*)g_object_new(ET_TYPE_ACOUSTID_DIALOG,
		"transient-for", MainWindow,
		"use-header-bar", use_header_bar, NULL);
}

ET_File* EtAcoustIDDialog::current_file()
{	return et_acoustid_dialog_get_instance_private(this)->current_file;
}

void EtAcoustIDDialog::current_file(ET_File* file)
{
	auto priv = et_acoustid_dialog_get_instance_private(this);

	xPtr<ET_File> ptr(xPtr<ET_File>::fromCptr(priv->current_file));
	ptr = file;
	priv->current_file = xPtr<ET_File>::toCptr(move(ptr));

	if (priv->file_path_label) // all widgets are null before the dialog shows first.
	{	auto prev_next = MainWindow->browser()->prev_next_if(priv->current_file, has_acoustid);
		gtk_widget_set_sensitive(priv->prev_button, prev_next.first != nullptr);
		gtk_widget_set_sensitive(priv->next_button, prev_next.second != nullptr);

		acoustid_refresh_dialog(this);
	}
}

void EtAcoustIDDialog::set_remaining_files(unsigned remaining)
{	auto priv = et_acoustid_dialog_get_instance_private(this);

	string rem;
	if (remaining)
		rem = strprintf(_("%u files remaining"), remaining);

	gtk_label_set_text(priv->remaining_label, rem.c_str());
	gtk_widget_set_sensitive(priv->stop_button, remaining != 0);
}

void EtAcoustIDDialog::update_button_sensitivity()
{
	if (!gtk_widget_is_visible(GTK_WIDGET(this)))
		return;
	auto priv = et_acoustid_dialog_get_instance_private(this);
	auto prev_next = MainWindow->browser()->prev_next_if(priv->current_file, has_acoustid);
	gtk_widget_set_sensitive(priv->prev_button, prev_next.first != nullptr);
	gtk_widget_set_sensitive(priv->next_button, prev_next.second != nullptr);
	gtk_widget_set_sensitive(priv->apply_next_button, prev_next.second != nullptr && gtk_widget_get_sensitive(priv->apply_button));
}

void EtAcoustIDDialog::reset()
{
	acoustid_stop(this);
	current_file(nullptr);
}

#endif
