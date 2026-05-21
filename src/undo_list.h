/* EasyTAG - tag editor for audio files
 * Copyright (C) 2024-2026  Marcel Müller <github@maazl.de>
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

#ifndef ET_UNDO_LIST_H_
#define ET_UNDO_LIST_H_

#include <functional>

/// Type erasure part of UndoList
class UndoListBase
{
public:
	class Intrusive
	{	Intrusive* Next;
		Intrusive* Prev;
		unsigned UndoKey;
		friend class UndoListBase;
	protected:
		constexpr Intrusive() noexcept : Next(nullptr), Prev(nullptr), UndoKey(0) {}
		constexpr Intrusive(const Intrusive&) noexcept : Intrusive() {}
		constexpr Intrusive(Intrusive&& r) noexcept : Next(r.Next), Prev(r.Prev), UndoKey(r.UndoKey)
		{	r.Prev = nullptr; r.Next = nullptr; }
	};

protected:
	Intrusive* Cur;
	Intrusive* New;

	constexpr UndoListBase() noexcept : Cur(nullptr), New(nullptr) {}
	UndoListBase(const UndoListBase&) = delete;
	void operator=(const UndoListBase&) = delete;

	void add(Intrusive* item, unsigned undo_key, void (*del)(Intrusive*));
	void foreach(std::function<void(Intrusive*)> func) const;

public:
	bool mark_saved() noexcept;
	bool is_saved() const noexcept { return Cur == New; }

	unsigned undo_key() const noexcept { return New && New->Prev ? New->UndoKey : 0; }
	unsigned redo_key() const noexcept { return New && New->Next ? New->Next->UndoKey : 0; }

	bool undo() noexcept { if (!New || !New->Prev) return false; New = New->Prev; return true; }
	bool redo() noexcept { if (!New || !New->Next) return false; New = New->Next; return true; }
};

/// List of versions of a item
/// @remarks This specialization adds type safety
/// and removes the need for the destructor of Intrusive to be virtual.
template <typename T>
class UndoList : public UndoListBase
{	typedef UndoListBase base;

	static void delete_item(Intrusive* i) { delete (T*)i; }
public:
	T* Cur() const noexcept { return (T*)base::Cur; }
	T* New() const noexcept { return (T*)base::New; }

public:
	void foreach(std::function<void(T*)> func) const { base::foreach([func](Intrusive* i) { func((T*)i); }); }
	~UndoList() {	base::foreach(&delete_item); }

	void add(T* item, unsigned undo_key) { base::add(item, undo_key, &delete_item); }
};

#endif
