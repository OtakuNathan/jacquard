// Jacquard metaprogramming utilities.
//
// Copied from portal/portal/type_list.h in the portal project, with the namespace
// changed to jacquard::meta and aliases and assertions added.
// Kept here instead of included to avoid an external dependency. These utilities
// can move together if the shared building blocks are split into a base library.
//
// Utilities: type_list, type_list_size, element_at(_t), and prepend(_t).
// type_list uses an index_sequence and one empty base class per element. That gives
// element_at O(1) random access at the cost of N type_list_leaf instantiations.

#ifndef JACQUARD_META_HPP
#define JACQUARD_META_HPP

#include <cstddef>
#include <type_traits>
#include <utility>

namespace jacquard {
namespace meta {

template <std::size_t I, class T>
struct type_list_leaf {
  using type = T;
};

template <class Is, class... Ts>
struct type_list_base;

template <std::size_t... idx, class... Ts>
struct type_list_base<std::index_sequence<idx...>, Ts...>
    : type_list_leaf<idx, Ts>... {};

template <class... Ts>
struct type_list : type_list_base<std::index_sequence_for<Ts...>, Ts...> {
  template <std::size_t I, class T>
  static typename type_list_leaf<I, T>::type element_at(type_list_leaf<I, T>) noexcept;
};

template <class T>
struct type_list_size;

template <class... Ts>
struct type_list_size<type_list<Ts...>>
    : std::integral_constant<std::size_t, sizeof...(Ts)> {};

template <std::size_t I, class TL>
struct element_at;

template <std::size_t I, class... Ts>
struct element_at<I, type_list<Ts...>> {
  static_assert(I < sizeof...(Ts), "element_at index out of range");
  using type = decltype(type_list<Ts...>::template element_at<I>(
      std::declval<type_list<Ts...>>()));
};

template <std::size_t I, class TL>
using element_at_t = typename element_at<I, TL>::type;

template <class T, class TL>
struct prepend;

template <class T, class... Ts>
struct prepend<T, type_list<Ts...>> {
  using type = type_list<T, Ts...>;
};

template <class T, class TL>
using prepend_t = typename prepend<T, TL>::type;

}  // namespace meta
}  // namespace jacquard

#endif  // JACQUARD_META_HPP
