## ADDED Requirements

### Requirement: Mobile controls
Touch browsers SHALL automatically present movement, look, strike, block and pause controls.

#### Scenario: Deploy from a coarse pointer device
- **GIVEN** the player opens the browser game on a touch device
- **WHEN** the battle begins
- **THEN** the touch overlay appears and a cancelled pointer clears its held action

### Requirement: Immersive battle
In an immersive WebXR session the game SHALL render both eyes and accept controller input for battles and upgrade selection.

#### Scenario: Leave an immersive session
- **GIVEN** the game is rendering immersive views
- **WHEN** the session ends
- **THEN** the game clears held input and returns to flat browser play
