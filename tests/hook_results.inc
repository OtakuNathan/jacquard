// Two ordered forks, one with payloads, plus a self-loop and reset.

BEGIN_MACHINE(HookResults)
  STATE(Ready)
  STATE(Primary)
  STATE(Alternative)

  EVENT(Run)
  EVENT(RunPayload)
  EVENT(Reset)
  EVENT(Loop)

  INIT(Ready)

  ON(Ready,       Run,        Primary)
  ON(Ready,       Run,        Alternative)
  ON(Ready,       RunPayload, Primary)
  ON(Ready,       RunPayload, Alternative)
  ON(Primary,     Reset,      Ready)
  ON(Alternative, Reset,      Ready)
  ON(Ready,       Loop,       Ready)
END_MACHINE
