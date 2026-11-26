// Cross-TU link test (part A): catch ODR violations caused by defining a
// non-template class's static members in a header. Both TUs include machine.hpp
// and odr-use the same transition table. This must link in C++14. The old definition:
//   constexpr SM_NAME::EdgeDesc SM_NAME::kEdges[];
// passed single-TU tests but gave multiple definitions when two TUs were linked.

#include <cstdio>

#define SM_DEF "jacquard/transitions.inc"
#include "jacquard/machine.hpp"

struct LinkJob : jacquard::Job::Machine<LinkJob> {
  struct CanRetry {
    bool operator()(guard_t<State::Failed, Event::Retry, State::Pending>,
                    const LinkJob&) const {
      return true;
    }
  };
  using Hooks = jacquard::overload<CanRetry, jacquard::no_hook, jacquard::allow_all>;
};

// The other translation unit also odr-uses these tables.
int link_other_tu();
bool same_metadata_addresses(const void* const* addresses, std::size_t size);

int main() {
  LinkJob j;
  j.dispatch(LinkJob::Event::Submit);
  j.dispatch(LinkJob::Event::Start);

  const auto* es = jacquard::Job::edges();
  std::printf("link_odr: %zu edges, first=%s->%s, other=%d\n",
              jacquard::Job::kNumEdges, jacquard::Job::state_name(es[0].from),
              jacquard::Job::state_name(es[1].from), link_other_tu());

  // Scalar metadata and tags also need C++14 definitions with a shared address across TUs.
  using SM = jacquard::Job;
  using Edge = SM::trans_t<SM::State::Idle, SM::Event::Submit, SM::State::Pending>;
  using Guard = SM::guard_t<SM::State::Idle, SM::Event::Submit, SM::State::Pending>;
  const void* addresses[] = {
      &SM::NS, &SM::NE, &SM::kInitial, &SM::kNumEdges, &SM::kMaxFanout,
      &Edge::from, &Edge::event, &Edge::to,
      &Guard::from, &Guard::event, &Guard::to,
      &SM::Tab::kEdgesCount, &SM::Tab::kMaxFanout};
  const bool metadata_ok = same_metadata_addresses(
      addresses, sizeof(addresses) / sizeof(addresses[0]));
  const int bad = (j.state() != LinkJob::State::Running) ||
                  (jacquard::Job::kNumEdges != 8) || (link_other_tu() <= 0) ||
                  !metadata_ok;
  std::printf("%s\n", bad ? "link_odr FAILED" : "link_odr passed");
  return bad ? 1 : 0;
}
