// Copyright (c) 2016 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <type_traits>
#include <utility>
#include <memory>
#include <functional>

#ifdef __clang__
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wmissing-braces"
#   pragma clang diagnostic ignored "-Wundefined-inline"
#   pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

namespace detail {

///////////////////// General utility stuff
template <std::size_t Index>
using size_t_ = std::integral_constant<std::size_t, Index >;

template <class T> struct identity{
    typedef T type;
};



// TODO: this is a helper class for binding to references,
// and it is neither tested nor used...
template <class T>
class reference_wrapper {
public:
  // types
  typedef T type;
 
  // construct/copy/destroy
  constexpr reference_wrapper() noexcept;   // Must be undefined
  reference_wrapper(T& ref) noexcept : ptr(std::addressof(ref)) {}
  reference_wrapper(T&&) = delete;
  constexpr reference_wrapper(const reference_wrapper&) noexcept = default;
 
  // assignment
  constexpr reference_wrapper& operator=(const reference_wrapper& x) noexcept = default;
 
  // access
  constexpr operator const T& () const noexcept { return *ptr; }

  T* ptr;
};

///////////////////// Tuple that holds it's values in the supplied order
namespace sequence_tuple {

template <std::size_t N, class T>
struct base_from_member {
    T value;
};
 
template <class I, class ...Tail>
struct tuple_base;
 
template <std::size_t... I, class ...Tail>
struct tuple_base< std::index_sequence<I...>, Tail... >
    : base_from_member<I , Tail>...
{};

template <std::size_t N, class T>
constexpr T& get_impl(base_from_member<N, T>& t) noexcept {
    return t.value;
}

template <std::size_t N, class T>
constexpr const T& get_impl(const base_from_member<N, T>& t) noexcept {
    return t.value;
}

template <class ...Values>
struct tuple: tuple_base< 
        std::make_index_sequence<sizeof...(Values)>, 
        Values...
    >
{};

template <std::size_t N, class ...T>
constexpr decltype(auto) get(tuple<T...>& t) noexcept {
    return get_impl<N>(t);
}

template <std::size_t N, class ...T>
constexpr decltype(auto) get(const tuple<T...>& t) noexcept {
    return get_impl<N>(t);
}

template <std::size_t N, class ...T>
constexpr decltype(auto) get(tuple<T...>&& t) noexcept {
    return std::move(
        get_impl<N>(t)
    );
}


template <class T> class tuple_size;
template <class T> class tuple_size<const T> 
    : public tuple_size<T>
{};

template <class T> class tuple_size<volatile T>
    : public tuple_size<T>
{};
template <class T> class tuple_size<const volatile T>
    : public tuple_size<T>
{};

template <class... Types> class tuple_size<tuple<Types...> >
    : public size_t_<sizeof...(Types)>
{};

} // namespace sequence_tuple

using sequence_tuple::get;
using sequence_tuple::tuple;
using sequence_tuple::tuple_size;


///////////////////// Array that has the constexpr
template <std::size_t N>
struct size_array {                         // libc++ misses constexpr on operator[]
    typedef std::size_t type;
    std::size_t data[N];
    
    static constexpr std::size_t size() noexcept { return N; }

    constexpr std::size_t count_nonzeros() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < size(); ++i) {
            if (data[i]) {
                ++ count;
            }
        }
        return count;
    }
};

template <std::size_t I, std::size_t N>
constexpr std::size_t get(const size_array<N>& a) noexcept {
    static_assert(I < N, "Out of bounds");
    return a.data[I];
}


template <class T> constexpr size_array<sizeof(T)> fields_count_and_type_ids_with_zeros() noexcept;

///////////////////// All the stuff for representing Type as integer and converting integer back to type
namespace typeid_conversions {

///////////////////// Helper constants and typedefs
constexpr std::size_t native_types_mask = 31;
constexpr std::size_t bits_per_extension = 3;
constexpr std::size_t extension_maks = (
    static_cast<std::size_t>((1 << bits_per_extension) - 1)
        << static_cast<std::size_t>(sizeof(std::size_t) * 8 - bits_per_extension)
);
constexpr std::size_t native_ptr_type = (
    static_cast<std::size_t>(1)
        << static_cast<std::size_t>(sizeof(std::size_t) * 8 - bits_per_extension)
);
constexpr std::size_t native_const_ptr_type = (
    static_cast<std::size_t>(2)
        << static_cast<std::size_t>(sizeof(std::size_t) * 8 - bits_per_extension)
);

constexpr std::size_t native_const_volatile_ptr_type = (
    static_cast<std::size_t>(3)
        << static_cast<std::size_t>(sizeof(std::size_t) * 8 - bits_per_extension)
);

constexpr std::size_t native_volatile_ptr_type = (
    static_cast<std::size_t>(4)
        << static_cast<std::size_t>(sizeof(std::size_t) * 8 - bits_per_extension)
);

constexpr std::size_t native_ref_type = (
    static_cast<std::size_t>(5)
        << static_cast<std::size_t>(sizeof(std::size_t) * 8 - bits_per_extension)
);

template <std::size_t Index, std::size_t Extension>
using if_extension = typename std::enable_if< (Index & extension_maks) == Extension >::type*;

///////////////////// Helper functions
template <std::size_t Unptr>
constexpr std::size_t type_to_id_extension_apply(std::size_t ext) noexcept {
    constexpr std::size_t native_id = (Unptr & native_types_mask);
    constexpr std::size_t extensions = (Unptr & ~native_types_mask);
    static_assert(
        !((extensions >> bits_per_extension) & native_types_mask),
        "max extensions reached"
    );

    return (extensions >> bits_per_extension) | native_id | ext;
}

template <std::size_t Index>
constexpr auto remove_1_ext() noexcept {
    return size_t_<
        ((Index & ~native_types_mask) << bits_per_extension) | (Index & native_types_mask) 
    >{};
}


///////////////////// Forward declarations

template <class Type> constexpr auto type_to_id(identity<Type*>) noexcept;
template <class Type> constexpr auto type_to_id(identity<const Type*>) noexcept;
template <class Type> constexpr auto type_to_id(identity<const volatile Type*>) noexcept;
template <class Type> constexpr auto type_to_id(identity<volatile Type*>) noexcept;
template <class Type> constexpr auto type_to_id(identity<Type&>) noexcept;
template <class Type> constexpr auto type_to_id(identity<Type>) noexcept;
    
template <std::size_t Index> constexpr auto id_to_type(size_t_<Index >, if_extension<Index, native_const_ptr_type> = 0) noexcept;
template <std::size_t Index> constexpr auto id_to_type(size_t_<Index >, if_extension<Index, native_ptr_type> = 0) noexcept;
template <std::size_t Index> constexpr auto id_to_type(size_t_<Index >, if_extension<Index, native_const_volatile_ptr_type> = 0) noexcept;
template <std::size_t Index> constexpr auto id_to_type(size_t_<Index >, if_extension<Index, native_volatile_ptr_type> = 0) noexcept;
template <std::size_t Index> constexpr auto id_to_type(size_t_<Index >, if_extension<Index, native_ref_type> = 0) noexcept;


///////////////////// Definitions of type_to_id and id_to_type for fundamental types
#define REGISTER_TYPE(Type, Index)                              \
    constexpr std::size_t type_to_id(identity<Type>) noexcept { \
        return Index;                                           \
    }                                                           \
    constexpr Type id_to_type( size_t_<Index > ) noexcept {     \
        Type res{};                                             \
        return res;                                             \
    }                                                           \
    /**/


// Register all base types here
REGISTER_TYPE(unsigned char         , 1)
REGISTER_TYPE(unsigned short        , 2)
REGISTER_TYPE(unsigned int          , 3)
REGISTER_TYPE(unsigned long         , 4)
REGISTER_TYPE(unsigned long long    , 5)
REGISTER_TYPE(signed char           , 6)
REGISTER_TYPE(short                 , 7)
REGISTER_TYPE(int                   , 8)
REGISTER_TYPE(long                  , 9)
REGISTER_TYPE(long long             , 10)
REGISTER_TYPE(char                  , 11)
REGISTER_TYPE(wchar_t               , 12)
REGISTER_TYPE(char16_t              , 13)
REGISTER_TYPE(char32_t              , 14)
REGISTER_TYPE(float                 , 15)
REGISTER_TYPE(double                , 16)
REGISTER_TYPE(long double           , 17)
REGISTER_TYPE(bool                  , 18)
REGISTER_TYPE(void*                 , 19)
REGISTER_TYPE(const void*           , 20)
REGISTER_TYPE(volatile void*        , 21)
REGISTER_TYPE(const volatile void*  , 22)
REGISTER_TYPE(std::nullptr_t        , 23)

///////////////////// Definitions of type_to_id and id_to_type for types with extensions and nested types
template <class Type>
constexpr auto type_to_id(identity<Type*>) noexcept {
    constexpr auto unptr = type_to_id(identity<Type>{});
    return type_to_id_extension_apply<unptr>(native_ptr_type);
}

template <class Type>
constexpr auto type_to_id(identity<const Type*>) noexcept {
    constexpr auto unptr = type_to_id(identity<Type>{});
    return type_to_id_extension_apply<unptr>(native_const_ptr_type);
}

template <class Type>
constexpr auto type_to_id(identity<const volatile Type*>) noexcept {
    constexpr auto unptr = type_to_id(identity<Type>{});
    return type_to_id_extension_apply<unptr>(native_const_volatile_ptr_type);
}

template <class Type>
constexpr auto type_to_id(identity<volatile Type*>) noexcept {
    constexpr auto unptr = type_to_id(identity<Type>{});
    return type_to_id_extension_apply<unptr>(native_volatile_ptr_type);
}

template <class Type>
constexpr auto type_to_id(identity<Type&>) noexcept {
    constexpr auto unptr = type_to_id(identity<Type>{});
    return type_to_id_extension_apply<unptr>(native_ref_type);
}

template <class Type>
constexpr auto type_to_id(identity<Type>) noexcept {
    return fields_count_and_type_ids_with_zeros<Type>();
}


template <std::size_t Index>
constexpr auto id_to_type(size_t_<Index >, if_extension<Index, native_ptr_type>) noexcept {
    typedef decltype( id_to_type(remove_1_ext<Index>()) )* res_t;
    return res_t{}; 
}

template <std::size_t Index>
constexpr auto id_to_type(size_t_<Index >, if_extension<Index, native_const_ptr_type>) noexcept {
    typedef const decltype( id_to_type(remove_1_ext<Index>()) )* res_t;
    return res_t{}; 
}

template <std::size_t Index>
constexpr auto id_to_type(size_t_<Index >, if_extension<Index, native_const_volatile_ptr_type>) noexcept {
    typedef const volatile decltype( id_to_type(remove_1_ext<Index>()) )* res_t;
    return res_t{}; 
}


template <std::size_t Index>
constexpr auto id_to_type(size_t_<Index >, if_extension<Index, native_volatile_ptr_type>) noexcept {
    typedef volatile decltype( id_to_type(remove_1_ext<Index>()) )* res_t;
    return res_t{}; 
}


template <std::size_t Index>
constexpr auto id_to_type(size_t_<Index >, if_extension<Index, native_ref_type>) noexcept {
    typedef reference_wrapper<decltype( id_to_type(remove_1_ext<Index>()) )> res_t;
    return res_t{}; 
}

} // namespace typeid_conversions

///////////////////// Structure that remembers types as integers on a `constexpr operator Type()` call
template <std::size_t I>
struct ubiq_val {
    std::size_t* ref_;
    
    template <class T>
    constexpr void assign(const T& typeids) const noexcept {
        for (std::size_t i = 0; i < T::size(); ++i)
            ref_[I + i] = typeids.data[i];        
    }

    constexpr void assign(std::size_t val) const noexcept {
        ref_[I] = val;
    }

    template <class Type>
    constexpr operator Type() const noexcept {
        constexpr auto typeids = typeid_conversions::type_to_id(identity<Type>{});
        assign(typeids);
        return Type{};
    }
};

///////////////////// Structure that remembers types as integers on a `constexpr operator Type&()` call
// Not used right now
template <std::size_t I>
struct ubiq_ref {
    std::size_t* ref_;
    
    template <class T>
    constexpr void assign(const T& ) const noexcept {
        static_assert(!sizeof(T), "References to structures are not allowed");
    }

    constexpr void assign(std::size_t val) const noexcept {
        ref_[I] = val;
    }
    
    template <class Type>
    constexpr operator Type&() const noexcept {
        constexpr auto typeids = typeid_conversions::type_to_id(identity<Type&>{});
        assign(typeids);
        Type local{};
        return local;
    }
};

///////////////////// Structure that can be converted to reference to anything
template <std::size_t I>
struct ubiq_constructor {    
    template <class Type> constexpr operator Type&() const noexcept; // Undefined
};

///////////////////// Structure that remembers size of the type on a `constexpr operator Type()` call
template <std::size_t I>
struct ubiq_sizes {
    std::size_t* ref_;

    template <class Type>
    constexpr operator Type() const noexcept {
        ref_[I] = sizeof(Type);
        return Type{};
    }
};

///////////////////// Returns array of (offsets without accounting alignments). Required for keeping places for nested type ids
template <class T, std::size_t N, std::size_t... I>
constexpr size_array<N> get_type_offsets() noexcept {
    typedef size_array<N> array_t;
    array_t sizes{};
    T tmp{ ubiq_sizes<I>{sizes.data}... };
    (void)tmp;

    array_t offsets{{0}};
    for (std::size_t i = 1; i < array_t::size(); ++i)
        offsets.data[i] = offsets.data[i - 1] + sizes.data[i - 1];

    return offsets;
}

template <class T, std::size_t N, std::size_t... I>
constexpr std::false_type has_reference_members(
        std::size_t* misc,
        decltype(T{ ubiq_sizes<I>{misc}... })* = 0
    ) noexcept 
{
    return std::false_type{};
}


template <class T, std::size_t N, std::size_t... I>
constexpr std::true_type has_reference_members(void*) noexcept {
    return std::true_type{};
}

///////////////////// Returns array of typeids and zeros if construtor of a type accepts sizeof...(I) parameters, substitution failure otherwise
template <class T, std::size_t N, std::size_t... I>
constexpr auto type_to_array_of_type_ids(std::size_t* types) noexcept
    -> typename std::add_pointer<decltype(T{ ubiq_constructor<I>{}... })>::type
{
    constexpr std::size_t* misc = nullptr;
    static_assert(
        !decltype(has_reference_members<T, I...>(misc))::value,
        "reference members are not supported"
    );

    constexpr auto offsets = get_type_offsets<T, N, I...>();
    T tmp{ ubiq_val< get<I>(offsets) >{types}... };
    (void)tmp;
    return nullptr;
}

///////////////////// Methods for detecting max parameters for construction of T, return array of typeids and zeros
template <class T, std::size_t N, std::size_t I0, std::size_t... I>
constexpr auto detect_fields_count_and_type_ids(std::size_t* types, std::index_sequence<I0, I...>) noexcept
    -> decltype( type_to_array_of_type_ids<T, N, I0, I...>(types) ) 
{
    return type_to_array_of_type_ids<T, N, I0, I...>(types);
}

template <class T, std::size_t N, std::size_t... I>
constexpr void detect_fields_count_and_type_ids(std::size_t* types, std::index_sequence<I...>) noexcept {
    detect_fields_count_and_type_ids<T, N - 1>(types, std::make_index_sequence<N - 1>{});
}

template <class T, std::size_t N>
constexpr void detect_fields_count_and_type_ids(std::size_t*, std::index_sequence<>) noexcept {
    static_assert(!!sizeof(T), "Failed for unknown reason");
}

///////////////////// Returns array of typeids and zeros
template <class T>
constexpr size_array<sizeof(T)> fields_count_and_type_ids_with_zeros() noexcept {
    size_array<sizeof(T)> types{};
    detect_fields_count_and_type_ids<T, sizeof(T)>(types.data, std::make_index_sequence<sizeof(T)>{});
    return types;
}

///////////////////// Returns array of typeids without zeros
template <class T>
constexpr auto array_of_type_ids() noexcept {
    constexpr auto types = fields_count_and_type_ids_with_zeros<T>();
    constexpr std::size_t count = types.count_nonzeros();
    size_array<count> res{};
    std::size_t j = 0;
    for (std::size_t i = 0; i < decltype(types)::size(); ++i) {
        if (types.data[i]) {
            res.data[j] = types.data[i];
            ++ j;
        }
    }

    return res;
}

///////////////////// Convert array of typeids into sequence_tuple::tuple
template <class T, std::size_t... I>
constexpr auto as_tuple_impl(std::index_sequence<I...>) noexcept {
    constexpr auto a = array_of_type_ids<T>();
    return tuple<
        decltype(typeid_conversions::id_to_type(
            size_t_<get<I>(a)>{}
        ))...
    >{};
}

template <class T>
constexpr auto as_tuple() noexcept {
    static_assert(std::is_pod<T>::value, "Not applyable");
    constexpr auto res = as_tuple_impl<T>(
        std::make_index_sequence< decltype(array_of_type_ids<T>())::size() >()
    );
    
    static_assert(sizeof(res) == sizeof(T), "size check failed");
    static_assert(
        std::alignment_of<decltype(res)>::value == std::alignment_of<T>::value,
        "alignment check failed"
    );

    return res;
}

} // namespace detail

#ifdef __clang__
#   pragma clang diagnostic pop
#endif


template <std::size_t I, class T>
decltype(auto) flat_get(const T& val) noexcept {
    const auto* const t = reinterpret_cast<const decltype(detail::as_tuple<T>())*>( std::addressof(val) );
    return detail::sequence_tuple::get<I>(*t);
}

template <std::size_t I, class T>
using flat_tuple_element 
    = detail::identity< decltype( flat_get<I>(std::declval<T>()) ) >;
    
template <std::size_t I, class T>
using flat_tuple_element_t
    = typename flat_tuple_element<I, T>::type;

template <class T>
using flat_tuple_size = detail::tuple_size< decltype(detail::as_tuple<T>()) >;

template <class T>
constexpr std::size_t flat_tuple_size_v { flat_tuple_size<T>::value };

