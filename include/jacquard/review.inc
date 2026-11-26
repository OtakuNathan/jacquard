// Guarded review demo: a verified result is classified by its score.

BEGIN_MACHINE(Review)
  STATE(Draft)
  STATE(Reviewing)
  STATE(Approved)
  STATE(Rejected)
  STATE(Cancelled)

  EVENT(Submit)
  EVENT(Decide)
  EVENT(Cancel)

  INIT(Draft)
  TERMINAL(Approved)
  TERMINAL(Rejected)
  TERMINAL(Cancelled)

  ON(Draft,     Submit, Reviewing)
  ON(Reviewing, Decide, Approved)
  ON(Reviewing, Decide, Rejected)
  ON(Draft,     Cancel, Cancelled)
  ON(Reviewing, Cancel, Cancelled)
END_MACHINE
