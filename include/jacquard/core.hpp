// Shared Jacquard infrastructure, defined once for all state machines.
//
// Contents:
//   - overload<Ts...>: C++14 overload set combining rulebook handlers into one callable.
//   - Tag base classes: trans_tag_base, guard_tag_base, and illegal_tag_base.
//   - Three explicit fallback policies:
//       * no_hook: accepts transitions and ignores illegal events without a handler.
//       * ignore_illegal: handles only illegal_t; missing transition hooks fail to compile.
//       * allow_all: accepts edges with no custom guard_t; omit it to require explicit guards.
//   - A constexpr slot table groups edge declaration indices by (from, event)
//     for O(1) dispatch lookup, replacing a recursive chain over the edges.
//
// Type utilities (type_list / element_at / prepend) live in jacquard/meta.hpp.

#ifndef JACQUARD_CORE_HPP
#define JACQUARD_CORE_HPP

#include <cstddef>
#include <cstdlib>
#include <type_traits>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#include <stdexcept>
#define JACQUARD_HAS_EXCEPTIONS 1
#else
#define JACQUARD_HAS_EXCEPTIONS 0
#endif

#define JACQUARD_STR_IMPL(x) #x
#define JACQUARD_STR(x) JACQUARD_STR_IMPL(x)
#define JACQUARD_JOIN_IMPL(a, b) a##b
#define JACQUARD_JOIN(a, b) JACQUARD_JOIN_IMPL(a, b)

namespace jacquard {

// Shared tag bases let all state machines use the same fallback policies.
struct trans_tag_base {};
struct guard_tag_base {};
struct illegal_tag_base {};

// allow_all returns a type that converts to true but is distinct from a user guard's bool.
struct always_true_t {
  constexpr operator bool() const noexcept { return true; }
};

// C++14 overload set; using Ts::operator()... requires C++17.
template <class... Ts>
struct overload;

template <class T>
struct overload<T> : T {
  using T::operator();
};

template <class T, class... R>
struct overload<T, R...> : T, overload<R...> {
  using T::operator();
  using overload<R...>::operator();
};

// Relaxed fallback: accepts transitions and ignores illegal events, but never handles guards.
struct no_hook {
  template <class Tag, class D, class... Args,
            typename std::enable_if<
                std::is_base_of<trans_tag_base, Tag>::value,
                int>::type = 0>
  bool operator()(Tag, D&, const Args&...) const noexcept { return true; }

  template <class Tag, class D, class... Args,
            typename std::enable_if<
                std::is_base_of<illegal_tag_base, Tag>::value,
                int>::type = 0>
  void operator()(Tag, D&, const Args&...) const noexcept {}
};

// Strict hook mode: ignore illegal events, but require handlers for all declared edges.
struct ignore_illegal {
  template <class Tag, class D, class... Args,
            typename std::enable_if<
                std::is_base_of<illegal_tag_base, Tag>::value,
                int>::type = 0>
  void operator()(Tag, D&, const Args&...) const noexcept {}
};

// Explicit guard fallback: accept edges that have no custom guard_t handler.
// Omit it from overload to require an explicit guard on every edge.
// Even with this fallback, every non-final branch for a (From, Event) pair
// needs a custom guard; otherwise the compiler reports the later branches as unreachable.
struct allow_all {
  template <class Tag, class D, class... Args,
            typename std::enable_if<
                std::is_base_of<guard_tag_base, Tag>::value,
                int>::type = 0>
  constexpr always_true_t operator()(Tag, const D&, const Args&...) const noexcept {
    return always_true_t{};
  }
};

namespace detail {

[[noreturn]] inline void fail_logic_error(const char* msg) {
#if JACQUARD_HAS_EXCEPTIONS
  throw std::logic_error(msg);
#else
  (void)msg;
  std::abort();
#endif
}

[[noreturn]] inline void fail_invalid_argument(const char* msg) {
#if JACQUARD_HAS_EXCEPTIONS
  throw std::invalid_argument(msg);
#else
  (void)msg;
  std::abort();
#endif
}

// Tag constants are inherited from a class template with C++14 definitions.
// This permits taking their addresses in multiple translation units.
template <class TagBase, class State, class Event, State F, Event Ev, State T>
struct edge_tag : TagBase {
  static constexpr State from = F;
  static constexpr Event event = Ev;
  static constexpr State to = T;
};

template <class TagBase, class State, class Event, State F, Event Ev, State T>
constexpr State edge_tag<TagBase, State, Event, F, Ev, T>::from;
template <class TagBase, class State, class Event, State F, Event Ev, State T>
constexpr Event edge_tag<TagBase, State, Event, F, Ev, T>::event;
template <class TagBase, class State, class Event, State F, Event Ev, State T>
constexpr State edge_tag<TagBase, State, Event, F, Ev, T>::to;

// Empty-slot sentinel; its value also bounds the number of edges in one table.
constexpr unsigned short kNoEdge = 0xFFFFu;

// Each cell has FANOUT slots holding edge indices in declaration order.
template <std::size_t CELLS, std::size_t FANOUT>
struct CellTable {
  unsigned short c[CELLS][FANOUT];
};

// Maximum number of branches for any (from, event) pair.
template <std::size_t NS, std::size_t NE, class Edge, std::size_t N>
constexpr std::size_t max_fanout(const Edge (&es)[N]) {
  std::size_t counts[NS * NE] = {};
  for (std::size_t i = 0; i < N; ++i) {
    ++counts[static_cast<std::size_t>(es[i].from) * NE +
             static_cast<std::size_t>(es[i].event)];
  }
  std::size_t m = 1;
  for (std::size_t i = 0; i < NS * NE; ++i) {
    if (counts[i] > m) m = counts[i];
  }
  return m;
}

// Build the dense table: cell = from * NE + event, with slots filled in declaration order.
template <std::size_t NS, std::size_t NE, std::size_t FANOUT, class Edge,
          std::size_t N>
constexpr CellTable<NS * NE, FANOUT> build_cells(const Edge (&es)[N]) {
  CellTable<NS * NE, FANOUT> t{};
  std::size_t used[NS * NE] = {};
  for (std::size_t i = 0; i < NS * NE; ++i) {
    for (std::size_t k = 0; k < FANOUT; ++k) t.c[i][k] = kNoEdge;
  }
  for (std::size_t i = 0; i < N; ++i) {
    const std::size_t cell = static_cast<std::size_t>(es[i].from) * NE +
                             static_cast<std::size_t>(es[i].event);
    t.c[cell][used[cell]++] = static_cast<unsigned short>(i);
  }
  return t;
}

// Detect duplicate (From, Event, To) triples in O(N^2) constexpr comparisons without instantiating types.
template <class Edge, std::size_t N>
constexpr bool has_duplicate_edge(const Edge (&es)[N]) {
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = i + 1; j < N; ++j) {
      if (es[i].from == es[j].from && es[i].event == es[j].event &&
          es[i].to == es[j].to) {
        return true;
      }
    }
  }
  return false;
}

// Each edge's index among edges with the same event, used for payload dispatch.
template <std::size_t N>
struct LocalIndex {
  unsigned short l[N];
};

template <std::size_t NE, class Edge, std::size_t N>
constexpr LocalIndex<N> build_local_index(const Edge (&es)[N]) {
  LocalIndex<N> t{};
  std::size_t used[NE] = {};
  for (std::size_t i = 0; i < N; ++i) {
    const std::size_t e = static_cast<std::size_t>(es[i].event);
    t.l[i] = static_cast<unsigned short>(used[e]++);
  }
  return t;
}

template <class E, class Edge, std::size_t N>
constexpr std::size_t count_event(const Edge (&es)[N], E e) {
  std::size_t c = 0;
  for (std::size_t i = 0; i < N; ++i) {
    if (es[i].event == e) ++c;
  }
  return c;
}

// Global index of the nth edge with event e; returns N if n is out of range.
template <class E, class Edge, std::size_t N>
constexpr std::size_t nth_edge_of_event(const Edge (&es)[N], E e,
                                       std::size_t n) {
  std::size_t c = 0;
  for (std::size_t i = 0; i < N; ++i) {
    if (es[i].event != e) continue;
    if (c == n) return i;
    ++c;
  }
  return N;
}

}  // namespace detail

}  // namespace jacquard

#endif  // JACQUARD_CORE_HPP
