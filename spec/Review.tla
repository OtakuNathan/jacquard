\* GENERATED from jacquard/review.inc -- DO NOT EDIT
\* Extend Review in a separate module for business properties.
---- MODULE Review ----

VARIABLES state, event

States == {"Draft", "Reviewing", "Approved", "Rejected", "Cancelled"}
Events == {"Submit", "Decide", "Cancel"}
AllowedTerminals == {"Approved", "Rejected", "Cancelled"}

Init == state = "Draft" /\ event = "NONE"

Next ==
  \/ /\ state = "Draft" /\ state' = "Reviewing" /\ event' = "Submit"
  \/ /\ state = "Reviewing" /\ state' = "Approved" /\ event' = "Decide"
  \/ /\ state = "Reviewing" /\ state' = "Rejected" /\ event' = "Decide"
  \/ /\ state = "Draft" /\ state' = "Cancelled" /\ event' = "Cancel"
  \/ /\ state = "Reviewing" /\ state' = "Cancelled" /\ event' = "Cancel"

TypeOK == state \in States /\ event \in (Events \cup {"NONE"})

\* Only declared terminals may lack a state-changing successor.
\* Guards are abstracted away; this is not a liveness property.
NoStuck == state \in AllowedTerminals \/ ENABLED (Next /\ state' # state)

Spec == Init /\ [][Next]_<<state, event>>
====
