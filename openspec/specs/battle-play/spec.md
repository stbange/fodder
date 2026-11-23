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

### Requirement: Solid living soldiers
The player and every living soldier SHALL block movement and melee contact through their body. Movement along an open side SHALL remain possible.

#### Scenario: Meet a living soldier
- **GIVEN** two living soldiers are moving toward one another
- **WHEN** their bodies meet
- **THEN** their movement stops at contact or slides along an open side

#### Scenario: Strike through an ally
- **GIVEN** an allied body stands directly between the player and an enemy
- **WHEN** the player's blade would reach the enemy
- **THEN** the ally obstructs the strike

### Requirement: Readable timed melee
A strike SHALL have visible wind-up, contact, and recovery. A new attack input SHALL start at most one strike. A well-timed frontal block SHALL open a counterstrike against the attacker.

#### Scenario: Time a guard
- **GIVEN** an enemy winds up a readable strike within reach
- **WHEN** the player raises their guard shortly before impact and counters during the opening
- **THEN** the enemy is staggered and the counter deals increased damage

#### Scenario: Step around a committed strike
- **GIVEN** an enemy has started a swing
- **WHEN** the player moves outside the swing arc before contact
- **THEN** the swing misses

### Requirement: Rear pressure
The built-in fantasy battles SHALL field flanking detachments that approach the allied rear and pressure exposed soldiers.

#### Scenario: Linger behind the line
- **GIVEN** the player remains in the allied rear
- **WHEN** a flanking detachment reaches that area
- **THEN** its soldiers can engage the player from behind the main line
