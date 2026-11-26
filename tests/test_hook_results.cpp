#include <cstdio>
#include <type_traits>

#define SM_DEF "hook_results.inc"
#include "jacquard/machine.hpp"

struct Results : jacquard::HookResults::Machine<Results> {
  struct Request {
    static constexpr Event event = Event::RunPayload;
    bool effect_succeeded;
  };
  template <Event Ev>
  using payload_type = typename std::conditional<Ev == Event::RunPayload, Request, void>::type;

  bool primary_guard = true;
  bool alternative_guard = true;
  bool primary_ok = true;
  bool alternative_ok = true;
  bool loop_ok = true;
  mutable unsigned primary_guards = 0, alternative_guards = 0;
  unsigned primary_hooks = 0, alternative_hooks = 0, loop_hooks = 0, illegal = 0;
  bool pre_state_ok = true;

  struct Rules {
    template <Event E, class... Args>
    bool operator()(guard_t<State::Ready, E, State::Primary>, const Results& m,
                    const Args&...) const {
      ++m.primary_guards;
      return m.primary_guard;
    }
    template <Event E, class... Args>
    bool operator()(guard_t<State::Ready, E, State::Alternative>, const Results& m,
                    const Args&...) const {
      ++m.alternative_guards;
      return m.alternative_guard;
    }

    bool primary(Results& m) const {
      ++m.primary_hooks;  // external/user data is not automatically rolled back
      m.pre_state_ok &= m.state() == State::Ready;
      return m.primary_ok;
    }
    bool operator()(trans_t<State::Ready, Event::Run, State::Primary>, Results& m) const {
      return primary(m);
    }
    bool operator()(trans_t<State::Ready, Event::RunPayload, State::Primary>, Results& m,
                    const Request& request) const {
      return primary(m) && request.effect_succeeded;
    }
    template <Event E, class... Args>
    bool operator()(trans_t<State::Ready, E, State::Alternative>, Results& m,
                    const Args&...) const {
      ++m.alternative_hooks;
      m.pre_state_ok &= m.state() == State::Ready;
      return m.alternative_ok;
    }
    template <State F>
    bool operator()(trans_t<F, Event::Reset, State::Ready>, Results&) const { return true; }
    bool operator()(trans_t<State::Ready, Event::Loop, State::Ready>, Results& m) const {
      ++m.loop_hooks;
      return m.loop_ok;
    }
    void operator()(illegal_t, Results& m) const { ++m.illegal; }
  };
  using Hooks = jacquard::overload<Rules, jacquard::allow_all>;
};

static_assert(Results::validate_rulebook(), "complete bool rulebook");

using Event = Results::Event;
using State = Results::State;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (false)

int main() {
  // Runtime enum and typed no-payload entry points must stop on hook failure.
  Results plain;
  plain.primary_ok = false;
  CHECK(!plain.dispatch(Event::Run));
  CHECK(!plain.dispatch<Event::Run>());
  CHECK(plain.state() == State::Ready && plain.primary_guards == 2 && plain.primary_hooks == 2);
  CHECK(plain.alternative_guards == 0 && plain.alternative_hooks == 0);
  CHECK(plain.rejects() == 0 && plain.illegal == 0 && plain.pre_state_ok);
  plain.primary_ok = true;
  CHECK(plain.dispatch(Event::Run) && plain.state() == State::Primary);
  CHECK(plain.dispatch(Event::Reset) && plain.state() == State::Ready);

  // Both payload entry points have the very same commit/failure semantics.
  Results payload;
  CHECK(!payload.dispatch<Event::RunPayload>(Results::Request{false}));
  CHECK(!payload.dispatch(Results::Request{false}));
  CHECK(payload.state() == State::Ready && payload.primary_hooks == 2 && payload.pre_state_ok);
  CHECK(payload.alternative_guards == 0 && payload.alternative_hooks == 0);
  CHECK(payload.rejects() == 0 && payload.illegal == 0);
  CHECK(payload.dispatch(Results::Request{true}) && payload.state() == State::Primary);

  // A guard rejection, unlike a failed hook, still tries the next branch.
  Results fork;
  fork.primary_guard = false;
  fork.alternative_ok = false;
  CHECK(!fork.dispatch(Event::Run));
  CHECK(fork.state() == State::Ready && fork.primary_hooks == 0 && fork.alternative_hooks == 1);
  CHECK(fork.primary_guards == 1 && fork.alternative_guards == 1);
  CHECK(fork.rejects() == 0 && fork.illegal == 0);
  fork.alternative_ok = true;
  CHECK(fork.dispatch(Event::Run) && fork.state() == State::Alternative);
  CHECK(fork.dispatch(Event::Reset));
  fork.alternative_guard = false;
  CHECK(!fork.dispatch(Event::Run));
  CHECK(fork.state() == State::Ready && fork.rejects() == 1 && fork.illegal == 1);
  CHECK(fork.primary_hooks == 0 && fork.alternative_hooks == 2 && fork.pre_state_ok);

  Results self;
  self.loop_ok = false;
  CHECK(!self.dispatch(Event::Loop));
  CHECK(self.state() == State::Ready && self.loop_hooks == 1 && self.rejects() == 0);
  self.loop_ok = true;
  CHECK(self.dispatch<Event::Loop>());
  CHECK(self.state() == State::Ready && self.loop_hooks == 2 && self.rejects() == 0);
  std::puts("Hook result contracts: all checks passed");
  return 0;
}
