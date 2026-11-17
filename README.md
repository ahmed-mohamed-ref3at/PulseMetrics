# PulseMetrics V2

A local Windows performance analytics application with a genuine **C17 metrics collector**, native process monitoring, SQLite WAL history, and four Streamlit/Plotly screens.

The collector receives StatsD-like UDP observations, validates source/type/unit contracts, aggregates bounded windows, evaluates threshold rules and commits SQLite transactions. Python owns lifecycle and reads the data; it does not replace the systems programming core.

**Delivery status:** the project lead reports **15/15 manual acceptance checks passed** and explicitly approves the current UI/UX, including offline operation/startup and exit/restart with retained history. These are user-reported results, separate from the recorded automated build/test evidence. Application source and UI remain exactly as approved at `22ab0b6`; this closeout changes documentation only. The intermittent alert-to-supporting-window display observation remains an unresolved review item, alongside the existing preservation exception and SQLite warning. See [acceptance evidence and review items](docs/testing.md).

## Start

Prerequisites: Windows x64, Python (tested 3.14.6), installed dependencies, and official MSYS2 UCRT64 at `C:\msys64`. See [setup](docs/setup.md) for exact versions and installation commands. Nothing in the launcher downloads software or changes persistent PATH.

```powershell
python scripts/build.py
python scripts/launch.py
```

The launcher opens the local application only after its own server becomes healthy. User data defaults to `%LOCALAPPDATA%\PulseMetricsV2`, outside this source repository. To use a workspace-local disposable data directory:

```powershell
python scripts/launch.py --data-dir ..\PulseMetrics_V2_RUNTIME\my-session
```

Build and verify Release, then launch that build:

```powershell
python scripts/verify.py --configuration Release
python scripts/launch.py --bin-dir ..\PulseMetrics_V2_BUILD\release\bin
```

## Use

- **Overview:** live system CPU/RAM, discovered/tracked counts, stored trends, top tracked processes, recent events and collection health.
- **Process Explorer:** real discovery, search/sort, PID plus exact creation identity, track/untrack, unavailable and ended states. No controls terminate monitored programs.
- **Performance Analytics:** source/range selection, actual counters/gauges/timers, weighted statistics, threshold evidence and observed memory changes.
- **Performance Time Machine:** select real committed windows, inspect their values, navigate previous/next, compare compatible observations and retain visible gaps.
- **Alerts / Settings / Collector Health:** supporting dialogs. **Exit PulseMetrics** stops the producer, drains the collector, reports the outcome, then closes the server. Closing a browser tab alone leaves collection running.

Run the small measured sender separately (default UDP port 8125):

```powershell
..\PulseMetrics_V2_BUILD\debug\bin\pulse_sender.exe --port 8125
```

It instruments five finite checksum operations with counters, an in-flight gauge and QPC-measured milliseconds. Sending UDP is not proof of receipt: inspect collector health and `Measured metric sender` in Analytics. There are no built-in demo modes or benchmark controls.

Defaults: **5s sampling · 10s aggregation · 5s refresh · 7-day retention**. Units, gaps, partial windows and freshness are explained in the [user guide](docs/user-guide.md).

## Repository

```text
native/core/        protocol, bounded store, clocks, JSON and checked SQLite writer
native/collector/   UDP receiver, control thread, bounded FIFO writer and shutdown
native/monitor/     Win32 discovery, process identity and selected sampling
native/sender/      standalone measured instrumentation example
native/vendor/      retained SQLite and compact cJSON sources/notices
native/tests/       V2 native correctness/recovery suites
python/pulsemetrics/controller.py, child.py, ipc.py, windows.py
python/pulsemetrics/data.py, analytics.py, ui/
database/schema.sql
config/example.json
scripts/           build, launch, verify and source-only packaging
tests/             Python and native-process integration
.streamlit/        local theme and server defaults
docs/              architecture, usage, acceptance and course report
```

Builds, logs, personal process history, downloaded packages and runtime databases belong outside the distributable repository. The obsolete GTK application and temporary legacy baseline targets are absent; their reviewed baseline is preserved in Git tag `phase0-native-passed` and in the untouched received implementation.

## Verification and limits

Current V2 acceptance uses two native suites plus 14 pytest cases, including real UDP, SQLite lock/recovery, source/type separation, interrupted tracking, clock rollback, retention, selected-process lifecycle, actual UI controls through AppTest, measured sender, alerts/history, restart and failed shutdown. [Testing](docs/testing.md) distinguishes these results from visual/offline checks still requiring a desktop. Evidence includes source and executable hashes; actual logs remain in the sibling build evidence directory.

SQLite 3.45.3 is retained from the received source. GCC16.2 Release compilation emits one vendor `-Wstringop-overread` diagnostic; no corresponding runtime failure was observed in the required tests. It remains a dependency review item, not a claimed false positive or completed vulnerability assessment. Multi-processor-group CPU measurement is explicitly unavailable. Local UDP is not authenticated against other programs running as the user. V1 databases are not imported.

Read [architecture and contracts](docs/architecture.md), [course/requirements coverage](docs/course-report.md), and [third-party notices](THIRD_PARTY_NOTICES.md). No team contributions, roster or university deadline are invented. The first-party project license and publication policy require project-lead approval; nothing has been pushed or published.

Preservation review: three received legacy runtime files differ from the original baseline; source/design/audit matches and exact exceptions are recorded in [the preservation manifest](docs/evidence/preservation.json). Full-tree preservation requires project-lead review. No original was restored or baseline reset.
