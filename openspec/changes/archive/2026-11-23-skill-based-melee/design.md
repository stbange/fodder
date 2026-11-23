## Design

Update the existing spatial grid when a soldier changes cells. Both teams and the
player share the same movement collision and a short melee line-of-contact check.
Soldiers can sidestep blocked paths. A battle pack can assign some enemies to
flanking routes while keeping the full roster fixed.

A press starts one strike with a wind-up, contact tick, and recovery. A block
raised shortly before a frontal hit staggers the attacker and opens a brief
counter window. The display uses the simulation's strike phase for both the
first-person sword and marked sword vertices in nearby soldier models.

Count a kill or a landed counter as one deed. Two deeds double the ranks granted
by the chosen survivor upgrade. Persist the earned rank count with the pending
reward so reloading and retrying a save preserve the same claim.
