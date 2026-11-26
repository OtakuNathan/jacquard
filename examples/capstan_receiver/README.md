# Capstan receiver: a downstream Jacquard pilot

This is a **standalone CMake consumer**, not a dependency of Jacquard and not a
replacement for Capstan's existing implementation or formal suite. It models
the phase-level behavior of `capstan::migration::transfer_receiver` in
`include/capstan/migration/transfer.hpp` with one valid, fixed manifest, two
chunks and an abstract validation result.

## Run

From the Jacquard root:

```sh
cmake -S examples/capstan_receiver -B build/capstan-receiver \
  -DCAPSTAN_SOURCE_DIR=/path/to/capstan \
  -DTLA2TOOLS_JAR=/path/to/tla2tools.jar
cmake --build build/capstan-receiver --parallel
ctest --test-dir build/capstan-receiver --output-on-failure
ctest --test-dir build/capstan-receiver -L counterexample -V
```

`CAPSTAN_SOURCE_DIR` is optional. If given, the example builds against Capstan's
real receiver header and compares selected executions; it does not configure,
modify or build the Capstan project. Without that parameter, the abstract C++
model runs independently. Without the jar, the C++ tests still run.

## What is shared

[`receiver.inc`](receiver.inc) is the only declaration of the phase graph.
[`model.hpp`](model.hpp) supplies a strict Jacquard rulebook for the bounded
executable abstraction. [`ReceiverProperties.tla`](ReceiverProperties.tla)
extends the emitted `CapstanReceiver` module and constrains its `Next` relation
with upper-layer data and validation semantics. It does **not** restate the
source/target phase pairs; `Step(e) == Next /\ event' = e` shares those pairs
with C++.

| Capstan operation | Logical event(s) | Projection |
|---|---|---|
| `begin` | `Begin` | Allocate abstract storage on first admission; idempotent in Receiving/Prepared |
| `append` | `Append` | Add a chunk to the received set; identical duplicates do not increase it |
| `seal` | `Seal` | Validate outside dispatch; commit only if the SealResult guard accepts the result |
| `seal` on Prepared | `Seal` (retry payload) | Idempotent even after storage release |
| `abort` | `Abort` | Enter or retain Aborted; clear receiver-owned storage/received set |
| Pending validator returns after Abort | Rejected `Seal` | No Seal edge exists from Aborted; the late result cannot commit |
| `release_prepared_storage` | `Release` | Release storage but retain Prepared evidence |

Capstan can run `abort()` inside the validator that `seal()` invokes, whereas
Jacquard dispatch is run-to-completion. The executable abstraction therefore
invokes the callback **before** dispatching `Seal` with a `SealResult` payload.
The `Receiving -> Prepared` guard requires an accepted result and complete
data; the Prepared self-loop accepts only an actual retry, not a delayed
validation result. There is no Seal edge from Aborted. The callback itself
is not a guard or a hook, so it can call Abort without violating Jacquard's
reentry contract.

TLA+ can model finer-grained timing without adding C++ events. Its
`StartValidation` and `RejectValidation` actions update the auxiliary
`validating` variable while leaving the generated `state` and `event`
unchanged. `CommitSeal` uses the generated Seal edge and the upper-layer
guard. Thus validation is visible to the checker but not an extra phase or
event in the C++ state machine.

## Model verification

| CTest | Expected outcome |
|---|---|
| `receiver.safe` | `ReceiverTypeOK`, `NoStuck`, complete preparation, terminal Abort, cleared Abort data, valid validation phase |
| `receiver.early_seal_mutant` | Synthetic configuration disabling the all-chunks guard violates `PreparedComplete` |
| `receiver.restart_mutant` | Synthetic extra topology edge violates `AbortIsTerminal` |
| `receiver.prepared_witness` | `NeverPrepared` is false: prepared state is reachable |
| `receiver.abort_during_validation_witness` | `NeverAbortDuringValidation` is false: a pending validation and Abort can overlap |
| `receiver.out_of_order_witness` | `NeverOutOfOrder` is false: chunk 2 may arrive before chunk 1 |

The actual TLC-generated counterexamples, inputs and reports are saved under
`build/capstan-receiver/tla/<test-name>/`. The two deliberately broken models
are confined to this example; they are not changes to Capstan.

The optional `receiver.correspondence` test compares the real receiver's public
phase, received-byte count, contiguous-byte prefix and prepared-data presence
with the abstract Jacquard instance after selected operations. It also checks a
reentrant Abort from the real validator while the callback retains its input.

## Boundary of the pilot

This is a **control-flow and two-chunk abstraction**, not a refinement proof or
a complete replacement for Capstan's `DataTransfer` or `WindowedDataTransfer`
models. It assumes one accepted manifest and identical duplicate data, and omits
identity/transport validation, capacity and allocation failures, real digest
computation, conflicting duplicate content, concurrent callers, authority
migration, and multiple simultaneous validators. Rejected operations with no
observable abstract effect are stuttering. Validation success/failure is an
environment choice; no fairness or progress guarantee is claimed. C++ and TLA+
business guards are still separately written and reviewed; only the transition
graph is shared automatically.
