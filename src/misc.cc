/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2014-2015  David King <amigadave@amigadave.com>
 * Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "misc.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "setting.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif /* G_OS_WIN32 */

#include <cmath>
#include <atomic>
#include <limits>
using namespace std;

string strprintf(const char* format, ...)
{	va_list va;
	va_start(va, format);
	size_t size = std::vsnprintf(nullptr, 0, format, va);
	va_end(va);
	string buf(size, 0);
	va_start(va, format);
	vsnprintf(&buf[0], size + 1, format, va);
	va_end(va);
	return buf;
}


void bswap(uint32_t& v)
{	uint8_t* cp = (uint8_t*)&v;
	swap(cp[0], cp[3]);
	swap(cp[1], cp[2]);
}

void bswap(uint16_t& v)
{	uint8_t* cp = (uint8_t*)&v;
	swap(cp[0], cp[1]);
}

const constexpr Guid Guid::empty = {0};

Guid Guid::parse(const char* s)
{	Guid result = {0};
	if (!s || !*s)
		return result;

	int i = 0;
	for (; *s; ++s)
	{	if (*s == '-')
			continue;
		uint8_t c = tolower(*s);
		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c >= 'a' && c <= 'f')
			c -= 'a' - 10;
		else
			goto error;

		if (i & 1)
			c <<= 4;
		result.Value[i << 1] |= c;
	}

	if (i == 32)
		return result;
error:
	result = empty;

	return result;
}

gchar* Guid::toString() const
{	if (*this == empty)
		return nullptr;

	gchar* result = (gchar*)g_malloc(37);
	gchar* dp = result;

	auto store_nibble = [&dp](uint8_t value)
	{	*dp++ = value + (value >= 10 ? 'a' - 10 : '0');
	};

	for (uint8_t c : Value)
	{	store_nibble(c >> 4);
		store_nibble(c & 0xf);
		switch (dp - result)
		{case 8:
		 case 13:
		 case 18:
		 case 23:
			*dp++ = '-';
		}
	}

	return result;
}


guint gIdleAdd(std::function<void()>* func, gint priority)
{	return g_idle_add_full(
		priority,
		[](void* user_data) { (*static_cast<std::function<void()>*>(user_data))(); return G_SOURCE_REMOVE; },
		func,
		[](void* user_data) { delete static_cast<std::function<void()>*>(user_data); });
}


/*
 * Add the 'string' passed in parameter to the list store
 * If this string already exists in the list store, it doesn't add it.
 * Returns TRUE if string was added.
 */
gboolean Add_String_To_Combo_List (GtkListStore *liststore, const gchar *str)
{
    GtkTreeIter iter;
    gchar *text;
    const gint HISTORY_MAX_LENGTH = 15;
    //gboolean found = FALSE;
    gchar *string = g_strdup(str);

    if (et_str_empty (string))
    {
        g_free (string);
        return FALSE;
    }

#if 0
    // We add the string to the beginning of the list store
    // So we will start to parse from the second line below
    gtk_list_store_prepend(liststore, &iter);
    gtk_list_store_set(liststore, &iter, MISC_COMBO_TEXT, string, -1);

    // Search in the list store if string already exists and remove other same strings in the list
    found = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(liststore), &iter);
    //gtk_tree_model_get(GTK_TREE_MODEL(liststore), &iter, MISC_COMBO_TEXT, &text, -1);
    while (found && gtk_tree_model_iter_next(GTK_TREE_MODEL(liststore), &iter))
    {
        gtk_tree_model_get(GTK_TREE_MODEL(liststore), &iter, MISC_COMBO_TEXT, &text, -1);
        //g_print(">0>%s\n>1>%s\n",string,text);
        if (g_utf8_collate(text, string) == 0)
        {
            g_free(text);
            // FIX ME : it seems that after it selects the next item for the
            // combo (changes 'string')????
            // So should select the first item?
            gtk_list_store_remove(liststore, &iter);
            // Must be rewinded?
            found = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(liststore), &iter);
            //gtk_tree_model_get(GTK_TREE_MODEL(liststore), &iter, MISC_COMBO_TEXT, &text, -1);
            continue;
        }
        g_free(text);
    }

    // Limit list size to HISTORY_MAX_LENGTH
    while (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(liststore),NULL) > HISTORY_MAX_LENGTH)
    {
        if ( gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(liststore),
                                           &iter,NULL,HISTORY_MAX_LENGTH) )
        {
            gtk_list_store_remove(liststore, &iter);
        }
    }

    g_free(string);
    // Place again to the beginning of the list, to select the right value?
    //gtk_tree_model_get_iter_first(GTK_TREE_MODEL(liststore), &iter);

    return TRUE;

#else

    // Search in the list store if string already exists.
    // FIXME : insert string at the beginning of the list (if already exists),
    //         and remove other same strings in the list
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(liststore), &iter))
    {
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(liststore), &iter, MISC_COMBO_TEXT, &text, -1);
            if (g_utf8_collate(text, string) == 0)
            {
                g_free (string);
                g_free(text);
                return FALSE;
            }

            g_free(text);
        } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(liststore), &iter));
    }

    /* We add the string to the beginning of the list store. */
    gtk_list_store_insert_with_values (liststore, &iter, 0, MISC_COMBO_TEXT,
                                       string, -1);

    // Limit list size to HISTORY_MAX_LENGTH
    while (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(liststore),NULL) > HISTORY_MAX_LENGTH)
    {
        if ( gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(liststore),
                                           &iter,NULL,HISTORY_MAX_LENGTH) )
        {
            gtk_list_store_remove(liststore, &iter);
        }
    }

    g_free(string);
    return TRUE;
#endif
}

gboolean et_variant_string_array_contains(GVariant* variant, const char* value)
{	if (!variant)
		return FALSE;
	GVariantIter iter;
	const gchar *flag;
	g_variant_iter_init(&iter, variant);
	while (g_variant_iter_next(&iter, "&s", &flag))
		if (strcmp(flag, value) == 0)
			return TRUE;
	return FALSE;
}

GVariant* et_variant_string_array_toggle(GVariant* variant, const char* value)
{	bool found = false;
	const gchar* newvalue[(variant ? g_variant_n_children(variant) : 0) + 1]; // space for new value
	const gchar** next = newvalue;
	if (variant)
	{	GVariantIter iter;
		const gchar *flag;
		g_variant_iter_init(&iter, variant);
		while (g_variant_iter_next(&iter, "&s", &flag))
			if (strcmp(flag, value) == 0)
				found = true;
			else
				*next++ = flag;
	}
	if (!found)
		*next++ = value;
	return g_variant_new_strv(newvalue, next - newvalue);
}

GVariant* et_variant_string_array_set(GVariant* variant, const char* value, gboolean set)
{	const gchar* newvalue[(variant ? g_variant_n_children(variant) : 0) + !!set]; // space for new value
	const gchar** next = newvalue;
	if (variant)
	{	GVariantIter iter;
		const gchar *flag;
		g_variant_iter_init(&iter, variant);
		while (g_variant_iter_next(&iter, "&s", &flag))
			if (strcmp(flag, value) != 0)
				*next++ = flag;
			else if (set)
				return g_variant_ref(variant);
	}
	if (set)
		*next++ = value;
	else
		return g_variant_ref(variant);
	return g_variant_new_strv(newvalue, next - newvalue);
}

string et_file_duration_to_string(double duration)
{	string result;
	if (duration > 0 && duration < numeric_limits<unsigned>::max())
	{	unsigned d = duration + .5;
		if (d > 86400)
			sprintf(&result.assign(20, 0).front(), "%i %02i:%02i:%02i", d /86400, d / 3600 % 24, d / 60 % 60, d % 60);
		else if (d > 3600)
			sprintf(&result.assign(10, 0).front(), "%i:%02i:%02i", d / 3600, d / 60 % 60, d % 60);
		else
			sprintf(&result.assign(6, 0).front(), "%i:%02i", d / 60, d % 60);
	}
	return result;
}

/*
 * et_rename_file:
 * @old_filepath: path of file to be renamed
 * @new_filepath: path of renamed file
 * @error: a #GError to provide information on errors, or %NULL to ignore
 *
 * Rename @old_filepath to @new_filepath.
 *
 * Returns: %TRUE if the rename was successful, %FALSE otherwise
 */
gboolean
et_rename_file (const char *old_filepath,
                const char *new_filepath,
                GError **error)
{
    GFile *file_old;
    GFile *file_new;
    GFile *file_new_parent;

    g_return_val_if_fail (old_filepath != NULL && new_filepath != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    file_old = g_file_new_for_path (old_filepath);
    file_new = g_file_new_for_path (new_filepath);
    file_new_parent = g_file_get_parent (file_new);

    if (!g_file_make_directory_with_parents (file_new_parent, NULL, error))
    {
        /* Ignore an error if the directory already exists. */
        if (!g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (file_new_parent);
            goto err;
        }

        g_clear_error (error);
    }

    g_assert (error == NULL || *error == NULL);
    g_object_unref (file_new_parent);

    /* Move the file. */
    if (!g_file_move (file_old, file_new, G_FILE_COPY_NONE, NULL, NULL, NULL,
                      error))
    {
        if (g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            /* Possibly a case change on a case-insensitive filesystem. */
            /* TODO: casefold the paths of both files, and check to see whether
             * they only differ by case? */
            gchar *tmp_filename;
            mode_t old_mode;
            gint fd;
            GFile *tmp_file;
            GError *tmp_error = NULL;

            tmp_filename = g_strconcat (old_filepath, ".XXXXXX", NULL);

            old_mode = umask (077);
            fd = g_mkstemp (tmp_filename);
            umask (old_mode);

            if (fd >= 0)
            {
                /* TODO: Handle error. */
                g_close (fd, NULL);
            }

            tmp_file = g_file_new_for_path (tmp_filename);
            g_free (tmp_filename);

            if (!g_file_move (file_old, tmp_file, G_FILE_COPY_OVERWRITE, NULL,
                              NULL, NULL, &tmp_error))
            {
                g_file_delete (tmp_file, NULL, NULL);

                g_object_unref (tmp_file);
                g_clear_error (error);
                g_propagate_error (error, tmp_error);
                goto err;
            }
            else
            {
                /* Move to temporary file succeeded, now move to the real new
                 * location. */
                if (!g_file_move (tmp_file, file_new, G_FILE_COPY_NONE, NULL,
                                  NULL, NULL, &tmp_error))
                {
                    g_file_move (tmp_file, file_old, G_FILE_COPY_NONE, NULL,
                                 NULL, NULL, NULL);
                    g_object_unref (tmp_file);
                    g_clear_error (error);
                    g_propagate_error (error, tmp_error);
                    goto err;
                }
                else
                {
                    /* Move succeeded, so clear the original error about the
                     * new file already existing. */
                    g_object_unref (tmp_file);
                    g_clear_error (error);
                    goto out;
                }
            }
        }
        else
        {
            /* Error moving file. */
            goto err;
        }
    }

out:
    g_object_unref (file_old);
    g_object_unref (file_new);
    g_assert (error == NULL || *error == NULL);
    return TRUE;

err:
    g_object_unref (file_old);
    g_object_unref (file_new);
    g_assert (error == NULL || *error != NULL);
    return FALSE;
}

/*// Replace dir separator by dot for more reasonable sort order
static void ds2dot(gchar* cp)
{	for (; *cp; ++cp)
		if (G_IS_DIR_SEPARATOR(*cp))
			*cp = '.';
}

PathCmp et_path_compare(const gchar* left, const gchar* right)
{
	size_t llen = strlen(left);
	if (!llen)
		return *right ? PathCmp::Less : PathCmp::Equal;
	size_t rlen = strlen(right);
	if (!rlen)
		return PathCmp::Greater;
	if (G_IS_DIR_SEPARATOR(left[llen - 1]))
		--llen;
	if (G_IS_DIR_SEPARATOR(right[rlen - 1]))
		--rlen;
	size_t clen = min(llen, rlen);
#ifdef G_OS_WIN32
	int cmp = strncasecmp(left, right, clen);
#else
	int cmp = strncmp(left, right, clen);
#endif

	// check for possible subdir relation.
	if (!cmp)
	{	if (llen == rlen)
			return PathCmp::Equal;
		if (llen < rlen)
			return G_IS_DIR_SEPARATOR(right[clen]) ? PathCmp::IsSuperDir : PathCmp::Less;
		else
			return G_IS_DIR_SEPARATOR(left[clen]) ? PathCmp::IsSubDir : PathCmp::Greater;
	}

	// no prefix relation => need to do slow comparison for collation
#ifdef G_OS_WIN32
	gString ck1(g_utf8_strdown(left, clen));
	gString ck2(g_utf8_strdown(right, clen));
#else
	gString ck1(g_filename_to_utf8(left, clen, NULL, &llen, NULL));
	gString ck2(g_filename_to_utf8(right, clen, NULL, &rlen, NULL));
#endif
	ds2dot(ck1.get());
	ds2dot(ck2.get());
	ck1 = gString(g_utf8_collate_key_for_filename(ck1, -1));
	ck2 = gString(g_utf8_collate_key_for_filename(ck2, -1));

	return (PathCmp)sign(strcmp(ck1, ck2));
}*/
