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

#include "xstring.h"

#include "misc.h"

#include <string>
#include <cstring>
#include <algorithm>
using namespace std;

const xString::literal<0> xString::empty_str("");

#ifndef NDEBUG
[[noreturn]] void xString::header::freestillused() const
{	fprintf(stderr, "xString literal destroyed while in use. Use count %d, value '%s'\n",
		RefCount.load(), (const char*)(this + 1));
	std::abort();
}
#endif

const xString::data<0>* xString::Init(const char* str, size_t len)
{	if (!len)
	{	++empty_str.RefCount;
		return &empty_str;
	}
	storage<0>* ptr = new(len) storage<0>;
	((char*)memcpy(const_cast<char*>(ptr->C.data()), str, len))[len] = 0;
	return ptr;
}

const xString::data<0>* xString::Init(const char* str)
{	if (!str)
		return nullptr;
	if (!*str)
	{	++empty_str.RefCount;
		return &empty_str;
	}
	size_t len = strlen(str);
	return (data<0>*)memcpy(const_cast<char*>((new(len) storage<0>)->C.data()), str, len + 1);
}

const xString::data<0>* xString::Init(const char* str, size_t len, GNormalizeMode mode)
{	if (!len)
	{	++empty_str.RefCount;
		return &empty_str;
	}
	// GLib has no check for already normalized UTF8 strings.
	// So perform a _very rough check_ to avoid excessive allocations.
	const char* cp = str;
	const char* cpe = cp + len;
	do
		if (*cp & 0x80)
			return Init(gString(g_utf8_normalize(str, len, mode)).get());
	while (++cp != cpe);
	// fast path
	return Init(str, cp - str);
}

const xString::data<0>* xString::Init(const char* str, GNormalizeMode mode)
{	if (!str)
		return nullptr;
	if (!*str)
	{	++empty_str.RefCount;
		return &empty_str;
	}
	// GLib has no check for already normalized UTF8 strings.
	// So perform a _very rough check_ to avoid excessive allocations.
	const char* cp = str;
	do
		if (*cp & 0x80)
			return Init(gString(g_utf8_normalize(str, -1, mode)).get());
	while (*++cp);
	// fast path
	return Init(str, cp - str);
}

bool xString::equals(const char* str) const noexcept
{	if (get() == str)
		return true;
	if (!Ptr || !str)
		return false;
	return strcmp(get(), str) == 0;
}

bool xString::equals(const char* str, size_t len) const noexcept
{	if (!Ptr)
		return false;
	return strncmp(get(), str, len) == 0 && !get()[len];
}

char* xString::alloc(size_t len)
{	this->~xString();
	Ptr = new(len) storage<0>;
	char* ptr = const_cast<char*>(Ptr->C.data());
	ptr[len] = 0; // ensure terminating \0
	return ptr;
}

xString& xString::operator=(const xString& r) noexcept
{	if (Ptr != r.Ptr)
	{	this->~xString();
		Ptr = r.AddRef();
	}
	return *this;
}

bool xString::trim()
{	if (!Ptr)
		return false;
	const char* start = *this;
	if (!*start)
		return false;
	while (isspace(*start))
		++start;
	size_t len = strlen(start);
	if (!len)
	{	*this = empty_str;
		return true;
	}
	const char* end = start + len;
	while (isspace(end[-1]))
		--end;
	if (start == *this && end == start + len)
		return false;
	xString(start, end - start).swap(*this);
	return true;
}

int xString::deduplicate(const xString& r) noexcept
{	if (Ptr == r.Ptr)
		return 0;
	if (!Ptr || !r.Ptr || strcmp(*this, r))
		return -1;

	gchar* ck = Header().CollationKey;
	if (ck && Header().RefCount == 1)
		// transfer collation key
		Header().CollationKey = r.Header().CollationKey.exchange(ck);

	this->~xString();
	Ptr = r.AddRef();
	return 1;
}

const gchar* xString::collation_key() const
{	if (!Ptr)
		return nullptr;
	gchar* ck = Header().CollationKey;
	if (ck)
		return ck;
	// Replace dir separator by dot for more reasonable sort order
	size_t len = strlen(*this) + 1;
	gchar tmp[len];
	memcpy(tmp, get(), len);
	replace(tmp, tmp + len - 1, G_DIR_SEPARATOR, '.');
	gchar* ck2 = g_utf8_collate_key_for_filename(tmp, -1);
	// atomically apply the new key
	if (G_LIKELY(Header().CollationKey.compare_exchange_strong(ck, ck2)))
		return ck2;
	// atomic operation failed => discard key
	g_free(ck2);
	return ck;
}

int xString::compare(const xString& r) const
{	if (Ptr == r.Ptr)
		return 0;
	if (!Ptr)
		return -1;
	if (!r.Ptr)
		return 1;
	return strcmp(collation_key(), r.collation_key());
}

bool xString0::equals(const char* str) const noexcept
{	if (xString::get() == str)
		return true;
	return et_str_empty(str) ? empty() : !empty() && strcmp(xString::get(), str) == 0;
}

bool xString0::equals(const char* str, size_t len) const noexcept
{	return empty() ? !len : len && strncmp(xString::get(), str, len) == 0 && !xString::get()[len];
}

int xString0::compare(const xString0& r) const
{	const char* lc = get();
	const char* rc = r.get();
	if (lc == rc)
		return 0;
	if (!*lc)
		return -1;
	if (!*rc)
		return 1;
	return strcmp(collation_key(), r.collation_key());
}
