// Phase projection of capstan::migration::transfer_receiver (one valid bound
// manifest). Seal commits a validation result; the external callback itself
// belongs outside the phase machine. Rejected events leave the phase unchanged.
#define SM_NAME CapstanReceiver
#define SM_STATES(X) X(Empty) X(Receiving) X(Prepared) X(Aborted)
#define SM_EVENTS(X) X(Begin) X(Append) X(Seal) X(Abort) X(Release)
#define SM_INIT(X) X(Empty)
#define SM_TERMINALS(X) X(Prepared) X(Aborted)

// A deliberately synthetic topology mutation used only by the TLA+ test.
#ifdef RECEIVER_RESTART_MUTANT
#define RECEIVER_RESTART_EDGE(X) X(Aborted, Begin, Receiving)
#else
#define RECEIVER_RESTART_EDGE(X)
#endif

#define SM_TRANSITIONS(X)                         \
  X(Empty, Begin, Receiving)                     \
  X(Receiving, Begin, Receiving)                 \
  X(Prepared, Begin, Prepared)                   \
  X(Receiving, Append, Receiving)                \
  X(Receiving, Seal, Prepared)                   \
  X(Prepared, Seal, Prepared)                    \
  X(Empty, Abort, Aborted)                        \
  X(Receiving, Abort, Aborted)                    \
  X(Prepared, Abort, Aborted)                     \
  X(Aborted, Abort, Aborted)                      \
  X(Prepared, Release, Prepared)                 \
  RECEIVER_RESTART_EDGE(X)
