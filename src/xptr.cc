/* EasyTAG - tag editor for audio files
 * Copyright (C) 2025 Marcel Müller
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

#include "xptr.h"
#include <cassert>

using namespace std;

xObj* atomic_xPtr_base::Acquire() const noexcept
{	uintptr_t oldvalue = Value.fetch_add(1U, memory_order_acq_rel) + 1U;
	const unsigned outer = Outer(oldvalue);
	assert(outer != 0U); // overflow condition
	xObj* const newptr = Pointer(oldvalue);
	if (newptr)
		// Transfer counter to obj->RefCount.
		newptr->RefCount.fetch_add(1U - Inner(outer - 1U), memory_order_release);
	// And reset it in *this.
	if (!Value.compare_exchange_strong(oldvalue, reinterpret_cast<uintptr_t>(newptr), memory_order_release) && newptr)
		// Someone else does the job already => undo.
		newptr->RefCount.fetch_add(Inner(outer), std::memory_order_release);
		// The global count cannot return to zero here, because we have an active reference.
	return newptr;
}

xObj* atomic_xPtr_base::LoadRelaxed() const noexcept
{	uintptr_t val = Value.load(memory_order_relaxed);
	assert(Outer(val) == 0U);
	return reinterpret_cast<xObj*>(val);
}

xObj* atomic_xPtr_base::Release() noexcept
{	uintptr_t var = Value.exchange(0U, memory_order_acq_rel);
	xObj* obj = Pointer(var);
	return obj && (obj->RefCount -= Inner(Outer(var)) + 1U) == 0U ? obj : nullptr;
}

xObj* atomic_xPtr_base::Swap(xObj* r) noexcept
{	uintptr_t val = Value.exchange(reinterpret_cast<uintptr_t>(r), memory_order_acq_rel);
	xObj* obj = Pointer(val);
	// Transfer hold count to the main counter and get the data with hold count 0.
	unsigned outer = Outer(val);
	if (outer && obj)
		obj->RefCount.fetch_add(-Inner(outer), std::memory_order_release);
	return obj;
}

xObj* atomic_xPtr_base::SwapRelaxed(xObj* r) noexcept
{	uintptr_t val = Value.exchange(reinterpret_cast<uintptr_t>(r), memory_order_acq_rel);
	assert(Outer(val) == 0U);
	return reinterpret_cast<xObj*>(val);
}

uintptr_t atomic_xPtr_base::CXCStrong(xObj* oldval, xObj* newval) noexcept
{	uintptr_t preval = reinterpret_cast<uintptr_t>(oldval);
	while (!Value.compare_exchange_weak(preval, reinterpret_cast<uintptr_t>(newval), std::memory_order_acq_rel))
		if (Pointer(preval) != oldval)
		{	// pointer part not equal
			xObj* curval = Acquire();
			if (curval != oldval)
			{	// release oldval
				preval = reinterpret_cast<uintptr_t>(curval);
				if (oldval && --oldval->RefCount == 0)
					++preval; // delete marker
				return preval;
			}
			// retrieved pointer happens to match oldval now => undo acquire and retry
			if (curval)
			{	unsigned count = curval->RefCount.fetch_add(-1, memory_order_release);
				// since there is at least one additional strong reference in oldval the release operation must not cause count 0.
				assert(count != 1);
			}
		}
	// success => update reference counts.
	xObj::AddRef(newval);
	// release oldval and transfer the outer count if any
	unsigned count = Outer(preval);
	preval -= count;
	if (oldval)
	{ count = oldval->RefCount.fetch_add(count - 1, memory_order_release);
		// since there is at least one additional strong reference in oldval the release operation must not cause count 0.
		assert(count != 1 - count);
	}
	return preval;
}
