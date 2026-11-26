// Two ordered forks, one with payloads, plus a self-loop and reset.
#define SM_NAME HookResults
#define SM_STATES(X) X(Ready) X(Primary) X(Alternative)
#define SM_EVENTS(X) X(Run) X(RunPayload) X(Reset) X(Loop)
#define SM_INIT(X) X(Ready)
#define SM_TRANSITIONS(X)                           \
  X(Ready, Run, Primary)                           \
  X(Ready, Run, Alternative)                       \
  X(Ready, RunPayload, Primary)                    \
  X(Ready, RunPayload, Alternative)                \
  X(Primary, Reset, Ready)                         \
  X(Alternative, Reset, Ready)                     \
  X(Ready, Loop, Ready)
