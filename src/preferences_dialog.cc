/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2022  Marcel Müller <github@maazl.de>
 * Copyright (C) 2013-2015  David King <amigadave@amigadave.com>
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
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "preferences_dialog.h"

#include <errno.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <string.h>

#include "application_window.h"
#include "setting.h"
#include "misc.h"
#include "scan_dialog.h"
#include "easytag.h"
#include "enums.h"
#include "browser.h"
#include "cddb_dialog.h"
#include "charset.h"
#include "win32/win32dep.h"
using namespace std;

typedef struct
{
    GtkWidget *browser_startup_check;
    GtkWidget *browser_subdirs_check;
    GtkWidget *browser_expand_subdirs_check;
    GtkWidget *browser_hidden_check;
    GtkWidget *browser_max_lines_check;
    GtkWidget *browser_max_lines;
    GtkWidget *hide_fields_subtitle_check;
    GtkWidget *hide_fields_album_artist_check;
    GtkWidget *hide_fields_disc_subtitle_check;
    GtkWidget *hide_fields_disc_number_check;
    GtkWidget *hide_fields_release_year_check;
    GtkWidget *hide_fields_description_check;
    GtkWidget *hide_fields_composer_check;
    GtkWidget *hide_fields_orig_artist_check;
    GtkWidget *hide_fields_orig_year_check;
    GtkWidget *hide_fields_copyright_check;
    GtkWidget *hide_fields_url_check;
    GtkWidget *hide_fields_encoded_by_check;
    GtkWidget *hide_fields_replaygain;
    GtkWidget *log_show_check;
    GtkWidget *header_show_check;
    GtkWidget *list_bold_radio;
    GtkWidget *file_name_replace_ascii;
    GtkWidget *file_name_replace_unicode;
    GtkWidget *file_name_replace_none;
    GtkWidget *name_lower_radio;
    GtkWidget *name_upper_radio;
    GtkWidget *name_no_change_radio;
    GtkWidget *file_preserve_check;
    GtkWidget *file_parent_check;
    GtkWidget *default_path_button;
    GtkWidget *file_encoding_try_alternative_radio;
    GtkWidget *file_encoding_transliterate_radio;
    GtkWidget *file_encoding_ignore_radio;
    GtkWidget *tags_auto_date_check;
    GtkWidget *tags_auto_image_type_check;
    GtkWidget *tags_track_check;
    GtkWidget *tags_track_button;
    GtkWidget *tags_disc_check;
    GtkWidget *tags_disc_button;
    GtkWidget *tags_preserve_focus_check;
    GtkWidget *tags_multiline_comment;
    GtkWidget *split_title_check;
    GtkWidget *split_subtitle_check;
    GtkWidget *split_artist_check;
    GtkWidget *split_album_artist_check;
    GtkWidget *split_album_check;
    GtkWidget *split_disc_subtitle_check;
    GtkWidget *split_genre_check;
    GtkWidget *split_comment_check;
    GtkWidget *split_description_check;
    GtkWidget *split_composer_check;
    GtkWidget *split_orig_artist_check;
    GtkWidget *split_url_check;
    GtkWidget *split_encoded_by_check;
    GtkWidget *split_delimiter;

    GtkWidget *id3_strip_check;
    GtkWidget *id3_v2_convert_check;
    GtkWidget *id3_v2_crc32_check;
    GtkWidget *id3_v2_compression_check;
    GtkWidget *id3_v2_genre_check;
    GtkWidget *id3_v2_check;
    GtkWidget *id3_v2_version_label;
    GtkWidget *id3_v2_version_combo;
    GtkWidget *id3_v2_encoding_label;
    GtkWidget *id3_v2_unicode_radio;
    GtkWidget *id3_v2_unicode_encoding_combo;
    GtkWidget *id3_v2_other_radio;
    GtkWidget *id3_v2_override_encoding_combo;
    GtkWidget *id3_v2_iconv_label;
    GtkWidget *id3_v2_none_radio;
    GtkWidget *id3_v2_transliterate_radio;
    GtkWidget *id3_v2_ignore_radio;
    GtkWidget *id3_v1_check;
    GtkWidget *id3_v1_auto_add_remove;
    GtkWidget *id3_v1_encoding_label;
    GtkWidget *id3_v1_encoding_combo;
    GtkWidget *id3_v1_iconv_label;
    GtkWidget *id3_v1_iconv_box;
    GtkWidget *id3_v1_none_radio;
    GtkWidget *id3_v1_transliterate_radio;
    GtkWidget *id3_v1_ignore_radio;
    GtkWidget *id3_read_encoding_check;
    GtkWidget *id3_read_encoding_combo;
    GtkWidget *preferences_notebook;
#ifdef ENABLE_REPLAYGAIN
    GtkWidget *replaygain_grid;
    GtkWidget *replaygain_nogroup_radio;
    GtkWidget *replaygain_album_radio;
    GtkWidget *replaygain_disc_radio;
    GtkWidget *replaygain_filepath_radio;
    GtkWidget *replaygain_v1_radio;
    GtkWidget *replaygain_v2_radio;
    GtkWidget *replaygain_v15_radio;
#endif
    GtkWidget *scanner_grid;
    GtkWidget *fts_underscore_p20_radio;
    GtkWidget *fts_spaces_radio;
    GtkWidget *fts_none_radio;
    GtkWidget *rfs_underscore_p20_radio;
    GtkWidget *rfs_spaces_radio;
    GtkWidget *rfs_remove_radio;
    GtkWidget *pfs_uppercase_prep_check;
    GtkWidget *overwrite_fields_check;
    GtkWidget *default_comment_check;
    GtkWidget *default_comment_entry;
    GtkWidget *crc32_default_check;
    GtkWidget *cddb_automatic_host1_combo;
    GtkWidget *cddb_automatic_port1_button;
    GtkWidget *cddb_automatic_path1_entry;
    GtkWidget *cddb_automatic_host2_combo;
    GtkWidget *cddb_automatic_port2_button;
    GtkWidget *cddb_automatic_path2_entry;
    GtkWidget *cddb_manual_host_combo;
    GtkWidget *cddb_manual_port_button;
    GtkWidget *cddb_manual_path_entry;
    GtkWidget *cddb_follow_check;
    GtkWidget *cddb_dlm_check;
    GtkWidget *confirm_write_check;
    GtkWidget *confirm_rename_check;
    GtkWidget *confirm_delete_check;
    GtkWidget *confirm_write_playlist_check;
    GtkWidget *confirm_unsaved_files_check;
    GtkWidget *scanner_dialog_startup_check;
    GtkWidget *background_threads;

    GtkListStore *default_path_model;
    GtkListStore *file_player_model;

    gint options_notebook_scanner;
} EtPreferencesDialogPrivate;

// learn correct return type for et_browser_get_instance_private
#define et_preferences_dialog_get_instance_private et_preferences_dialog_get_instance_private_
G_DEFINE_TYPE_WITH_PRIVATE (EtPreferencesDialog, et_preferences_dialog, GTK_TYPE_DIALOG)
#undef et_preferences_dialog_get_instance_private
#define et_preferences_dialog_get_instance_private(x) (EtPreferencesDialogPrivate*)et_preferences_dialog_get_instance_private_(x)

/**************
 * Prototypes *
 **************/
/* Options window */
static void notify_id3_settings_active (GObject *object, GParamSpec *pspec, EtPreferencesDialog *self);

static void et_preferences_on_response (GtkDialog *dialog, gint response_id,
                                        gpointer user_data);


/*************
 * Functions *
 *************/
static void
et_prefs_current_folder_changed (EtPreferencesDialog *self,
                                 GtkFileChooser *default_path_button)
{
    gchar *path;

    /* The path that is currently selected, not that which is currently being
     * displayed. */
    path = gtk_file_chooser_get_filename (default_path_button);

    if (path)
    {
        g_settings_set_value (MainSettings, "default-path",
                              g_variant_new_bytestring (path));
        g_free (path);
    }
}

static void
on_default_path_changed (GSettings *settings,
                         const gchar *key,
                         GtkFileChooserButton *default_path_button)
{
    GVariant *default_path;
    const gchar *path;

    default_path = g_settings_get_value (settings, key);
    path = g_variant_get_bytestring (default_path);

    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (default_path_button),
                                         path);
    g_variant_unref (default_path);
}

#ifdef ENABLE_ID3LIB
static gboolean
et_preferences_id3v2_version_get (GValue *value,
                                  GVariant *variant,
                                  gpointer user_data)
{
    gboolean id3v24;

    id3v24 = g_variant_get_boolean (variant);

    g_value_set_int (value, id3v24 ? 1 : 0);

    return TRUE;
}

static GVariant *
et_preferences_id3v2_version_set (const GValue *value,
                                  const GVariantType *variant_type,
                                  gpointer user_data)
{
    GVariant *id3v24;
    gint active_row;

    active_row = g_value_get_int (value);

    id3v24 = g_variant_new_boolean (active_row == 1);

    return id3v24;
}
#endif /* ENABLE_ID3LIB */

static gboolean
et_preferences_id3v2_unicode_charset_get (GValue *value,
                                          GVariant *variant,
                                          gpointer user_data)
{
    const gchar *charset;

    charset = g_variant_get_string (variant, NULL);

    if (strcmp (charset, "UTF-8") == 0)
    {
        g_value_set_int (value, 0);
    }
    else if (strcmp (charset, "UTF-16") == 0)
    {
        g_value_set_int (value, 1);
    }
    else
    {
        return FALSE;
    }

    return TRUE;
}

static GVariant *
et_preferences_id3v2_unicode_charset_set (const GValue *value,
                                          const GVariantType *variant_type,
                                          gpointer user_data)
{
    gint active_row;

    active_row = g_value_get_int (value);

    switch (active_row)
    {
        case 0:
            return g_variant_new_string ("UTF-8");
            break;
        case 1:
            return g_variant_new_string ("UTF-16");
            break;
        default:
            g_assert_not_reached ();
    }
}

struct enum_info
{	const char* enum_nick;
	const char* setting;
};

static gint strv_indexof(const gchar*const* strv, const char* value)
{	if (strv)
		for (const gchar*const* strvi = strv; *strvi; ++strvi)
			if (strcmp(value, *strvi) == 0)
				return strvi - strv;
	return -1;
}

static gboolean flags_value_get(GValue *value, GVariant *variant, gpointer user_data)
{	const enum_info* data = (enum_info*)user_data;
	const gchar** values = g_variant_get_strv(variant, NULL);
	g_value_set_boolean(value, values && strv_indexof(values, data->enum_nick) >= 0);
	g_free(values);
	return TRUE;
}

static GVariant* flags_value_set(const GValue *value, const GVariantType *variant_type, gpointer user_data)
{	const enum_info* data = (enum_info*)user_data;
	GVariant* variant = g_settings_get_value(MainSettings, data->setting);
	gsize n;
	const gchar** values = g_variant_get_strv(variant, &n);
	GVariant* result = NULL;
	gint p = strv_indexof(values, data->enum_nick);
	if (g_value_get_boolean(value) ^ (p >= 0))
	{	// value does not match
		if (g_value_get_boolean(value))
		{	// add value
			values[n] = data->enum_nick;
			result = g_variant_new_strv(values, n + 1);
		} else
		{	// remove value
			copy(values + p + 1, values + n, values + p);
			result = g_variant_new_strv(values, n - 1);
		}
	}
	g_free(values);
	g_variant_unref(variant);
	return result;
}

static void bind_flags_value(const char* setting, GtkWidget* widget)
{	g_settings_bind_with_mapping(MainSettings, setting, widget, "active",
		G_SETTINGS_BIND_DEFAULT, flags_value_get, flags_value_set,
		new enum_info { gtk_widget_get_name(widget), setting }, operator delete);
}

static void bind_boolean(const char* setting, GtkWidget* widget)
{	g_settings_bind(MainSettings, setting, widget, "active", G_SETTINGS_BIND_DEFAULT);
}

static void bind_radio(const char* setting, GtkWidget* widget)
{	g_settings_bind_with_mapping(MainSettings, setting, widget, "active",
		G_SETTINGS_BIND_DEFAULT, et_settings_enum_radio_get, et_settings_enum_radio_set, widget, NULL);
}

/*
 * The window for options
 */
static void
et_preferences_dialog_init (EtPreferencesDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
    EtPreferencesDialogPrivate *priv;

    priv = et_preferences_dialog_get_instance_private (self);

    /* Browser. */
    on_default_path_changed (MainSettings, "default-path",
                             GTK_FILE_CHOOSER_BUTTON (priv->default_path_button));
    g_signal_connect (MainSettings, "changed::default-path",
                      G_CALLBACK (on_default_path_changed),
                      priv->default_path_button);

    /* Load directory on startup */
    bind_boolean("load-on-startup", priv->browser_startup_check);

    /* Browse subdirectories */
    bind_boolean("browse-subdir", priv->browser_subdirs_check);

    /* Open the node to show subdirectories */
    bind_boolean("browse-expand-children", priv->browser_expand_subdirs_check);

    /* Browse hidden directories */
    bind_boolean("browse-show-hidden", priv->browser_hidden_check);

    /* Row max lines */
    bind_boolean("browse-limit-lines", priv->browser_max_lines_check);
    g_settings_bind (MainSettings, "browse-max-lines",
        priv->browser_max_lines, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "browse-limit-lines",
        priv->browser_max_lines, "sensitive", G_SETTINGS_BIND_GET);

    bind_flags_value("hide-fields", priv->hide_fields_subtitle_check);
    bind_flags_value("hide-fields", priv->hide_fields_album_artist_check);
    bind_flags_value("hide-fields", priv->hide_fields_disc_subtitle_check);
    bind_flags_value("hide-fields", priv->hide_fields_disc_number_check);
    bind_flags_value("hide-fields", priv->hide_fields_release_year_check);
    bind_flags_value("hide-fields", priv->hide_fields_description_check);
    bind_flags_value("hide-fields", priv->hide_fields_composer_check);
    bind_flags_value("hide-fields", priv->hide_fields_orig_artist_check);
    bind_flags_value("hide-fields", priv->hide_fields_orig_year_check);
    bind_flags_value("hide-fields", priv->hide_fields_copyright_check);
    bind_flags_value("hide-fields", priv->hide_fields_url_check);
    bind_flags_value("hide-fields", priv->hide_fields_encoded_by_check);
    bind_flags_value("hide-fields", priv->hide_fields_replaygain);

#ifdef ENABLE_REPLAYGAIN
    gtk_widget_show(priv->replaygain_grid);
#endif

    /* Show / hide log view. */
    bind_boolean("log-show", priv->log_show_check);
   
    /* Show header information. */
    bind_boolean("file-show-header", priv->header_show_check);

    /* Display color mode for changed files in list. */
    /* Set "new" Gtk+-2.0ish black/bold style for changed items. */
    bind_boolean("file-changed-bold", priv->list_bold_radio);
    g_signal_connect_swapped (priv->list_bold_radio, "notify::active",
                              G_CALLBACK(et_browser_refresh_list),
                              MainWindow->browser());

    /*
     * File Settings
     */
    /* File (name) Options */
    bind_radio("rename-replace-illegal-chars", priv->file_name_replace_ascii);
    bind_radio("rename-replace-illegal-chars", priv->file_name_replace_unicode);
    bind_radio("rename-replace-illegal-chars", priv->file_name_replace_none);

    /* Extension case (lower/upper?) */
    bind_radio("rename-extension-mode", priv->name_lower_radio);
    bind_radio("rename-extension-mode", priv->name_upper_radio);
    bind_radio("rename-extension-mode", priv->name_no_change_radio);

    /* Preserve modification time */
    bind_boolean("file-preserve-modification-time", priv->file_preserve_check);

    /* Change directory modification time */
    bind_boolean("file-update-parent-modification-time", priv->file_parent_check);

    /* Character Set for Filename */
    bind_radio("rename-encoding", priv->file_encoding_try_alternative_radio);
    bind_radio("rename-encoding", priv->file_encoding_transliterate_radio);
    bind_radio("rename-encoding", priv->file_encoding_ignore_radio);

    /*
     * Tag Settings
     */
    /* Tag Options */
    bind_boolean("tag-date-autocomplete", priv->tags_auto_date_check);
    bind_boolean("tag-image-type-automatic", priv->tags_auto_image_type_check);

    /* Track formatting. */
    bind_boolean("tag-number-padded", priv->tags_track_check);
    g_settings_bind (MainSettings, "tag-number-length",
        priv->tags_track_button, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "tag-number-padded",
        priv->tags_track_button, "sensitive", G_SETTINGS_BIND_GET);

    /* Disc formatting. */
    bind_boolean("tag-disc-padded", priv->tags_disc_check);
    g_settings_bind (MainSettings, "tag-disc-length",
        priv->tags_disc_button, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "tag-disc-padded",
        priv->tags_disc_button, "sensitive", G_SETTINGS_BIND_GET);
    g_signal_emit_by_name (G_OBJECT (priv->tags_disc_check), "toggled");

    /* Tag field focus */
    bind_boolean("tag-preserve-focus", priv->tags_preserve_focus_check);

    bind_boolean("tag-multiline-comment", priv->tags_multiline_comment);

    /* Tag Splitting */
    g_settings_bind (MainSettings, "split-delimiter",
        priv->split_delimiter, "text", G_SETTINGS_BIND_DEFAULT);
    bind_flags_value("ogg-split-fields", priv->split_title_check);
    bind_flags_value("ogg-split-fields", priv->split_subtitle_check);
    bind_flags_value("ogg-split-fields", priv->split_artist_check);
    bind_flags_value("ogg-split-fields", priv->split_album_artist_check);
    bind_flags_value("ogg-split-fields", priv->split_album_check);
    bind_flags_value("ogg-split-fields", priv->split_disc_subtitle_check);
    bind_flags_value("ogg-split-fields", priv->split_genre_check);
    bind_flags_value("ogg-split-fields", priv->split_comment_check);
    bind_flags_value("ogg-split-fields", priv->split_description_check);
    bind_flags_value("ogg-split-fields", priv->split_composer_check);
    bind_flags_value("ogg-split-fields", priv->split_orig_artist_check);
    bind_flags_value("ogg-split-fields", priv->split_url_check);
    bind_flags_value("ogg-split-fields", priv->split_encoded_by_check);

    /*
     * ID3 Tag Settings
     */
    /* Strip tag when fields (managed by EasyTAG) are empty */
    bind_boolean("id3-strip-empty", priv->id3_strip_check);

    /* Convert old ID3v2 tag version */
    bind_boolean("id3v2-convert-old", priv->id3_v2_convert_check);

    /* Use CRC32 */
    bind_boolean("id3v2-crc32", priv->id3_v2_crc32_check);

    /* Use Compression */
    bind_boolean("id3v2-compression", priv->id3_v2_compression_check);
	
    /* Write Genre in text */
    bind_boolean("id3v2-text-only-genre", priv->id3_v2_genre_check);

    /* Write ID3v2 tag */
    bind_boolean("id3v2-enabled", priv->id3_v2_check);
    g_signal_connect (priv->id3_v2_check, "notify::active",
                      G_CALLBACK (notify_id3_settings_active), self);

#ifdef ENABLE_ID3LIB
    /* ID3v2 tag version */
    g_settings_bind_with_mapping (MainSettings, "id3v2-version-4",
                                  priv->id3_v2_version_combo, "active",
                                  G_SETTINGS_BIND_DEFAULT,
                                  et_preferences_id3v2_version_get,
                                  et_preferences_id3v2_version_set, self,
                                  NULL);
    g_signal_connect (MainSettings, "changed::id3v2-version-4",
                      G_CALLBACK (notify_id3_settings_active), self);
#endif

    /* Charset */
    /* Unicode. */
    g_settings_bind_with_mapping (MainSettings, "id3v2-unicode-charset",
                                  priv->id3_v2_unicode_encoding_combo,
                                  "active", G_SETTINGS_BIND_DEFAULT,
                                  et_preferences_id3v2_unicode_charset_get,
                                  et_preferences_id3v2_unicode_charset_set,
                                  NULL, NULL);

    g_settings_bind (MainSettings, "id3v2-enable-unicode",
                     priv->id3_v2_other_radio, "active",
                     G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);
    g_signal_connect (priv->id3_v2_unicode_radio, "notify::active",
                      G_CALLBACK (notify_id3_settings_active), self);

    /* Non-Unicode. */
    Charset_Populate_Combobox (GTK_COMBO_BOX (priv->id3_v2_override_encoding_combo),
                               g_settings_get_enum (MainSettings, "id3v2-no-unicode-charset"));
    g_settings_bind_with_mapping (MainSettings, "id3v2-no-unicode-charset",
                                  priv->id3_v2_override_encoding_combo,
                                  "active", G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_get, et_settings_enum_set,
                                  GSIZE_TO_POINTER (ET_TYPE_CHARSET), NULL);
    g_signal_connect (priv->id3_v2_other_radio, "notify::active",
                      G_CALLBACK (notify_id3_settings_active), self);

    /* ID3v2 Additional iconv() options. */
    bind_radio("id3v2-encoding-option", priv->id3_v2_none_radio);
    bind_radio("id3v2-encoding-option", priv->id3_v2_transliterate_radio);
    bind_radio("id3v2-encoding-option", priv->id3_v2_ignore_radio);

    /* Write ID3v1 tag */
    bind_boolean("id3v1-enabled", priv->id3_v1_check);
    g_signal_connect (priv->id3_v1_check, "notify::active",
                      G_CALLBACK (notify_id3_settings_active), self);
    bind_boolean("id3v1-auto-add-remove", priv->id3_v1_auto_add_remove);

    /* Id3V1 writing character set */
    Charset_Populate_Combobox (GTK_COMBO_BOX (priv->id3_v1_encoding_combo),
                               g_settings_get_enum (MainSettings, "id3v1-charset"));
    g_settings_bind_with_mapping (MainSettings, "id3v1-charset",
                                  priv->id3_v1_encoding_combo,
                                  "active", G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_get, et_settings_enum_set,
                                  GSIZE_TO_POINTER (ET_TYPE_CHARSET), NULL);

    /* ID3V1 Additional iconv() options*/
    bind_radio("id3v1-encoding-option", priv->id3_v1_none_radio);
    bind_radio("id3v1-encoding-option", priv->id3_v1_transliterate_radio);
    bind_radio("id3v1-encoding-option", priv->id3_v1_ignore_radio);

    /* Character Set for reading tag */
    /* "File Reading Charset" Check Button + Combo. */
    bind_boolean("id3-override-read-encoding", priv->id3_read_encoding_check);

    Charset_Populate_Combobox (GTK_COMBO_BOX (priv->id3_read_encoding_combo),
                               g_settings_get_enum (MainSettings, "id3v1v2-charset"));
    g_settings_bind_with_mapping (MainSettings, "id3v1v2-charset",
                                  priv->id3_read_encoding_combo,
                                  "active", G_SETTINGS_BIND_DEFAULT,
                                  et_settings_enum_get, et_settings_enum_set,
                                  GSIZE_TO_POINTER (ET_TYPE_CHARSET), NULL);
    g_settings_bind (MainSettings, "id3-override-read-encoding",
        priv->id3_read_encoding_combo, "sensitive", G_SETTINGS_BIND_GET);
    notify_id3_settings_active (NULL, NULL, self);

    /*
     * ReplayGain
     */
#ifdef ENABLE_REPLAYGAIN
    bind_radio("replaygain-groupby", priv->replaygain_nogroup_radio);
    bind_radio("replaygain-groupby", priv->replaygain_album_radio);
    bind_radio("replaygain-groupby", priv->replaygain_disc_radio);
    bind_radio("replaygain-groupby", priv->replaygain_filepath_radio);

    bind_radio("replaygain-model", priv->replaygain_v1_radio);
    bind_radio("replaygain-model", priv->replaygain_v2_radio);
    bind_radio("replaygain-model", priv->replaygain_v15_radio);
#endif

    /*
     * Scanner
     */
    /* Save the number of the page. Asked in Scanner window */
    priv->options_notebook_scanner = gtk_notebook_page_num (GTK_NOTEBOOK (priv->preferences_notebook),
                                                            priv->scanner_grid);

    /* Character conversion for the 'Fill Tag' scanner (=> FTS...) */
    bind_radio("fill-convert-spaces", priv->fts_underscore_p20_radio);
    bind_radio("fill-convert-spaces", priv->fts_spaces_radio);
    bind_radio("fill-convert-spaces", priv->fts_none_radio);
    /* TODO: No change tooltip. */

    /* Character conversion for the 'Rename File' scanner (=> RFS...) */
    bind_radio("rename-convert-spaces", priv->rfs_underscore_p20_radio);
    bind_radio("rename-convert-spaces", priv->rfs_spaces_radio);
    bind_radio("rename-convert-spaces", priv->rfs_remove_radio);

    /* Character conversion for the 'Process Fields' scanner (=> PFS...) */
    bind_boolean("process-uppercase-prepositions", priv->pfs_uppercase_prep_check);

    /* Other options */
    bind_boolean("fill-overwrite-tag-fields", priv->overwrite_fields_check);

    /* Set a default comment text or CRC-32 checksum. */
    bind_boolean("fill-set-default-comment", priv->default_comment_check);
    g_settings_bind (MainSettings, "fill-set-default-comment",
        priv->default_comment_entry, "sensitive", G_SETTINGS_BIND_GET);
    g_settings_bind (MainSettings, "fill-default-comment",
        priv->default_comment_entry, "text", G_SETTINGS_BIND_DEFAULT);

    /* CRC32 comment. */
    bind_boolean("fill-crc32-comment", priv->crc32_default_check);

    /* CDDB */
    /* 1st automatic search server. */
    g_settings_bind (MainSettings, "cddb-automatic-search-hostname",
        gtk_bin_get_child (GTK_BIN (priv->cddb_automatic_host1_combo)), "text", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "cddb-automatic-search-port",
        priv->cddb_automatic_port1_button, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "cddb-automatic-search-path",
        priv->cddb_automatic_path1_entry, "text", G_SETTINGS_BIND_DEFAULT);

    /* 2nd automatic search server. */
    g_settings_bind (MainSettings, "cddb-automatic-search-hostname2",
        gtk_bin_get_child (GTK_BIN (priv->cddb_automatic_host2_combo)), "text", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "cddb-automatic-search-port2",
        priv->cddb_automatic_port2_button, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "cddb-automatic-search-path2",
        priv->cddb_automatic_path2_entry, "text", G_SETTINGS_BIND_DEFAULT);

    /* CDDB Server Settings (Manual Search). */
    g_settings_bind (MainSettings, "cddb-manual-search-hostname",
        gtk_bin_get_child (GTK_BIN (priv->cddb_manual_host_combo)), "text", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "cddb-manual-search-port",
        priv->cddb_manual_port_button, "value", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind (MainSettings, "cddb-manual-search-path",
        priv->cddb_manual_path_entry, "text", G_SETTINGS_BIND_DEFAULT);

    /* Track Name list (CDDB results). */
    bind_boolean("cddb-follow-file", priv->cddb_follow_check);

    /* Check box to use DLM. */
    bind_boolean("cddb-dlm-enabled", priv->cddb_dlm_check);

    /* Confirmation */
    bind_boolean("confirm-write-tags", priv->confirm_write_check);
    bind_boolean("confirm-rename-file", priv->confirm_rename_check);
    bind_boolean("confirm-delete-file", priv->confirm_delete_check);
    bind_boolean("confirm-write-playlist", priv->confirm_write_playlist_check);
    bind_boolean("confirm-when-unsaved-files", priv->confirm_unsaved_files_check);

    /* background processing */
    g_settings_bind (MainSettings, "background-threads",
        priv->background_threads, "value", G_SETTINGS_BIND_DEFAULT);

    /* Properties of the scanner window */
    bind_boolean("scan-startup", priv->scanner_dialog_startup_check);

    /* Load the default page */
    g_settings_bind (MainSettings, "preferences-page",
        priv->preferences_notebook, "page", G_SETTINGS_BIND_DEFAULT);
}

static void
notify_id3_settings_active (GObject *object,
                            GParamSpec *pspec,
                            EtPreferencesDialog *self)
{
    EtPreferencesDialogPrivate *priv;
    gboolean active;

    priv = et_preferences_dialog_get_instance_private (self);

    active = g_settings_get_boolean (MainSettings, "id3v2-enable-unicode");

    if (g_settings_get_boolean (MainSettings, "id3v2-enabled"))
    {
        gtk_widget_set_sensitive (priv->id3_v2_encoding_label, TRUE);

#ifdef ENABLE_ID3LIB
        gtk_widget_set_sensitive (priv->id3_v2_version_label, TRUE);
        gtk_widget_set_sensitive (priv->id3_v2_version_combo, TRUE);

        if (!g_settings_get_boolean (MainSettings, "id3v2-version-4"))
        {
            /* When "ID3v2.3" is selected. */
            gtk_combo_box_set_active (GTK_COMBO_BOX (priv->id3_v2_unicode_encoding_combo), 1);
            gtk_widget_set_sensitive (priv->id3_v2_unicode_encoding_combo,
                                      FALSE);
        }
        else
        {
            /* When "ID3v2.4" is selected, set "UTF-8" as default value. */
            gtk_combo_box_set_active (GTK_COMBO_BOX (priv->id3_v2_unicode_encoding_combo),
                                      0);
            gtk_widget_set_sensitive (priv->id3_v2_unicode_encoding_combo,
                                      active);
        }
#else 
        gtk_widget_set_sensitive (priv->id3_v2_unicode_encoding_combo,
                                  active);
#endif
        gtk_widget_set_sensitive(priv->id3_v2_unicode_radio, TRUE);
        gtk_widget_set_sensitive(priv->id3_v2_other_radio, TRUE);
        gtk_widget_set_sensitive (priv->id3_v2_override_encoding_combo,
                                  !active);
        gtk_widget_set_sensitive (priv->id3_v2_iconv_label, !active);
        gtk_widget_set_sensitive (priv->id3_v2_none_radio, !active);
        gtk_widget_set_sensitive (priv->id3_v2_transliterate_radio, !active);
        gtk_widget_set_sensitive (priv->id3_v2_ignore_radio, !active);
        gtk_widget_set_sensitive (priv->id3_v2_crc32_check, TRUE);
        gtk_widget_set_sensitive (priv->id3_v2_compression_check, TRUE);
        gtk_widget_set_sensitive (priv->id3_v2_genre_check, TRUE);
        gtk_widget_set_sensitive (priv->id3_v2_convert_check, TRUE);

    }else
    {
        gtk_widget_set_sensitive (priv->id3_v2_encoding_label, FALSE);
#ifdef ENABLE_ID3LIB
        gtk_widget_set_sensitive (priv->id3_v2_version_label, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_version_combo, FALSE);
#endif
        gtk_widget_set_sensitive (priv->id3_v2_unicode_radio, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_other_radio, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_unicode_encoding_combo, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_override_encoding_combo, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_iconv_label, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_none_radio, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_transliterate_radio, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_ignore_radio, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_crc32_check, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_compression_check, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_genre_check, FALSE);
        gtk_widget_set_sensitive (priv->id3_v2_convert_check, FALSE);
    }

    active = g_settings_get_boolean (MainSettings, "id3v1-enabled");

    gtk_widget_set_sensitive (priv->id3_v1_encoding_label, active);
    gtk_widget_set_sensitive (priv->id3_v1_encoding_combo, active);
    gtk_widget_set_sensitive (priv->id3_v1_iconv_label, active);
    gtk_widget_set_sensitive (priv->id3_v1_iconv_box, active);
}

/*
 * Check_Config: Check if config information are correct
 *
 * Problem noted : if a character is escaped (like : 'C\351line DION') in
 *                 gtk_file_chooser it will converted to UTF-8. So after, there
 *                 is a problem to convert it in the right system encoding to be
 *                 passed to stat(), and it can find the directory.
 * exemple :
 *  - initial file on system                        : C\351line DION - D'eux (1995)
 *  - converted to UTF-8 (path_utf8)                : Céline DION - D'eux (1995)
 *  - try to convert to system encoding (path_real) : ?????
 */
static gboolean
Check_DefaultPathToMp3 (EtPreferencesDialog *self)
{
    GVariant *default_path;
    const gchar *path_real;
    GFile *file;
    GFileInfo *fileinfo;

    default_path = g_settings_get_value (MainSettings, "default-path");
    path_real = g_variant_get_bytestring (default_path);

    if (!*path_real)
    {
        g_variant_unref (default_path);
        return TRUE;
    }

    file = g_file_new_for_path (path_real);
    fileinfo = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                  G_FILE_QUERY_INFO_NONE, NULL, NULL);
    g_variant_unref (default_path);
    g_object_unref (file);

    if (fileinfo)
    {
        if (g_file_info_get_file_type (fileinfo) == G_FILE_TYPE_DIRECTORY)
        {
            g_object_unref (fileinfo);
            return TRUE; /* Path is good */
        }
        else
        {
            GtkWidget *msgdialog;
            const gchar *path_utf8;

            path_utf8 = g_file_info_get_display_name (fileinfo);
            msgdialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                                GTK_DIALOG_MODAL
                                                | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_CLOSE,
                                                "%s",
                                                _("The selected default path is invalid"));
            gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msgdialog),
                                                      _("Path: ‘%s’\nError: %s"),
                                                      path_utf8,
                                                      g_strerror (errno));
            gtk_window_set_title (GTK_WINDOW (msgdialog),
                                  _("Invalid Path Error"));

            gtk_dialog_run (GTK_DIALOG (msgdialog));
            gtk_widget_destroy (msgdialog);
        }

        g_object_unref (fileinfo);
    }

    return FALSE;
}

static gboolean
Check_Config (EtPreferencesDialog *self)
{
    if (Check_DefaultPathToMp3 (self))
        return TRUE; /* No problem detected */
    else
        return FALSE; /* Oops! */
}

/* Callback from et_preferences_dialog_on_response. */
static void
OptionsWindow_Save_Button (EtPreferencesDialog *self)
{
    if (!Check_Config (self)) return;

    gtk_widget_hide (GTK_WIDGET (self));
}

void
et_preferences_dialog_show_scanner (EtPreferencesDialog *self)
{
    EtPreferencesDialogPrivate *priv;

    g_return_if_fail (ET_PREFERENCES_DIALOG (self));

    priv = et_preferences_dialog_get_instance_private (self);

    gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->preferences_notebook),
                                   priv->options_notebook_scanner);
    gtk_window_present (GTK_WINDOW (self));
}

/*
 * et_preferences_on_response:
 * @dialog: the dialog which trigerred the response signal
 * @response_id: the response which was triggered
 * @user_data: user data set when the signal was connected
 *
 * Signal handler for the response signal, to check whether the OK or cancel
 * button was clicked, or if a delete event was received.
 */
static void
et_preferences_on_response (GtkDialog *dialog, gint response_id,
                            gpointer user_data)
{
    switch (response_id)
    {
        case GTK_RESPONSE_CLOSE:
            OptionsWindow_Save_Button (ET_PREFERENCES_DIALOG (dialog));
            break;
        case GTK_RESPONSE_DELETE_EVENT:
            break;
        default:
            g_assert_not_reached ();
    }
}

static void
et_preferences_dialog_class_init (EtPreferencesDialogClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/EasyTAG/preferences_dialog.ui");
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, default_path_button);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, browser_startup_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, browser_subdirs_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, browser_expand_subdirs_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, browser_hidden_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, browser_max_lines_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, browser_max_lines);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_subtitle_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_album_artist_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_disc_subtitle_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_disc_number_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_release_year_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_description_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_composer_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_orig_artist_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_orig_year_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_copyright_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_url_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_encoded_by_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, hide_fields_replaygain);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, log_show_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, header_show_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, list_bold_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, file_name_replace_ascii);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, file_name_replace_unicode);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, file_name_replace_none);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, name_lower_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, name_upper_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, name_no_change_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, file_preserve_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, file_parent_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, file_encoding_try_alternative_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, file_encoding_transliterate_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, file_encoding_ignore_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, tags_auto_date_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, tags_auto_image_type_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, tags_track_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, tags_track_button);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, tags_disc_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, tags_disc_button);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, tags_preserve_focus_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, tags_multiline_comment);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_title_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_subtitle_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_artist_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_album_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_album_artist_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_disc_subtitle_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_genre_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_comment_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_description_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_composer_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_orig_artist_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_url_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_encoded_by_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, split_delimiter);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_strip_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_convert_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_crc32_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_compression_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_genre_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_version_label);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_version_combo);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_encoding_label);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_unicode_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_unicode_encoding_combo);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_other_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_override_encoding_combo);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_iconv_label);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_none_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_transliterate_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v2_ignore_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v1_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v1_auto_add_remove);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v1_encoding_label);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v1_encoding_combo);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v1_iconv_label);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v1_iconv_box);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v1_none_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v1_transliterate_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_v1_ignore_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_read_encoding_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, id3_read_encoding_combo);
#ifdef ENABLE_REPLAYGAIN
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, replaygain_grid);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, replaygain_nogroup_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, replaygain_album_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, replaygain_disc_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, replaygain_filepath_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, replaygain_v1_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, replaygain_v2_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, replaygain_v15_radio);
#endif
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, preferences_notebook);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, scanner_grid);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, fts_underscore_p20_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, fts_spaces_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, fts_none_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, rfs_underscore_p20_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, rfs_spaces_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, rfs_remove_radio);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, pfs_uppercase_prep_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, overwrite_fields_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, default_comment_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, default_comment_entry);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, crc32_default_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, cddb_automatic_host1_combo);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, cddb_automatic_port1_button);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, cddb_automatic_path1_entry);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, cddb_automatic_host2_combo);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, cddb_automatic_port2_button);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, cddb_automatic_path2_entry);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, cddb_manual_host_combo);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, cddb_manual_port_button);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, cddb_manual_path_entry);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, cddb_follow_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, cddb_dlm_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, confirm_write_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, confirm_rename_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, confirm_delete_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, confirm_write_playlist_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, confirm_unsaved_files_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, scanner_dialog_startup_check);
    gtk_widget_class_bind_template_child_private(widget_class, EtPreferencesDialog, background_threads);
    gtk_widget_class_bind_template_callback(widget_class, et_preferences_on_response);
    gtk_widget_class_bind_template_callback(widget_class, et_prefs_current_folder_changed);
}

/*
 * et_preferences_dialog_new:
 *
 * Create a new EtPreferencesDialog instance.
 *
 * Returns: a new #EtPreferencesDialog
 */
EtPreferencesDialog *
et_preferences_dialog_new (GtkWindow *parent)
{
    GtkSettings *settings;
    gboolean use_header_bar = FALSE;

    g_return_val_if_fail (GTK_WINDOW (parent), NULL);

    settings = gtk_settings_get_default ();

    if (settings)
    {
        g_object_get (settings, "gtk-dialogs-use-header", &use_header_bar, NULL);
    }

    return (EtPreferencesDialog*)g_object_new (ET_TYPE_PREFERENCES_DIALOG, "transient-for", parent,
                         "use-header-bar", use_header_bar, NULL);
}
