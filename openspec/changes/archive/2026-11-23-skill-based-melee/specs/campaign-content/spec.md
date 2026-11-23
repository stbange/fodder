## MODIFIED Requirements

### Requirement: Permanent survivor rewards
After surviving the game SHALL let the player claim exactly one permanent upgrade. All earned ranks SHALL remain after death. A survivor with at least two kills or successful counters, counted once per strike, SHALL receive two ranks in their chosen upgrade.

#### Scenario: Reload after an upgrade and death
- **GIVEN** the player survived and claimed a rank
- **WHEN** the player dies and opens the same profile
- **THEN** the earned rank is active and the previous reward is unavailable

#### Scenario: Recover a bonus rank choice
- **GIVEN** the player survives after earning at least two combat deeds
- **WHEN** the player reloads and claims their pending reward
- **THEN** the selected upgrade gains two ranks and the same reward cannot be claimed twice
