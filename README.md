# PulseMetrics V2

Local Windows performance analytics with a native C17 StatsD-like collector, native process monitoring, SQLite WAL, and a Streamlit/Plotly interface.

**Current status: Phase 0 blocked by missing native build tools. This repository is the implementation foundation, not a runnable application.** No collector/UI production modules have been implemented here yet. There is no launch command or successful fresh build to claim.

The approved product has four screens: Overview, Process Explorer, Performance Analytics and Performance Time Machine, with Alerts, Settings, Health and explicit Exit. Defaults are 5-second sampling, 10-second aggregation, 5-second refresh and 7-day retention. C remains responsible for collection, validation, aggregation, synchronization and persistence; Python reads telemetry and controls lifecycle. Only the collector writes SQLite.

Start with [setup](docs/setup.md), [progress and next task](PROGRESS.md), and [focused implementation corrections](docs/architecture.md). The approved design package and preserved audit evidence remain in sibling directories; they are not copied into this repository. Later implementation will bring the adopted contracts into the repository's documentation.

No cloud, REST application service, AI, built-in workload modes or benchmark infrastructure is planned. The external sender will measure real work. Essential arithmetic, regression, bounded-capacity, failure and integration tests are required.

The project license and publication policy remain for the project lead to decide. No remote repository is configured or publication authorized. Third-party source and notices will be added together when reuse begins; none has been copied yet. No team contribution claims are made by this foundation.
