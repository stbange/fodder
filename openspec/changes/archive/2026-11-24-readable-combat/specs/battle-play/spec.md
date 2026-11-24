## MODIFIED Requirements

### Requirement: Readable timed melee
Strikes SHALL have visible wind-up, contact, and recovery. AI SHALL use a slash or overhead strike with fixed timing and distinct tells. Each attack input SHALL start at most one strike. A timed frontal block SHALL expose the attacker. A counter begun in the opening SHALL retain that enemy through contact while in reach and aim. Holding guard SHALL cost more stamina against an overhead strike than a slash.

#### Scenario: Time a guard
- **GIVEN** an enemy winds up a readable strike within reach
- **WHEN** the player raises their guard shortly before impact and counters during the opening
- **THEN** the enemy is staggered and the counter deals increased damage

#### Scenario: Step around a committed strike
- **GIVEN** an enemy has started a swing
- **WHEN** the player moves outside the swing arc before contact
- **THEN** the swing misses

#### Scenario: Counter the staggered attacker in a crowd
- **GIVEN** the staggered attacker is within reach and a second enemy is closer but outside the blade path
- **WHEN** the player aims and swings during the opening
- **THEN** the first enemy takes the counterstrike

### Requirement: Rear pressure
The built-in fantasy battles SHALL field flanking detachments that approach the allied rear and pressure exposed soldiers. The player SHALL get a directional warning when a detachment approaches within warning range before attempting melee contact.

#### Scenario: Linger behind the line
- **GIVEN** the player remains in the allied rear
- **WHEN** a flanking detachment reaches that area
- **THEN** a warning indicates the direction before it engages the player

## ADDED Requirements

### Requirement: Legible sword contact and threats
An ordinary strike SHALL favor the enemy under the player's aim within reach. A living body or ruin in the blade path SHALL stop the blade. The game SHALL distinguish clean hits, misses, shield hits, and obstructions in its feedback. An enemy winding up against the player SHALL show the attack kind and direction through a visual cue and positional sound when cue audio is available.

#### Scenario: Provoke a nearby enemy
- **GIVEN** a soldier is striking an ally
- **WHEN** the player hits that soldier and remains close by
- **THEN** the enemy can turn to confront the player after the committed swing

#### Scenario: Aim past a closer opponent
- **GIVEN** a closer enemy is to the side of the aim and another enemy is centered within reach
- **WHEN** the player strikes with a clear blade path
- **THEN** the centered enemy takes the hit

#### Scenario: Obstruct a counter
- **GIVEN** the player has opened a counterstrike and a living body steps directly into the blade path
- **WHEN** the player strikes at the staggered enemy
- **THEN** the sword stops at the obstruction and the enemy takes no damage from that swing
