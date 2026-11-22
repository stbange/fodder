# Track a thousand soldiers in each battle

## Design

The C engine assigns a stable array slot to each soldier, keeps spatial cell lists, reuses squad lane targets and staggers distant AI decisions. The renderer batches meshes and billboards independently of the simulated roster.
