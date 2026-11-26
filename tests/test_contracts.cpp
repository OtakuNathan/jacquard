// Regressions for reentry, exception recovery, payload contracts, and events without candidate edges.
#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include <utility>

#define SM_DEF "contracts.inc"
#include "jacquard/machine.hpp"

#define SM_DEF "jacquard/conn.inc"
#include "jacquard/machine.hpp"

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

template <class Exception, class F>
bool throws(F f) {
  try {
    f();
  } catch (const Exception&) {
    return true;
  } catch (...) {
  }
  return false;
}

struct Transaction : jacquard::Contracts::Machine<Transaction> {
  enum class Mode {
    Normal, HookThrows, NestedThenThrows, ABA, CatchNested,
    GuardThrows, GuardReenters, IllegalThrows, IllegalReenters, OtherInstance,
    CopyInHook, MoveInHook, AssignInHook, MoveAssignInHook
  };
  Mode mode = Mode::Normal;
  Transaction* target = nullptr;
  mutable int guards = 0;
  int hooks = 0;
  int illegal = 0;
  int caught = 0;
  bool copied_instance_ran = false;

  struct Guard {
    template <State F, Event Ev, State T>
    bool operator()(guard_t<F, Ev, T>, const Transaction& m) const {
      ++m.guards;
      if (m.mode == Mode::GuardThrows) throw 17;
      // A const reference does not prevent reentry through an alias. Protect the same instance.
      if (m.mode == Mode::GuardReenters) m.target->dispatch(Event::Enter);
      return true;
    }
  };
  struct Transition {
    template <State F, Event Ev, State T>
    bool operator()(trans_t<F, Ev, T>, Transaction& m) const {
      ++m.hooks;
      if (Ev != Event::Submit) return true;
      switch (m.mode) {
        case Mode::HookThrows:
          throw 17;
        case Mode::NestedThenThrows:
          m.dispatch(Event::Enter);
          throw 17;
        case Mode::ABA:
          m.dispatch(Event::Enter);
          m.dispatch(Event::Leave);
          break;
        case Mode::CatchNested:
          try {
            m.dispatch(Event::Enter);
          } catch (const std::logic_error&) {
            ++m.caught;
          }
          break;
        case Mode::OtherInstance:
          m.target->dispatch(Event::Enter);
          break;
        case Mode::CopyInHook: {
          Transaction copy(m);
          copy.mode = Mode::Normal;
          m.copied_instance_ran =
              copy.dispatch(Event::Enter) && copy.state() == State::Busy;
          break;
        }
        case Mode::MoveInHook: {
          Transaction copy(std::move(m));
          copy.mode = Mode::Normal;
          m.copied_instance_ran =
              copy.dispatch(Event::Enter) && copy.state() == State::Busy;
          break;
        }
        case Mode::AssignInHook: {
          Transaction replacement;
          replacement.dispatch(Event::Enter);
          m = replacement;
          break;
        }
        case Mode::MoveAssignInHook: {
          Transaction replacement;
          replacement.dispatch(Event::Enter);
          m = std::move(replacement);
          break;
        }
        default:
          break;
      }
      return true;
    }
  };
  struct Illegal {
    void operator()(illegal_t, Transaction& m) const {
      ++m.illegal;
      if (m.mode == Mode::IllegalThrows) throw 17;
      if (m.mode == Mode::IllegalReenters) m.dispatch(Event::Enter);
    }
  };
  using Hooks = jacquard::overload<Guard, Transition, Illegal>;
};

static_assert(Transaction::validate_rulebook(), "complete transaction rulebook");

struct PayloadConn : jacquard::Conn::Machine<PayloadConn> {
  struct Request {
    static constexpr Event event = Event::Dial;
    bool allowed;
  };
  template <Event Ev>
  using payload_type =
      typename std::conditional<Ev == Event::Dial, Request, void>::type;

  mutable int guards = 0;
  int hooks = 0;
  bool reenter = false;

  struct Gate {
    bool operator()(guard_t<State::Closed, Event::Dial, State::Connecting>,
                    const PayloadConn& m, const Request& request) const {
      ++m.guards;
      return request.allowed;
    }
  };
  struct OnDial {
    bool operator()(trans_t<State::Closed, Event::Dial, State::Connecting>,
                    PayloadConn& m, const Request&) const {
      ++m.hooks;
      if (m.reenter) m.dispatch(Event::Dial);
      return true;
    }
  };
  using Hooks = jacquard::overload<Gate, OnDial, jacquard::no_hook, jacquard::allow_all>;
};

static_assert(PayloadConn::validate_rulebook(), "canonical payload is checked");

static bool throw_from_hooks_destructor = false;
struct DestructorConn : jacquard::Conn::Machine<DestructorConn> {
  struct Hooks : jacquard::overload<jacquard::no_hook, jacquard::allow_all> {
    ~Hooks() noexcept(false) {
      if (throw_from_hooks_destructor) {
        throw_from_hooks_destructor = false;
        throw 23;
      }
    }
  };
};

static void test_transaction_boundary() {
  using Mode = Transaction::Mode;
  using Event = Transaction::Event;
  using State = Transaction::State;

  // Previously a nested dispatch could commit before the outer one threw, or return to From so the outer one could commit.
  for (Mode mode : {Mode::NestedThenThrows, Mode::ABA}) {
    Transaction m;
    m.mode = mode;
    CHECK(throws<std::logic_error>([&] { m.dispatch(Event::Submit); }));
    CHECK(m.state() == State::Idle && m.rejects() == 0);
    CHECK(m.guards == 1 && m.hooks == 1 && m.illegal == 0);
    m.mode = Mode::Normal;
    CHECK(m.dispatch(Event::Enter));  // The exception released the dispatch lock.
    CHECK(m.state() == State::Busy);
  }

  Transaction caught;
  caught.mode = Mode::CatchNested;
  CHECK(caught.dispatch(Event::Submit));
  CHECK(caught.state() == State::Done && caught.rejects() == 0);
  CHECK(caught.caught == 1 && caught.guards == 1 && caught.hooks == 1);

  for (Mode mode : {Mode::HookThrows, Mode::GuardThrows}) {
    Transaction m;
    m.mode = mode;
    CHECK(throws<int>([&] { m.dispatch(Event::Submit); }));
    CHECK(m.state() == State::Idle && m.rejects() == 0);
    CHECK(m.hooks == (mode == Mode::HookThrows ? 1 : 0));
    m.mode = Mode::Normal;
    CHECK(m.dispatch(Event::Submit));
    CHECK(m.state() == State::Done);
  }

  Transaction guard;
  guard.mode = Mode::GuardReenters;
  guard.target = &guard;
  CHECK(throws<std::logic_error>([&] { guard.dispatch(Event::Enter); }));
  CHECK(guard.state() == State::Idle && guard.rejects() == 0);
  CHECK(guard.guards == 1 && guard.hooks == 0 && guard.illegal == 0);
  guard.mode = Mode::Normal;
  CHECK(guard.dispatch(Event::Enter));

  for (Mode mode : {Mode::IllegalThrows, Mode::IllegalReenters}) {
    Transaction m;
    m.mode = mode;
    if (mode == Mode::IllegalThrows) {
      CHECK(throws<int>([&] { m.dispatch(Event::Spare); }));
    } else {
      CHECK(throws<std::logic_error>([&] { m.dispatch(Event::Spare); }));
    }
    CHECK(m.state() == State::Idle && m.rejects() == 0);
    CHECK(m.illegal == 1 && m.guards == 0 && m.hooks == 0);
    m.mode = Mode::Normal;
    CHECK(!m.dispatch<Event::Spare>());
    CHECK(m.rejects() == 1 && m.illegal == 2);
    CHECK(m.dispatch(Event::Enter));
  }

  Transaction outer, peer;
  outer.mode = Mode::OtherInstance;
  outer.target = &peer;
  CHECK(outer.dispatch(Event::Submit));
  CHECK(outer.state() == State::Done && peer.state() == State::Busy);
  CHECK(outer.hooks == 1 && peer.hooks == 1);

  // The dispatch flag belongs to the instance; copying an active machine must not leave the copy locked.
  for (Mode mode : {Mode::CopyInHook, Mode::MoveInHook}) {
    Transaction m;
    m.mode = mode;
    CHECK(m.dispatch(Event::Submit));
    CHECK(m.state() == State::Done && m.copied_instance_ran);
    CHECK(m.hooks == 1 && m.rejects() == 0);
  }

  // Assigning to an active machine must not clear its lock or overwrite the uncommitted source state.
  for (Mode mode : {Mode::AssignInHook, Mode::MoveAssignInHook}) {
    Transaction m;
    m.mode = mode;
    CHECK(throws<std::logic_error>([&] { m.dispatch(Event::Submit); }));
    CHECK(m.state() == State::Idle && m.rejects() == 0);
    CHECK(m.hooks == 1 && m.mode == mode);
    m.mode = Mode::Normal;
    CHECK(m.dispatch(Event::Submit));
  }

  Transaction source, assigned;
  CHECK(source.dispatch(Event::Enter));
  assigned = source;
  CHECK(assigned.state() == State::Busy);
  CHECK(assigned.dispatch(Event::Leave));
  source = std::move(assigned);
  CHECK(source.state() == State::Idle);
  CHECK(source.dispatch(Event::Enter));

  // Both entry points call the illegal hook for the same event with no candidates.
  Transaction empty;
  CHECK(!empty.dispatch<Event::Spare>());
  CHECK(!empty.dispatch(Event::Spare));
  CHECK(empty.state() == State::Idle && empty.rejects() == 2);
  CHECK(empty.illegal == 2 && empty.guards == 0 && empty.hooks == 0);
}

static void test_payload_contract() {
  using Event = PayloadConn::Event;
  using State = PayloadConn::State;
  PayloadConn m;
  CHECK(throws<std::invalid_argument>([&] { m.dispatch(Event::Dial); }));
  CHECK(m.state() == State::Closed && m.rejects() == 0);
  CHECK(m.guards == 0 && m.hooks == 0);
  CHECK(!m.dispatch(PayloadConn::Request{false}));
  CHECK(m.state() == State::Closed && m.rejects() == 1);
  CHECK(m.guards == 1 && m.hooks == 0);
  CHECK(m.dispatch<Event::Dial>(PayloadConn::Request{true}));
  CHECK(m.state() == State::Connecting && m.hooks == 1);
  CHECK(m.dispatch(Event::Connected));  // Payload-free events still support runtime dispatch.

  PayloadConn nested;
  nested.reenter = true;
  bool reentry_threw = false;
  try {
    nested.dispatch(PayloadConn::Request{true});
  } catch (const std::invalid_argument&) {
    // invalid_argument derives from logic_error; distinguish event validation from reentry rejection.
  } catch (const std::logic_error&) {
    reentry_threw = true;
  }
  CHECK(reentry_threw);
  CHECK(nested.state() == State::Closed && nested.rejects() == 0);
  CHECK(nested.guards == 1 && nested.hooks == 1);
  nested.reenter = false;
  CHECK(nested.dispatch(PayloadConn::Request{true}));
  CHECK(nested.state() == State::Connecting);
}

static void test_hooks_destructor_exception() {
  using Event = DestructorConn::Event;
  using State = DestructorConn::State;
  DestructorConn edge;
  throw_from_hooks_destructor = true;
  CHECK(throws<int>([&] { edge.dispatch(Event::Dial); }));
  CHECK(!throw_from_hooks_destructor);
  CHECK(edge.state() == State::Closed && edge.rejects() == 0);
  CHECK(edge.dispatch(Event::Dial));
  CHECK(edge.state() == State::Connecting);

  DestructorConn illegal;
  throw_from_hooks_destructor = true;
  CHECK(throws<int>([&] { illegal.dispatch(Event::Connected); }));
  CHECK(!throw_from_hooks_destructor);
  CHECK(illegal.state() == State::Closed && illegal.rejects() == 0);
  CHECK(!illegal.dispatch(Event::Connected));
  CHECK(illegal.rejects() == 1);
  CHECK(illegal.dispatch(Event::Dial));
  CHECK(illegal.state() == State::Connecting);
}

int main() {
  test_transaction_boundary();
  test_payload_contract();
  test_hooks_destructor_exception();
  std::printf("contract regressions: %s\n", failures ? "FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
