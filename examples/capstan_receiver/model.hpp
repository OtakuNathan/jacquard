#pragma once

#define SM_DEF "receiver.inc"
#include "jacquard/machine.hpp"

// A small executable abstraction of the two-chunk receiver. It is not a
// replacement for Capstan's transfer_receiver: byte content, manifest checking,
// allocation, digest and the external validator are outside this projection.
namespace receiver_example {
struct Model : jacquard::CapstanReceiver::Machine<Model> {
  struct Chunk {
    static constexpr Event event = Event::Append;
    unsigned index;
  };
  enum class SealOutcome { Accepted, Rejected, Retry };
  struct SealResult {
    static constexpr Event event = Event::Seal;
    SealOutcome outcome;
  };
  template <Event Ev>
  using payload_type = typename std::conditional<
      Ev == Event::Append, Chunk,
      typename std::conditional<Ev == Event::Seal, SealResult, void>::type>::type;

  unsigned received = 0;  // set of two abstract chunks (bits 0 and 1)
  bool storage = false;

  bool ready_for_validation() const noexcept {
    return state() == State::Receiving && received == 3 && storage;
  }

  template <class Validate>
  bool seal(Validate&& validate) {
    if (state() == State::Prepared) {
      return dispatch(SealResult{SealOutcome::Retry});
    }
    if (!ready_for_validation()) return false;
    // Do not call external code inside dispatch or a guard. Abort may run
    // during validation; the subsequent dispatch checks the current phase.
    const bool accepted = static_cast<bool>(std::forward<Validate>(validate)());
    return dispatch(SealResult{accepted ? SealOutcome::Accepted : SealOutcome::Rejected});
  }

  struct Rules {
    template <State F, State T>
    bool operator()(trans_t<F, Event::Begin, T>, Model& m) const {
      if (F == State::Empty) m.storage = true;
      return true;
    }

    template <State F, State T>
    bool operator()(guard_t<F, Event::Append, T>, const Model&, const Chunk& c) const {
      return c.index < 2;
    }
    template <State F, State T>
    bool operator()(trans_t<F, Event::Append, T>, Model& m, const Chunk& c) const {
      m.received |= 1u << c.index;
      return true;
    }

    bool operator()(guard_t<State::Receiving, Event::Seal, State::Prepared>,
                    const Model& m, const SealResult& result) const {
      return result.outcome == SealOutcome::Accepted && m.ready_for_validation();
    }
    bool operator()(guard_t<State::Prepared, Event::Seal, State::Prepared>,
                    const Model&, const SealResult& result) const {
      // A fresh retry is not a delayed result from a previously started Seal.
      return result.outcome == SealOutcome::Retry;
    }
    template <State F, State T>
    bool operator()(trans_t<F, Event::Seal, T>, Model&, const SealResult&) const { return true; }

    template <State F, State T>
    bool operator()(trans_t<F, Event::Abort, T>, Model& m) const {
      m.received = 0;
      m.storage = false;
      return true;
    }
    template <State F, State T>
    bool operator()(trans_t<F, Event::Release, T>, Model& m) const {
      m.storage = false;
      return true;
    }
  };

  using Hooks = jacquard::overload<Rules, jacquard::ignore_illegal, jacquard::allow_all>;
};

static_assert(Model::validate_rulebook(), "every receiver edge needs a rule");
}  // namespace receiver_example
