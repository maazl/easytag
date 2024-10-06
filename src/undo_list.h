/* EasyTAG - tag editor for audio files
 * Copyright (C) 2024  Marcel Müller <github@maazl.de>
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

/// List of versions of a item
template <typename T>
class UndoList
{
public:
	class Intrusive
	{	T* Next;
		T* Prev;
		unsigned UndoKey;
		friend struct UndoList;
		Intrusive(const Intrusive&) = delete;
		void operator=(const Intrusive&) = delete;
	protected:
		Intrusive() : Next(nullptr), Prev(nullptr), UndoKey(0) {}
	};

	T* Cur;
	T* New;

private:
	static void delete_all(T* start, T* end);

public:
	UndoList() : Cur(nullptr), New(nullptr) {}
	~UndoList()
	{	if (Cur != New)
			delete_all(Cur, New);
		delete_all(New, Cur);
	}

	void add(T* item, unsigned undo_key);
	bool is_saved() const { return Cur == New; }
	bool mark_saved();

	unsigned undo_key() const { return New && New->Prev ? New->UndoKey : 0; }
	unsigned redo_key() const { return New && New->Next ? New->Next->UndoKey : 0; }

	bool undo() { if (!New || !New->Prev) return false; New = New->Prev; return true; }
	bool redo() { if (!New || !New->Next) return false; New = New->Next; return true; }
};

template <typename T>
void UndoList<T>::delete_all(T* start, T* end)
{	if (!start)
		return;
	T* i = start->Next;
	while (i && i != end)
	{	T* n = i->Next;
		delete i;
		i = n;
	}
	i = start->Prev;
	while (i && i != end)
	{	T* n = i->Prev;
		delete i;
		i = n;
	}
	delete start;
}

template <typename T>
void UndoList<T>::add(T* item, unsigned undo_key)
{	// Initial call?
	if (New)
	{	item->UndoKey = undo_key;
		/* How it works : Cut the list after the current item, then append the new item to the list.
		 * The cut items are discarded unless they are referenced by the current (saved) state. */
		T* cut_list = New->Next;
		while (cut_list)
		{	T* n = cut_list->Next;
			if (cut_list != Cur)
				delete cut_list;
			else
				cut_list->Prev = cut_list->Next = nullptr;
			cut_list = n;
		}

		item->Prev = New;
		New->Next = item;
	}
	New = item;
}

template <typename T>
bool UndoList<T>::mark_saved()
{	if (Cur == New)
		return false;
	if (Cur && !Cur->Prev && !Cur->Next)
		delete Cur; // delete orphaned current item left by add when cutting list.
	Cur = New;
	return true;
}

#endif
