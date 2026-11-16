# Implementation progress

## Current phase
Phases 0 and 1 PASSED. Phase 2 in progress: collector-owned checked persistence and lifecycle recovery.

## Verified baseline
- Toolchain accessible at C:/msys64/ucrt64/bin: GCC16.2.0, CMake/CTest4.5.0, Ninja1.13.2. PATH is process-local only.
- Reviewed native core/collector/monitor, C DAL and five tests copied into this repository; SQLite/cJSON notices preserved. No GTK, caches, binaries or runtime databases copied.
- Fresh Debug configuration and compilation succeeded. Resume check: `cmake --build ../PulseMetrics_V2_BUILD/debug --parallel 2` reported no work to do.
- `ctest --test-dir ../PulseMetrics_V2_BUILD/debug --output-on-failure`: **Passed5/5**, including actual UDP/SQLite integration,4.76s total.
- Exact logs: sibling `PulseMetrics_V2_BUILD/evidence/phase0-{configure,build,ctest}.log`; build/test data stay outside source repository.
- PowerShell policy blocks invoking .ps1 files; direct native commands work. No policy was changed. A Python build entry point avoids requiring script-policy changes.

## Checkpoints and next task
Stable baseline tag: `phase0-native-passed`. Original blocked checkpoint7ec1983 remains in Git. No remote configured.
Next: implement source/name strict parser, immutable catalog types/units, bounded complete store and actual-duration math. Add deterministic tests before the persistence phase. Original five tests establish baseline behavior only; they do not prove V2 correctness.

## Acceptance remaining
Phase1 strict protocol/store; Phase2 checked writer/alerts/retention and corrections A/B/E; Phase3 monitor/IPC; Phase4 controller/readiness correction C, DAL and first screens; Phase5 complete product/UX; Phase6 failure/integration/visual acceptance; Phase7 delivery documentation. All unexecuted V2 tests remain Not Run. No finished application claim.

## Preservation
Original implementation/specification, design and audit artifacts are read-only references. Only V2 and sibling build/evidence directories have been written in this resumed implementation.

## Phase 1 checkpoint
Strict source/name parser, bounded1024-series store, type/unit checks and monotonic clock utilities implemented. `python scripts/build.py`: **6/6 CTest passed** (5.07s), including independent V2 numeric/source/capacity cases. Evidence: `../PulseMetrics_V2_BUILD/evidence/phase1.log`. Build helper corrected to resolve native tool executables by absolute path on Windows; no environment setting changed. Stable tag: `phase1-contract-passed`. Next: new schema with A/B corrections, single writer/retry queue with E accounting, and real failure integration tests.
