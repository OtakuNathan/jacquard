# Jacquard

**Weaving executable and verifiable state machines.**

Single-source, multi-consumer explicit state machines (C++14, header-only). One `.inc`
transition definition drives three consumers: a constexpr slot-table state machine in C++,
the rulebook (guards and side effects), and a symmetric TLA+ spec.

| Consumer of the transition definition | Result |
|---|---|
| `machine.hpp` | Dense slot table, per-edge thunks, and typed dispatch |
| Your rulebook class | Guard and transition handlers selected by edge tags |
| `tools/emit_tla.cpp` | A TLA+ transition relation and an enabled structural progress invariant |

## Design decisions

**1. One source, many consumers, multiple machines per translation unit.**
[include/jacquard/core.hpp](include/jacquard/core.hpp) holds the shared pieces (expanded once);
[include/jacquard/machine.hpp](include/jacquard/machine.hpp) **deliberately has no include guard**.
Redefine `SM_DEF` and include it again, and each pass exposes an independent
`<Name>` machine type inside `namespace jacquard`, with all DSL macros `#undef`ed at the end:

```cpp
// jacquard/conn.inc
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
```

```cpp
#define SM_DEF "jacquard/transitions.inc"
#include "jacquard/machine.hpp"   // generates jacquard::Job

#define SM_DEF "jacquard/conn.inc"
#include "jacquard/machine.hpp"   // generates jacquard::Conn
```

The public name is an alias of a generated class-template specialization so its C++14
metadata can have header-safe definitions. The `jacquard::Conn::State` and CRTP usage
patterns remain the same as before the project was renamed, but
`namespace jacquard { struct Conn; }` forward declarations must be replaced with the
generated include. Rebuild dependent translation units: the namespace rename changes
type identity and is not ABI-compatible with old object files. The `.inc` schema uses
replayable `BEGIN_MACHINE` / `STATE` / `EVENT` / `INIT` / `TERMINAL` / `ON` / `END_MACHINE`
declarations (selected via `SM_DEF`), allowing inline comments and `#ifdef` branches without
trailing backslashes.

Meta utilities (`type_list`, `type_list_size`, `element_at`, `prepend`) live in
[include/jacquard/meta.hpp](include/jacquard/meta.hpp), a vendored copy of
`portal/portal/type_list.h` under `namespace jacquard::meta`. They are copied rather than
included so that **Jacquard has zero external dependencies**; they will be extracted into a
shared base library together with the rest of the low-level pieces.

**2. Dense slot table plus per-edge thunks: value-level preprocessing, O(1) lookup.**
`ON(From, Event, To)` declarations first expand into a constexpr value array `kEdges` (trailing commas are
legal in brace initialization, so there is no sentinel and therefore no edge-count
recursion such as `strip_last`), then folds into a dense slot table `cells()[row][fanout]`
where `row = from * NE + event` and each slot stores the index of the declared edge.
`dispatch` locates one cell and tries its candidate edges in declaration order:

- **O(1) lookup, O(f) candidate checks.** There is no scan over unrelated edges. A cell with
  `f` candidates can make up to `f` indirect calls, plus the cost of its guards and hook.
  Pointer tables use `index_sequence` pack expansion rather than a recursive dispatch chain.
- **Preprocessing is not uniformly linear.** Duplicate-edge checking is O(E²), and some
  per-edge analyses rescan the table. Earlier measurements showed substantially lower
  compilation cost after replacing recursive type dispatch (576 edges: 21.1 s / 3.2 GB
  to 0.96 s / 128 MB); these are historical measurements, not a complexity guarantee or
  a benchmark of the current safety checks.
- **Dense storage, edge-only handlers.** Empty slots are `0xFFFF` and instantiate no edge
  handler, but the slot table still occupies `NS * NE * max_fanout` entries. This trades
  space for direct indexing and may be expensive for a very sparse graph.
- **C++14 ODR-safe metadata.** Public constants, edge-tag constants, and edge storage have
  header-safe template definitions; derived tables use function-local statics.
  `tests/link_odr_*.cpp` takes addresses in two translation units and checks shared identity.
- **Ordered forks.** Multiple edges for the same `(From, Event)` occupy consecutive slots;
  the first guard that returns true selects the hook and target state.

**3. Guards and side effects live in the same rulebook.** Every edge maps to two
zero-sized tag types:

- `guard_t<From, Event, To>`: takes `const Derived&` plus the declared event payload, if any,
  and returns `bool`. The reference prevents ordinary mutation through that reference;
  purity remains a rulebook obligation (`mutable`, pointers, and global state still exist);
- `trans_t<From, Event, To>`: takes `Derived&` plus an optional payload, returns `bool`.
  `true` confirms the business action succeeded and permits the selected transition to commit;
  `false` aborts the dispatch without changing the state. `void` and other return types are
  compile-time errors;
- `illegal_t`: fires when there is no declared edge, or when every candidate guard returns
  `false`. `rejects()` increments after this hook returns normally.

Compose them into `Hooks` with `jacquard::overload<...>`; overload resolution picks the
matching granularity automatically:

| Granularity | Form |
|---|---|
| exact-edge guard | `bool operator()(guard_t<State::Failed, Event::Retry, State::Pending>, const MyJob&) const` |
| exact-edge hook | `bool operator()(trans_t<State::Idle, Event::Submit, State::Pending>, MyJob&) const` |
| per-event hook | `template <State F, State T> bool operator()(trans_t<F, Event::Cancel, T>, MyJob&) const` |
| per-state hook | `template <Event Ev, State T> bool operator()(trans_t<State::Running, Ev, T>, MyJob&) const` |
| non-const fallback | `template <class Tag> bool operator()(Tag, MyJob&) const` (never captures a `const MyJob&` `guard_t`) |

> Note: if an event-level and a state-level template handler both match the same crossing
> edge, C++ overload partial ordering reports an ambiguity — add an exact-edge handler for
> that edge to disambiguate.

**4. Three explicit strictness levels, selected by which fallbacks you put into `overload`:**

- **Relaxed:** `overload<..., jacquard::no_hook, jacquard::allow_all>` — unimplemented transition
  hooks return `true` without side effects, unwritten guards default to allow.
- **Strict hooks:** `overload<..., jacquard::ignore_illegal, jacquard::allow_all>` — instantiated
  edges must have a `trans_t` handler; a missing one fails to compile.
- **Ultra-strict:** `overload<..., jacquard::ignore_illegal>` (no `allow_all`) — instantiated
  edges must also have explicit `guard_t` coverage.

Typed dispatch instantiates only that event's edges. To validate the **entire rulebook**,
including events never dispatched by a particular program, call the explicit compile-time
validator after the derived class is complete:

```cpp
static_assert(MyJob::validate_rulebook(), "incomplete rulebook");
```

It checks every declared edge using that event's canonical payload signature, as well as
the illegal-event handler. The validator does not execute guards or hooks.

Whatever the level, these are checked at compile time:

- fully duplicate `(From, Event, To)` declarations are rejected;
- when a `(From, Event)` has several fork edges, **every non-final branch must define a user
  guard** (it may not fall through to `allow_all`), otherwise a `static_assert` flags the
  default-allow branch. A user-written guard that always returns true is not detected;
- `Hooks` must be a stateless empty type (`std::is_empty`).

**5. Strict encapsulation and commit order: hook first, state last.**
`state_` and `rejects_` are `private` in `Machine`, with read access through `state()` and
`rejects()`. Hitting an edge runs **guard → `trans_t` hook → commit `state_ = To` only on `true`**.
There are three distinct outcomes:

| Outcome | `dispatch` | State | Further branches | Reject counter |
|---|---|---|---|---|
| Guard accepts and hook returns `true` | `true` | Selected target | None | Unchanged |
| Guard accepts and hook returns `false` | `false` | Original | **None** | Unchanged |
| No edge or all guards return `false` | `false` | Original | Exhausted | Increment after `illegal_t` returns |

An effect failure is not an illegal event and must not be treated as another guard rejection:
the selected hook may already have performed some work, so trying a sibling would be unsafe.
Only guard rejection advances to the next candidate. An exception in either hook path also
leaves the machine's state and rejection count unchanged and releases its dispatch-in-progress
guard. User-owned fields and external effects are not rolled back; the business hook must
return `true` only when it is ready to acknowledge completion.

- inside a hook, `state()` reports the **pre-transition** state; the target state is
  available as the compile-time constant `trans_t::to`, so it never needs to be asked for;
- dispatching **the same instance** while it is already dispatching throws `std::logic_error`
  before invoking another handler or changing the reject counter. This applies to guards,
  transition hooks, and illegal-event hooks. It prevents nested commits and state-value ABA;
- dispatching another instance is allowed. For a chain on the same instance, issue the next
  event after the first call returns. A callback may catch the reentry error and continue;
  only the outer operation can then commit.

The guard is not a lock: callers must serialize access to each machine. Copies are
independent instances with an inactive dispatch guard. Assignment to an executing machine
throws before updating its base state; callers must not replace its object lifetime in a callback.

**6. Explicit payload contracts and TLA+.**
- The inherited `template<Event Ev> using payload_type = void` means events have no payload
  by default. A derived rulebook overrides this alias to declare each payload-bearing event's
  exact type. For example, after defining `Request` in a `Conn` rulebook:

  ```cpp
  template <Event Ev>
  using payload_type = typename std::conditional<
      Ev == Event::Dial, Request, void>::type;
  ```

  `dispatch<Event::Dial>(request)` and `dispatch(request)` forward that type to both guard
  and hook (the latter requires `Request::event`). Missing or wrong payloads in typed calls
  fail to compile. A runtime `dispatch(Event::Dial)` throws `std::invalid_argument` before
  invoking a handler or changing state/counters. Payload-free events still support both
  enum and typed dispatch. Merely adding a payload-specific overload does not declare a
  payload contract; the alias is authoritative, and existing payload users must add it.
- `emit_tla` generates `VARIABLES state, event` from the same `.inc` (`event` records the
  last successful transition event, initially `"NONE"`), rendering every edge — fork branches included —
  as one disjunct of `Next`. Custom invariants belong in a separate module that does
  `EXTENDS <Name>`; never edit the generated files by hand. This is an **over-approximation**:
  in C++ the branches of a fork are evaluated in order and the first true guard wins, while
  TLA+ treats each branch as an independent nondeterministic choice. TLC checks the enabled
  properties over that abstract graph; it does not validate guard expressions or hook effects.
  A failed hook leaves the projected state and last successful event unchanged, so it is
  represented by a stuttering step in the generated spec.
- **Allowed terminals are a business declaration.** Optional `TERMINAL(State)` lines list states
  allowed to stop changing state; omitting it declares an empty set. Job explicitly lists
  `Done` and `Cancelled`; Conn has none. Missing outgoing edges never grant terminal status.
  The generated config enables `TypeOK` and this invariant:

  ```tla
  NoStuck == state \in AllowedTerminals \/ ENABLED (Next /\ state' # state)
  ```

  It rejects reachable, undeclared pure dead ends and self-loop-only sinks. An explicitly
  allowed terminal may have idempotent self-loops. `CHECK_DEADLOCK FALSE` lets valid terminal
  states exist; the enabled `NoStuck` invariant supplies the independent check.
- **This is structural potential progress, not eventual completion.** An outgoing edge may
  have a false C++ guard or a failing hook, a cycle may run forever, and `Spec` permits stuttering. Completion
  properties belong in a separate module with the protocol's actual environment/fairness
  assumptions. The generator does not invent those assumptions.

## Usage

Build and run the test suite with CMake 3.20+, a C++14 compiler, and Python 3:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

cmake --build build --target spec        # regenerate spec/Job.*, spec/Conn.*, spec/Review.*
cmake --build build --target check-spec  # fail if the checked-in specs drifted
```

`spec`, `check-spec`, and CTest's example model tests share the registrations in
[`CMakeLists.txt`](CMakeLists.txt). The output file names come from `BEGIN_MACHINE(<Name>)` in
each `.inc`, not the input file name. The compiler's header dependency tracking
rebuilds an emitter whenever its own `.inc` changes. The default build and CTest do not
rewrite checked-in specs; regeneration is an explicit target.

An upstream CMake project can use `add_subdirectory(path/to/jacquard)` and link
`jacquard::jacquard`. This interface target is header-only; consuming the library alone
does not require Python, Java, or Jacquard's example tests.

### Model checking and counterexamples

To enable TLC (Java and an existing `tla2tools.jar` are required; no automatic downloads):

```bash
cmake -S . -B build -DTLA2TOOLS_JAR=/path/to/tla2tools.jar
cmake --build build --parallel
ctest --test-dir build -L tla --output-on-failure
ctest --test-dir build -L counterexample -V
```

`TLA2TOOLS_JAR` can also be supplied as an environment variable on the initial
configure. Without a jar, CTest still runs all C++ and generator checks and CMake
reports that TLC tests are disabled. With a jar, plain CTest includes the TLC suite.
Each negative model is a separate test. A counterexample test passes only when
TLC exits with code 12 and reports the **specified invariant violation**; a syntax
error, timeout, tool failure, different invariant violation or unexpectedly safe
mutant fails the test. Each case preserves its model/config, `tlc.log`, `report.json`
and, if applicable, `counterexample.txt` under `build/tla/<test-name>/`.

| Test (`tla.*`) | Model change or property | Required result |
|---|---|---|
| `job`, `conn`, `healthy` | Unchanged graphs | Safe |
| `allowed_terminal_selfloop` | Explicit terminal has an idempotent self-loop | Safe |
| `spin`, `missing_completion` | Reachable sink, or missing completion edge | `NoStuck` counterexample |
| `selfloop_completion` | Completion returns to the same non-terminal state | `NoStuck` counterexample |
| `omitted_terminals` | A machine no longer declares its allowed terminal | `NoStuck` counterexample |
| `business_safe` | Handwritten `DoneRequiresComplete` invariant | Safe |
| `early_completion_structural_safe` | Early completion with only the default structural invariants | Safe |
| `early_completion` | Add an edge that skips working and completes early | `DoneRequiresComplete` counterexample |
| `done_witness` | Assert `NeverDone` on the healthy model | Counterexample witnesses that `Done` is reachable |
| `review`, `review_guards` | Review graph and its explicit score/result guards | Safe |
| `review_missing_guard` | Disable result-validity checking in the example model | `ReviewedWithValidResult` counterexample |
| `review_hook_failure_commits` | Allow a decision despite a failed external hook | `ReviewedAfterHook` counterexample |
| `review_approved_witness` | Assert `NeverApproved` on the guarded model | Counterexample witnesses that `Approved` is reachable |

The early-completion and completion-witness cases distinguish a broken business rule from an intentional
reachability witness. The generated `NoStuck` property alone cannot detect an
early completion edge. The extra checks live in
[`tests/models/LifecycleProperties.tla`](tests/models/LifecycleProperties.tla),
which `EXTENDS Lifecycle` rather than editing the generated module. Mutations
are compiled into emitters from the same [`lifecycle.inc`](tests/models/lifecycle.inc).

Upstream projects can reuse the same CTest helpers. After adding Jacquard as a
subdirectory and enabling CTest in the parent project:

```cmake
include(path/to/jacquard/cmake/JacquardTla.cmake)
jacquard_add_emitter(my_emitter "${CMAKE_CURRENT_SOURCE_DIR}/my_protocol.inc")
jacquard_add_tla_test(NAME protocol.safe EMITTER my_emitter
  MODULE MyProtocolProperties.tla CONFIG Safe.cfg)
jacquard_add_tla_test(NAME protocol.witness EMITTER my_emitter
  MODULE MyProtocolProperties.tla CONFIG Witness.cfg
  EXPECT_INVARIANT NeverCompleted KIND witness)
```

`jacquard_add_emitter` accepts additional compiler definitions after the `.inc` path
to construct mutant variants. `jacquard_add_tla_test` may run a generated module, a
handwritten module, or both with additional `SUPPORT_MODULES`. Tests use isolated
directories and can run independently through CTest. For larger upstream models,
set optional `HEAP` (e.g. `512m`), `WORKERS`, and `TIMEOUT` (seconds) arguments.
The standalone runner
[`tools/tla.py`](tools/tla.py) also supports `generate --help` and `check --help`.

These counterexamples are concrete paths in the **abstract graph/model**, not
necessarily feasible C++ executions. A witness to an abstract state's reachability
does not prove any actual guard, hook or environment permits that path.

For a real downstream experiment, see the standalone
[`examples/capstan_receiver`](examples/capstan_receiver/README.md) project. It
models Capstan's data receiver, shares the phase graph between an executable
Jacquard abstraction and an extended TLA+ module, and can compare selected
executions against a local Capstan checkout without changing Capstan itself.

### Rulebook

For a complete runnable guard example, see
[`examples/guarded_review/main.cpp`](examples/guarded_review/main.cpp) and its
[`jacquard/review.inc`](include/jacquard/review.inc) definition:

```bash
cmake --build build --target guarded_review
ctest --test-dir build -R 'demo\.guarded_review' -V
```

`Decide` carries an externally verified score. Both fork guards check result
validity, then compare against the review's minimum score (60 by default).
A verified passing result enters `Approved`; a verified lower score enters
`Rejected`. Unverified and out-of-range results match neither guard: the
machine stays in `Reviewing`, runs no decision hook, and increments its reject
counter. A late result after `Cancel` is also denied. The
example simulates a failed write of a verified decision: the hook returns `false`,
leaving the state and reject counter unchanged. Only a successful retry commits the decision.
The separate
[`ReviewProperties.tla`](examples/guarded_review/ReviewProperties.tla) models the
guards at threshold 60, without modifying the generated `Review.tla`.

A rulebook outline (strict hooks plus a guard fork; supply the remaining edge handlers):

```cpp
#define SM_DEF "jacquard/transitions.inc"
#include "jacquard/machine.hpp"

struct MyJob : jacquard::Job::Machine<MyJob> {
  int attempts = 0;

  struct CanRetry {
    bool operator()(guard_t<State::Failed, Event::Retry, State::Pending>,
                    const MyJob& j) const {
      return j.attempts < 2;
    }
  };
  struct OnRetryOk {
    bool operator()(trans_t<State::Failed, Event::Retry, State::Pending>,
                    MyJob& j) const {
      ++j.attempts;
      return true;
    }
  };
  // ... handlers for the remaining edges

  using Hooks = jacquard::overload<CanRetry, OnRetryOk, /* ... */,
                                jacquard::ignore_illegal, jacquard::allow_all>;
};
```

## Test layout

| File | Purpose |
|---|---|
| `tests/test_machine.cpp` | multiple machines in one TU, ordered guard forks, rejection, hook-before-commit, declared payload dispatch, `dump()` |
| `tests/test_contracts.cpp` + `contracts.inc` | exception recovery, reentry and ABA prevention, independent instances, payload rejection, and events with no edges |
| `tests/test_hook_results.cpp` + `hook_results.inc` | bool hook commit/failure, no fallback or illegal notification on hook failure, all dispatch forms, and no-exceptions mode |
| `tests/hook_wrong_result.cpp` | negative: both `void` and non-bool transition hook results are rejected |
| `tests/strict_ok.cpp` | strict and ultra-strict rulebooks, with explicit whole-rulebook validation |
| `tests/link_odr_a.cpp` + `link_odr_b.cpp` | two-TU storage identity for tables, public constants, and edge-tag constants |
| `tests/strict_missing.cpp` | negative: one missing `trans_t` must fail to compile and name `trans_t<...Failed...Retry...Cancelled>` |
| `tests/strict_missing_guard.cpp` | negative: no `allow_all` and a missing `guard_t` must fail to compile and name `guard_t<...>` |
| `tests/dead_branch.cpp` | negative: a non-final fork branch without a guard falls into `allow_all` and must trip the `non-final branch` assertion |
| `tests/payload_missing.cpp` + `payload_wrong.cpp` | negative: missing or wrong declared payloads, including event-object dispatch |
| `tests/rulebook_missing.cpp` | negative: full validation catches missing handlers even when only a different event is dispatched |
| `tests/selfloop.inc` | reachable undeclared self-loop sink; the generated invariant must fail |
| `tests/models/` | shared lifecycle definition, model mutants, upstream business invariant and reachability witness |
| `tests/compile_negative.py` | checks compiler failure against the expected diagnostic; preserves logs |
| `tools/tla.py` | spec generation, drift checks, and optional TLC checks with preserved counterexample traces and reports |

## Known limitations

- Runtime costs include the dispatch guard, payload-contract checks, and one indirect call
  per attempted candidate. The historical timings above have not been remeasured for these
  checks, and dense slot storage scales with state count, event count, and maximum fanout.
- Exception safety covers the machine's dispatch-managed state and rejection count. Hooks
  must manage rollback of their own data and external effects, for example with RAII or leases.
  When built with `-fno-exceptions` (`JACQUARD_HAS_EXCEPTIONS == 0`), contract violations (same-instance
  reentry, assignment during dispatch, or runtime enum dispatch of a payload-required event) call
  `std::abort()` instead of throwing (`ctest --test-dir build -L noexceptions` verifies this build mode on compilers supporting `-fno-exceptions`).
- Immediate same-instance reentry is rejected. The library does not provide a deferred event
  queue or synchronization between threads.
- Payload signatures and complete rulebook validation are explicit. Declare `payload_type`
  for payload events and call `validate_rulebook()` to check uninstantiated events too.
- The generated model omits payload values, guard logic, external effects, and reject counts.
  Its default checks establish type closure and structural potential progress, not business
  safety or liveness properties that have not been supplied.
