## ADDED Requirements

### Requirement: Individual roster
The standard battle SHALL begin with exactly 500 soldiers per side, including the player as one of 500 allies.

#### Scenario: Deploy into the full battle
- **GIVEN** the standard fantasy encounter was selected
- **WHEN** the player deploys
- **THEN** the battlefield has 499 AI allies, 500 AI enemies and the player

### Requirement: Off-screen casualties
Distant soldiers SHALL retain their team, position, health and death state when their decision cadence changes.

#### Scenario: Fight outside the player's view
- **GIVEN** allied and enemy formations meet off-screen
- **WHEN** the simulation runs for several seconds
- **THEN** individual movement and casualties are recorded and team counts match the surviving roster
