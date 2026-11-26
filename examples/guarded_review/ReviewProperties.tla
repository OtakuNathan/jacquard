---- MODULE ReviewProperties ----
EXTENDS Review, Naturals

CONSTANTS RequireValid, RequireHookSuccess
VARIABLES verified, score, applied

vars == <<state, event, verified, score, applied>>
Reviewed == state \in {"Approved", "Rejected"}
ReviewInit == Init /\ verified = FALSE /\ score = 0 /\ applied = FALSE
Step(e) == Next /\ event' = e

SubmitOrCancel == /\ (Step("Submit") \/ Step("Cancel"))
                  /\ UNCHANGED <<verified, score, applied>>

\* Model the two C++ guards on top of the generated fork. The bounded
\* domain includes an out-of-range score. A rejected input or effect failure
\* does not commit the decision and is represented by stuttering.
Decide == /\ Step("Decide")
          /\ \E v \in BOOLEAN, s \in 0..101, written \in BOOLEAN:
              /\ (~RequireValid \/ (v /\ s <= 100))
              /\ (~RequireHookSuccess \/ written)
              /\ (IF s >= 60 THEN state' = "Approved" ELSE state' = "Rejected")
              /\ verified' = v /\ score' = s /\ applied' = written

ReviewNext == SubmitOrCancel \/ Decide
ReviewSpec == ReviewInit /\ [][ReviewNext]_vars

ReviewTypeOK == TypeOK /\ verified \in BOOLEAN /\ score \in 0..101 /\ applied \in BOOLEAN
ReviewedWithValidResult == Reviewed => (verified /\ score <= 100)
ReviewedAfterHook == Reviewed => applied
ApprovedAboveThreshold == state = "Approved" => score >= 60
RejectedBelowThreshold == state = "Rejected" => score < 60
NeverApproved == state # "Approved"

====
