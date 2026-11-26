// Generate a TLA+ module and TLC configuration from SM_DEF.
//   emit_tla       > spec/<SM_NAME>.tla
//   emit_tla cfg   > spec/<SM_NAME>.cfg
//   emit_tla name  prints the module name for build tooling.
//
// Each declared edge becomes a Next alternative. Guarded forks are
// nondeterministic here: this overapproximates successful C++ transitions,
// without modeling guards, payloads, hook effects, or scheduling fairness.
// The ghost variable event records the latest successful transition.
//
// SM_TERMINALS(X) is a separate business contract: these states may remain
// quiescent. Omitting it means an empty set, never inferred from missing edges.
// NoStuck checks potential structural progress from every other reachable
// state. It does not require that progress actually occurs.

#include <cstddef>
#include <cstdio>
#include <cstring>

#define JACQUARD_KEEP_MACROS
#include "jacquard/machine.hpp"

#ifndef SM_TERMINALS
#define SM_TERMINALS(X)
#endif

namespace {

void emit_cfg() {
  std::printf("SPECIFICATION Spec\n");
  std::printf("INVARIANT TypeOK\n");
  std::printf("INVARIANT NoStuck\n");
  std::printf(
      "\\* NoStuck permits only explicitly declared terminal/quiescent states\n"
      "\\* to lack a state-changing successor. Self-loops alone do not count.\n"
      "\\* This checks structural potential progress, not guards or liveness.\n"
      "\\* The invariant handles legitimate terminals; raw deadlocks are allowed.\n"
      "CHECK_DEADLOCK FALSE\n");
}

template <class SM>
void emit_terminals() {
  std::printf("AllowedTerminals == {");
  // Each macro argument must name a real enum member. The bound check also
  // rejects the enum's COUNT sentinel, which is not a declared state.
  bool first = true;
  (void)first;  // An empty terminal list is valid.
#define JACQUARD_EMIT_TERMINAL(S)                                               \
  static_assert(SM::ix(SM::State::S) < SM::NS,                                 \
                "SM_TERMINALS must name a declared state");                   \
  std::printf("%s\"%s\"", first ? "" : ", ", SM::state_name(SM::State::S)); \
  first = false;
  SM_TERMINALS(JACQUARD_EMIT_TERMINAL)
#undef JACQUARD_EMIT_TERMINAL
  std::printf("}\n\n");
}

template <class SM>
void emit_tla() {
  std::printf("\\* GENERATED from %s -- DO NOT EDIT\n", SM_DEF);
  std::printf("\\* Extend %s in a separate module for business properties.\n",
              SM::name());
  std::printf("---- MODULE %s ----\n\n", SM::name());
  std::printf("VARIABLES state, event\n\n");

  std::printf("States == {");
  for (std::size_t s = 0; s < SM::NS; ++s) {
    std::printf("%s\"%s\"", s == 0 ? "" : ", ",
                SM::state_name(static_cast<typename SM::State>(s)));
  }
  std::printf("}\n");

  std::printf("Events == {");
  for (std::size_t e = 0; e < SM::NE; ++e) {
    std::printf("%s\"%s\"", e == 0 ? "" : ", ",
                SM::event_name(static_cast<typename SM::Event>(e)));
  }
  std::printf("}\n");
  emit_terminals<SM>();

  std::printf("Init == state = \"%s\" /\\ event = \"NONE\"\n\n",
              SM::state_name(SM::kInitial));

  std::printf("Next ==\n");
  const typename SM::EdgeDesc* es = SM::edges();
  for (std::size_t i = 0; i < SM::kNumEdges; ++i) {
    std::printf(
        "  \\/ /\\ state = \"%s\" /\\ state' = \"%s\" /\\ event' = \"%s\"\n",
        SM::state_name(es[i].from), SM::state_name(es[i].to),
        SM::event_name(es[i].event));
  }
  std::printf("\n");

  std::printf(
      "TypeOK == state \\in States /\\ event \\in (Events \\cup {\"NONE\"})\n\n"
      "\\* Only declared terminals may lack a state-changing successor.\n"
      "\\* Guards are abstracted away; this is not a liveness property.\n"
      "NoStuck == state \\in AllowedTerminals \\/ ENABLED (Next /\\ state' # state)\n\n"
      "Spec == Init /\\ [][Next]_<<state, event>>\n"
      "====\n");
}

}  // namespace

int main(int argc, char** argv) {
  using Target = jacquard::SM_NAME;
  if (argc > 1 && std::strcmp(argv[1], "name") == 0) {
    std::printf("%s\n", Target::name());
    return 0;
  }
  if (argc > 1 && std::strcmp(argv[1], "cfg") == 0) {
    emit_cfg();
    return 0;
  }
  emit_tla<Target>();
  return 0;
}
