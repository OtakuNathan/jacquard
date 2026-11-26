// Strict mode tests:
//   1. Strict hooks (ignore_illegal and allow_all, without no_hook):
//      every declared edge needs a trans_t handler; omissions fail to compile.
//   2. Ultra-strict mode (ignore_illegal only; no allow_all):
//      every edge needs explicit guard_t and trans_t handlers.

#include <cstdio>

#define SM_DEF "jacquard/transitions.inc"
#include "jacquard/machine.hpp"

#define SM_DEF "jacquard/conn.inc"
#include "jacquard/machine.hpp"

static int g_failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      ++g_failures;                                               \
    }                                                             \
  } while (0)

// Strict hooks: all eight edges need trans_t and the first branch needs an explicit guard_t.
struct StrictJob : jacquard::Job::Machine<StrictJob> {
  int edges = 0;
  int attempts = 0;

  struct CanRetry {
    bool operator()(guard_t<State::Failed, Event::Retry, State::Pending>,
                    const StrictJob& j) const {
      return j.attempts < 1;
    }
  };
  struct OnSubmit {
    bool operator()(trans_t<State::Idle, Event::Submit, State::Pending>,
                    StrictJob& j) const {
      ++j.edges;
      return true;
    }
  };
  struct OnStart {
    bool operator()(trans_t<State::Pending, Event::Start, State::Running>,
                    StrictJob& j) const {
      ++j.edges;
      return true;
    }
  };
  struct OnComplete {
    bool operator()(trans_t<State::Running, Event::Complete, State::Done>,
                    StrictJob& j) const {
      ++j.edges;
      return true;
    }
  };
  struct OnFail {
    bool operator()(trans_t<State::Running, Event::Fail, State::Failed>,
                    StrictJob& j) const {
      ++j.edges;
      return true;
    }
  };
  struct OnRetryOk {
    bool operator()(trans_t<State::Failed, Event::Retry, State::Pending>,
                    StrictJob& j) const {
      ++j.attempts;
      ++j.edges;
      return true;
    }
  };
  struct OnRetryExhausted {
    bool operator()(trans_t<State::Failed, Event::Retry, State::Cancelled>,
                    StrictJob& j) const {
      ++j.edges;
      return true;
    }
  };
  struct OnAnyCancel {
    template <State F, State T>
    bool operator()(trans_t<F, Event::Cancel, T>, StrictJob& j) const {
      ++j.edges;
      return true;
    }
  };

  using Hooks = jacquard::overload<CanRetry, OnSubmit, OnStart, OnComplete, OnFail,
                                OnRetryOk, OnRetryExhausted, OnAnyCancel,
                                jacquard::ignore_illegal, jacquard::allow_all>;
};

// Ultra-strict mode (neither allow_all nor no_hook): every edge needs guard_t and trans_t handlers.
struct UltraStrictConn : jacquard::Conn::Machine<UltraStrictConn> {
  bool can_dial = true;
  int transitions = 0;

  struct GuardDial {
    bool operator()(guard_t<State::Closed, Event::Dial, State::Connecting>,
                    const UltraStrictConn& c) const {
      return c.can_dial;
    }
  };
  struct GuardConnected {
    bool operator()(guard_t<State::Connecting, Event::Connected, State::Open>,
                    const UltraStrictConn&) const {
      return true;
    }
  };
  struct GuardAnyDrop {
    template <State F, State T>
    bool operator()(guard_t<F, Event::Drop, T>, const UltraStrictConn&) const {
      return true;
    }
  };

  struct OnDial {
    bool operator()(trans_t<State::Closed, Event::Dial, State::Connecting>,
                    UltraStrictConn& c) const {
      ++c.transitions;
      return true;
    }
  };
  struct OnConnected {
    bool operator()(trans_t<State::Connecting, Event::Connected, State::Open>,
                    UltraStrictConn& c) const {
      ++c.transitions;
      return true;
    }
  };
  struct OnAnyDrop {
    template <State F, State T>
    bool operator()(trans_t<F, Event::Drop, T>, UltraStrictConn& c) const {
      ++c.transitions;
      return true;
    }
  };

  using Hooks = jacquard::overload<GuardDial, GuardConnected, GuardAnyDrop, OnDial,
                                OnConnected, OnAnyDrop, jacquard::ignore_illegal>;
};

// Validate the full rulebook, independent of which dispatch<Ev>() calls main instantiates.
static_assert(StrictJob::validate_rulebook(), "complete strict rulebook");
static_assert(UltraStrictConn::validate_rulebook(), "complete guard rulebook");

int main() {
  StrictJob j;
  CHECK(j.state() == StrictJob::State::Idle);
  CHECK(j.dispatch(StrictJob::Event::Submit));
  CHECK(j.dispatch(StrictJob::Event::Start));
  CHECK(j.dispatch(StrictJob::Event::Fail));
  CHECK(j.dispatch(StrictJob::Event::Retry));  // attempts 0 -> 1, Pending
  CHECK(j.state() == StrictJob::State::Pending);
  CHECK(j.dispatch(StrictJob::Event::Start));
  CHECK(j.dispatch(StrictJob::Event::Fail));
  CHECK(j.dispatch(StrictJob::Event::Retry));  // attempts == 1 -> Cancelled
  CHECK(j.state() == StrictJob::State::Cancelled);
  CHECK(j.edges == 7);

  CHECK(!j.dispatch(StrictJob::Event::Start));
  CHECK(j.rejects() == 1);
  CHECK(j.edges == 7);

  UltraStrictConn c;
  c.can_dial = false;
  CHECK(!c.dispatch(UltraStrictConn::Event::Dial));
  CHECK(c.rejects() == 1);
  c.can_dial = true;
  CHECK(c.dispatch(UltraStrictConn::Event::Dial));
  CHECK(c.dispatch(UltraStrictConn::Event::Connected));
  CHECK(c.dispatch(UltraStrictConn::Event::Drop));
  CHECK(c.state() == UltraStrictConn::State::Closed);
  CHECK(c.transitions == 3);

  if (g_failures == 0) {
    std::printf("strict mode: all checks passed.\n");
    return 0;
  }
  return 1;
}
