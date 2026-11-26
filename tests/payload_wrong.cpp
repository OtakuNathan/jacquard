// Must fail to compile: allow_all and no_hook cannot accept a payload of the wrong type.
#include <type_traits>
#define SM_DEF "jacquard/conn.inc"
#include "jacquard/machine.hpp"

struct Conn : jacquard::Conn::Machine<Conn> {
  struct Request { bool allowed; };
  struct WrongRequest {
    static constexpr Event event = Event::Dial;
    bool allowed;
  };
  template <Event Ev>
  using payload_type =
      typename std::conditional<Ev == Event::Dial, Request, void>::type;
  using Hooks = jacquard::overload<jacquard::no_hook, jacquard::allow_all>;
};
int main() {
  Conn c;
#ifdef JACQUARD_TEST_EVENT_OBJECT
  c.dispatch(Conn::WrongRequest{true});
#else
  c.dispatch<Conn::Event::Dial>(Conn::WrongRequest{true});
#endif
}
