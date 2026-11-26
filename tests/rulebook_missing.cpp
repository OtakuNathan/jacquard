// Must fail to compile: main only dispatches Dial, but full validation must catch the missing Connected and Drop hooks.
#define SM_DEF "jacquard/conn.inc"
#include "jacquard/machine.hpp"

struct Conn : jacquard::Conn::Machine<Conn> {
  struct OnDial {
    bool operator()(trans_t<State::Closed, Event::Dial, State::Connecting>,
                    Conn&) const { return true; }
  };
  using Hooks =
      jacquard::overload<OnDial, jacquard::ignore_illegal, jacquard::allow_all>;
};
static_assert(Conn::validate_rulebook(), "complete rulebook required");
int main() {
  Conn c;
  c.dispatch<Conn::Event::Dial>();
}
