## ADDED Requirements

### Requirement: Optional combat cue audio
An API 1 pack MAY provide WAV files named by the `swing`, `hit`, `obstruction`, `slash`, `overhead`, and `flank` roles in its manifest. The native and web loaders SHALL accept an API 1 pack that omits these roles and SHALL validate paths declared for them.

#### Scenario: Load a legacy pack
- **GIVEN** an API 1 pack with the original audio roles
- **WHEN** the player selects it on desktop or in the browser
- **THEN** the pack opens with the existing audio and the updated visual combat cues
