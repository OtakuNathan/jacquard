// Second example machine: redefine SM_DEF and include "jacquard/machine.hpp"
// more than once to define multiple machines in one translation unit.

#define SM_NAME Conn

#define SM_STATES(X) \
  X(Closed)          \
  X(Connecting)      \
  X(Open)

#define SM_EVENTS(X) \
  X(Dial)            \
  X(Connected)       \
  X(Drop)

#define SM_INIT(X) \
  X(Closed)

// Every state must retain a possible state-changing transition.
#define SM_TERMINALS(X)

#define SM_TRANSITIONS(X)            \
  X(Closed, Dial, Connecting)        \
  X(Connecting, Connected, Open)     \
  X(Connecting, Drop, Closed)        \
  X(Open, Drop, Closed)
