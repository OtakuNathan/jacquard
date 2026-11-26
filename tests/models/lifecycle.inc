// All variants share one transition definition. A mutant changes exactly one
// rule; none of these switches are part of the production example machines.

BEGIN_MACHINE(Lifecycle)
  STATE(Idle)
  STATE(Working)
  STATE(Done)

  EVENT(Start)
  EVENT(Complete)
  EVENT(Tick)

  INIT(Idle)

#if defined(JACQUARD_BAD_TERMINAL)
  TERMINAL(JACQUARD_BAD_TERMINAL)
#elif !defined(JACQUARD_OMIT_TERMINALS)
  TERMINAL(Done)
#endif

  ON(Idle, Start, Working)

#if defined(JACQUARD_MISSING_COMPLETION)
  // Intentionally omit completion to trigger NoStuck.
#elif defined(JACQUARD_SELFLOOP_COMPLETION)
  ON(Working, Complete, Working)
#else
  ON(Working, Complete, Done)
#endif

#ifdef JACQUARD_TERMINAL_SELFLOOP
  ON(Done, Tick, Done)
#endif

#ifdef JACQUARD_EARLY_COMPLETION
  ON(Idle, Start, Done)
#endif
END_MACHINE
