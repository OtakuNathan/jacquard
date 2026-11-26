// Transaction-boundary regression: Submit's hook can try Enter / Leave to recreate the ABA case.
// Spare has no edges from any state, exercising typed dispatch with an empty candidate set.

BEGIN_MACHINE(Contracts)
  STATE(Idle)
  STATE(Busy)
  STATE(Done)

  EVENT(Submit)
  EVENT(Enter)
  EVENT(Leave)
  EVENT(Spare)

  INIT(Idle)

  ON(Idle, Submit, Done)
  ON(Idle, Enter,  Busy)
  ON(Busy, Leave,  Idle)
END_MACHINE
