# campaign-content Specification

## Purpose
Keep survivor progression durable and allow players to author and install separate content packs.

## Requirements

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

### Requirement: Editable packs
The native game SHALL open loose packs. The web game SHALL install and persist valid ZIP packs without rebuilding the executable.

#### Scenario: Reopen an imported variant
- **GIVEN** an edited Lua pack with its own ID was imported
- **WHEN** the browser reloads and the player selects it
- **THEN** its modified rules are loaded and the base profile remains intact

### Requirement: Optional combat cue audio
An API 1 pack MAY provide WAV files named by the `swing`, `hit`, `obstruction`, `slash`, `overhead`, and `flank` roles in its manifest. The native and web loaders SHALL accept an API 1 pack that omits these roles and SHALL validate paths declared for them.

#### Scenario: Load a legacy pack
- **GIVEN** an API 1 pack with the original audio roles
- **WHEN** the player selects it on desktop or in the browser
- **THEN** the pack opens with the existing audio and the updated visual combat cues
