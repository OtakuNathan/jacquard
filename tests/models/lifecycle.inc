// All variants share one transition definition. A mutant changes exactly one
// rule; none of these switches are part of the production example machines.
#define SM_NAME Lifecycle
#define SM_STATES(X) X(Idle) X(Working) X(Done)
#define SM_EVENTS(X) X(Start) X(Complete) X(Tick)
#define SM_INIT(X) X(Idle)

#if defined(JACQUARD_BAD_TERMINAL)
#define SM_TERMINALS(X) X(JACQUARD_BAD_TERMINAL)
#elif !defined(JACQUARD_OMIT_TERMINALS)
#define SM_TERMINALS(X) X(Done)
#endif

#if defined(JACQUARD_MISSING_COMPLETION)
#define LIFECYCLE_COMPLETE(X)
#elif defined(JACQUARD_SELFLOOP_COMPLETION)
#define LIFECYCLE_COMPLETE(X) X(Working, Complete, Working)
#else
#define LIFECYCLE_COMPLETE(X) X(Working, Complete, Done)
#endif

#ifdef JACQUARD_TERMINAL_SELFLOOP
#define LIFECYCLE_TICK(X) X(Done, Tick, Done)
#else
#define LIFECYCLE_TICK(X)
#endif

#ifdef JACQUARD_EARLY_COMPLETION
#define LIFECYCLE_EARLY(X) X(Idle, Start, Done)
#else
#define LIFECYCLE_EARLY(X)
#endif

#define SM_TRANSITIONS(X) \
  X(Idle, Start, Working) \
  LIFECYCLE_COMPLETE(X)   \
  LIFECYCLE_TICK(X)       \
  LIFECYCLE_EARLY(X)
