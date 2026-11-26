// Transition hooks must return bool, not void or an implicitly convertible int.
#define SM_DEF "jacquard/conn.inc"
#include "jacquard/machine.hpp"

struct Wrong : jacquard::Conn::Machine<Wrong> {
  struct Dial {
#ifdef JACQUARD_VOID_HOOK
    void operator()(trans_t<State::Closed, Event::Dial, State::Connecting>, Wrong&) const {}
#else
    int operator()(trans_t<State::Closed, Event::Dial, State::Connecting>, Wrong&) const { return 1; }
#endif
  };
  using Hooks = jacquard::overload<Dial, jacquard::no_hook, jacquard::allow_all>;
};

#ifdef JACQUARD_DISPATCH_ONLY
int main() { Wrong w; return w.dispatch(Wrong::Event::Dial) ? 0 : 1; }
#else
static_assert(Wrong::validate_rulebook(), "complete rulebook required");
#endif
