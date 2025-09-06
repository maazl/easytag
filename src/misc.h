/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2024-2025  Marcel Müller <github@maazl.de>
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

#include <string>
#include <memory>
#include <array>
#include <cmath>
#include <type_traits>
#include <functional>

std::string strprintf(const char* format, ...)
#ifdef __GNUC__
__attribute__ ((format(printf, 1, 2)))
#endif
;

void bswap(uint32_t& v);
void bswap(uint16_t& v);

template <typename T>
constexpr inline int sign(T value) { return (value > 0) - (value < 0); }

#define MAKE_FLAGS_ENUM(t) \
constexpr inline t operator|(t l, t r) { return (t)((int)l | (int)r); } \
constexpr inline t operator&(t l, t r) { return (t)((int)l & (int)r); }

MAKE_FLAGS_ENUM(GtkDialogFlags)
MAKE_FLAGS_ENUM(GtkDestDefaults)
MAKE_FLAGS_ENUM(GSettingsBindFlags)

/// Deleter for GLIB allocations
struct gAllocDeleter
{	void operator()(void* ptr) { g_free(ptr); }
};
/// Managed GLIB allocation
template <typename T>
using gAlloc = std::unique_ptr<T, gAllocDeleter>;

/// Managed GLIB string
struct gString : gAlloc<gchar>
{	constexpr gString() noexcept { }
	gString(gString&& r) noexcept : gAlloc<gchar>(std::move(r)) { }
	explicit gString(gchar* ptr) noexcept : gAlloc<gchar>(ptr) { }
	operator const gchar*() const noexcept { return get(); }
	gString& operator=(gchar* ptr) noexcept { reset(ptr); return *this; }
	using gAlloc<gchar>::operator=;
};

/// Managed GLIB object
template <typename T>
class gObject
{	T* Ptr;
public:
	constexpr T* release() noexcept { T* ptr = Ptr; Ptr = nullptr; return ptr; }
	constexpr gObject(nullptr_t = nullptr) noexcept : Ptr(nullptr) {}
	constexpr explicit gObject(T* ptr) noexcept : Ptr(ptr) {}
	gObject(const gObject<T>& r) : Ptr(r.Ptr) { g_object_ref(Ptr); }
	constexpr gObject(gObject<T>&& r) noexcept : Ptr(r.release()) {}
	~gObject() { if (Ptr) g_object_unref(Ptr); }
	constexpr explicit operator bool() const noexcept { return Ptr != nullptr; }
	constexpr T* get() const noexcept { return Ptr; }
	void reset() noexcept { this->~gObject(); Ptr = nullptr; }
	gObject<T>& operator=(const gObject<T>& r) { if (Ptr != r.Ptr) { this->~gObject(); g_object_ref(Ptr = r.Ptr); } return *this; }
	constexpr gObject<T>& operator=(gObject<T>&& r) noexcept { T* tmp = Ptr; Ptr = r.Ptr; r.Ptr = tmp; return *this; }
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

class gBytes
{	GBytes* Bytes;
	friend class std::hash<gBytes>;
public:
	constexpr gBytes() : Bytes(nullptr) {}
	gBytes(const void* data, std::size_t size) : Bytes(g_bytes_new(data, size)) {}
	gBytes(const gBytes& r) : Bytes(g_bytes_ref(r.Bytes)) {}
	gBytes(const gBytes& r, std::size_t offset, std::size_t length) : Bytes(g_bytes_new_from_bytes(r.get(), offset, length)) {}
	constexpr gBytes(gBytes&& r) noexcept : Bytes(r.Bytes) { r.Bytes = nullptr; }
	explicit gBytes(GBytes* bytes) : Bytes(bytes) {}
	~gBytes() { g_bytes_unref(Bytes); }
	void swap(gBytes& r) noexcept { std::swap(Bytes, r.Bytes); }
	gBytes& operator=(const gBytes& r) { gBytes(r).swap(*this); return *this; }
	gBytes& operator=(gBytes&& r) noexcept { swap(r); return *this; }
	void assign(const void* data, std::size_t size) { this->~gBytes(); Bytes = g_bytes_new(data, size); }
	const void* data() const { return Bytes ? g_bytes_get_data(Bytes, nullptr) : nullptr; }
	std::size_t size() const { return Bytes ? g_bytes_get_size(Bytes) : 0U; }
	constexpr GBytes* get() const noexcept { return Bytes; }
	friend bool operator==(const gBytes& l, const gBytes& r) { return (bool)g_bytes_equal(l.Bytes, r.Bytes); }
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
	gListP(gList<T>* p = nullptr) : ptr(p) {}
	explicit gListP(GList* p) : ptr((gList<T>*)p) {}
	explicit gListP(T t) : ptr((gList<T>*)g_list_append(nullptr, (gpointer)t)) {}
	gListP<T>& operator =(gList<T>* p) { ptr = p; return *this; }
	operator GList*() const { return (GList*)ptr; }
	gList<T>* operator ->() const { return ptr; }
	gListP<T> last() const { return gListP<T>(g_list_last(*this)); }
	gListP<T> prepend(T t) { return gListP<T>(g_list_prepend(*this, (gpointer)t)); }
	gListP<T> append(T t) { return gListP<T>(g_list_append(*this, (gpointer)t)); }
	template<typename U, typename std::enable_if<std::is_convertible<T, U>::value, bool>::type = true>
	gListP<T> insert_sorted(T t, gint (*cmp)(U a, U b))
	{ return gListP<T>(g_list_insert_sorted(*this, (gpointer)t, (GCompareFunc)cmp)); }
	gListP<T> concat(gListP<T> l) { return gListP<T>(g_list_concat(*this, l)); }
	gListP<T> reverse() { return gListP<T>(g_list_reverse(*this)); }
};

/// Execute function asynchronously in main loop.
/// @param func Pointer to the function to be executed.
/// The function takes the ownership of this object.
/// It will be deleted after invocation or removal of the event.
/// @param priority Optional priority level, G_PRIORITY_DEFAULT_IDLE by default.
/// @return ID of the registered event source.
guint gIdleAdd(std::function<void()>* func, gint priority = G_PRIORITY_DEFAULT_IDLE);


/// Binary search with exact match handling.
/// @tparam I iterator type
/// @tparam T value type, not necessarily the same as the iterator's value type.
/// @tparam C comparer to use, must return <=> 0 with the signature <tt>int C(I, T)</tt>.
template <typename I, typename T, typename C>
std::pair<I, bool> binary_find(I first, I last, const T& value, C comp)
{	while (first != last)
	{	I m = first + ((last - first) >> 1);
		int cmp = comp(*m, value);
		if (cmp == 0)
			return std::make_pair(m, true);
		if (cmp < 0)
			first = m+1;
		else
			last = m;
	}
	return std::make_pair(first, false);
}

/// simple GUID class
struct Guid
{	uint8_t Value[16];
	static const Guid empty;
	static Guid parse(const char* s);
	gchar* toString() const;
	friend bool operator==(const Guid& l, const Guid& r) { return memcmp(l.Value, r.Value, 16) == 0; }
};

/// Convert file duration into a human readable format.
std::string et_file_duration_to_string(double seconds);

/*
 * Combobox misc functions
 */

/* To number columns of ComboBox */
enum
{
    MISC_COMBO_TEXT, // = 0 (First column)
    MISC_COMBO_COUNT // = 1 (Number of columns in ComboBox)
};

gboolean Add_String_To_Combo_List(GtkListStore *liststore, const gchar *string);

gboolean et_variant_string_array_contains(GVariant* variant, const char* value);
GVariant* et_variant_string_array_toggle(GVariant* variant, const char* value);
GVariant* et_variant_string_array_set(GVariant* variant, const char* value, gboolean set);

inline bool operator==(const GtkTreeIter& l, const GtkTreeIter& r)
{	// like gtkmm
	return l.user_data == r.user_data
		&& l.user_data2 == r.user_data2
		&& l.user_data3 == r.user_data3;
}

gboolean et_rename_file (const gchar *old_filename, const gchar *new_filename, GError **error);

/*
 * et_str_empty:
 * @str: string to test for emptiness
 *
 * Test if @str is empty, in other words either %NULL or the empty string.
 *
 * Returns: %TRUE is @str is either %NULL or "", %FALSE otherwise
 */
inline gboolean et_str_empty (const gchar *str) { return !str || !str[0]; }

#endif /* ET_MISC_H_ */
