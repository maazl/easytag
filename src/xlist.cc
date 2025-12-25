/* EasyTAG - tag editor for audio files
 * Copyright (C) 2025  Marcel Müller <github@maazl.de>
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

#include "xlist.h"


bool xListObjBase::detach() noexcept
{	if (Prev == Next)
	{	// detached or the only node in the list
		if (!Next)
			return false;
		auto list = (xListObjBase*)Next; // no need to reset LSB in this case
		list->Prev = list->Next = 0;
	} else
	{	next().Prev = Prev;
		prev().Next = Next;
	}
	Prev = Next = 0;
	return true;
}

void xListBase::push_front(xListObjBase& item) noexcept
{	g_assert(!item.Next); // Node must not be attached.
	item.Prev = (link_type)static_cast<xListObjBase*>(this);
	item.Next = Next;
	next(item);
	if (Prev)
		item.next().Prev = Next;
	else
	{	Prev = Next;
		item.Next = item.Prev;
	}
}

void xListBase::push_back(xListObjBase& item) noexcept
{	g_assert(!item.Next); // Node must not be attached.
	item.Next = (link_type)static_cast<xListObjBase*>(this);
	item.Prev = Prev;
	prev(item);
	if (Next)
		item.prev().Next = Prev;
	else
	{	Next = Prev;
		item.Prev = item.Next;
	}
}

