## Design

Keep the existing input actions and share strike phase between simulation and
rendering. AI chooses a fixed-time attack when it commits its facing. A heavy
strike has a narrower arc, a longer recovery, and a higher ordinary guard cost.
A timed block uses the same narrow window and low stamina cost for both strikes.

In a crowd, prefer the opponent closest to the player's aim. A counter locks its
target when pressed during the opening, and still requires reach and aim at
contact. Test the blade path for bodies and obstacles before applying damage so
the swing can visibly stop at an obstruction. A nearby enemy struck by the
player turns to face them after completing its current attack.

Show the kind and direction of attacks aimed at the player. Use stereo cues for
their position, mark the staggered opponent, and warn about flankers before they
reach melee range. Put the cue recordings in the fantasy pack as optional WAV
roles so older pack manifests and save files continue to load.
