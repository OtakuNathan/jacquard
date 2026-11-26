#include "model.hpp"
#include <capstan/migration/transfer.hpp>

#include <cstdio>

namespace migration = capstan::migration;
using capstan::status;
using receiver_example::Model;
using Event = Model::Event;
using State = Model::State;
using Receiver = migration::transfer_receiver;
using Phase = Receiver::phase;

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (false)

static migration::manifest example_manifest() {
  migration::manifest m;
  m.transaction = {7, 8};
  m.object = {1, 2, 3};
  m.source_host = 1;
  m.target_host = 2;
  m.source_instance = 11;
  m.target_instance = 22;
  m.generation = 1;
  m.size = Receiver::chunk_bytes + 7;
  return m;
}

static bool agrees(const Receiver& real, const Model& model) {
  Phase expected = Phase::empty;
  switch (model.state()) {
    case State::Empty: expected = Phase::empty; break;
    case State::Receiving: expected = Phase::receiving; break;
    case State::Prepared: expected = Phase::prepared; break;
    case State::Aborted: expected = Phase::aborted; break;
    case State::COUNT: return false;
  }
  const auto bytes = ((model.received & 1) ? Receiver::chunk_bytes : 0) +
                     ((model.received & 2) ? 7 : 0);
  const auto prefix = (model.received & 1) ? bytes : 0;
  return real.state() == expected && real.received() == bytes &&
         real.contiguous_bytes() == prefix &&
         (bool(real.prepared_data()) == (model.state() == State::Prepared && model.storage));
}

int main() {
  const auto id = example_manifest();
  const migration::buffer first(Receiver::chunk_bytes, 42), last(7, 17);
  const auto accept = [](const migration::buffer&, const std::array<std::uint8_t, 32>&) noexcept { return status::ok; };
  const auto reject = [](const migration::buffer&, const std::array<std::uint8_t, 32>&) noexcept { return status::invalid; };
  const auto model_accept = [] { return true; };
  const auto model_reject = [] { return false; };

  Receiver real;
  Model model;
  CHECK(agrees(real, model));
  CHECK(real.begin(id) == status::ok && model.dispatch(Event::Begin) && agrees(real, model));
  CHECK(real.begin(id) == status::ok && model.dispatch(Event::Begin) && agrees(real, model));
  CHECK(real.seal(id, accept) == status::pending && !model.seal(model_accept) && agrees(real, model));
  CHECK(real.append(id, first.size(), last) == status::ok && model.dispatch(Model::Chunk{1}) && agrees(real, model));
  CHECK(real.append(id, first.size(), last) == status::ok && model.dispatch(Model::Chunk{1}) && agrees(real, model));
  CHECK(real.seal(id, accept) == status::pending && !model.seal(model_accept) && agrees(real, model));
  CHECK(real.append(id, 0, first) == status::ok && model.dispatch(Model::Chunk{0}) && agrees(real, model));
  CHECK(real.seal(id, reject) == status::invalid && !model.seal(model_reject) && agrees(real, model));
  CHECK(real.seal(id, accept) == status::ok && model.seal(model_accept) && agrees(real, model));
  real.release_prepared_storage();
  CHECK(model.dispatch(Event::Release) && agrees(real, model));
  CHECK(real.seal(id, accept) == status::ok && model.seal(model_accept) && agrees(real, model));
  CHECK(real.begin(id) == status::ok && model.dispatch(Event::Begin) && agrees(real, model));
  CHECK(real.abort(id) == status::ok && model.dispatch(Event::Abort) && agrees(real, model));
  CHECK(real.begin(id) == status::closed && !model.dispatch(Event::Begin) && agrees(real, model));

  Receiver early;
  Model early_model;
  CHECK(early.abort(id) == status::ok && early_model.dispatch(Event::Abort) && agrees(early, early_model));
  CHECK(early.begin(id) == status::closed && !early_model.dispatch(Event::Begin) && agrees(early, early_model));

  // Capstan intentionally permits Abort from the validator callback. The
  // abstraction invokes the validator before dispatching Seal, so it can
  // also accept Abort and then reject the successful but obsolete result.
  Receiver deferred;
  Model deferred_model;
  CHECK(deferred.begin(id) == status::ok && deferred_model.dispatch(Event::Begin));
  CHECK(deferred.append(id, 0, first) == status::ok && deferred_model.dispatch(Model::Chunk{0}));
  CHECK(deferred.append(id, first.size(), last) == status::ok && deferred_model.dispatch(Model::Chunk{1}));
  bool callback_matched = false;
  const auto result = deferred.seal(id, [&](const migration::buffer& held, const std::array<std::uint8_t, 32>&) noexcept {
    callback_matched = deferred.abort(id) == status::ok && held.size() == id.size;
    return status::ok;
  });
  CHECK(callback_matched && result == status::closed);
  bool model_aborted = false;
  CHECK(!deferred_model.seal([&] {
    model_aborted = deferred_model.dispatch(Event::Abort);
    return true;
  }));
  CHECK(model_aborted && agrees(deferred, deferred_model) && deferred_model.state() == State::Aborted);
  std::puts("Capstan correspondence: selected receiver transitions and observations match");
}
