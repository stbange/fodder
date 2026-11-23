# Persist earned upgrades and install edited packs

## Design

Save a versioned profile per pack. A result records an unclaimed reward, and one save transaction claims it and advances the variant. Browser imports stage files before loading the Lua script and artwork. Valid packs persist in IndexedDB.
