# battle-play Specification

## Purpose
Define the controls, combat resolution, and individual outcome for a first-person battle.

## Requirements

### Requirement: Personal survival
A living player SHALL survive when either army routs or the horn sounds. The game SHALL record enemy kills once per casualty.

#### Scenario: Survive an allied rout
- **GIVEN** the player's army meets the rout threshold and the player is alive
- **WHEN** the final tick resolves
- **THEN** the game records survival even though the player's army lost

### Requirement: Final-tick damage
The simulation SHALL resolve damage before checking whether the player survived.

#### Scenario: Take lethal damage at the horn
- **GIVEN** the player takes lethal damage on the final tick
- **WHEN** the outcome is resolved
- **THEN** the battle records a death and issues no survivor reward
