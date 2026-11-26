// Cross-TU link test (part B): odr-use all three tables so the linker catches duplicate definitions.

#define SM_DEF "jacquard/transitions.inc"
#include "jacquard/machine.hpp"

int link_other_tu() {
  const auto* es = jacquard::Job::edges();
  const auto& cells = jacquard::Job::cells();
  const auto& local = jacquard::Job::local_of();
  return static_cast<int>(jacquard::Job::kNumEdges) +
         static_cast<int>(cells.c[0][0]) + static_cast<int>(local.l[0]) +
         static_cast<int>(es[1].to) + static_cast<int>(jacquard::Job::kMaxFanout);
}

bool same_metadata_addresses(const void* const* addresses, std::size_t size) {
  using SM = jacquard::Job;
  using Edge = SM::trans_t<SM::State::Idle, SM::Event::Submit, SM::State::Pending>;
  using Guard = SM::guard_t<SM::State::Idle, SM::Event::Submit, SM::State::Pending>;
  const void* local[] = {
      &SM::NS, &SM::NE, &SM::kInitial, &SM::kNumEdges, &SM::kMaxFanout,
      &Edge::from, &Edge::event, &Edge::to,
      &Guard::from, &Guard::event, &Guard::to,
      &SM::Tab::kEdgesCount, &SM::Tab::kMaxFanout};
  if (size != sizeof(local) / sizeof(local[0])) return false;
  for (std::size_t i = 0; i < size; ++i) {
    if (addresses[i] != local[i]) return false;
  }
  return true;
}
