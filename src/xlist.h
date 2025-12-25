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


#ifndef ET_XLIST_H
#define ET_XLIST_H

#include <type_traits>
#include <cstdint>

#include <glib.h>


/// Type erasure part of \ref xListObj<T>.
class alignas(2) xListObjBase
{	template <typename T>
	friend class xListObj;
	friend class xListBase;

	/// Link pointer type.
	/// @details This is an aggregate.
	/// The LSB identifies whether the reference is targets a valid node.
	/// The remaining bits are the link pointer of type \c xListObjBase*.
	/// \p Valid values:
	/// - 0 = nullptr: this node is not attached to a list. Either both pointer must be 0 or none.
	/// - pointer with LSB cleared: Link to the containing list, more specifically the \c xListObjBase slice.
	/// - pointer with LSB set: normal link pointer to the next/previous item.
	typedef std::uintptr_t link_type;
	/// Marker for a used link that points to a valid node.
	constexpr static const link_type NodeLink = 1;

	/// Link to previous node or the xList owner.
	/// @details In context of the base of xListObj this is the head pointer.
	link_type Next;
	/// Link to previous node or the xList owner.
	/// @details In context of the base of xListObj this is the tail pointer.
	link_type Prev;

public:
	/// True if this node is currently attached to a list.
	constexpr bool is_attached() const noexcept { return Next != 0; }
	/// True if the node has a successor.
	constexpr bool has_next() const noexcept { return Next & NodeLink; }
	/// True if the node has a predecessor.
	constexpr bool has_prev() const noexcept { return Prev & NodeLink; }
	/// Detach the current node from it's list.
	/// @return True if the node was attached to a list before, false: no-op.
	bool detach() noexcept;
private:
	constexpr xListObjBase() noexcept : Next(0), Prev(0) {}
	~xListObjBase() noexcept { detach(); }
	xListObjBase(const xListObjBase&) = delete;
	void operator=(const xListObjBase&) = delete;

	xListObjBase& next() const noexcept { return *reinterpret_cast<xListObjBase*>(Next & ~NodeLink); }
	xListObjBase& prev() const noexcept { return *reinterpret_cast<xListObjBase*>(Prev & ~NodeLink); }
	void next(xListObjBase& item) noexcept { Next = reinterpret_cast<link_type>(&item) | NodeLink; }
	void prev(xListObjBase& item) noexcept { Prev = reinterpret_cast<link_type>(&item) | NodeLink; }
	void assert_next() const noexcept { g_assert(has_next()); }
	void assert_prev() const noexcept { g_assert(has_prev()); }
};

/// Base class for nodes of intrusive doubly linked list \ref xList<T>.
/// @remarks All nodes of \ref xList<T> must derive from this class.
template <typename T>
class xListObj : public xListObjBase
{
protected:
	xListObj() : xListObjBase() {}
public:
	/// Fetch the next node.
	/// @pre \ref has_next() must be true, UB otherwise.
	T& next() const noexcept { assert_next(); return static_cast<T&>(xListObjBase::next());
		static_assert(std::is_base_of<xListObj<T>, T>::value, "Type must derive from xListObj<T> to be used with xList."); }
	/// Fetch the previous node.
	/// @pre \ref has_prev() must be true, UB otherwise.
	T& prev() const noexcept { assert_prev(); return static_cast<T&>(xListObjBase::prev()); }
};

/// Type erasure part of \ref xList<T>
class xListBase : private xListObjBase
{
protected:
	class iterator_base
	{
	protected:
		/// Pointer to the current node or pointer to the list, i.e. end().
		::xListObjBase* Ptr;
		constexpr explicit iterator_base(::xListObjBase* ptr = nullptr) noexcept : Ptr(ptr) {}
		void inc() noexcept { Ptr = &Ptr->next(); }
		void dec() noexcept { Ptr = &Ptr->prev(); }
	};

public:
	constexpr bool empty() const noexcept { return !is_attached(); }
protected:
	void assert_not_empty() const noexcept { g_assert(!empty()); }
	void push_front(xListObjBase& item) noexcept;
	void push_back(xListObjBase& item) noexcept;
	constexpr void clear() noexcept { Prev = Next = 0; }
	using xListObjBase::prev;
	using xListObjBase::next;
	constexpr xListObjBase& root() const noexcept { return const_cast<xListBase&>(*this); }
public:
	void pop_front() noexcept { assert_not_empty(); next().detach(); }
	void pop_back() noexcept { assert_not_empty(); prev().detach(); }
};

/// Intrusive doubly linked list.
/// @details The list can only handle items that derive from \ref xListObj<T>.
///\p The list DOES NOT OWN THE NODES. It only manages the content.
template <typename T>
class xList : public xListBase
{	static_assert(std::is_base_of<xListObj<T>, T>::value, "Type must derive from xListObj<T> to be used with xList.");
public:
	typedef T value_type;
	typedef const T& const_reference;
	typedef T& reference;
	typedef T* pointer;
	class const_iterator : protected iterator_base
	{	friend class xList<T>;
		constexpr explicit const_iterator(::xListObjBase& ptr) noexcept : iterator_base(&ptr) {}
	public:
		constexpr const_iterator() noexcept {}
		constexpr const_reference operator*() const noexcept { return static_cast<const T&>(*iterator_base::Ptr); }
		constexpr const T* operator->() const noexcept { return &static_cast<const T&>(*iterator_base::Ptr); }
		const_iterator operator++() noexcept { iterator_base::inc(); return *this; }
		const_iterator operator++(int) noexcept { const_iterator old = *this; iterator_base::inc(); return old; }
		const_iterator operator--() noexcept { iterator_base::dec(); return *this; }
		const_iterator operator--(int) noexcept { const_iterator old = *this; iterator_base::dec(); return old; }
		constexpr friend bool operator==(const_iterator l, const_iterator r) noexcept { return l.Ptr == r.Ptr; }
		constexpr friend bool operator!=(const_iterator l, const_iterator r) noexcept { return l.Ptr != r.Ptr; }
	};
	class iterator : public const_iterator
	{	friend class xList<T>;
		constexpr explicit iterator(::xListObjBase& ptr) noexcept : const_iterator(ptr) {}
	public:
		constexpr iterator() noexcept {}
		constexpr reference operator*() const noexcept { return static_cast<T&>(*iterator_base::Ptr); }
		constexpr T* operator->() const noexcept { return &static_cast<T&>(*iterator_base::Ptr); }
		iterator operator++() noexcept { iterator_base::inc(); return *this; }
		iterator operator++(int) noexcept { iterator old = *this; iterator_base::inc(); return old; }
		iterator operator--() noexcept { iterator_base::dec(); return *this; }
		iterator operator--(int) noexcept { iterator old = *this; iterator_base::dec(); return old; }
	};
public:
	reference front() noexcept { assert_not_empty(); return static_cast<T&>(xListBase::next()); }
	reference back() noexcept { assert_not_empty(); return static_cast<T&>(xListBase::prev()); }
	const_reference front() const noexcept { assert_not_empty(); return static_cast<const T&>(xListBase::next()); }
	const_reference back() const noexcept { assert_not_empty(); return static_cast<const T&>(xListBase::prev()); }
	iterator begin() noexcept { return iterator(empty() ? root() : front()); }
	constexpr iterator end() noexcept { return iterator(root()); }
	const_iterator begin() const noexcept { return const_iterator(empty() ? root() : front()); }
	constexpr const_iterator end() const noexcept { return const_iterator(root()); }
	const_iterator cbegin() const noexcept { return begin(); }
	constexpr const_iterator cend() const noexcept { return end(); }
	void push_front(reference item) noexcept { xListBase::push_front(item); }
	void push_back(reference item) noexcept { xListBase::push_back(item); }
};

/// Intrusive doubly linked list that owns its content.
/// @tparam D Deleter. The Expression D()(T* item) must destroy the item.
/// @details The list can only handle items that derive from \ref xListObj<T>.
///\p The list DOES NOT OWN THE NODES. It only manages the content.
template <typename T, typename D>
class xListOwn : public xList<T>, private D
{
public:
	/// Removes and discards all elements from the list.
	void clear();
	~xListOwn() { clear(); }
};

template <typename T, typename D>
void xListOwn<T,D>::clear()
{	for (T& t : *this)
		D::operator()(&t);
	xListBase::clear();
}

#endif // ET_XLIST_H
