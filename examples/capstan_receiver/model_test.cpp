#include "model.hpp"

#include <cstdio>

using receiver_example::Model;
using Event = Model::Event;
using State = Model::State;
using Outcome = Model::SealOutcome;

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (false)

int main() {
  Model r;
  int validations = 0;
  const auto accept = [&] { ++validations; return true; };
  const auto reject = [&] { ++validations; return false; };
  CHECK(r.dispatch(Event::Begin));
  CHECK(r.dispatch(Event::Begin));
  CHECK(r.state() == State::Receiving && r.storage);
  CHECK(!r.seal(accept) && validations == 0);
  CHECK(!r.dispatch(Model::SealResult{Outcome::Accepted}));
  CHECK(r.dispatch(Model::Chunk{1}));
  CHECK(r.dispatch(Model::Chunk{1}));
  CHECK(r.received == 2 && !r.seal(accept) && validations == 0);
  CHECK(r.dispatch(Model::Chunk{0}));
  CHECK(r.received == 3 && !r.dispatch(Model::SealResult{Outcome::Retry}));
  CHECK(!r.seal(reject) && validations == 1 && r.state() == State::Receiving);
  CHECK(r.seal(accept) && validations == 2);
  CHECK(r.state() == State::Prepared && r.storage);
  CHECK(!r.dispatch(Model::SealResult{Outcome::Accepted}));
  CHECK(r.dispatch(Event::Release));
  CHECK(r.seal(accept) && validations == 2);
  CHECK(r.dispatch(Event::Begin));
  CHECK(r.state() == State::Prepared && !r.storage && r.received == 3);
  CHECK(r.dispatch(Event::Abort));
  CHECK(r.state() == State::Aborted && !r.storage && r.received == 0);
  CHECK(!r.dispatch(Event::Begin));
  CHECK(!r.seal(accept) && validations == 2);

  Model pending;
  CHECK(pending.dispatch(Event::Begin));
  CHECK(pending.dispatch(Model::Chunk{0}) && pending.dispatch(Model::Chunk{1}));
  bool callback_aborted = false;
  CHECK(!pending.seal([&] {
    callback_aborted = pending.dispatch(Event::Abort);
    return true;  // Even successful validation cannot commit after Abort.
  }));
  CHECK(callback_aborted && pending.state() == State::Aborted && !pending.storage);
  std::puts("Receiver abstraction: Seal guard, retries and callback Abort passed");
}
