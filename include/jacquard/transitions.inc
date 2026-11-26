// Single source of truth for the machine name, states, events, initial state, and edges.
//
// Multiple consumers replay this file with different macro projections:
//   BEGIN_MACHINE(Name)    Class and TLA+ module name (jacquard::<Name>)
//   STATE(Name)            States, in enum declaration order
//   EVENT(Name)            Event alphabet
//   INIT(State)            Initial state (exactly one)
//   TERMINAL(State)        Allowed terminal states (optional; defaults to none)
//   ON(From, Event, To)    Allowed transitions; branches may share a source and event
//   END_MACHINE            Closes the machine block
//
// Rules:
//   * A missing (From, Event) pair or a pair whose guards all return false is
//     an illegal transition: call the illegal_t hook and count the rejection.
//   * One (From, Event) pair can have edges to several targets. Evaluate their
//     guards in declaration order; a missing non-final guard is a compile-time error.
//   * Duplicate (From, Event, To) triples fail a static assertion.
//   * Only the graph is declared here; the C++ rulebook owns guards and effects.

BEGIN_MACHINE(Job)
  STATE(Idle)
  STATE(Pending)
  STATE(Running)
  STATE(Done)
  STATE(Failed)
  STATE(Cancelled)

  EVENT(Submit)
  EVENT(Start)
  EVENT(Complete)
  EVENT(Fail)
  EVENT(Cancel)
  EVENT(Retry)

  INIT(Idle)

  // A business contract, independent of which outgoing edges are present.
  TERMINAL(Done)
  TERMINAL(Cancelled)

  ON(Idle,    Submit,   Pending)
  ON(Pending, Start,    Running)
  ON(Pending, Cancel,   Cancelled)
  ON(Running, Complete, Done)
  ON(Running, Fail,     Failed)
  ON(Running, Cancel,   Cancelled)
  ON(Failed,  Retry,    Pending)
  ON(Failed,  Retry,    Cancelled)
END_MACHINE
