---- MODULE LifecycleProperties ----
EXTENDS Lifecycle

\* This is a business property, not an invariant supplied by jacquard.
DoneRequiresComplete == state # "Done" \/ event = "Complete"

\* Deliberately false if the healthy machine can reach completion.
NeverDone == state # "Done"

====
