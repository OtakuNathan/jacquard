// Phase projection of capstan::migration::transfer_receiver (one valid bound
// manifest). Seal commits a validation result; the external callback itself
// belongs outside the phase machine. Rejected events leave the phase unchanged.

BEGIN_MACHINE(CapstanReceiver)
  STATE(Empty)
  STATE(Receiving)
  STATE(Prepared)
  STATE(Aborted)

  EVENT(Begin)
  EVENT(Append)
  EVENT(Seal)
  EVENT(Abort)
  EVENT(Release)

  INIT(Empty)
  TERMINAL(Prepared)
  TERMINAL(Aborted)

  // Begin / Append
  ON(Empty,     Begin,   Receiving)
  ON(Receiving, Begin,   Receiving)
  ON(Prepared,  Begin,   Prepared)
  ON(Receiving, Append,  Receiving)

  // Seal
  ON(Receiving, Seal,    Prepared)
  ON(Prepared,  Seal,    Prepared)

  // Abort
  ON(Empty,     Abort,   Aborted)
  ON(Receiving, Abort,   Aborted)
  ON(Prepared,  Abort,   Aborted)
  ON(Aborted,   Abort,   Aborted)

  // Release
  ON(Prepared,  Release, Prepared)

  // A deliberately synthetic topology mutation used only by the TLA+ test.
#ifdef RECEIVER_RESTART_MUTANT
  ON(Aborted,   Begin,   Receiving)
#endif
END_MACHINE
