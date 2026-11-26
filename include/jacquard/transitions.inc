// Single source of truth for the machine name, states, events, initial state, and edges.
//
// Multiple consumers use the same declarations:
//   SM_NAME                                  Class and TLA+ module name (jacquard::<SM_NAME>)
//   SM_STATES(X)       X(State)              States, in enum declaration order
//   SM_EVENTS(X)       X(Event)              Event alphabet
//   SM_INIT(X)         X(State)              Initial state
//   SM_TERMINALS(X)    X(State)              Allowed terminal states (optional; defaults to none)
//   SM_TRANSITIONS(X)  X(From, Event, To)    Allowed transitions; branches may share a source and event
//
// Rules:
//   * A missing (From, Event) pair or a pair whose guards all return false is
//     an illegal transition: call the illegal_t hook and count the rejection.
//   * One (From, Event) pair can have edges to several targets. Evaluate their
//     guards in declaration order; a missing non-final guard is a compile-time error.
//   * Duplicate (From, Event, To) triples fail a static assertion.
//   * Only the graph is declared here; the C++ rulebook owns guards and effects.

#define SM_NAME Job

#define SM_STATES(X) \
  X(Idle)            \
  X(Pending)         \
  X(Running)         \
  X(Done)            \
  X(Failed)          \
  X(Cancelled)

#define SM_EVENTS(X) \
  X(Submit)          \
  X(Start)           \
  X(Complete)        \
  X(Fail)            \
  X(Cancel)          \
  X(Retry)

#define SM_INIT(X) \
  X(Idle)

// A business contract, independent of which outgoing edges are present.
#define SM_TERMINALS(X) \
  X(Done)              \
  X(Cancelled)

#define SM_TRANSITIONS(X)          \
  X(Idle, Submit, Pending)         \
  X(Pending, Start, Running)       \
  X(Pending, Cancel, Cancelled)    \
  X(Running, Complete, Done)       \
  X(Running, Fail, Failed)         \
  X(Running, Cancel, Cancelled)    \
  X(Failed, Retry, Pending)        \
  X(Failed, Retry, Cancelled)
