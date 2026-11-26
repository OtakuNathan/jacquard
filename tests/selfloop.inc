// Deliberately invalid model: Draining is reachable and has only a self-loop.
// It is not an allowed terminal. TLC must report an invariant NoStuck failure.
// Open has both a self-loop and a state-changing successor, so it is allowed.

#define SM_NAME Spin

#define SM_STATES(X) \
  X(Closed)          \
  X(Open)            \
  X(Draining)

#define SM_EVENTS(X) \
  X(Dial)          \
  X(Reset)         \
  X(Tick)

#define SM_INIT(X) \
  X(Closed)

#define SM_TERMINALS(X)

#define SM_TRANSITIONS(X)  \
  X(Closed, Dial, Open)     \
  X(Open, Tick, Open)       \
  X(Open, Reset, Draining)  \
  X(Draining, Reset, Draining)
