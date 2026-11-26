// Second example machine: redefine SM_DEF and include "jacquard/machine.hpp"
// more than once to define multiple machines in one translation unit.
// No TERMINAL(...) is declared: every state must retain a possible
// state-changing transition.

BEGIN_MACHINE(Conn)
  STATE(Closed)
  STATE(Connecting)
  STATE(Open)

  EVENT(Dial)
  EVENT(Connected)
  EVENT(Drop)

  INIT(Closed)

  ON(Closed,     Dial,      Connecting)
  ON(Connecting, Connected, Open)
  ON(Connecting, Drop,      Closed)
  ON(Open,       Drop,      Closed)
END_MACHINE
