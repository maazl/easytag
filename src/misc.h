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

#ifndef ET_MISC_H_
#define ET_MISC_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
#include <string>
#include <memory>
#include <array>
#include <type_traits>

std::string strprintf(const char* format, ...)
#ifdef __GNUC__
__attribute__ ((format(printf, 1, 2)))
#endif
;

template <typename T>
constexpr inline int sign(T value) { return (value > 0) - (value < 0); }

#define MAKE_FLAGS_ENUM(t) \
constexpr inline t operator|(t l, t r) { return (t)((int)l | (int)r); } \
constexpr inline t operator&(t l, t r) { return (t)((int)l & (int)r); }

MAKE_FLAGS_ENUM(GtkDialogFlags)
MAKE_FLAGS_ENUM(GtkDestDefaults)
MAKE_FLAGS_ENUM(GSettingsBindFlags)

/// Deleter for GLIB objects
struct gDeleter
{	void operator()(gchar* ptr) { g_free(ptr); }
};
/// Managed GLIB object
template <typename T>
using gObject = std::unique_ptr<T, gDeleter>;
/// Managed GLIB string
struct gString : gObject<gchar>
{	gString() { }
	explicit gString(gchar* ptr) : std::unique_ptr<gchar, gDeleter>(ptr) { }
	operator const gchar*() const { return get(); }
	gString& operator=(gchar* ptr) { reset(ptr); return *this; }
};

/// Strongly typed GList entry
/// @tparam T Must be a pointer like type.
/// @remarks Binary compatible to GList, use reinterpret cast.
template <typename T>
struct gList
{	T data;
	gList<T> *next;
	gList<T> *prev;
};
/// Strongly typed GList pointer
/// @tparam T Must be a pointer like type.
template <typename T>
class gListP
{	gList<T>* ptr;
public:
	gListP(gList<T>* p) : ptr(p) {}
	explicit gListP(GList* p) : ptr((gList<T>*)p) {}
	explicit gListP(T t) : ptr((gList<T>*)g_list_append(nullptr, t)) {}
	gListP<T>& operator =(gList<T>* p) { ptr = p; return *this; }
	operator GList*() const { return (GList*)ptr; }
	gList<T>* operator ->() const { return ptr; }
	gListP<T> last() const { return gListP<T>(g_list_last(*this)); }
	gListP<T> append(T t) { return gListP<T>(g_list_append(*this, t)); }
	template<typename U, typename std::enable_if<std::is_convertible<T, U>::value, bool>::type = true>
	gListP<T> insert_sorted(T t, gint (*cmp)(U a, U b))
	{ return gListP<T>(g_list_insert_sorted(*this, t, (GCompareFunc)cmp)); }
	gListP<T> concat(gListP<T> l) { return gListP<T>(g_list_concat(*this, l)); }
};

/** create unique pointer with explicit deleter
 * @tparam T pointer target type
 * @tparam D deleter type
 * @param ptr
 * @param deleter
 * @return the unique pointer
 * @example This function can be used to create managed objects for C APIs.
 * @code auto s = make_unique(g_strdup(...), g_free);
 */
template <typename T, typename D>
std::unique_ptr<T, D> make_unique(T* ptr, D deleter)
{	return std::unique_ptr<T, D>(ptr, deleter);
}

// Reference helper to allow array of references
template <typename T>
struct reference
{	T& Ref;
	constexpr operator T&() const { return Ref; }
};

#endif

G_BEGIN_DECLS

/*
 * Combobox misc functions
 */
gboolean Add_String_To_Combo_List(GtkListStore *liststore, const gchar *string);

gboolean et_variant_string_array_contains(GVariant* variant, const char* value);
GVariant* et_variant_string_array_toggle(GVariant* variant, const char* value);
GVariant* et_variant_string_array_set(GVariant* variant, const char* value, gboolean set);

gchar *Convert_Duration (gulong duration);

gboolean et_run_audio_player (GList *files, GError **error);
gboolean et_run_program (const gchar *program_name, GList *args_list, GError **error);

/** Pad the number with trailing zeros according to settings "tag-disc-padded", "tag-disc-length".
 * @param disc_number
 * @return new string
 */
gchar* et_disc_number_to_string (const gchar* disc_number);
/** Pad the number with trailing zeros according to settings "tag-number-padded", "tag-number-length".
 * @param disc_number
 * @return new string
 */
gchar* et_track_number_to_string (const gchar* track_number);

gboolean et_rename_file (const gchar *old_filename, const gchar *new_filename, GError **error);

guint et_undo_key_new (void);
gint et_normalized_strcmp0 (const gchar *str1, const gchar *str2);
gint et_normalized_strcasecmp0 (const gchar *str1, const gchar *str2);
/*
 * et_str_empty:
 * @str: string to test for emptiness
 *
 * Test if @str is empty, in other words either %NULL or the empty string.
 *
 * Returns: %TRUE is @str is either %NULL or "", %FALSE otherwise
 */
inline gboolean et_str_empty (const gchar *str) { return !str || !str[0]; }

G_END_DECLS

#endif /* ET_MISC_H_ */
