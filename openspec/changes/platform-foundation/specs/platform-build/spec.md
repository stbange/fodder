## ADDED Requirements

### Requirement: Loose runtime content
A regular build SHALL load gameplay scripts and art from loose files next to the native package or a web asset tree.

#### Scenario: Run a packaged build
- **GIVEN** the game was built with the bundled fantasy pack
- **WHEN** a player opens it
- **THEN** the game loads the pack, shaders and UI font at runtime
