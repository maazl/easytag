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
		return nullptr;
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
{	if (et_str_empty(str))
		return nullptr;
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

bool operator==(const xString& l, const xString& r) noexcept
{	if (l.Ptr == r.Ptr)
		return true;
	if (!l.Ptr || !l.Ptr)
		return false;
	return strcmp(l, r) == 0;
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

bool xString::cmpassign(const xString& r)
{	if (*this == r)
		return false;
	this->~xString();
	Ptr = r.AddRef();
	return true;
}

bool xString::cmpassign(cstring str)
{	if (get() == str.Str)
		return false;
	if (Ptr && str.Str && strcmp(*this, str.Str) == 0)
		return false;
	xString(str).swap(*this);
	return true;
}

bool xString::cmpassign(cppstring str)
{	if (Ptr && strcmp(*this, str.Str.c_str()) == 0)
		return false;
	xString(str).swap(*this);
	return true;
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
	this->~xString();
	Ptr = r.AddRef();
	return 1;
}

const gchar* xString::collation_key() const
{	if (!Ptr)
		return nullptr;
	gchar* ck = static_cast<const storage<0>&>(*Ptr).CollationKey;
	if (ck)
		return ck;
	// Replace dir separator by dot for more reasonable sort order
	size_t len = strlen(*this) + 1;
	gchar tmp[len];
	memcpy(tmp, get(), len);
	replace(tmp, tmp + len - 1, G_DIR_SEPARATOR, '.');
	gchar* ck2 = g_utf8_collate_key_for_filename(tmp, -1);
	// atomically apply the new key
	if (G_LIKELY(static_cast<const storage<0>&>(*Ptr).CollationKey.compare_exchange_strong(ck, ck2)))
		return ck2;
	// atomic operation failed => discard key
	g_free(ck2);
	return ck;
}

int compare(const xString& l, const xString& r)
{	if (l.Ptr == r.Ptr)
		return 0;
	if (!l.Ptr)
		return -1;
	if (!r.Ptr)
		return 1;
	return strcmp(l.collation_key(), r.collation_key());
}
