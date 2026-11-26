\* GENERATED from jacquard/conn.inc -- DO NOT EDIT
\* Extend Conn in a separate module for business properties.
---- MODULE Conn ----

VARIABLES state, event

States == {"Closed", "Connecting", "Open"}
Events == {"Dial", "Connected", "Drop"}
AllowedTerminals == {}

Init == state = "Closed" /\ event = "NONE"

Next ==
  \/ /\ state = "Closed" /\ state' = "Connecting" /\ event' = "Dial"
  \/ /\ state = "Connecting" /\ state' = "Open" /\ event' = "Connected"
  \/ /\ state = "Connecting" /\ state' = "Closed" /\ event' = "Drop"
  \/ /\ state = "Open" /\ state' = "Closed" /\ event' = "Drop"

TypeOK == state \in States /\ event \in (Events \cup {"NONE"})

\* Only declared terminals may lack a state-changing successor.
\* Guards are abstracted away; this is not a liveness property.
NoStuck == state \in AllowedTerminals \/ ENABLED (Next /\ state' # state)

Spec == Init /\ [][Next]_<<state, event>>
====
