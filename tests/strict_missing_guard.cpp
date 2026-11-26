// Negative test 2 (ultra-strict guards): omit allow_all and deliberately leave out a guard_t.
// Compilation must fail and name the missing guard_t<...>.

#define SM_DEF "jacquard/conn.inc"
#include "jacquard/machine.hpp"

struct MissingGuardConn : jacquard::Conn::Machine<MissingGuardConn> {
  struct GuardDial {
    bool operator()(guard_t<State::Closed, Event::Dial, State::Connecting>,
                    const MissingGuardConn&) const {
      return true;
    }
  };
  // Intentionally omit jacquard::allow_all and the Connecting -Connected-> Open guard.
  using Hooks = jacquard::overload<GuardDial, jacquard::no_hook>;
};

int main() {
  MissingGuardConn c;
  c.dispatch(MissingGuardConn::Event::Dial);
  return 0;
}
