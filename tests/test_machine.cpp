// Main test suite coverage:
//   1. Two machines in one TU: include jacquard/machine.hpp twice to generate Job and Conn.
//   2. Slot-table dispatch, three handler match levels, and hook-before-commit order.
//   3. Rulebook guards: Retry forks to Pending or Cancelled; all false invokes poison.
//   4. Payload dispatch through dispatch<Ev>(payload) and dispatch(EvObj).
//   5. Out-of-range events safely invoke poison; dump() shows branches in the same cell.

#include <cstdio>
#include <type_traits>

#define SM_DEF "jacquard/transitions.inc"
#include "jacquard/machine.hpp"

#define SM_DEF "jacquard/conn.inc"
#include "jacquard/machine.hpp"

#if JACQUARD_HAS_EXCEPTIONS
#include <stdexcept>
#endif

static int g_failures = 0;

#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
      ++g_failures;                                                    \
    }                                                                  \
  } while (0)

// 1. Job with relaxed hooks and guards defined in the rulebook.
struct MyJob : jacquard::Job::Machine<MyJob> {
  int generic = 0;
  int idle_submit = 0;
  int cancel = 0;
  int retry_ok = 0;
  int retry_exhausted = 0;
  int illegal = 0;
  int attempts = 0;
  int max_attempts = 2;
  bool saw_pre_state_in_hook = false;

  // Guard: Failed -Retry-> Pending only while the retry budget is not exhausted.
  struct CanRetry {
    bool operator()(guard_t<State::Failed, Event::Retry, State::Pending>,
                    const MyJob& j) const {
      return j.attempts < j.max_attempts;
    }
  };

  // L3: exact-edge handler. Verify that state() still reports the source state.
  // The hook runs before the commit and can get the destination from trans_t::to.
  struct OnIdleSubmit {
    bool operator()(trans_t<State::Idle, Event::Submit, State::Pending>,
                    MyJob& j) const {
      ++j.idle_submit;
      j.saw_pre_state_in_hook = (j.state() == State::Idle);
      return true;
    }
  };

  struct OnRetryOk {
    bool operator()(trans_t<State::Failed, Event::Retry, State::Pending>,
                    MyJob& j) const {
      ++j.attempts;
      ++j.retry_ok;
      return true;
    }
  };

  struct OnRetryExhausted {
    bool operator()(trans_t<State::Failed, Event::Retry, State::Cancelled>,
                    MyJob& j) const {
      ++j.retry_exhausted;
      return true;
    }
  };

  // L2: all Cancel edges share one handler, regardless of source.
  struct OnAnyCancel {
    template <State F, State T>
    bool operator()(trans_t<F, Event::Cancel, T>, MyJob& j) const {
      ++j.cancel;
      return true;
    }
  };

  // Illegal transitions include out-of-range events and cases in which all guards reject.
  struct OnIllegal {
    void operator()(illegal_t, MyJob& j) const { ++j.illegal; }
  };

  // L1: a non-const fallback does not capture guard_t handlers taking const MyJob&.
  struct Rest {
    template <class Tag>
    bool operator()(Tag, MyJob& j) const {
      ++j.generic;
      return true;
    }
  };

  using Hooks = jacquard::overload<CanRetry, OnIdleSubmit, OnRetryOk,
                                OnRetryExhausted, OnAnyCancel, OnIllegal, Rest,
                                jacquard::allow_all>;
};

#if JACQUARD_HAS_EXCEPTIONS
// 2. Reentry: dispatch(Event::Start) from the Idle -Submit-> Pending hook.
struct AutoStartJob : jacquard::Job::Machine<AutoStartJob> {
  struct CanRetry {
    bool operator()(guard_t<State::Failed, Event::Retry, State::Pending>,
                    const AutoStartJob&) const {
      return true;
    }
  };
  struct OnSubmit {
    bool operator()(trans_t<State::Idle, Event::Submit, State::Pending>,
                    AutoStartJob& j) const {
      j.dispatch(Event::Start);
      return true;
    }
  };
  using Hooks =
      jacquard::overload<CanRetry, OnSubmit, jacquard::no_hook, jacquard::allow_all>;
};

// 3. A throwing hook leaves the state unchanged and propagates the exception.
struct ThrowingJob : jacquard::Job::Machine<ThrowingJob> {
  struct CanRetry {
    bool operator()(guard_t<State::Failed, Event::Retry, State::Pending>,
                    const ThrowingJob&) const {
      return true;
    }
  };
  struct Boom {
    bool operator()(trans_t<State::Idle, Event::Submit, State::Pending>,
                    ThrowingJob&) const {
      throw 42;
    }
  };
  using Hooks =
      jacquard::overload<CanRetry, Boom, jacquard::no_hook, jacquard::allow_all>;
};
#endif

// All guards return false, so the transition goes through poison.
struct BlockedRetryJob : jacquard::Job::Machine<BlockedRetryJob> {
  int illegal = 0;

  struct NeverRetryPending {
    bool operator()(guard_t<State::Failed, Event::Retry, State::Pending>,
                    const BlockedRetryJob&) const {
      return false;
    }
  };
  struct NeverRetryCancel {
    bool operator()(guard_t<State::Failed, Event::Retry, State::Cancelled>,
                    const BlockedRetryJob&) const {
      return false;
    }
  };
  struct OnIllegal {
    void operator()(illegal_t, BlockedRetryJob& j) const { ++j.illegal; }
  };

  using Hooks =
      jacquard::overload<NeverRetryPending, NeverRetryCancel, OnIllegal,
                      jacquard::no_hook, jacquard::allow_all>;
};

// 4. Job with event payloads: dispatch<Ev>(payload) and dispatch(EvObj).
struct PayloadJob : jacquard::Job::Machine<PayloadJob> {
  struct SubmitReq {
    static constexpr Event event = Event::Submit;
    int priority;
  };
  struct RetryBudget {
    static constexpr Event event = Event::Retry;
    int limit;
  };

  template <Event Ev>
  using payload_type = typename std::conditional<
      Ev == Event::Submit, SubmitReq,
      typename std::conditional<Ev == Event::Retry, RetryBudget, void>::type>::type;

  int priority = 0;
  int attempts = 0;

  struct GuardSubmit {
    bool operator()(guard_t<State::Idle, Event::Submit, State::Pending>,
                    const PayloadJob&, const SubmitReq& req) const {
      return req.priority > 0;
    }
  };
  struct OnSubmit {
    bool operator()(trans_t<State::Idle, Event::Submit, State::Pending>,
                    PayloadJob& j, const SubmitReq& req) const {
      j.priority = req.priority;
      return true;
    }
  };
  struct GuardRetry {
    bool operator()(guard_t<State::Failed, Event::Retry, State::Pending>,
                    const PayloadJob& j, const RetryBudget& b) const {
      return j.attempts < b.limit;
    }
  };
  struct OnRetryOk {
    bool operator()(trans_t<State::Failed, Event::Retry, State::Pending>,
                    PayloadJob& j, const RetryBudget&) const {
      ++j.attempts;
      return true;
    }
  };

  using Hooks = jacquard::overload<GuardSubmit, OnSubmit, GuardRetry, OnRetryOk,
                                jacquard::no_hook, jacquard::allow_all>;
};

static_assert(PayloadJob::validate_rulebook(), "complete payload rulebook");

// 5. A second independent machine (jacquard::Conn) in the same translation unit.
struct MyConn : jacquard::Conn::Machine<MyConn> {
  int drops = 0;

  struct OnDrop {
    template <State F, State T>
    bool operator()(trans_t<F, Event::Drop, T>, MyConn& c) const {
      ++c.drops;
      return true;
    }
  };

  using Hooks = jacquard::overload<OnDrop, jacquard::no_hook, jacquard::allow_all>;
};

static_assert(jacquard::Job::NS == 6 && jacquard::Job::NE == 6, "unexpected Job dims");
static_assert(jacquard::Conn::NS == 3 && jacquard::Conn::NE == 3,
              "unexpected Conn dims");

// The metaprogramming utilities came from portal; Transitions must match kEdges one-to-one.
static_assert(jacquard::meta::type_list_size<jacquard::Job::Transitions>::value ==
                  jacquard::Job::kNumEdges,
              "Transitions should mirror kEdges");
static_assert(
    std::is_same<jacquard::meta::element_at_t<0, jacquard::Job::Transitions>,
                 jacquard::Job::trans_t<jacquard::Job::State::Idle,
                                     jacquard::Job::Event::Submit,
                                     jacquard::Job::State::Pending>>::value,
    "first declared edge should be Idle -Submit-> Pending");
static_assert(
    jacquard::meta::type_list_size<
        jacquard::meta::prepend_t<int, jacquard::meta::type_list<>>>::value == 1,
    "prepend onto an empty type_list should yield a one-element list");

int main() {
  // --- Happy path: Idle -Submit-> Pending -Start-> Running -Complete-> Done
  MyJob m;
  CHECK(m.state() == MyJob::State::Idle);
  CHECK(m.dispatch(MyJob::Event::Submit));
  CHECK(m.state() == MyJob::State::Pending);
  CHECK(m.idle_submit == 1);
  CHECK(m.saw_pre_state_in_hook);
  CHECK(m.generic == 0);

  CHECK(m.dispatch(MyJob::Event::Start));
  CHECK(m.state() == MyJob::State::Running);
  CHECK(m.generic == 1);

  CHECK(m.dispatch(MyJob::Event::Complete));
  CHECK(m.state() == MyJob::State::Done);
  CHECK(m.generic == 2);

  // --- Done has no outgoing edges; invalid transitions and out-of-range events go to poison.
  CHECK(!m.dispatch(MyJob::Event::Cancel));
  CHECK(!m.dispatch(MyJob::Event::COUNT));
  CHECK(m.state() == MyJob::State::Done);
  CHECK(m.rejects() == 2);
  CHECK(m.illegal == 2);
  CHECK(m.generic == 2);

  // --- L2: Cancel from Pending and Running uses the same OnAnyCancel handler.
  MyJob c1;
  CHECK(c1.dispatch(MyJob::Event::Submit));
  CHECK(c1.dispatch(MyJob::Event::Cancel));
  CHECK(c1.state() == MyJob::State::Cancelled);
  CHECK(c1.cancel == 1);

  MyJob c2;
  CHECK(c2.dispatch(MyJob::Event::Submit));
  CHECK(c2.dispatch(MyJob::Event::Start));
  CHECK(c2.dispatch(MyJob::Event::Cancel));
  CHECK(c2.state() == MyJob::State::Cancelled);
  CHECK(c2.cancel == 1);
  CHECK(c2.generic == 1);

  // --- Guard fork: the first two retries go to Pending; the third goes to Cancelled.
  MyJob f;
  CHECK(f.dispatch(MyJob::Event::Submit));
  for (int i = 0; i < 2; ++i) {
    CHECK(f.dispatch(MyJob::Event::Start));
    CHECK(f.dispatch(MyJob::Event::Fail));
    CHECK(f.state() == MyJob::State::Failed);
    CHECK(f.dispatch(MyJob::Event::Retry));
    CHECK(f.state() == MyJob::State::Pending);
  }
  CHECK(f.attempts == 2);
  CHECK(f.retry_ok == 2);
  CHECK(f.retry_exhausted == 0);

  CHECK(f.dispatch(MyJob::Event::Start));
  CHECK(f.dispatch(MyJob::Event::Fail));
  CHECK(f.dispatch(MyJob::Event::Retry));  // attempts == 2: take the Cancelled branch.
  CHECK(f.state() == MyJob::State::Cancelled);
  CHECK(f.retry_exhausted == 1);

#if JACQUARD_HAS_EXCEPTIONS
  // --- Reject same-instance reentry before validating the event; the outer dispatch does not commit.
  AutoStartJob aj;
  bool reentry_threw = false;
  try {
    aj.dispatch(AutoStartJob::Event::Submit);
  } catch (const std::logic_error&) {
    reentry_threw = true;
  }
  CHECK(reentry_threw);
  CHECK(aj.state() == AutoStartJob::State::Idle);
  CHECK(aj.rejects() == 0);

  // --- A throwing hook leaves both the state and rejection count unchanged.
  {
    ThrowingJob tj;
    bool threw = false;
    try {
      tj.dispatch(ThrowingJob::Event::Submit);
    } catch (int code) {
      threw = (code == 42);
    }
    CHECK(threw);
    CHECK(tj.state() == ThrowingJob::State::Idle);
    CHECK(tj.rejects() == 0);
  }
#endif

  // --- All guards return false: illegal transition.
  BlockedRetryJob bj;
  CHECK(bj.dispatch(BlockedRetryJob::Event::Submit));
  CHECK(bj.dispatch(BlockedRetryJob::Event::Start));
  CHECK(bj.dispatch(BlockedRetryJob::Event::Fail));
  CHECK(!bj.dispatch(BlockedRetryJob::Event::Retry));
  CHECK(bj.state() == BlockedRetryJob::State::Failed);
  CHECK(bj.rejects() == 1);
  CHECK(bj.illegal == 1);

  // --- Payload dispatch passes through both guard_t and trans_t.
  PayloadJob pj;
  CHECK(!pj.dispatch(PayloadJob::SubmitReq{0}));  // The guard rejects priority <= 0.
  CHECK(pj.state() == PayloadJob::State::Idle);
  CHECK(pj.rejects() == 1);

  CHECK(pj.dispatch(PayloadJob::SubmitReq{7}));
  CHECK(pj.state() == PayloadJob::State::Pending);
  CHECK(pj.priority == 7);

  CHECK(pj.dispatch<PayloadJob::Event::Start>());
  CHECK(pj.dispatch<PayloadJob::Event::Fail>());
  CHECK(pj.dispatch(PayloadJob::RetryBudget{1}));  // attempts(0) < 1 -> Pending
  CHECK(pj.state() == PayloadJob::State::Pending);
  CHECK(pj.attempts == 1);

  CHECK(pj.dispatch<PayloadJob::Event::Start>());
  CHECK(pj.dispatch<PayloadJob::Event::Fail>());
  CHECK(pj.dispatch<PayloadJob::Event::Retry>(
      PayloadJob::RetryBudget{1}));  // attempts (1) < 1 is false: go to Cancelled.
  CHECK(pj.state() == PayloadJob::State::Cancelled);

  // --- Conn is the second machine in the same TU.
  MyConn conn;
  CHECK(conn.state() == MyConn::State::Closed);
  CHECK(!conn.dispatch(MyConn::Event::Connected));
  CHECK(conn.rejects() == 1);
  CHECK(conn.dispatch(MyConn::Event::Dial));
  CHECK(conn.state() == MyConn::State::Connecting);
  CHECK(conn.dispatch(MyConn::Event::Connected));
  CHECK(conn.state() == MyConn::State::Open);
  CHECK(conn.dispatch(MyConn::Event::Drop));
  CHECK(conn.state() == MyConn::State::Closed);
  CHECK(conn.drops == 1);

  std::printf("\n--- Job table ---\n");
  f.dump();
  std::printf("\n--- Conn table ---\n");
  conn.dump();

  if (g_failures == 0) {
    std::printf("\nAll checks passed.\n");
    return 0;
  }
  std::printf("\n%d check(s) failed.\n", g_failures);
  return 1;
}
