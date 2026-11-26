// Must fail to compile: Dial requires a Request; allow_all cannot make the payload optional.
#include <type_traits>
#define SM_DEF "jacquard/conn.inc"
#include "jacquard/machine.hpp"

struct Conn : jacquard::Conn::Machine<Conn> {
  struct Request { bool allowed; };
  template <Event Ev>
  using payload_type =
      typename std::conditional<Ev == Event::Dial, Request, void>::type;
  using Hooks = jacquard::overload<jacquard::no_hook, jacquard::allow_all>;
};
int main() {
  Conn c;
  c.dispatch<Conn::Event::Dial>();
}
