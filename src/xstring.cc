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
#include "charset.h"

#include <string>
#include <cstring>
#include <algorithm>
#include <unordered_set>
#include <mutex>
using namespace std;

unsigned xString::hasher::operator()(const char* s) const
{	if (!s)
		return 0;
	unsigned hash = 2166136261U;
	for (; *s; ++s)
	{	hash ^= *s;
		hash *= 16777619U;
	}
	return hash;
}

const xString::literal<0> xString::empty_str("");

#ifndef NDEBUG
void xString::header::checkstillused() const
{	if ((RefCount & (std::numeric_limits<unsigned>::max() >> 1)) == 1) // ignore xStringD repository
		return;
	fprintf(stderr, "xString literal destroyed while in use. Use count %d, value '%s'\n",
		RefCount.load(), static_cast<const storage<0>*>(this)->C.data());
	std::abort();
}
#endif

xString::storage<0>* xString::Factory(const char* str, gssize len)
{	len = len < 0 ? strlen(str) : strnlen(str, len); // real string length
	storage<0>* ptr = new(len) storage<0>;
	((char*)memcpy(const_cast<char*>(ptr->C.data()), str, len))[len] = 0;
	return ptr;
}

const xString::data<0>* xString::Init(const char* str, gssize len)
{	if (!str)
		return nullptr;
	if (!len || !*str)
	{	++empty_str.RefCount;
		return &empty_str;
	}
	return Factory(str, len);
}

const xString::data<0>* xString::Init(const char* str, gssize len, GNormalizeMode mode)
{	if (!str)
		return nullptr;
	if (!len || !*str)
	{	++empty_str.RefCount;
		return &empty_str;
	}
	// GLib has no check for already normalized UTF8 strings.
	// So perform a _very rough check_ to avoid excessive allocations.
	const char* cp = str;
	const char* cpe = cp + len;
	do
		if (*cp & 0x80)
		{	gString s(g_utf8_normalize(str, len, mode));
			if (!s)
				s = g_utf8_normalize(gString(Convert_Invalid_Utf8_String(str, len)), -1, mode);
			return Factory(s, -1);
		}
	while (++cp != cpe && *cp);
	// fast path
	storage<0>* ptr = new(len) storage<0>;
	((char*)memcpy(const_cast<char*>(ptr->C.data()), str, len))[len] = 0;
	return ptr;
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
	if (!len)
	{	++empty_str.RefCount;
		return const_cast<char*>((Ptr = &empty_str)->C.data());
	}
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


// Repository with deduplicated xStringD::storage instances.
// To avoid a C++20 dependency for find operations instead of storage a pointer to the character data is stored.
// A downcast to the storage instance is always valid. Each intance in this collection is one reference count.
static unordered_set<const char*, xString::hasher, xString::equal> Instances;

static mutex InstancesMutex;

xString::storage<0>* xStringD::Factory(const char* str, gssize len)
{	storage<0>* ptr;
	unique_lock<mutex> lock(InstancesMutex, defer_lock_t());
	unordered_set<const char*, hasher, equal>::iterator p;
	if (len > 0 && str[len])
	{	// If str is not null terminated Instances.find(str) cannot work (would require C++20).
		// => Create null terminated string first.
		ptr = xString::Factory(str, len);
		lock.lock();
		p = Instances.find(&ptr->C[0]);
		if (p != Instances.end())
		{	delete ptr;
		match:
			ptr = (storage<0>*)(data<0>*)*p;
			++ptr->RefCount;
			return ptr;
		}
	}
	else
	{	lock.lock();
		p = Instances.find(str);
		if (p != Instances.end())
			goto match;
		ptr = xString::Factory(str, len);
	}

	// add previously unknown string
	ptr->RefCount += DedupRefCount;
		Instances.insert(p, &ptr->C[0]);
	return ptr;
}

const xStringD::data<0>* xStringD::Init(const char* str, gssize len)
{	if (!str)
		return nullptr;
	if (!len || !*str)
	{	++empty_str.RefCount;
		return &empty_str;
	}
	return Factory(str, len);
}

const xStringD::data<0>* xStringD::Init(const char* str, gssize len, GNormalizeMode mode)
{	if (!str)
		return nullptr;
	if (!len || !*str)
	{	++empty_str.RefCount;
		return &empty_str;
	}
	// GLib has no check for already normalized UTF8 strings.
	// So perform a _very rough check_ to avoid excessive allocations.
	const char* cp = str;
	const char* cpe = cp + len;
	do
		if (*cp & 0x80)
		{	gString s(g_utf8_normalize(str, len, mode));
			if (!s)
				s = g_utf8_normalize(gString(Convert_Invalid_Utf8_String(str, len)), -1, mode);
			return Factory(s, -1);
		}
	while (++cp != cpe && *cp);
	// fast path
	return Factory(str, cp - str);
}

const xStringD::data<0>* xStringD::Init(const data<0>* ptr)
{	if (!ptr)
		return ptr;

	if (ptr->C[0] && (static_cast<const storage<0>&>(*ptr).RefCount & DedupRefCount) == 0) // empty or already deduplicated?
	{	lock_guard<mutex> lock(InstancesMutex);
		auto p = Instances.insert(&ptr->C[0]);
		if (p.second)
		{	((const storage<0>*)ptr)->RefCount += DedupRefCount + 1;
			return ptr;
		}
		ptr = (data<0>*)*(p.first);
	}
	++((const storage<0>*)ptr)->RefCount;
	return ptr;
}

const xStringD::data<0>* xStringD::Init(const data<0>* ptr, GNormalizeMode mode)
{	if (!ptr)
		return ptr;
	// GLib has no check for already normalized UTF8 strings.
	// So perform a _very rough check_ to avoid excessive allocations.
	const char* cp = ptr->C.data();
	while(*cp)
		if (*cp++ & 0x80)
		{	gString s(g_utf8_normalize(ptr->C.data(), -1, mode));
			if (!s)
				s = g_utf8_normalize(gString(Convert_Invalid_Utf8_String(ptr->C.data(), -1)), -1, mode);
			return Factory(s, -1);
		}
	// fast path
	return Init(ptr);
}

bool xStringD::trim()
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
	xStringD(start, end - start).swap(*this);
	return true;
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

bool xStringD0::equals(const char* str) const noexcept
{	if (xStringD::get() == str)
		return true;
	return et_str_empty(str) ? empty() : !empty() && strcmp(xStringD::get(), str) == 0;
}

bool xStringD0::equals(const xStringD0& r) const noexcept
{	const data<0>* lp = Ptr;
	if (!lp)
		lp = &empty_str;
	const data<0>* rp = r.Ptr;
	if (!rp)
		rp = &empty_str;
	return lp == rp;
}

bool xStringD0::equals(const char* str, size_t len) const noexcept
{	return empty() ? !len : len && strncmp(xStringD::get(), str, len) == 0 && !xString::get()[len];
}

int xStringD0::compare(const xStringD0& r) const
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
