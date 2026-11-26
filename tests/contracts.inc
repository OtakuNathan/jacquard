// Transaction-boundary regression: Submit's hook can try Enter / Leave to recreate the ABA case.
// Spare has no edges from any state, exercising typed dispatch with an empty candidate set.
#define SM_NAME Contracts
#define SM_STATES(X) X(Idle) X(Busy) X(Done)
#define SM_EVENTS(X) X(Submit) X(Enter) X(Leave) X(Spare)
#define SM_INIT(X) X(Idle)
#define SM_TRANSITIONS(X) \
  X(Idle, Submit, Done)   \
  X(Idle, Enter, Busy)    \
  X(Busy, Leave, Idle)
