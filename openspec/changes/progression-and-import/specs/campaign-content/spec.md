## ADDED Requirements

### Requirement: Permanent survivor rewards
After surviving the game SHALL let the player claim exactly one permanent upgrade. All earned ranks SHALL remain after death.

#### Scenario: Reload after an upgrade and death
- **GIVEN** the player survived and claimed a rank
- **WHEN** the player dies and opens the same profile
- **THEN** the earned rank is active and the previous reward is unavailable

### Requirement: Editable packs
The native game SHALL open loose packs. The web game SHALL install and persist valid ZIP packs without rebuilding the executable.

#### Scenario: Reopen an imported variant
- **GIVEN** an edited Lua pack with its own ID was imported
- **WHEN** the browser reloads and the player selects it
- **THEN** its modified rules are loaded and the base profile remains intact
