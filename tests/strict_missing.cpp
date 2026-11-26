// Negative test 1 (strict hooks): deliberately omit the Failed -Retry-> Cancelled edge handler.
// Compilation must fail and name the missing trans_t<Failed, Retry, Cancelled>.

#define SM_DEF "jacquard/transitions.inc"
#include "jacquard/machine.hpp"

struct MissingHookJob : jacquard::Job::Machine<MissingHookJob> {
  struct CanRetry {
    bool operator()(guard_t<State::Failed, Event::Retry, State::Pending>,
                    const MissingHookJob&) const {
      return true;
    }
  };
  struct OnSubmit {
    bool operator()(trans_t<State::Idle, Event::Submit, State::Pending>,
                    MissingHookJob&) const { return true; }
  };
  struct OnStart {
    bool operator()(trans_t<State::Pending, Event::Start, State::Running>,
                    MissingHookJob&) const { return true; }
  };
  struct OnComplete {
    bool operator()(trans_t<State::Running, Event::Complete, State::Done>,
                    MissingHookJob&) const { return true; }
  };
  struct OnFail {
    bool operator()(trans_t<State::Running, Event::Fail, State::Failed>,
                    MissingHookJob&) const { return true; }
  };
  struct OnRetryOk {
    bool operator()(trans_t<State::Failed, Event::Retry, State::Pending>,
                    MissingHookJob&) const { return true; }
  };
  struct OnAnyCancel {
    template <State F, State T>
    bool operator()(trans_t<F, Event::Cancel, T>, MissingHookJob&) const { return true; }
  };

  // Intentionally missing the Failed -Retry-> Cancelled handler.
  using Hooks =
      jacquard::overload<CanRetry, OnSubmit, OnStart, OnComplete, OnFail,
                      OnRetryOk, OnAnyCancel, jacquard::ignore_illegal,
                      jacquard::allow_all>;
};

int main() {
  MissingHookJob j;
  j.dispatch(MissingHookJob::Event::Submit);
  return 0;
}
