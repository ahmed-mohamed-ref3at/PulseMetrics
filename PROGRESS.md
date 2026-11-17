# Implementation progress

## Current phase
Phases 0, 1 and 2 PASSED. Next: Phase 3 native Windows monitor and process-identity integration.

## Verified checkpoints
- Phase 0: fresh UCRT64 Debug build, five original CTest tests passed. Tag `phase0-native-passed` (`b002d27`). Toolchain GCC16.2.0, CMake/CTest4.5.0, Ninja1.13.2 at C:/msys64/ucrt64/bin. PATH changes are process-local.
- Phase 1: strict source/name protocol, immutable metric contract, bounded 1024-series store, independent arithmetic and capacity fixtures. Six CTest tests passed. Tag `phase1-contract-passed` (`3e075e9`).
- Phase 2: collector-owned WAL transactions, six-slot queue including in-flight work, retained retry batches, bounded shutdown, schema recovery, alerts and retention. Seven CTest tests passed in 5.69s. Three native-process integration tests passed in 20.11s.
- Regression A: tracking started after the last committed window survives interruption with unknown end, never a fabricated boundary.
- Regression B: alert opens in session 1 and recovers at earlier UTC in session 2; session ordering and original timestamps preserved.
- Regression E: SQLite lock fills exactly six pending slots plus frozen active store; seven accepted observations all commit exactly once after unlock. Refused lines counted. Failed shutdown reports remaining accepted-but-uncommitted sample and nonzero exit.
- Real UDP arithmetic/type/input validation, SQLite query-only failure/retry, restart history, foreign-key/integrity checks and bounded JSON round-trip passed.

## Evidence and commands
Build/test data are outside the source repository at `../PulseMetrics_V2_BUILD/`.
- `python scripts/build.py` (Debug configure, incremental compile, CTest).
- `python -m pytest tests/integration/test_collector.py -q -p no:cacheprovider --basetemp=../PulseMetrics_V2_BUILD/pytest-phase2-final`.
- Logs: `evidence/phase0-{configure,build,ctest}.log`, `phase1.log`, `phase2-build.log`, `phase2-integration.log`.
- PowerShell policy blocks .ps1 invocation; Python build entry point works without changing that policy.

## Modified modules / current limitations
V2 parser/store/clock, JSON safety adapter, schema, persistence and collector engine are implemented. Original collector/monitor/DAL targets and five tests remain temporarily as baseline references; they are not the V2 application. New runtime target is currently `pulsecollector_v2`.

No complete application claim: monitor V2, controller, four UI pages, full lifecycle, final acceptance and delivery documentation are still Not Run / not implemented. Phase 3 gate requires fresh native monitor build, real selected-process sampling, access/ended states, PID identity checks and integration with collector. Phase 4 must verify readiness before any browser client. Later gates include all four functional/visual pages and full restart/shutdown.

## Next task
Implement bounded Toolhelp discovery, exact PID+creation identity, QPC-normalized process CPU, honest unavailable/warmup states, selected-only UDP history and narrow monitor stdin/stdout control. Reuse reviewed Win32 sampling algorithms; do not retain name-grouped identities or fabricated response times. Add real-process integration tests before Phase 4.

## Preservation and Git
Only V2 and sibling build/evidence are modified. Original specification, received implementation, designs and audit artifacts remain read-only references. Local checkpoints only; no remote/publishing. Latest stable checkpoint: tag `phase2-persistence-passed` (resolve using `git rev-parse`). Original `7ec1983` and prior phase tags retained.

## Phase 3 checkpoint
Native monitor V2 and standalone C sender compile without reported warnings. Seven CTest tests pass (4.14s). `python -m pytest tests/integration/test_monitor.py -v -p no:cacheprovider --basetemp=../PulseMetrics_V2_BUILD/pytest-phase3`: 1 passed (4.83s). Real Win32 discovery, exact creation FILETIME (>2^53, transported as string), mismatched identity rejection, selected CPU/RAM history, untrack/retrack, confirmed termination and graceful native shutdown passed. PID reuse is tested through identity mismatch rather than forcing Windows to recycle a PID. Unsupported multi-group CPU topology remains explicitly unavailable; actual multi-group hardware testing is Not Run. Sender is compiled; end-to-end sender acceptance remains for integration hardening.

Current phase: Phase 4. Next: narrow named-pipe controller, child ownership/readiness, read-only DAL, Overview and Process Explorer. Stable checkpoint tag `phase3-monitor-passed`.

## Phase 4 checkpoint
Implemented current-user ACL / remote-rejecting named pipe, bounded JSON control, native child reader/acknowledgements, single-instance runtime lock, Win32 Job ownership, atomic validated settings, read-only SQLite DAL and first two Streamlit screens. Installed dependencies used as-is (no installation).

`pytest tests/test_ipc.py`: 1 passed. `pytest tests/integration/test_controller.py`: 2 passed (23.28s), including server readiness without a browser client, owned TCP listener check, port conflict refusal, live Overview rendering via AppTest, Process Explorer search and real track/untrack controls, idempotent settings, invalid setting rejection, explicit Exit and clean final native commit. AppTest is functional evidence, not visual screenshot acceptance. Evidence: `../PulseMetrics_V2_BUILD/evidence/phase4-integration.log`.

Current phase: Phase 5. Next: Analytics, Time Machine, alert controls and visual acceptance. Phase 4 stable tag `phase4-controller-passed`. Native target names still end in `_v2` until legacy baseline cleanup during final hardening.

## Phase 5 functional checkpoint
All four screens, threshold rule controls and evidence-based analytics are implemented. `pytest tests/test_analytics.py tests/integration/test_controller.py`: 3 passed (16.44s). Actual measured C sender observations populated Analytics; a native counter rule opened and recovered; Time Machine rendered stored windows and previous navigation; Alerts displayed the stored event. Evidence: `phase5-integration.log`.

Visual acceptance remains BLOCKED, not passed: browser host reports unavailable; the desktop wrapper cannot capture minimized Edge and does not expose activation. The skill's native helper reports `failed to connect native pipe ... os error 2`. No application screenshots were captured or fabricated. The independently launched application server ran successfully, accepted 1,139 real native system observations, and stopped cleanly through its local IPC with zero pending/uncommitted observations. Manual desktop acceptance will be supplied. Continue independent Phase 6 hardening; do not repeatedly retry these same graphical access methods.
