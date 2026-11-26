\* GENERATED from jacquard/transitions.inc -- DO NOT EDIT
\* Extend Job in a separate module for business properties.
---- MODULE Job ----

VARIABLES state, event

States == {"Idle", "Pending", "Running", "Done", "Failed", "Cancelled"}
Events == {"Submit", "Start", "Complete", "Fail", "Cancel", "Retry"}
AllowedTerminals == {"Done", "Cancelled"}

Init == state = "Idle" /\ event = "NONE"

Next ==
  \/ /\ state = "Idle" /\ state' = "Pending" /\ event' = "Submit"
  \/ /\ state = "Pending" /\ state' = "Running" /\ event' = "Start"
  \/ /\ state = "Pending" /\ state' = "Cancelled" /\ event' = "Cancel"
  \/ /\ state = "Running" /\ state' = "Done" /\ event' = "Complete"
  \/ /\ state = "Running" /\ state' = "Failed" /\ event' = "Fail"
  \/ /\ state = "Running" /\ state' = "Cancelled" /\ event' = "Cancel"
  \/ /\ state = "Failed" /\ state' = "Pending" /\ event' = "Retry"
  \/ /\ state = "Failed" /\ state' = "Cancelled" /\ event' = "Retry"

TypeOK == state \in States /\ event \in (Events \cup {"NONE"})

\* Only declared terminals may lack a state-changing successor.
\* Guards are abstracted away; this is not a liveness property.
NoStuck == state \in AllowedTerminals \/ ENABLED (Next /\ state' # state)

Spec == Init /\ [][Next]_<<state, event>>
====
