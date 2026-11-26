// Deliberately invalid model: Draining is reachable and has only a self-loop.
// It is not an allowed terminal. TLC must report an invariant NoStuck failure.
// Open has both a self-loop and a state-changing successor, so it is allowed.

BEGIN_MACHINE(Spin)
  STATE(Closed)
  STATE(Open)
  STATE(Draining)

  EVENT(Dial)
  EVENT(Reset)
  EVENT(Tick)

  INIT(Closed)

  ON(Closed,   Dial,  Open)
  ON(Open,     Tick,  Open)
  ON(Open,     Reset, Draining)
  ON(Draining, Reset, Draining)
END_MACHINE
