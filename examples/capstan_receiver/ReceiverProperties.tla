---- MODULE ReceiverProperties ----
EXTENDS CapstanReceiver, Naturals, FiniteSets

CONSTANTS MaxChunks, RequireComplete
VARIABLES received, validating, storage, everAborted

Chunks == 1..MaxChunks
vars == <<state, event, received, validating, storage, everAborted>>

ReceiverInit == Init /\ received = {} /\ validating = FALSE
                /\ storage = FALSE /\ everAborted = FALSE

\* Every committed C++ event is an edge in the generated base module.
\* Environment actions may change auxiliary variables while leaving the
\* base state and last committed event unchanged.
Step(e) == Next /\ event' = e

Begin == /\ Step("Begin")
         /\ storage' = IF state \in {"Empty", "Aborted"} THEN TRUE ELSE storage
         /\ UNCHANGED <<received, validating, everAborted>>

Append == /\ Step("Append")
          /\ \E chunk \in Chunks: received' = received \cup {chunk}
          /\ UNCHANGED <<validating, storage, everAborted>>

Complete == ~RequireComplete \/ received = Chunks

StartValidation == /\ state = "Receiving" /\ storage /\ ~validating /\ Complete
                   /\ validating' = TRUE
                   /\ UNCHANGED <<state, event, received, storage, everAborted>>

\* The external validator resolves successfully, fails, or returns after
\* Abort. Its outcome is an environment choice, not a computed hash.
CommitSeal == /\ Step("Seal") /\ state = "Receiving" /\ storage
              /\ validating /\ Complete /\ validating' = FALSE
              /\ UNCHANGED <<received, storage, everAborted>>

RejectValidation == /\ state \in {"Receiving", "Aborted"} /\ validating
                    /\ validating' = FALSE
                    /\ UNCHANGED <<state, event, received, storage, everAborted>>

RetrySeal == /\ Step("Seal") /\ state = "Prepared"
             /\ UNCHANGED <<received, validating, storage, everAborted>>

Abort == /\ Step("Abort") /\ everAborted' = TRUE
         /\ received' = {} /\ storage' = FALSE
         /\ UNCHANGED validating

Release == /\ Step("Release") /\ storage' = FALSE
           /\ UNCHANGED <<received, validating, everAborted>>

ReceiverNext == Begin \/ Append \/ StartValidation \/ CommitSeal \/ RejectValidation \/ RetrySeal \/ Abort \/ Release
ReceiverSpec == ReceiverInit /\ [][ReceiverNext]_vars

ReceiverTypeOK == /\ TypeOK /\ received \subseteq Chunks
                  /\ validating \in BOOLEAN /\ storage \in BOOLEAN /\ everAborted \in BOOLEAN
PreparedComplete == state = "Prepared" => (received = Chunks /\ ~validating)
AbortIsTerminal == everAborted => state = "Aborted"
AbortCleared == state = "Aborted" => (received = {} /\ ~storage)
ValidationPhase == validating => state \in {"Receiving", "Aborted"}

\* Negated reachability goals, checked only by witness configurations.
NeverPrepared == state # "Prepared"
NeverAbortDuringValidation == ~(state = "Aborted" /\ validating)
NeverOutOfOrder == ~(2 \in received /\ 1 \notin received)

====
