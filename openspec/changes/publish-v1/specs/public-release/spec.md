## ADDED Requirements

### Requirement: Deployment gate
The release SHALL publish the tested WebAssembly and loose asset artifact from main after checks pass.

#### Scenario: Deploy a passing revision
- **GIVEN** the Pages source is GitHub Actions and the checks pass on main
- **WHEN** the deployment runs
- **THEN** the public site serves the game and builds successfully load assets under its project path

### Requirement: Separate art license
The distribution SHALL include the MIT code license, CC BY 4.0 art attribution and notices for bundled dependencies.

#### Scenario: Inspect the release package
- **GIVEN** a native or web package was created
- **WHEN** a user reads its credits and license files
- **THEN** code and original art licenses are identified
