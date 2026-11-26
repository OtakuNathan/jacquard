// Guarded review demo: a verified result is classified by its score.
#define SM_NAME Review
#define SM_STATES(X) X(Draft) X(Reviewing) X(Approved) X(Rejected) X(Cancelled)
#define SM_EVENTS(X) X(Submit) X(Decide) X(Cancel)
#define SM_INIT(X) X(Draft)
#define SM_TERMINALS(X) X(Approved) X(Rejected) X(Cancelled)

#define SM_TRANSITIONS(X)             \
  X(Draft, Submit, Reviewing)         \
  X(Reviewing, Decide, Approved)      \
  X(Reviewing, Decide, Rejected)      \
  X(Draft, Cancel, Cancelled)         \
  X(Reviewing, Cancel, Cancelled)
