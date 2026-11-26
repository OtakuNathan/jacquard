// Jacquard: a state-machine generator with a re-includable header (header-only C++14).
//
// There is intentionally no include guard. Redefine SM_DEF and include this file
// again to create a separate definition and public <Name> alias in namespace jacquard.
//
// Usage:
//   #define SM_DEF "jacquard/transitions.inc"
//   #include "jacquard/machine.hpp"
//
//   #define SM_DEF "jacquard/conn.inc"
//   #include "jacquard/machine.hpp"
//
// If SM_DEF is not defined, use "jacquard/transitions.inc".
//
// How it works:
//   1. SM_DEF uses BEGIN_MACHINE(Name) / STATE / EVENT / INIT / TERMINAL / ON / END_MACHINE.
//      This header replays SM_DEF across projection passes and #undefs all DSL macros afterward.
//      ON(...) expands into the constexpr kEdges array (a trailing comma is valid, so no sentinel
//      or recursive strip_last is needed). The dense slot table indexes cells by from * NE + event
//      and stores edge indices in declaration order.
//   2. Dispatch finds the candidate cell in O(1), then tries its guards in order.
//      An index_sequence expansion builds the function pointer table without recursion.
//   3. A (From, Event) pair can have several edges, tried in declaration order:
//      - If a guard accepts, run its trans_t hook before committing state_ = To.
//        Reentry is blocked. If the hook returns false or throws, the state stays
//        unchanged and no sibling is tried. The hook sees the source state and must return true to commit.
//      - A non-final branch that falls back to allow_all fails a compile-time assertion.
//      - If no guard accepts, call illegal_t, then count the rejection and return false.
//   4. state_ and rejects_ are private. Dispatch blocks reentry and assignment on the same instance and clears the entry flag on exceptions.
//   5. Supported entry points include dispatch(Event), typed dispatch<Ev>(),
//      and payload dispatch through dispatch<Ev>(payload) or dispatch(EvObj).
//   6. The edge type list (meta::type_list<trans_t<...>...>) is for tools and assertions,
//      not dispatch. Call validate_rulebook() to check the full table explicitly.

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <utility>

#include "jacquard/core.hpp"
#include "jacquard/meta.hpp"

#ifndef SM_DEF
#define SM_DEF "jacquard/transitions.inc"
#endif

namespace jacquard {

// Pass 1: open the generated class template (C++14 ODR-safe header definitions)
// and emit enum class State.
#define BEGIN_MACHINE(Name)                                                   \
  template <class DefinitionTag = void>                                       \
  struct JACQUARD_JOIN(Name, _definition) {                                   \
    using Self = JACQUARD_JOIN(Name, _definition);                            \
    static constexpr const char* name() noexcept { return JACQUARD_STR(Name); } \
    enum class State : std::size_t {
#define STATE(S) S,
#define EVENT(E)
#define INIT(S)
#define TERMINAL(S)
#define ON(F, Ev, T)
#define END_MACHINE \
  COUNT             \
  }                 \
  ;
#include SM_DEF
#undef BEGIN_MACHINE
#undef STATE
#undef EVENT
#undef INIT
#undef TERMINAL
#undef ON
#undef END_MACHINE

// Pass 2: emit enum class Event.
#define BEGIN_MACHINE(Name) enum class Event : std::size_t {
#define STATE(S)
#define EVENT(E) E,
#define INIT(S)
#define TERMINAL(S)
#define ON(F, Ev, T)
#define END_MACHINE \
  COUNT             \
  }                 \
  ;
#include SM_DEF
#undef BEGIN_MACHINE
#undef STATE
#undef EVENT
#undef INIT
#undef TERMINAL
#undef ON
#undef END_MACHINE

  static constexpr std::size_t NS = static_cast<std::size_t>(State::COUNT);
  static constexpr std::size_t NE = static_cast<std::size_t>(Event::COUNT);

  static constexpr std::size_t ix(State s) noexcept {
    return static_cast<std::size_t>(s);
  }
  static constexpr std::size_t ix(Event e) noexcept {
    return static_cast<std::size_t>(e);
  }

// Pass 3: initial state (missing or duplicate INIT fails to compile).
#define BEGIN_MACHINE(Name)
#define STATE(S)
#define EVENT(E)
#define INIT(S) static constexpr State kInitial = State::S;
#define TERMINAL(S)
#define ON(F, Ev, T)
#define END_MACHINE
#include SM_DEF
#undef BEGIN_MACHINE
#undef STATE
#undef EVENT
#undef INIT
#undef TERMINAL
#undef ON
#undef END_MACHINE

  static_assert(ix(kInitial) < NS, "INIT must name a declared state");

// Pass 4: validate every declared TERMINAL at compile time (including COUNT).
#define BEGIN_MACHINE(Name)
#define STATE(S)
#define EVENT(E)
#define INIT(S)
#define TERMINAL(S) \
  static_assert(ix(State::S) < NS, "TERMINAL must name a declared state");
#define ON(F, Ev, T)
#define END_MACHINE
#include SM_DEF
#undef BEGIN_MACHINE
#undef STATE
#undef EVENT
#undef INIT
#undef TERMINAL
#undef ON
#undef END_MACHINE

  static const char* state_name(State s) noexcept {
    static const char* const kNames[] = {
#define BEGIN_MACHINE(Name)
#define STATE(S) #S,
#define EVENT(E)
#define INIT(S)
#define TERMINAL(S)
#define ON(F, Ev, T)
#define END_MACHINE
#include SM_DEF
#undef BEGIN_MACHINE
#undef STATE
#undef EVENT
#undef INIT
#undef TERMINAL
#undef ON
#undef END_MACHINE
    };
    const std::size_t i = ix(s);
    return i < NS ? kNames[i] : "<invalid>";
  }

  static const char* event_name(Event e) noexcept {
    static const char* const kNames[] = {
#define BEGIN_MACHINE(Name)
#define STATE(S)
#define EVENT(E) #E,
#define INIT(S)
#define TERMINAL(S)
#define ON(F, Ev, T)
#define END_MACHINE
#include SM_DEF
#undef BEGIN_MACHINE
#undef STATE
#undef EVENT
#undef INIT
#undef TERMINAL
#undef ON
#undef END_MACHINE
    };
    const std::size_t i = ix(e);
    return i < NE ? kNames[i] : "<invalid>";
  }

  template <class Fn>
  static void for_each_terminal(Fn fn) {
    (void)fn;
#define BEGIN_MACHINE(Name)
#define STATE(S)
#define EVENT(E)
#define INIT(S)
#define TERMINAL(S) fn(State::S);
#define ON(F, Ev, T)
#define END_MACHINE
#include SM_DEF
#undef BEGIN_MACHINE
#undef STATE
#undef EVENT
#undef INIT
#undef TERMINAL
#undef ON
#undef END_MACHINE
  }

  // A transition is an empty tag carrying its source, event, and target.
  template <State F, Event Ev, State T>
  struct trans_t
      : detail::edge_tag<trans_tag_base, State, Event, F, Ev, T> {};

  // Guard tag for the same edge: takes const Derived& and any payload, and returns bool.
  template <State F, Event Ev, State T>
  struct guard_t
      : detail::edge_tag<guard_tag_base, State, Event, F, Ev, T> {};

  // Illegal transition: no declared edge or all candidate guards returned false.
  struct illegal_t : illegal_tag_base {
    State from;
    Event event;
    constexpr illegal_t(State f, Event ev) noexcept : from(f), event(ev) {}
  };

  // Edge descriptors for dump and emit_tla; dispatch looks up candidates in the slot table.
  struct EdgeDesc {
    State from;
    Event event;
    State to;
  };

  // Value representation of the declarations, one entry per edge. A trailing comma is valid; no sentinel is needed.
  //
  // A class template owns the edge table and public constants, allowing their definitions in this header
  // and ODR use across TUs in C++14. Inline functions hold the derived index tables.
  template <class = void>
  struct Tables {
    static constexpr EdgeDesc kEdges[] = {
#define BEGIN_MACHINE(Name)
#define STATE(S)
#define EVENT(E)
#define INIT(S)
#define TERMINAL(S)
#define ON(F, Ev, T) {State::F, Event::Ev, State::T},
#define END_MACHINE
#include SM_DEF
#undef BEGIN_MACHINE
#undef STATE
#undef EVENT
#undef INIT
#undef TERMINAL
#undef ON
#undef END_MACHINE
    };

    static constexpr std::size_t kEdgesCount =
        sizeof(kEdges) / sizeof(kEdges[0]);
    static constexpr std::size_t kMaxFanout =
        detail::max_fanout<NS, NE>(kEdges);
  };

  using Tab = Tables<>;

  static constexpr std::size_t kNumEdges = Tab::kEdgesCount;
  static constexpr std::size_t kMaxFanout = Tab::kMaxFanout;
  using CellTable = detail::CellTable<NS * NE, kMaxFanout>;
  using LocalIndex = detail::LocalIndex<kNumEdges>;

  static_assert(!detail::has_duplicate_edge(Tab::kEdges),
                "duplicate (From, Event, To) transition declaration");
  static_assert(kNumEdges < detail::kNoEdge,
                "too many transitions for the 16-bit slot index");

  // The two derived tables are only needed at runtime, so keep them as function-local
  // static constexpr objects: no first-call initialization lock and one object across TUs.
  static const CellTable& cells() noexcept {
    static constexpr CellTable t =
        detail::build_cells<NS, NE, kMaxFanout>(Tab::kEdges);
    return t;
  }

  static const LocalIndex& local_of() noexcept {
    static constexpr LocalIndex t = detail::build_local_index<NE>(Tab::kEdges);
    return t;
  }

  template <Event Ev>
  static constexpr std::size_t count_of() noexcept {
    return detail::count_event(Tab::kEdges, Ev);
  }

  template <Event Ev, std::size_t J>
  static constexpr std::size_t nth_of() noexcept {
    return detail::nth_edge_of_event(Tab::kEdges, Ev, J);
  }

  // Edge type list for tools and assertions, not dispatch. Built by pack expansion.
  template <std::size_t... I>
  static meta::type_list<trans_t<Tab::kEdges[I].from, Tab::kEdges[I].event,
                                 Tab::kEdges[I].to>...>
  transitions_of(std::index_sequence<I...>);
  using Transitions =
      decltype(transitions_of(std::make_index_sequence<kNumEdges>{}));

  static const EdgeDesc* edges() noexcept { return Tab::kEdges; }

  template <class D>
  class Machine {
   public:
    using Spec = Self;
    using State = typename Spec::State;
    using Event = typename Spec::Event;
    template <State F, Event Ev, State T>
    using trans_t = typename Spec::template trans_t<F, Ev, T>;
    template <State F, Event Ev, State T>
    using guard_t = typename Spec::template guard_t<F, Ev, T>;
    using illegal_t = typename Spec::illegal_t;
    using Transitions = typename Spec::Transitions;

    // A Derived class may shadow this alias with its event-to-payload map.
    // void means the event takes no payload; other entries name its exact type.
    template <Event Ev>
    using payload_type = void;

   private:
    State state_ = kInitial;
    std::size_t rejects_ = 0;
    bool dispatching_ = false;

    // A transition, including guards and illegal-event hooks, runs to
    // completion on one instance. Another instance can still be dispatched.
    class DispatchScope {
      Machine& machine_;

     public:
      explicit DispatchScope(Machine& machine) : machine_(machine) {
        if (machine_.dispatching_) {
          detail::fail_logic_error("jacquard: recursive dispatch on the same instance");
        }
        machine_.dispatching_ = true;
      }
      ~DispatchScope() noexcept { machine_.dispatching_ = false; }
      DispatchScope(const DispatchScope&) = delete;
      DispatchScope& operator=(const DispatchScope&) = delete;
    };

    static constexpr bool validate_hooks_policy() {
      static_assert(std::is_empty<typename D::Hooks>::value,
                    "Hooks must be stateless (empty) functors");
      static_assert(std::is_default_constructible<typename D::Hooks>::value,
                    "Hooks must be default constructible");
      return true;
    }

    static bool poison(D* d, Event e) {
      Machine* m = static_cast<Machine*>(d);
      // End the functor lifetime before committing: its destructor may throw.
      {
        typename D::Hooks hooks;
        hooks(illegal_t{m->state_, e}, *d);
      }
      ++m->rejects_;
      return false;
    }

    template <std::size_t I>
    static constexpr bool has_later_sibling() {
      for (std::size_t j = I + 1; j < Spec::kNumEdges; ++j) {
        if (Spec::Tab::kEdges[j].from == Spec::Tab::kEdges[I].from &&
            Spec::Tab::kEdges[j].event == Spec::Tab::kEdges[I].event) {
          return true;
        }
      }
      return false;
    }

    // Used both by a selected dispatch signature and the explicit full-table
    // validator. Ordinary typed dispatch continues to instantiate one event.
    template <std::size_t I, class... Args>
    static constexpr bool validate_signature() {
      constexpr State F = Spec::Tab::kEdges[I].from;
      constexpr Event Ev = Spec::Tab::kEdges[I].event;
      constexpr State T = Spec::Tab::kEdges[I].to;
      using GuardTag = guard_t<F, Ev, T>;
      using GuardRet = decltype(std::declval<typename D::Hooks&>()(
          GuardTag{}, std::declval<const D&>(),
          std::declval<const Args&>()...));
      using TransRet = decltype(std::declval<typename D::Hooks&>()(
          trans_t<F, Ev, T>{}, std::declval<D&>(),
          std::declval<const Args&>()...));
      static_assert(std::is_convertible<GuardRet, bool>::value,
                    "guard_t handler must return bool (or always_true_t)");
      static_assert(
          !has_later_sibling<I>() ||
              !std::is_same<GuardRet, always_true_t>::value,
          "non-final branch for the same (From, Event) must define a guard; "
          "otherwise later branch(es) are unreachable");
      static_assert(std::is_same<TransRet, bool>::value,
                    "trans_t handler must return bool");
      return true;
    }

    template <std::size_t I, class Payload>
    struct EdgeValidation {
      static constexpr bool check() { return validate_signature<I, Payload>(); }
    };

    template <std::size_t I>
    struct EdgeValidation<I, void> {
      static constexpr bool check() { return validate_signature<I>(); }
    };

    template <std::size_t... I>
    static constexpr bool validate_all(std::index_sequence<I...>) {
      using IllegalRet = decltype(std::declval<typename D::Hooks&>()(
          std::declval<illegal_t>(), std::declval<D&>()));
      const bool checked[] = {
          validate_hooks_policy(), std::is_same<IllegalRet, IllegalRet>::value,
          EdgeValidation<
              I, typename D::template payload_type<
                     Spec::Tab::kEdges[I].event>>::check()...};
      for (bool ok : checked) {
        if (!ok) return false;
      }
      return true;
    }

    // Only a rejected guard permits a later sibling. Once a guard selects
    // an edge, a failed effect aborts dispatch rather than selecting a new edge.
    enum class EdgeResult : unsigned char { guard_rejected, hook_failed, committed };

    // No callback can dispatch this instance again before this commit.
    template <std::size_t I, class... Args>
    static EdgeResult edge(D* d, const Args&... args) {
      static_assert(validate_signature<I, Args...>(), "invalid edge rule");
      constexpr State F = Spec::Tab::kEdges[I].from;
      constexpr Event Ev = Spec::Tab::kEdges[I].event;
      constexpr State T = Spec::Tab::kEdges[I].to;
      {
        typename D::Hooks hooks;
        if (!static_cast<bool>(hooks(guard_t<F, Ev, T>{},
                                     static_cast<const D&>(*d), args...))) {
          return EdgeResult::guard_rejected;
        }
        if (!hooks(trans_t<F, Ev, T>{}, *d, args...)) {
          return EdgeResult::hook_failed;
        }
      }
      static_cast<Machine*>(d)->state_ = T;
      return EdgeResult::committed;
    }

    // The enum-only entry point must not instantiate payload-only handlers.
    template <std::size_t I>
    static EdgeResult runtime_edge(D* d, std::true_type) {
      return edge<I>(d);
    }

    template <std::size_t I>
    static EdgeResult runtime_edge(D*, std::false_type) {
      detail::fail_invalid_argument("jacquard: event requires its declared payload");
    }

    template <std::size_t I>
    static EdgeResult runtime_edge(D* d) {
      using Payload = typename D::template payload_type<
          Spec::Tab::kEdges[I].event>;
      return runtime_edge<I>(d, typename std::is_void<Payload>::type{});
    }

    template <std::size_t... I>
    static EdgeResult fire_runtime(D* d, std::size_t ei, std::index_sequence<I...>) {
      using fn = EdgeResult (*)(D*);
      static const std::array<fn, sizeof...(I)> tab = {{&runtime_edge<I>...}};
      return tab[ei](d);
    }

    template <std::size_t... E>
    static bool requires_payload(Event e, std::index_sequence<E...>) {
      static constexpr std::array<bool, sizeof...(E)> required = {{
          !std::is_void<typename D::template payload_type<
              static_cast<Event>(E)>>::value...}};
      return required[ix(e)];
    }

    template <Event Ev, class... Args>
    struct FireFor {
      using fn = EdgeResult (*)(D*, const Args&...);
      static EdgeResult run(D*, std::size_t, std::index_sequence<>,
                      const Args&...) noexcept {
        return EdgeResult::guard_rejected;
      }
      template <std::size_t... J>
      static EdgeResult run(D* d, std::size_t local, std::index_sequence<J...>,
                      const Args&... args) {
        static const std::array<fn, sizeof...(J)> tab = {{
            &edge<Spec::template nth_of<Ev, J>(), Args...>...}};
        return tab[local](d, args...);
      }
    };

    template <Event Ev, class... Args>
    bool dispatch_filtered(const Args&... args) {
      static_assert(validate_hooks_policy(), "invalid Hooks policy");
      D* d = static_cast<D*>(this);
      const std::size_t cell = ix(state_) * NE + ix(Ev);
      for (std::size_t k = 0; k < kMaxFanout; ++k) {
        const unsigned short ei = Spec::cells().c[cell][k];
        if (ei == detail::kNoEdge) break;
        const EdgeResult result = FireFor<Ev, Args...>::run(
            d, Spec::local_of().l[ei],
            std::make_index_sequence<Spec::template count_of<Ev>()>{}, args...);
        if (result != EdgeResult::guard_rejected) return result == EdgeResult::committed;
      }
      return poison(d, Ev);
    }

    template <Event Ev>
    bool dispatch_without_payload(std::true_type) {
      return dispatch_filtered<Ev>();
    }

    template <Event Ev>
    bool dispatch_without_payload(std::false_type) {
      return false;  // The public entry point's static_assert diagnoses this.
    }

    template <Event Ev, class Payload>
    bool dispatch_payload(const Payload& payload, std::true_type) {
      return dispatch_filtered<Ev>(payload);
    }

    template <Event Ev, class Payload>
    bool dispatch_payload(const Payload&, std::false_type) {
      return false;  // Avoid instantiating handlers with an invalid signature.
    }

    bool dispatch_impl(Event e) {
      static_assert(validate_hooks_policy(), "invalid Hooks policy");
      D* d = static_cast<D*>(this);
      if (ix(e) >= NE) return poison(d, e);
      if (requires_payload(e, std::make_index_sequence<NE>{})) {
        detail::fail_invalid_argument("jacquard: event requires its declared payload");
      }
      const std::size_t cell = ix(state_) * NE + ix(e);
      for (std::size_t k = 0; k < kMaxFanout; ++k) {
        const unsigned short ei = Spec::cells().c[cell][k];
        if (ei == detail::kNoEdge) break;
        const EdgeResult result = fire_runtime(d, ei, std::make_index_sequence<kNumEdges>{});
        if (result != EdgeResult::guard_rejected) return result == EdgeResult::committed;
      }
      return poison(d, e);
    }

   public:
    Machine() = default;
    // Value copies are independent instances, including a copy made by a hook.
    Machine(const Machine& other) noexcept
        : state_(other.state_), rejects_(other.rejects_), dispatching_(false) {}
    Machine(Machine&& other) noexcept
        : Machine(static_cast<const Machine&>(other)) {}
    Machine& operator=(const Machine& other) {
      if (dispatching_) {
        detail::fail_logic_error("jacquard: assignment during dispatch");
      }
      state_ = other.state_;
      rejects_ = other.rejects_;
      return *this;
    }
    Machine& operator=(Machine&& other) {
      return operator=(static_cast<const Machine&>(other));
    }

    State state() const noexcept { return state_; }
    std::size_t rejects() const noexcept { return rejects_; }

    // Call after Derived is complete to check all edges with their declared
    // payload signatures, including events no normal dispatch instantiates.
    static constexpr bool validate_rulebook() {
      return validate_all(std::make_index_sequence<kNumEdges>{});
    }

    bool dispatch(Event e) {
      DispatchScope scope(*this);
      return dispatch_impl(e);
    }

    template <Event Ev>
    bool dispatch() {
      static_assert(ix(Ev) < NE, "dispatch<Ev>: event out of range");
      using Payload = typename D::template payload_type<Ev>;
      using NoPayload = typename std::is_void<Payload>::type;
      static_assert(NoPayload::value, "dispatch<Ev>: missing declared payload");
      DispatchScope scope(*this);
      return dispatch_without_payload<Ev>(NoPayload{});
    }

    template <Event Ev, class Payload>
    bool dispatch(const Payload& payload) {
      static_assert(ix(Ev) < NE, "dispatch<Ev>: event out of range");
      using Expected = typename D::template payload_type<Ev>;
      using Matches = std::integral_constant<
          bool, !std::is_void<Expected>::value &&
                    std::is_same<Expected, Payload>::value>;
      static_assert(Matches::value,
                    "dispatch<Ev>: payload type must match declared payload");
      DispatchScope scope(*this);
      return dispatch_payload<Ev>(payload, Matches{});
    }

    template <class EvObj,
              typename std::enable_if<!std::is_same<EvObj, Event>::value,
                                      int>::type = 0>
    bool dispatch(const EvObj& ev) {
      return dispatch<EvObj::event>(ev);
    }

    // Print the transition matrix; a cell with multiple branches is shown as -> A|B.
    void dump(std::FILE* out = stdout) const {
      const EdgeDesc* es = edges();
      char cells[NS][NE][64];
      std::size_t col_w[NE];
      std::size_t row_w = std::strlen("state\\event");

      for (std::size_t s = 0; s < NS; ++s) {
        const std::size_t len = std::strlen(state_name(static_cast<State>(s)));
        if (len > row_w) row_w = len;
      }
      for (std::size_t e = 0; e < NE; ++e) {
        col_w[e] = std::strlen(event_name(static_cast<Event>(e)));
        for (std::size_t s = 0; s < NS; ++s) {
          cells[s][e][0] = '\0';
        }
      }

      for (std::size_t i = 0; i < kNumEdges; ++i) {
        const std::size_t s = ix(es[i].from);
        const std::size_t e = ix(es[i].event);
        char* buf = cells[s][e];
        const std::size_t cur = std::strlen(buf);
        const char* prefix = cur == 0 ? "-> " : "|";
        std::snprintf(buf + cur, sizeof(cells[s][e]) - cur, "%s%s", prefix,
                      state_name(es[i].to));
      }

      for (std::size_t s = 0; s < NS; ++s) {
        for (std::size_t e = 0; e < NE; ++e) {
          if (cells[s][e][0] == '\0') {
            std::snprintf(cells[s][e], sizeof(cells[s][e]), "!!");
          }
          const std::size_t len = std::strlen(cells[s][e]);
          if (len > col_w[e]) col_w[e] = len;
        }
      }

      std::fprintf(out, "%-*s", static_cast<int>(row_w), "state\\event");
      for (std::size_t e = 0; e < NE; ++e) {
        std::fprintf(out, " | %-*s", static_cast<int>(col_w[e]),
                     event_name(static_cast<Event>(e)));
      }
      std::fprintf(out, "\n");

      for (std::size_t s = 0; s < NS; ++s) {
        std::fprintf(out, "%-*s", static_cast<int>(row_w),
                     state_name(static_cast<State>(s)));
        for (std::size_t e = 0; e < NE; ++e) {
          std::fprintf(out, " | %-*s", static_cast<int>(col_w[e]), cells[s][e]);
        }
        std::fprintf(out, "%s\n", ix(state_) == s ? "  <- now" : "");
      }
    }
  };
};

// C++14 definitions: all owners are templates, so these are ODR-safe in a
// header even when callers take addresses or bind references across TUs.
#define BEGIN_MACHINE(Name)                                                    \
  template <class DefinitionTag>                                               \
  constexpr std::size_t JACQUARD_JOIN(Name, _definition)<DefinitionTag>::NS;   \
  template <class DefinitionTag>                                               \
  constexpr std::size_t JACQUARD_JOIN(Name, _definition)<DefinitionTag>::NE;   \
  template <class DefinitionTag>                                               \
  constexpr typename JACQUARD_JOIN(Name, _definition)<DefinitionTag>::State    \
      JACQUARD_JOIN(Name, _definition)<DefinitionTag>::kInitial;               \
  template <class DefinitionTag>                                               \
  constexpr std::size_t                                                        \
      JACQUARD_JOIN(Name, _definition)<DefinitionTag>::kNumEdges;              \
  template <class DefinitionTag>                                               \
  constexpr std::size_t                                                        \
      JACQUARD_JOIN(Name, _definition)<DefinitionTag>::kMaxFanout;             \
  template <class DefinitionTag>                                               \
  template <class TableTag>                                                    \
  constexpr typename JACQUARD_JOIN(Name, _definition)<DefinitionTag>::EdgeDesc \
      JACQUARD_JOIN(Name, _definition)<DefinitionTag>::Tables<TableTag>::kEdges[]; \
  template <class DefinitionTag>                                               \
  template <class TableTag>                                                    \
  constexpr std::size_t                                                        \
      JACQUARD_JOIN(Name, _definition)<DefinitionTag>::Tables<TableTag>::kEdgesCount; \
  template <class DefinitionTag>                                               \
  template <class TableTag>                                                    \
  constexpr std::size_t                                                        \
      JACQUARD_JOIN(Name, _definition)<DefinitionTag>::Tables<TableTag>::kMaxFanout; \
  using Name = JACQUARD_JOIN(Name, _definition)<>;
#define STATE(S)
#define EVENT(E)
#define INIT(S)
#define TERMINAL(S)
#define ON(F, Ev, T)
#define END_MACHINE
#include SM_DEF
#undef BEGIN_MACHINE
#undef STATE
#undef EVENT
#undef INIT
#undef TERMINAL
#undef ON
#undef END_MACHINE

}  // namespace jacquard

#ifndef JACQUARD_KEEP_MACROS
#undef SM_DEF
#endif
