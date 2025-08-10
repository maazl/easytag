/* EasyTAG - tag editor for audio files
 * Copyright (C) 2024 Marcel Müller
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

#ifndef ET_FILE_XSTRING_H_
#define ET_FILE_XSTRING_H_

#include <cstdlib>
#include <atomic>
#include <array>
#include <cstdio>
#include <utility>
#include <limits>

#include <glib.h>

namespace
{	template <std::size_t N, std::size_t ... I>
	constexpr std::array<char, N> to_array_impl(const char (&a)[N], std::index_sequence<I...>) noexcept
	{	return {{a[I]...}}; }
}

/// Initialize <tt>std::array\<const char, N></tt> from string literal of length \c N.
template <std::size_t N>
constexpr std::array<char, N> to_array(const char (&a)[N]) noexcept
{	return to_array_impl(a, std::make_index_sequence<N>()); }


/// Reference counted, immutable string class
/// @remarks The stored string storage is shared on assignment or copying.\n
/// Instances of this class are <em>binary compatible to <tt>const char* const</tt></em>,
/// i.e. C strings. Of course, you must neither alter the value nor the pointer.
class xString
{
protected:
	/// Shared reference counted storage for xString
	struct header
	{	mutable std::atomic<gchar*> CollationKey;
		mutable std::atomic<unsigned> RefCount;

		void* operator new(std::size_t) = delete;
		void* operator new[](std::size_t) = delete;
		void operator delete(void* ptr) = delete;
		void operator delete[](void*) = delete;
		header(const header&) = delete;
		header& operator=(const header&) = delete;
		constexpr header() noexcept : CollationKey(nullptr), RefCount(1) {}
		~header() { g_free(CollationKey); }
		#ifndef NDEBUG
		void checkstillused() const;
		#endif
	};

	template <size_t L>
	struct data
	{	std::array<char, L+1> C;
		constexpr data() {}
		constexpr data(const char (&value)[L+1]) noexcept : C(to_array<L+1>(value)) {}
		constexpr data(const data<L>& r) noexcept : C(r.C) {}
	};

	template <size_t L>
	struct storage : public header, public data<L>
	{	void* operator new(std::size_t) = delete;
		void* operator new(std::size_t, std::size_t len) { return (char*)malloc(offsetof(storage<0>, C) + len + 1); }
		void operator delete(void* ptr) { free(ptr); }
		using data<L>::data;
		constexpr storage(const storage<L>& r) : header(), data<L>(r) {}
	};

	/// The storage of this string instance. <em>Do not access content directly.</em>
	/// @remarks Note that unless the pointer is \c nullptr it always points \em behind the storage instance,
	/// i.e. at the start of the character data. So the type is in fact <tt>const char*</tt>,
	/// but this would cause problems with some \c constexpr functions.\n
	/// Use RefCount to access the storage content and cast to <tt>const char*</tt> to get the C string.
	const data<0>* Ptr;

	/// Runtime typed factory for storage.
	/// @details Validated \a len and constructs and initializes \c storage&lt;len&gt;\.
	static storage<0>* Factory(const char* str, gssize len);

	/// Create \c xString from raw storage
	constexpr xString(const data<0>* ptr) noexcept : Ptr(ptr) {}

private:
	constexpr const header& Header() const noexcept { return static_cast<const storage<0>&>(*Ptr); }
	constexpr const data<0>* AddRef() const noexcept { if (Ptr) ++Header().RefCount; return Ptr; }
	static const data<0>* Init(const char* str, gssize len);
	static const data<0>* Init(const char* str, gssize len, GNormalizeMode mode);

public:
	/// Proxy class to allow initialization from C/C++ string.
	struct cstring
	{	const char* Str;
		gssize Len;
		constexpr cstring(const char* str) : Str(str), Len(-1) {}
		constexpr cstring(const char* str, gssize len) : Str(str), Len(len) {}
		cstring(const std::string& str) : Str(str.c_str()), Len(str.length()) {}
	};

	/// C string hash function
	struct hasher
	{	unsigned operator()(const char* s) const;
		unsigned operator()(const char* s, std::size_t len) const;
	};
	/// C string equality comparer
	struct equal
	{	bool operator()(const char* l, const char* r) const { return g_strcmp0(l, r) == 0; }
	};

	/// xString storage that avoids any dynamic allocation.
	/// @remarks This is intended for static xString constants to avoid allocating storage.
	/// The class should be used for static storage only.
	/// <b>You must not have existing \c xString instances that refer to the literal
	/// when this class goes out of scope.</b>\n
	/// Typically you want to use \c xStringL to create \c xString literals.
	template <std::size_t L>
	class literal : public storage<L>
	{
	public:
		constexpr literal(const char (&value)[L+1]) noexcept : storage<L>(value) {}
		constexpr literal(const literal<L>& r) noexcept : storage<L>(r) {}
		#ifndef NDEBUG
		~literal() { this->checkstillused(); }
		#endif
		literal<L>& operator=(const literal&) = delete;
	};
	/// Static value with empty string.
	/// @remarks This is automatically shared to deduplicate storage
	/// when the value is set to a string with length 0.
	static const literal<0> empty_str;

	/// Create \c xString with value \c nullptr.
	constexpr xString() noexcept : Ptr(nullptr) {}
	/// Create \c xString with value \c nullptr.
	constexpr xString(nullptr_t) : xString() {}
	/// Copy constructor.
	/// @remarks This constructor does not cause memory allocation.
	constexpr xString(const xString& r) noexcept : Ptr(r.AddRef()) {}
	/// Move constructor
	constexpr xString(xString&& r) noexcept : Ptr(r.Ptr) { r.Ptr = nullptr; }
	/// Initialization from \c xString literal.
	/// @remarks This constructor does not cause memory allocation.
	constexpr xString(const header& r) noexcept : Ptr(&static_cast<const storage<0>&>(r)) { ++Header().RefCount; }
	/// Initialization from raw C string storage.
	/// @param len Length of the string excluding the termination 0.
	/// @remarks Although this construction has a length property
	/// \c xString only supports null terminated strings
	/// and will always append a terminating 0.
	xString(const char* str, gssize len) : Ptr(Init(str, len)) {}
	/// Initialization from C/C++ string.
	xString(cstring str) : Ptr(Init(str.Str, str.Len)) {}
	/// Initialization from C string literal.
	/// @remarks Use this function with care if the same literal is assigned often.
	/// In this case using a static deduplicated \c xString literal should be preferred.
	template<std::size_t N>
	xString(const char (&str)[N]) : Ptr(Init(str, N-1)) {}
	/// Create normalized UTF-8 string.
	/// @remarks Normalization follows the rules of g_utf8_normalize.
	/// Invalid UTF-8 strings are converted to something valid with \ref Convert_Invalid_Utf8_String.
	xString(const char* str, gssize len, GNormalizeMode mode) : Ptr(Init(str, len, mode)) {}
	/// Destructor
	/// @remarks It will free the storage if this is the last instance that owns the storage.
	~xString() noexcept { if (Ptr && --Header().RefCount == 0) delete &static_cast<const storage<0>&>(*Ptr); }

	/// Check if string is not null
	constexpr explicit operator bool() const noexcept { return Ptr != nullptr; }
	/// Check if string is null or empty
	constexpr bool empty() const noexcept { return !Ptr || Ptr == &empty_str; }
	/// Compare for equality
	bool equals(const char* str) const noexcept;
	/// Compare for equality
	bool equals(const xString& r) const noexcept { return equals(r.get()); }
	/// Compare for equality
	bool equals(const char* str, std::size_t len) const noexcept;
	/// Compare for equality
	bool equals(const std::string& str) const noexcept { return equals(str.c_str(), str.length()); }
	/// Compare for equality
	friend bool operator==(const xString& l, const xString& r) noexcept { return l.equals(r); }
	/// Compare for equality
	friend bool operator==(const xString& l, const char* r) noexcept { return l.equals(r); }
	/// Compare for equality
	friend bool operator==(const char* l, const xString& r) noexcept { return r.equals(l); }
	/// Compare for equality
	friend bool operator==(const xString& l, const std::string& r) noexcept { return l.equals(r); }
	/// Compare for equality
	friend bool operator==(const std::string& l, const xString& r) noexcept { return r.equals(l); }
	/// Compare for inequality
	friend bool operator!=(const xString& l, const xString& r) noexcept { return !l.equals(r); }
	/// Compare for inequality
	friend bool operator!=(const xString& l, const char* r) noexcept { return !l.equals(r); }
	/// Compare for inequality
	friend bool operator!=(const char* l, const xString& r) noexcept { return !r.equals(l); }
	/// Compare for inequality
	friend bool operator!=(const xString& l, const std::string& r) noexcept { return !l.equals(r); }
	/// Compare for inequality
	friend bool operator!=(const std::string& l, const xString& r) noexcept { return !r.equals(l); }

	/// Current share count.
	/// @return Number of \c xString instance that share the same storage
	/// or 0 in case the current value is \c nullptr.
	/// @remarks In multi-threaded environment the returned value is volatile
	/// unless it is \<= 1, i.e. you are the only owner or it is \c nullptr.
	constexpr unsigned use_count() const noexcept { return Ptr ? Header().RefCount.load() : 0; }

	/// Implicit conversion to C string.
	constexpr operator const char*() const noexcept { return &Ptr->C.at(0); }
	/// Explicit conversion to C string.
	constexpr const char* get() const noexcept { return *this; }

	/// Swap value with another instance.
	void swap(xString& r) noexcept { std::swap(Ptr, r.Ptr); }
	/// Swap two instances.
	friend void swap(xString& l, xString& r) noexcept { l.swap(r); }
	/// Reset value to \c nullptr.
	void reset() noexcept { this->~xString(); Ptr = nullptr; }
	/// Assign a new, uninitialized storage of given size.
	/// @param len Length of the storage excluding the terminating 0.
	/// @return The raw storage.
	/// @remarks You must not write more than \p len characters or anything but 0 to position \p len.\n
	/// The returned storage keeps valid until the current instance is assigned.
	/// However, you should not alter the storage after you assigned this instance to other \c xString instances,
	/// especially not in multi-threaded environments.
	char* alloc(size_t len);
	/// Assign another value.
	xString& operator=(const xString& r) noexcept;
	/// Assign another rvalue.
	xString& operator=(xString&& r) noexcept { r.swap(*this); return *this; }
	/// Assign a new C/C++ string.
	xString& operator=(cstring str) { xString(str).swap(*this); return *this; }
	/// Assign a new C string literal.
	template<std::size_t N>
	xString& operator=(const char (&str)[N]) { xString(str).swap(*this); return *this; }
	/// Assign NFC normalized UTF-8 string.
	/// @remarks Short cut for <tt>=xString(..., G_NORMALIZE_NFC)</tt>
	void assignNFC(const char* str, gssize len = -1) { xString(str, len, G_NORMALIZE_NFC).swap(*this); }
	/// Assign NFC normalized UTF-8 string.
	/// @remarks Short cut for <tt>=xString(..., G_NORMALIZE_NFC)</tt>
	void assignNFC(const std::string& str) { this->~xString(); Ptr = Init(str.c_str(), str.length(), G_NORMALIZE_NFC); }

	/// Remove trailing and leading whitespace
	/// @return true if the operation caused a change.
	bool trim();
	/// Make this instance reference equal to r if its content is the same.
	/// @return Deduplication result\n
	/// -1 => the strings are not equal.\n
	/// 0 => no operation, the strings are already selfsame.\n
	/// 1 => \a r assigned to \c *this.
	int deduplicate(const xString& r) noexcept;

	/// Get collation key of the current string content.
	/// @return Collation key or \c nullptr if the current instance is null.
	/// @remarks The collation key is generated on the first call and cached.
	/// Furthermore is is a file name collation key with special handling for numbers.
	const gchar* collation_key() const;
	/// Relational comparison (spaceship operator)
	/// @details Compares the collation keys.
	/// Null strings are considered to be the smallest possible value.
	int compare(const xString& r) const;
};

/// Make \c xString literal from string literal.
/// @details Example: \code{.cpp}
/// const auto someConstant(xStringL("The value"));\endcode
/// @remarks User defined \c xString literals would require C++20.
template <std::size_t L>
constexpr const xString::literal<L-1> xStringL(const char (&value)[L])
{	return xString::literal<L-1>(value); }


/// Variant of \ref xString with implicit deduplication.
class xStringD : protected xString
{	static const unsigned DedupRefCount = (std::numeric_limits<unsigned>::max() >> 1) + 1; // MSB only

	static storage<0>* Factory(const char* str, gssize len);

	static const data<0>* Init(const char* str, gssize len);
	static const data<0>* Init(const char* str, gssize len, GNormalizeMode mode);
	static const data<0>* Init(const data<0>* ptr);
	static const data<0>* Init(const data<0>* ptr, GNormalizeMode mode);

public:
	static void garbage_collector();

	/// Create \c xString with value \c nullptr.
	constexpr xStringD() noexcept {}
	/// Create \c xString with value \c nullptr.
	constexpr xStringD(nullptr_t) noexcept : xStringD() {}
	/// Copy constructor.
	/// @remarks This constructor does not cause memory allocation.
	constexpr xStringD(const xStringD& r) noexcept : xString(r) {}
	/// Move constructor
	constexpr xStringD(xStringD&& r) noexcept : xString(r) {}
	/// Initialization from non-deduplicated \ref xString\.
	xStringD(const xString& r) : xString(Init((const data<0>*)r.get())) {}
	/// Initialization from \c xString literal.
	/// @remarks This constructor does not cause memory allocation.
	xStringD(const header& r) : xString(Init(&static_cast<const storage<0>&>(r))) {}
	/// Initialization from raw C string storage.
	/// @param len Length of the string excluding the termination 0.
	/// @remarks Although this construction has a length property
	/// \c xString only supports null terminated strings
	/// and will always append a terminating 0.
	xStringD(const char* str, gssize len) : xString(Init(str, len)) {}
	/// Initialization from C/C++ string.
	xStringD(cstring str) : xString(Init(str.Str, str.Len)) {}
	/// Initialization from C string literal.
	/// @remarks Use this function with care if the same literal is assigned often.
	/// In this case using a static deduplicated \c xString literal should be preferred.
	template<std::size_t N>
	xStringD(const char (&str)[N]) : xString(Init(str, N-1)) {}
	/// Create normalized UTF8 string.
	/// @remarks Normalization follows the rules of g_utf8_normalize.
	xStringD(const char* str, gssize len, GNormalizeMode mode) : xString(Init(str, len, mode)) {}
	/// Initialization from non-deduplicated \ref xString\.
	xStringD(const xString& r, GNormalizeMode mode) : xString(Init((const data<0>*)r.get(), mode)) {}

	using xString::operator bool;
	using xString::empty;

	using xString::equals;
	/// Compare for equality
	constexpr bool equals(const xStringD& r) const noexcept { return Ptr == r.Ptr; }
	/// Compare for equality
	friend constexpr bool operator==(const xStringD& l, const xStringD& r) noexcept { return l.equals(r); }
	/// Compare for inequality
	friend constexpr bool operator!=(const xStringD& l, const xStringD& r) noexcept { return !l.equals(r); }

	using xString::use_count;
	using xString::operator const char*;
	using xString::get;

	/// Swap value with another instance.
	void swap(xStringD& r) noexcept { std::swap(Ptr, r.Ptr); }
	/// Swap two instances.
	friend void swap(xStringD& l, xStringD& r) noexcept { l.swap(r); }
	using xString::reset;
	/// Assign another value.
	xStringD& operator=(const xStringD& r) noexcept { xString::operator=(r); return *this; }
	/// Assign another value.
	xStringD& operator=(const xString& r) noexcept { if (equals(r)) xStringD(r).swap(*this); return *this; }
	/// Assign another rvalue.
	xStringD& operator=(xStringD&& r) noexcept { r.swap(*this); return *this; }
	/// Assign a new C/C++ string.
	xStringD& operator=(cstring str) { xStringD(str).swap(*this); return *this; }
	/// Assign a new C string literal.
	template<std::size_t N>
	xStringD& operator=(const char (&str)[N]) { xStringD(str).swap(*this); return *this; }
	/// Initialization from \c xString literal.
	/// @remarks This constructor does not cause memory allocation.
	xStringD& operator=(const header& r) { this->~xStringD(); Ptr = Init(&static_cast<const storage<0>&>(r)); return *this; }
	/// Assign NFC normalized UTF-8 string.
	/// @remarks Short cut for <tt>=xString(..., G_NORMALIZE_NFC)</tt>
	void assignNFC(const xString& str) { xStringD(str, G_NORMALIZE_NFC).swap(*this); }
	/// Assign NFC normalized UTF-8 string.
	/// @remarks Short cut for <tt>=xString(..., G_NORMALIZE_NFC)</tt>
	void assignNFC(const char* str, gssize len = -1) { xStringD(str, len, G_NORMALIZE_NFC).swap(*this); }
	/// Assign NFC normalized UTF-8 string.
	/// @remarks Short cut for <tt>=xString(..., G_NORMALIZE_NFC)</tt>
	void assignNFC(const std::string& str) { this->~xString(); Ptr = Init(str.c_str(), str.length(), G_NORMALIZE_NFC); }
	/// Remove trailing and leading whitespace
	/// @return true if the operation caused a change.
	bool trim();
};


/// Variant of \ref xString that treats `nullptr` as an empty string
class xString0 : public xString
{public:
	using xString::xString;
	/// Copy
	constexpr xString0(const xString0& r) = default;
	/// Move
	constexpr xString0(xString0&& r) = default;

	using xString::operator=;
	/// Assignment
	xString0& operator=(const xString0& r) = default;
	/// Assignment
	xString0& operator=(xString0&& r) = default;

	/// Implicit conversion to C string.
	constexpr operator const char*() const noexcept { return Ptr ? xString::get() : empty_str.C.data(); }
	/// Explicit conversion to C string.
	constexpr const char* get() const noexcept { return *this; }

	/// Check if string is not empty
	constexpr explicit operator bool() const noexcept { return !empty(); }
	/// Compare for equality
	bool equals(const char* str) const noexcept;
	/// Compare for equality
	bool equals(const xString0& r) const noexcept { return equals(r.get()); }
	/// Compare for equality
	bool equals(const char* str, std::size_t len) const noexcept;
	/// Compare for equality
	bool equals(const std::string& str) const noexcept { return equals(str.c_str(), str.length()); }
	/// Compare for equality
	friend bool operator==(const xString0& l, const xString0& r) noexcept { return l.equals(r.get()); }
	/// Compare for equality
	friend bool operator==(const xString0& l, const char* r) noexcept { return l.equals(r); }
	/// Compare for equality
	friend bool operator==(const char* l, const xString0& r) noexcept { return r.equals(l); }
	/// Compare for equality
	friend bool operator==(const xString0& l, const std::string& r) noexcept { return l.equals(r); }
	/// Compare for equality
	friend bool operator==(const std::string& l, const xString0& r) noexcept { return r.equals(l); }
	/// Compare for inequality
	friend bool operator!=(const xString0& l, const xString0& r) noexcept { return !l.equals(r.get()); }
	/// Compare for inequality
	friend bool operator!=(const xString0& l, const char* r) noexcept { return !l.equals(r); }
	/// Compare for inequality
	friend bool operator!=(const char* l, const xString0& r) noexcept { return !r.equals(l); }
	/// Compare for inequality
	friend bool operator!=(const xString0& l, const std::string& r) noexcept { return !l.equals(r); }
	/// Compare for inequality
	friend bool operator!=(const std::string& l, const xString0& r) noexcept { return !r.equals(l); }

	/// Get collation key of the current string content.
	/// @return Collation key or \c nullptr if the current instance is null.
	/// @remarks The collation key is generated on the first call and cached.
	/// Furthermore is is a file name collation key with special handling for numbers.
	const gchar* collation_key() const { return empty() ? empty_str.C.data() : xString::collation_key(); }
	/// Relational comparison (spaceship operator)
	/// @details Compares the collation keys.
	int compare(const xString0& r) const;

	/// Remove trailing and leading whitespace
	/// @return true if the operation caused a change.
	bool trim() { return !empty() && xString::trim(); }
};


/// Variant of \ref xStringD that treats \c `nullptr` as an empty string
class xStringD0 : public xStringD
{public:
	using xStringD::xStringD;
	/// Copy
	constexpr xStringD0(const xStringD0& r) = default;
	/// Move
	constexpr xStringD0(xStringD0&& r) = default;

	using xStringD::operator=;
	/// Assignment
	xStringD0& operator=(const xStringD0& r) = default;
	/// Assignment
	xStringD0& operator=(xStringD0&& r) = default;

	/// Implicit conversion to C string.
	constexpr operator const char*() const noexcept { return Ptr ? xStringD::get() : empty_str.C.data(); }
	/// Explicit conversion to C string.
	constexpr const char* get() const noexcept { return *this; }

	/// Check if string is not empty
	constexpr explicit operator bool() const noexcept { return !empty(); }
	/// Compare for equality
	bool equals(const char* str) const noexcept;
	/// Compare for equality
	bool equals(const xStringD0& r) const noexcept;
	/// Compare for equality
	bool equals(const char* str, std::size_t len) const noexcept;
	/// Compare for equality
	bool equals(const std::string& str) const noexcept { return equals(str.c_str(), str.length()); }
	/// Compare for equality
	friend bool operator==(const xStringD0& l, const xStringD0& r) noexcept { return l.equals(r); }
	/// Compare for equality
	friend bool operator==(const xStringD0& l, const char* r) noexcept { return l.equals(r); }
	/// Compare for equality
	friend bool operator==(const char* l, const xStringD0& r) noexcept { return r.equals(l); }
	/// Compare for equality
	friend bool operator==(const xStringD0& l, std::string& r) noexcept { return l.equals(r); }
	/// Compare for equality
	friend bool operator==(std::string& l, const xStringD0& r) noexcept { return r.equals(l); }
	/// Compare for inequality
	friend bool operator!=(const xStringD0& l, const xStringD0& r) noexcept { return !l.equals(r); }
	/// Compare for inequality
	friend bool operator!=(const xStringD0& l, const char* r) noexcept { return !l.equals(r); }
	/// Compare for inequality
	friend bool operator!=(const char* l, const xStringD0& r) noexcept { return !r.equals(l); }
	/// Compare for inequality
	friend bool operator!=(const xStringD0& l, const std::string& r) noexcept { return !l.equals(r); }
	/// Compare for inequality
	friend bool operator!=(const std::string& l, const xStringD0& r) noexcept { return !r.equals(l); }

	/// Get collation key of the current string content.
	/// @return Collation key or \c nullptr if the current instance is null.
	/// @remarks The collation key is generated on the first call and cached.
	/// Furthermore is is a file name collation key with special handling for numbers.
	const gchar* collation_key() const { return empty() ? empty_str.C.data() : xStringD::collation_key(); }
	/// Relational comparison (spaceship operator)
	/// @details Compares the collation keys.
	int compare(const xStringD0& r) const;

	/// Remove trailing and leading whitespace
	/// @return true if the operation caused a change.
	bool trim() { return !empty() && xStringD::trim(); }
};

#endif
