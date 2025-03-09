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

#ifndef ET_XPTR_H
#define ET_XPTR_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>


template <typename T> class xPtr;
template <typename T> class std::atomic<xPtr<T>>;

/// Inherit from this type to enable an object to be managed by xPtr.
class alignas(8) xObj
{	template <typename T> friend class xPtr;
	friend class atomic_xPtr_base;

	mutable std::atomic<unsigned> RefCount;

	template <typename T>
	static T* AddRef(T* p) { if (p) ++p->xObj::RefCount; return p; }

	template <typename T>
	struct Deleter : public std::default_delete<T>
	{	void operator()(const T* ptr)
		{	if (--ptr->RefCount == 0)
				std::default_delete<T>()(const_cast<T*>(ptr));
		}
	};

protected:
	xObj() : RefCount(0) {}
};

/// Intrusive reference counted smart pointer for objects derived from xObj.
/// @remarks xPtr is similar to std::shared_ptr with std::make_shared
/// but with a smaller memory overhead and no weak_ptr support.
/// Additionally it provides safe conversion from raw pointers.
template <typename T>
class xPtr : private std::unique_ptr<T, xObj::Deleter<typename std::remove_const<T>::type>>
{	static_assert(std::is_base_of<xObj, T>::value, "Type must derive from xObj to be used with xPtr.");
	using base = std::unique_ptr<T, xObj::Deleter<typename std::remove_const<T>::type>>;

public:
	using typename base::pointer;
	using typename base::element_type;
	using typename base::deleter_type;

private:
	xPtr(base&& b) : base(std::move(b)) {}
public:
	constexpr xPtr(std::nullptr_t = nullptr) noexcept {}
	xPtr(T* p) noexcept : base(xObj::AddRef(p)) {}
	xPtr(xPtr<T>&& u) noexcept : base(std::move(u)) {}
	template <typename U, typename = std::enable_if<std::is_convertible<U*, T*>::value>>
	xPtr(xPtr<U>&& u) noexcept : xPtr(toCptr(u)) {}
	xPtr(const xPtr<T>& u) : base(xObj::AddRef(u.get())) {}
	template <typename U, typename = std::enable_if<std::is_convertible<U*, T*>::value>>
	xPtr(const xPtr<U>& u) : base(xObj::AddRef<T>(u.get())) {}

	void reset(T* p = nullptr) noexcept { base::reset(xObj::AddRef(p)); }
	void swap(xPtr<T>& r) noexcept { base::swap(r); }

	xPtr<T>& operator=(xPtr<T>&& r) noexcept { base::operator=(std::move((base&&)r)); return *this; }
	xPtr<T>& operator=(const xPtr<T>& r) noexcept { reset(r.get()); return *this; }

	using base::get;
	using base::get_deleter;
	using base::operator bool;
	using base::operator*;
	using base::operator->;

	operator const T*() const noexcept { return get(); }

	template <typename U>
	friend bool operator==(const xPtr<T>& l, const xPtr<U>& r) noexcept { return (const base&)l == (const typename xPtr<U>::base&)r; }
	friend bool operator==(const xPtr<T>& u, std::nullptr_t) noexcept { return (const base&)u == nullptr; }
	friend bool operator==(std::nullptr_t, const xPtr<T>& u) noexcept { return (const base&)u == nullptr; }
	template <typename U>
	friend bool operator!=(const xPtr<T>& l, const xPtr<U>& r) noexcept { return (const base&)l != (const typename xPtr<U>::base&)r; }
	friend bool operator!=(const xPtr<T>& u, std::nullptr_t) noexcept { return (const base&)u != nullptr; }
	friend bool operator!=(std::nullptr_t, const xPtr<T>& u) noexcept { return (const base&)u != nullptr; }

	/// Extract the currently held instance as C pointer.
	/// @details This does not release the ownership.
	/// The pointer must be passed to \ref fromCptr later exactly once.
	static T* toCptr(xPtr<T> u) { return u.release(); }
	/// Assign C pointer previously received from \ref toCptr.
	static xPtr<T> fromCptr(T* p) { return xPtr(base(p)); }
};

constexpr unsigned numberOfBits(std::size_t n)
{	return n <= 1 ? n : 1 + numberOfBits(n >> 1);
}

/// Type erasure part of atomic<xPtr<T>>
class atomic_xPtr_base
{
	/// Pointer to xObj and outer count
	mutable std::atomic<uintptr_t> Value;
	/// Mask to extract outer count from \ref Ptr.
	static constexpr unsigned Outer(uintptr_t val) { return (unsigned)val & (alignof(xObj) - 1); }
	/// Mask to extract pointer from \ref Ptr.
	static xObj* Pointer(uintptr_t val) { return reinterpret_cast<xObj*>(val & ~(alignof(xObj) - 1)); }
	/// Adjust inner count.
	static constexpr unsigned Inner(unsigned outer) { return outer << (std::numeric_limits<unsigned>::digits - numberOfBits(alignof(xObj))); }

	atomic_xPtr_base(const atomic_xPtr_base&) = delete;
	void operator=(const atomic_xPtr_base&) = delete;
protected:
	constexpr atomic_xPtr_base() noexcept : Value(0) {}
	explicit atomic_xPtr_base(xObj* ptr) : Value(reinterpret_cast<uintptr_t>(ptr)) {}
	xObj* Acquire() const noexcept;
	xObj* LoadRelaxed() const noexcept;
	xObj* Release() noexcept;
	xObj* Swap(xObj* r) noexcept;
	xObj* SwapRelaxed(xObj* r) noexcept;
	/// Compare exchange core
	/// @param oldval Value to compare with
	/// @param newval Value at assign if comparison succeeds
	/// @return Previous value, distinct from \a oldval if comparison failed,
	/// and flag whether to delete \a oldval.
	uintptr_t CXCStrong(xObj* oldval, xObj* newval) noexcept;
};

namespace std {
/// Strongly thread safe version of xPtr.
/// @details Thread safe instances cannot be dereferenced directly.
/// The only actions are assignment or acquiring thread local xPtr instances.
/// @remarks the implementation utilizes double reference counting and
/// 3 stolen bits to implement the feature w/o a memory overhead.
/// To prevent the stolen bits from overflowing the second counter is transferred ASAP
/// to the main reference counter. This is theoretically safe as long as no
/// more than 8 threads acquire a strong reference to the same pointer in parallel.
/// The restricted number of worker threads of EasyTAG is sufficient to ensure this.
template <typename T>
class atomic<xPtr<T>> : atomic_xPtr_base
{	using self = atomic<xPtr<T>>;
	static xPtr<T> toxPtr(xObj* ptr) noexcept { return xPtr<T>::fromCptr(static_cast<T*>(ptr)); }
	static bool compare_exchange(xPtr<T>& oldval, uintptr_t action)
	{	T* cur = static_cast<T*>(Pointer(action));
		if (cur == oldval.get())
			return true;
		// replace value in oldval and optionally delete old instance
		T* ptr = xPtr<T>::toCptr(oldval);
		if (action & 1)
			std::default_delete<T>()(ptr);
		oldval = xPtr<T>::fromCptr(cur);
		return false;
	}

public:
	using value_type = T;

	constexpr atomic(nullptr_t = nullptr) noexcept {}
	atomic(xPtr<T> r) noexcept : atomic_xPtr_base(xPtr<T>::toCptr(r)) {}
	~atomic() noexcept { toxPtr(LoadRelaxed()); }

	operator xPtr<T>() const noexcept { return toxPtr(Acquire()); }
	xPtr<T> load(std::memory_order order = std::memory_order_acq_rel) const noexcept
	{	if (order == std::memory_order_relaxed)
			return xPtr<T>(static_cast<T*>(LoadRelaxed()));
		else
			return *this;
	}
	xPtr<T> exchange(xPtr<T> r, std::memory_order order = std::memory_order_acq_rel) noexcept
	{	return toxPtr((this->*(order == std::memory_order_relaxed ? &atomic_xPtr_base::SwapRelaxed : &atomic_xPtr_base::Swap))(xPtr<T>::toCptr(r)));
	}
	void store(xPtr<T> r, std::memory_order order = std::memory_order_acq_rel) noexcept
	{	exchange(r, order);
	}
	void operator=(nullptr_t) noexcept { delete Release(); }
	void operator=(xPtr<T> r) noexcept { toxPtr(Swap(xPtr<T>::toCptr(r))); }
	bool compare_exchange_strong(xPtr<T>& expected, xPtr<T> desired) noexcept
	{	return compare_exchange(expected, CXCStrong(expected.get(), desired.get()));
	}
};

}

#endif
