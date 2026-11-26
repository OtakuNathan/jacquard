// Negative test 3: unreachable branch detection.
// Failed -Retry-> Pending is followed by Failed -Retry-> Cancelled.
// The first branch falls back to allow_all, so the second branch is unreachable.
// A static assertion must reject this at compile time.

#define SM_DEF "jacquard/transitions.inc"
#include "jacquard/machine.hpp"

struct DeadBranchJob : jacquard::Job::Machine<DeadBranchJob> {
  using Hooks = jacquard::overload<jacquard::no_hook, jacquard::allow_all>;
};

int main() {
  DeadBranchJob j;
  j.dispatch(DeadBranchJob::Event::Submit);
  return 0;
}
