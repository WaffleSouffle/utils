//----------------------------------------------------------------------
// utils/memory: Smart pointers and objects based on external allocation/
//               ref-counting mechanism
//----------------------------------------------------------------------
//
//          Copyright kennytm (auraHT Ltd.) 2011.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file doc/LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

/**

``<utils/memory.hpp>`` --- Smart custom pointer/handles
=======================================================

This package provides a C++11-style interface to pointers and handles managed
by a custom deallocation or reference-counting mechanism.

*/

#ifndef MEMORY_HPP_E43O7TFU5KG
#define MEMORY_HPP_E43O7TFU5KG

#include <cstdio>
#include <utility>
#include <functional>
#include <type_traits>
#include <boost/config.hpp>

namespace utils {

/**
Members
-------
*/

//{{{ unique_handle

/**
.. type:: class utils::unique_handle<HandleDeleter> final

    ::

        struct POSIXFileDeleter
        {
            static const int null = -1;
            static void release(int handle) noexcept
            {
                close(handle);
            }
        };

        ...

        {
            utils::unique_handle<int, POSIXFileDeleter> fd (open(...));
            read(fd.get(), ...);
            ...
        }

    A unique handle is a generalization of ``std::unique_ptr`` which can manage
    resources associated with arbitrary scalar types.

    In the :type:`~utils::unique_handle` class template, the *HandleDeleter*
    is a structure that describes how to deallocate a resource associated with
    the handle. It should provide two static members::

        static const T null;
        // Set the invalid state of the handle. (T must be a scalar type.)

        static void release(T handle) noexcept;
        // Release a resource associated with the handle.

    The unique_handle class itself contains almost members of unique_ptr,
    except that the functions that takes and returns a pointer now uses a handle
    (``T``) instead. The missing members are:

    * ``operator*``
    * ``operator->``
    * ``operator bool``
    * ``get_deleter``
*/
template <typename HandleDeleter>
class unique_handle
{
public:
    typedef typename std::remove_cv<decltype(HandleDeleter::null)>::type type;
    typedef HandleDeleter deleter_type;

    static_assert(std::is_scalar<type>::value, "Only scalar types are supported.");
    // ^ an arbitrary restriction, may lift in the future.

    type get() const noexcept { return _obj; }

    type release() noexcept
    {
        type retval = _obj;
        _obj = HandleDeleter::null;
        return retval;
    }

    void reset(type new_obj = HandleDeleter::null) noexcept
    {
        type old_obj = _obj;
        _obj = new_obj;
        if (old_obj != HandleDeleter::null)
            HandleDeleter::release(old_obj);
    }

    ~unique_handle()
    {
        if (_obj != HandleDeleter::null)
            HandleDeleter::release(_obj);
    }

    constexpr unique_handle() noexcept : _obj(HandleDeleter::null) {}
    explicit unique_handle(type obj) noexcept : _obj(obj) {}

    unique_handle(unique_handle&& other) noexcept : _obj(other._obj)
    { other._obj = HandleDeleter::null; }

    unique_handle(const unique_handle&) = delete;
    unique_handle& operator=(const unique_handle&) = delete;

    unique_handle& operator=(unique_handle&& other) noexcept
    {
        this->reset(other.release());
        return *this;
    }

    unique_handle& operator=(std::nullptr_t) noexcept
    {
        this->reset();
        return *this;
    }

    void swap(unique_handle& other) noexcept
    {
        std::swap(_obj, other._obj);
    }

    explicit operator bool() const noexcept { return _obj != HandleDeleter::null; }

private:
    type _obj;
};

template <typename HandleDeleter>
bool operator==(const unique_handle<HandleDeleter>& a,
                const unique_handle<HandleDeleter>& b) noexcept
{
    return a.get() == b.get();
}
template <typename HandleDeleter>
bool operator!=(const unique_handle<HandleDeleter>& a,
                const unique_handle<HandleDeleter>& b) noexcept
{
    return a.get() != b.get();
}
template <typename HandleDeleter>
bool operator>=(const unique_handle<HandleDeleter>& a,
                const unique_handle<HandleDeleter>& b) noexcept
{
    return a.get() >= b.get();
}
template <typename HandleDeleter>
bool operator<=(const unique_handle<HandleDeleter>& a,
                const unique_handle<HandleDeleter>& b) noexcept
{
    return a.get() <= b.get();
}
template <typename HandleDeleter>
bool operator>(const unique_handle<HandleDeleter>& a,
                const unique_handle<HandleDeleter>& b) noexcept
{
    return a.get() > b.get();
}
template <typename HandleDeleter>
bool operator<(const unique_handle<HandleDeleter>& a,
               const unique_handle<HandleDeleter>& b) noexcept
{
    return a.get() < b.get();
}

//}}}

//{{{ common base of all smart pointers.

namespace xx_impl
{
    template <typename T, typename GenericDeleter, template <typename, typename> class Template>
    class generic_smart_ptr
    {
        typedef Template<T, GenericDeleter> Self;

    public:
        typedef T* pointer;
        typedef T element_type;

        typedef GenericDeleter deleter_type;

        T* operator->() const noexcept { return _p; }
        typename std::add_lvalue_reference<T>::type operator*() const { return *_p; }

        T* get() const noexcept { return _p; }
        explicit operator bool() const noexcept { return _p != nullptr; }

        T* release() noexcept
        {
            T* retval = _p;
            _p = nullptr;
            return retval;
        }

        void reset(T* ptr = nullptr) noexcept
        {
            T* old_ptr = _p;
            _p = ptr;
            if (old_ptr != nullptr)
                GenericDeleter::release(old_ptr);
        }

        ~generic_smart_ptr()
        {
            if (_p != nullptr)
                GenericDeleter::release(_p);
        }

        void swap(Self& other) noexcept
        {
            std::swap(_p, other._p);
        }

        template <typename U>
        Self& operator=(Template<U, GenericDeleter>&& other) noexcept
        {
            this->reset(other.release());
            return *static_cast<Self*>(this);
        }

        Self& operator=(std::nullptr_t) noexcept
        {
            this->reset();
            return *static_cast<Self*>(this);
        }

    protected:
        constexpr generic_smart_ptr() noexcept : _p(nullptr) {}

        template <typename U>
        explicit generic_smart_ptr(U* ptr) noexcept : _p(ptr) {}

        template <typename U>
        generic_smart_ptr(U&& other) noexcept : _p(other._p)
        { other._p = nullptr; }

    private:
        T* _p;

        template <typename, typename, template<typename, typename> class>
        friend class generic_smart_ptr;
    };

    template <typename T, typename SA, template<typename, typename> class P>
    bool operator==(const utils::xx_impl::generic_smart_ptr<T, SA, P>& a, std::nullptr_t) noexcept
    {
        return !!a;
    }
    template <typename T, typename SA, template<typename, typename> class P>
    bool operator==(std::nullptr_t, const utils::xx_impl::generic_smart_ptr<T, SA, P>& a) noexcept
    {
        return !!a;
    }
    template <typename T, typename SA, template<typename, typename> class P>
    bool operator!=(const utils::xx_impl::generic_smart_ptr<T, SA, P>& a, std::nullptr_t) noexcept
    {
        return !a;
    }
    template <typename T, typename SA, template<typename, typename> class P>
    bool operator!=(std::nullptr_t, const utils::xx_impl::generic_smart_ptr<T, SA, P>& a) noexcept
    {
        return !a;
    }
}

//}}}

//{{{ generic_unique_ptr

#define BASE_GTI9R9EEWSN(self) \
    xx_impl::generic_smart_ptr<T, GenericDeleter, utils::generic_##self##_ptr>

/**
.. type:: class utils::generic_unique_ptr<T, GenericDeleter>
          class utils::generic_shared_ptr<T, GenericDeleter>

    ::

        struct GLibDeleter
        {
            static void release(gpointer obj) noexcept
            {
                g_object_unref(obj);
            }
            static void add_ref(gpointer obj) noexcept
            {
                g_object_ref(obj);
            }
        };

        ...

        {
            utils::unique_ptr<GObject, GLibDeallocator> my_gobject (g_object_new(...));
            utils::shared_ptr<GObject, GLibDeallocator> another_object (std::move(my_gobject));
            ...
        }

    These are generic version of the ``std::unique_ptr`` and ``std::shared_ptr``
    which uses an external reference-counting mechanism (like GLib, COM, Core
    Foundation, etc.).

    In these class templates, the *GenericDeleter* is a structure that describes
    how to do reference counting on a resource held by the pointer ``T*``. It
    should provide these static members::

        static void release(T* ptr) noexcept;
        // decrease the reference count an object held by 'ptr', possibly
        // deallocating it.
        //
        // required by generic_unique_ptr<T> and generic_shared_ptr<T>.

        static void add_ref(T* ptr);
        // increase the reference count of the object.
        //
        // required by generic_shared_ptr<T>

        static long use_count(T* ptr) noexcept;
        // returns the reference count of the object.
        //
        // optional -- and if it cannot be done with noexcept, please do not define it.

    Most members of in the standard classes should also be available in these
    generic versions, except:

    * The array variant of ``unique_ptr``
    * Anything that related to ``weak_ptr`` and ``auto_ptr``
    * Atomic access (libstdc++ doesn't have these either)
    * ``unique_ptr::get_deleter``
    * ``make_shared``, ``allocate_shared``
    * ``dynamic_pointer_cast``

    The :type:`~utils::generic_shared_ptr` is similar to `boost::intrusive_ptr
    <http://www.boost.org/doc/libs/1_47_0/libs/smart_ptr/intrusive_ptr.html>`_,
    except the latter does not follow the usual ``shared_ptr`` rules ---
    constructing from a raw pointer will unnecessarily add a reference count by
    default.
*/

template <typename T, typename GenericDeleter>
class generic_unique_ptr : public BASE_GTI9R9EEWSN(unique)
{
public:
    constexpr generic_unique_ptr() noexcept : BASE_GTI9R9EEWSN(unique)() {}
    constexpr generic_unique_ptr(std::nullptr_t) noexcept : BASE_GTI9R9EEWSN(unique)() {}

    template <typename U>
    explicit generic_unique_ptr(U* ptr) noexcept : BASE_GTI9R9EEWSN(unique)(ptr) {}

    template <typename U>
    generic_unique_ptr(generic_unique_ptr<U, GenericDeleter>&& other) noexcept
        : BASE_GTI9R9EEWSN(unique)(std::move(other))
    {}

    generic_unique_ptr(generic_unique_ptr&& other) noexcept
        : BASE_GTI9R9EEWSN(unique)(std::move(other))
    {}

    generic_unique_ptr& operator=(generic_unique_ptr&& other)
    {
        return BASE_GTI9R9EEWSN(unique)::operator=(std::move(other));
    };

    generic_unique_ptr(const generic_unique_ptr&) = delete;
    generic_unique_ptr& operator=(const generic_unique_ptr&) = delete;
};

//}}}

//{{{ generic_unique_ptr comparisons

template <typename T1, typename T2, typename GenericDeleter>
bool operator==(const generic_unique_ptr<T1, GenericDeleter>& a,
                const generic_unique_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() == b.get();
}
template <typename T1, typename T2, typename GenericDeleter>
bool operator!=(const generic_unique_ptr<T1, GenericDeleter>& a,
                const generic_unique_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() != b.get();
}
template <typename T1, typename T2, typename GenericDeleter>
bool operator>=(const generic_unique_ptr<T1, GenericDeleter>& a,
                const generic_unique_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() >= b.get();
}
template <typename T1, typename T2, typename GenericDeleter>
bool operator<=(const generic_unique_ptr<T1, GenericDeleter>& a,
                const generic_unique_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() <= b.get();
}
template <typename T1, typename T2, typename GenericDeleter>
bool operator>(const generic_unique_ptr<T1, GenericDeleter>& a,
               const generic_unique_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() > b.get();
}
template <typename T1, typename T2, typename GenericDeleter>
bool operator<(const generic_unique_ptr<T1, GenericDeleter>& a,
               const generic_unique_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() < b.get();
}

//}}}

//{{{ generic_shared_ptr

template <typename T, typename GenericDeleter>
class generic_shared_ptr : public BASE_GTI9R9EEWSN(shared)
{
public:
    constexpr generic_shared_ptr() noexcept : BASE_GTI9R9EEWSN(shared)() {}
    constexpr generic_shared_ptr(std::nullptr_t) noexcept : BASE_GTI9R9EEWSN(shared)() {}

    template <typename U>
    explicit generic_shared_ptr(U* ptr) noexcept : BASE_GTI9R9EEWSN(shared)(ptr) {}

    template <typename U>
    generic_shared_ptr(generic_shared_ptr<U, GenericDeleter>&& other) noexcept
        : BASE_GTI9R9EEWSN(shared)(std::move(other))
    {}

    template <typename U>
    generic_shared_ptr(generic_unique_ptr<U, GenericDeleter>&& other) noexcept
        : BASE_GTI9R9EEWSN(shared)(std::move(other))
    {}

    generic_shared_ptr(generic_shared_ptr&& other) noexcept
        : BASE_GTI9R9EEWSN(shared)(std::move(other))
    {}

    template <typename U>
    generic_shared_ptr(const generic_shared_ptr<U, GenericDeleter>& other)
        : BASE_GTI9R9EEWSN(shared)(other.get())
    {
        if (this->get() != nullptr)
            GenericDeleter::add_ref(this->get());
    }

    generic_shared_ptr(const generic_shared_ptr& other)
        : BASE_GTI9R9EEWSN(shared)(other.get())
    {
        if (this->get() != nullptr)
            GenericDeleter::add_ref(this->get());
    }

    template <typename U>
    generic_shared_ptr& operator=(generic_unique_ptr<U, GenericDeleter>&& other) noexcept
    {
        return BASE_GTI9R9EEWSN(shared)::operator=(std::move(other));
    }

    generic_shared_ptr& operator=(generic_shared_ptr&& other) noexcept
    {
        return BASE_GTI9R9EEWSN(shared)::operator=(std::move(other));
    }

    template <typename U>
    generic_shared_ptr& operator=(const generic_shared_ptr<U, GenericDeleter>& other)
    {
        generic_shared_ptr(other).swap(*this);
        return *this;
    }

    generic_shared_ptr& operator=(const generic_shared_ptr& other)
    {
        generic_shared_ptr(other).swap(*this);
        return *this;
    }

    long use_count() const noexcept
    {
        return GenericDeleter::use_count(this->_p);
    }

    bool unique() const noexcept
    {
        return use_count() == 1;
    }
};
#undef BASE_GTI9R9EEWSN

namespace xx_impl
{
    template <typename T>
    struct is_generic_shared_ptr
    {
        enum { value = false };
    };
    template <typename T, typename SA>
    struct is_generic_shared_ptr<generic_shared_ptr<T, SA>>
    {
        enum { value = true };
    };
}

//}}}

//{{{ generic_shared_ptr comparisons

template <typename T1, typename T2, typename GenericDeleter>
bool operator==(const generic_shared_ptr<T1, GenericDeleter>& a,
                const generic_shared_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() == b.get();
}
template <typename T1, typename T2, typename GenericDeleter>
bool operator!=(const generic_shared_ptr<T1, GenericDeleter>& a,
                const generic_shared_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() != b.get();
}
template <typename T1, typename T2, typename GenericDeleter>
bool operator>=(const generic_shared_ptr<T1, GenericDeleter>& a,
                const generic_shared_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() >= b.get();
}
template <typename T1, typename T2, typename GenericDeleter>
bool operator<=(const generic_shared_ptr<T1, GenericDeleter>& a,
                const generic_shared_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() <= b.get();
}
template <typename T1, typename T2, typename GenericDeleter>
bool operator>(const generic_shared_ptr<T1, GenericDeleter>& a,
               const generic_shared_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() > b.get();
}
template <typename T1, typename T2, typename GenericDeleter>
bool operator<(const generic_shared_ptr<T1, GenericDeleter>& a,
               const generic_shared_ptr<T2, GenericDeleter>& b) noexcept
{
    return a.get() < b.get();
}

//}}}

//{{{ file_ptr

namespace xx_impl
{
    struct FileAllocator
    {
        static void release(FILE* f) noexcept { fclose(f); }
    };
}

/**
.. type:: type utils::unique_file_ptr = utils::generic_unique_ptr<FILE, (unspecified)>

    ::

        {
            utils::unique_file_ptr f (fopen("1.txt", "r"));
            fscanf(f.get(), "%d", &res);
        }

    This class is a specialization of :type:`~utils::generic_unique_ptr` to the
    ``FILE*`` type. When destructing, the file will be closed by ``fclose()``.

    .. warning:: Do not use this with ``popen()``.
*/
typedef generic_unique_ptr<FILE, xx_impl::FileAllocator> unique_file_ptr;

//}}}

}

//{{{ swaps & hashes

namespace std
{
    template <typename HandleDeleter>
    void swap(utils::unique_handle<HandleDeleter>& a,
              utils::unique_handle<HandleDeleter>& b) noexcept
    {
        a.swap(b);
    }

    template <typename T, typename SA>
    void swap(utils::generic_unique_ptr<T, SA>& a,
              utils::generic_unique_ptr<T, SA>& b) noexcept
    {
        a.swap(b);
    }

    template <typename T, typename SA>
    void swap(utils::generic_shared_ptr<T, SA>& a,
              utils::generic_shared_ptr<T, SA>& b) noexcept
    {
        a.swap(b);
    }

    template <typename HandleDeleter>
    struct hash<utils::unique_handle<HandleDeleter>>
        : std::unary_function<utils::unique_handle<HandleDeleter>, size_t>
    {
        size_t operator()(const utils::unique_handle<HandleDeleter>& x) const
        {
            return std::hash<typename utils::unique_handle<HandleDeleter>::type>()(x.get());
        }
    };

    template <typename T, typename SA>
    struct hash<utils::generic_unique_ptr<T, SA>>
        : std::unary_function<utils::generic_unique_ptr<T, SA>, size_t>::type
    {
        size_t operator()(const T& x) const
        {
            return std::hash<T*>()(x.get());
        }
    };

    template <typename T, typename SA>
    struct hash<utils::generic_shared_ptr<T, SA>>
        : std::unary_function<utils::generic_shared_ptr<T, SA>, size_t>::type
    {
        size_t operator()(const T& x) const
        {
            return std::hash<T*>()(x.get());
        }
    };

    template <typename T, typename U>
    typename std::enable_if<utils::xx_impl::is_generic_shared_ptr<U>::value,
                            utils::generic_shared_ptr<T, typename U::deleter_type>>::type
        static_pointer_cast(const U& r) noexcept
    {
        T* ret_ptr = static_cast<T*>(r.get());
        if (ret_ptr != nullptr)
            U::deleter_type::add_ref(ret_ptr);
        return utils::generic_shared_ptr<T, typename U::deleter_type>(ret_ptr);
    }

    template <typename T, typename U>
    typename std::enable_if<utils::xx_impl::is_generic_shared_ptr<U>::value,
                            utils::generic_shared_ptr<T, typename U::deleter_type>>::type
        const_pointer_cast(const U& r) noexcept
    {
        T* ret_ptr = const_cast<T*>(r.get());
        if (ret_ptr != nullptr)
            U::deleter_type::add_ref(ret_ptr);
        return utils::generic_shared_ptr<T, typename U::deleter_type>(ret_ptr);
    }
}

//}}}

#ifndef BOOST_NO_TEMPLATE_ALIASES

/**
.. macro:: UTILS_DEF_SMART_PTR_ALIAS(ptr_type, GenericDeleter)
           UTILS_DEF_SMART_PTR_STD_FUNCS(Namespace, ptr_type)

    ::

        namespace glib
        {
            UTILS_DEF_SMART_PTR_ALIAS(unique, GLibAllocator)
            // ^ defines the 'unique_ptr<T>' type in this namespace as an alias to
            //   'utils::generic_unique_ptr<T, GLibAllocator>'

            UTILS_DEF_SMART_PTR_ALIAS(shared, GLibAllocator)
            // ^ this is for 'shared_ptr'
        }

        UTILS_DEF_SMART_PTR_STD_FUNCS(glib, unique)
        // ^ specializes 'std::swap' and 'std::hash' if required.
        //
        // (note: must be called in global namespace)

        UTILS_DEF_SMART_PTR_STD_FUNCS(glib, shared)
        // ^ this is for 'glib::shared_ptr'

    The :type:`~utils::generic_unique_ptr` and :type:`~utils::generic_shared_ptr`
    smart pointers are not meant to be used directly, but as a template alias.
    However, not all compilers support template aliases yet. Without template
    aliases, the aliases have to be rewritten using subclassing. Doing so for
    every type is error prone. Also, since now the subclass is a different type,
    the ``std::swap`` and ``std::hash`` have to be defined again.

    Such boilerplate codes are collected to the
    :macro:`UTILS_DEF_SMART_PTR_ALIAS` and :macro:`UTILS_DEF_SMART_PTR_STD_FUNCS`
    macros, such that a correct subclass will be generated and have the
    corresponding standard functions overloaded on use. Of course, if the
    compiler supports template alias (as recognized by Boost), these will simply
    expand to the ``using`` statement.
*/

#define UTILS_DEF_SMART_PTR_ALIAS(PtrType, GenericDeleter) \
    template <typename T> \
    using PtrType##_ptr = ::utils::generic_##PtrType##_ptr<T, GenericDeleter>;

#define UTILS_DEF_SMART_PTR_STD_FUNCS(PtrType, GenericDeleter)

#else

#define UTILS_DEF_SMART_PTR_ALIAS(PtrType, GenericDeleter) \
    template <typename T> \
    class PtrType##_ptr : public ::utils::generic_##PtrType##_ptr<T, GenericDeleter> \
    { \
    public: \
        template <typename Param> \
        PtrType##_ptr(Param&& param) \
            : ::utils::generic_##PtrType##_ptr<T, GenericDeleter>( \
                std::forward<Param>(param) \
            ) \
        { static_assert(!std::is_pointer<Param>::value, \
                        "Cannot implicitly convert a raw pointer to a smart pointer"); } \
        template <typename U> \
        explicit PtrType##_ptr(U* ptr) noexcept \
            : ::utils::generic_##PtrType##_ptr<T, GenericDeleter>(ptr) \
        {} \
        PtrType##_ptr() noexcept \
            : ::utils::generic_##PtrType##_ptr<T, GenericDeleter>() \
        {} \
    };

#define UTILS_DEF_SMART_PTR_STD_FUNCS(Namespace, PtrType) \
namespace std \
{ \
    template <typename T> \
    void swap(Namespace::PtrType##_ptr<T>& a, \
              Namespace::PtrType##_ptr<T>& b) noexcept { a.swap(b); } \
    template <typename T> \
    struct hash<Namespace::PtrType##_ptr<T>> \
        : public std::unary_function<Namespace::PtrType##_ptr<T>, size_t> \
    { \
        size_t operator()(const Namespace::PtrType##_ptr<T>& x) \
        { return hash<T*>()(x.get()); } \
    }; \
}

#endif

#endif

