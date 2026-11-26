#include <cstdio>
#include <type_traits>

#define SM_DEF "jacquard/review.inc"
#include "jacquard/machine.hpp"

struct ReviewDemo : jacquard::Review::Machine<ReviewDemo> {
  struct Result {
    static constexpr Event event = Event::Decide;
    unsigned score;
    bool verified;
  };
  template <Event Ev>
  using payload_type = typename std::conditional<Ev == Event::Decide, Result, void>::type;

  unsigned minimum_score = 60;
  unsigned recorded_score = 0;
  unsigned decisions = 0;
  bool write_succeeds = true;  // simulate committing the external decision

  struct Rules {
    static bool valid(const Result& result) noexcept {
      return result.verified && result.score <= 100;
    }

    // Both guards must accept only valid data. They partition valid scores,
    // rather than using an unconditional fallback for invalid events.
    bool operator()(guard_t<State::Reviewing, Event::Decide, State::Approved>,
                    const ReviewDemo& review, const Result& result) const {
      return valid(result) && result.score >= review.minimum_score;
    }
    bool operator()(guard_t<State::Reviewing, Event::Decide, State::Rejected>,
                    const ReviewDemo& review, const Result& result) const {
      return valid(result) && result.score < review.minimum_score;
    }

    // A single event-level hook covers both final decision edges. It runs
    // only after a guard accepts the result, before the state is committed.
    template <State F, State T>
    bool operator()(trans_t<F, Event::Decide, T>, ReviewDemo& review,
                    const Result& result) const {
      if (!review.write_succeeds) return false;
      review.recorded_score = result.score;
      ++review.decisions;
      return true;
    }

    template <State F, State T>
    bool operator()(trans_t<F, Event::Submit, T>, ReviewDemo&) const { return true; }
    template <State F, State T>
    bool operator()(trans_t<F, Event::Cancel, T>, ReviewDemo&) const { return true; }
  };

  using Hooks = jacquard::overload<Rules, jacquard::allow_all, jacquard::ignore_illegal>;
};

static_assert(ReviewDemo::validate_rulebook(), "incomplete review rulebook");

using State = ReviewDemo::State;
using Event = ReviewDemo::Event;

static bool show(const char* operation, const ReviewDemo& review, bool accepted,
                 bool expected, State state, unsigned decisions, unsigned score,
                 std::size_t rejects) {
  std::printf("%-30s %s -> %-10s decisions=%u score=%u rejects=%zu\n", operation,
              accepted ? "accepted" : "denied  ", jacquard::Review::state_name(review.state()),
              review.decisions, review.recorded_score, review.rejects());
  return accepted == expected && review.state() == state && review.decisions == decisions &&
         review.recorded_score == score && review.rejects() == rejects;
}

int main() {
  bool ok = true;
  ReviewDemo approved;
  ok &= show("Submit", approved, approved.dispatch(Event::Submit), true, State::Reviewing, 0, 0, 0);
  ok &= show("Decide (90, unverified)", approved, approved.dispatch(ReviewDemo::Result{90, false}),
             false, State::Reviewing, 0, 0, 1);
  ok &= show("Decide (101, verified)", approved, approved.dispatch(ReviewDemo::Result{101, true}),
             false, State::Reviewing, 0, 0, 2);
  approved.write_succeeds = false;
  ok &= show("Decide (60, write failed)", approved, approved.dispatch(ReviewDemo::Result{60, true}),
             false, State::Reviewing, 0, 0, 2);
  approved.write_succeeds = true;
  ok &= show("Decide (60, verified)", approved, approved.dispatch(ReviewDemo::Result{60, true}),
             true, State::Approved, 1, 60, 2);
  ok &= show("Decide after approval", approved, approved.dispatch(ReviewDemo::Result{40, true}),
             false, State::Approved, 1, 60, 3);

  ReviewDemo rejected;
  ok &= show("Submit", rejected, rejected.dispatch(Event::Submit), true, State::Reviewing, 0, 0, 0);
  ok &= show("Decide (59, verified)", rejected, rejected.dispatch(ReviewDemo::Result{59, true}),
             true, State::Rejected, 1, 59, 0);

  ReviewDemo cancelled;
  ok &= show("Submit", cancelled, cancelled.dispatch(Event::Submit), true, State::Reviewing, 0, 0, 0);
  ok &= show("Cancel", cancelled, cancelled.dispatch(Event::Cancel), true, State::Cancelled, 0, 0, 0);
  ok &= show("Late verified result", cancelled, cancelled.dispatch(ReviewDemo::Result{90, true}),
             false, State::Cancelled, 0, 0, 1);

  std::puts(ok ? "Guarded review demo passed" : "Guarded review demo FAILED");
  return ok ? 0 : 1;
}
