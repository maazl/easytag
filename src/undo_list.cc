/* EasyTAG - tag editor for audio files
 * Copyright (C) 2026  Marcel Müller <github@maazl.de>
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

#include "undo_list.h"

void UndoListBase::add(Intrusive* item, unsigned undo_key, void (*del)(Intrusive*))
{	// Initial call?
	if (New)
	{	item->UndoKey = undo_key;
		/* How it works : Cut the list after the current item, then append the new item to the list.
		 * The cut items are discarded unless they are referenced by the current (saved) state. */
		Intrusive* cut_list = New->Next;
		while (cut_list)
		{	Intrusive* n = cut_list->Next;
			if (cut_list != Cur)
				del(cut_list);
			else
				cut_list->Prev = cut_list->Next = nullptr;
			cut_list = n;
		}

		item->Prev = New;
		New->Next = item;
	}
	New = item;
}

void UndoListBase::foreach(std::function<void(Intrusive*)> func) const
{
	if (Cur && !Cur->Prev && !Cur->Next)
	{	func(Cur); // handle orphaned current item left by add when cutting list.
		if (Cur == New)
			return;
	} else if (!New)
		return;
	Intrusive* i = New->Next;
	while (i)
	{	Intrusive* n = i->Next;
		func(i);
		i = n;
	}
	i = New->Prev;
	while (i)
	{	Intrusive* n = i->Prev;
		func(i);
		i = n;
	}
	func(New);
}

bool UndoListBase::mark_saved() noexcept
{	if (Cur == New)
		return false;
	if (Cur && !Cur->Prev && !Cur->Next)
		delete Cur; // delete orphaned current item left by add when cutting list.
	Cur = New;
	return true;
}
