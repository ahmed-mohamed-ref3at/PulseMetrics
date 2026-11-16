# Approved architecture: focused implementation corrections

The approved hybrid design remains the baseline: native C17 receiver/aggregation/persistence, native Windows monitoring, collector-only SQLite writer, read-only Python data/analytics, one Python controller and four Streamlit/Plotly screens. This document records the project lead's five corrections for implementation. **None is implemented or verified in this Phase 0 foundation.** No alternative architecture or additional service is introduced.

## A. Interrupted tracking is uncertain, not a fabricated end

Give tracking intervals an explicit state: `active`, `closed`, or `interrupted`. A closed interval requires a known end >= its start. An interrupted interval may have an unknown (`NULL`) end and records its recovery reason separately. The unique active-selection constraint applies to state `active`, not simply `end IS NULL`.

On recovery, mark prior active intervals `interrupted`; do not use the prior session's last committed boundary as a claimed process end. Preserve an optional last-observed time only if a real observation exists inside that interval. A selection that began after the last committed window therefore has no observed end or last sample. A new interval for a revalidated surviving identity is allowed. Historical UI labels uncertain duration; it does not display zero-duration or negative-duration tracking.

Required Phase 2/3 regression: session commits a window, starts tracking later, then is interrupted before another commit. Restart must keep the interval start, store unknown end/interrupted state, preserve schema validity and allow a new active interval without inventing observations.

## B. Wall-clock rollback does not determine lifecycle order

Keep QPC-derived, session-anchored measurement timestamps unchanged. Give sessions a durable monotonically allocated integer order (SQLite AUTOINCREMENT session key, with UUID retained as external identity). Alert lifecycle records refer to the opening, latest-evaluation and closure session orders. Within one session, QPC-derived times are ordered; across sessions, session order defines lifecycle order even if UTC decreased.

Replace cross-session `last_evaluated_utc >= opened_utc` / `closed_utc >= opened_utc` constraints with checks on session order, and require the timestamp ordering only when the session order is equal. Retain the actual UTC timestamps; do not clamp, offset or rewrite them to make old checks pass. Persist a visible rollback warning on the new session. Historical queries and alert displays include session identity and distinguish chronological lifecycle order from wall-clock labels; overlapping clock ranges must not be merged into a fabricated continuous series. Session-order identity is never recycled by retention.

Required Phase 2 regression: open an alert in session1 at a later UTC time, restart session2 with an earlier wall clock, then evaluate and recover it. The same event closes without a duplicate, foreign keys/ordering remain valid, original timestamps are unchanged, and the UI data contract exposes the clock warning. Missing observations still do not resolve an alert.

## C. Server readiness precedes browser/UI readiness

Start the owned Streamlit process, wait for its server to listen and its built-in local health check to succeed within a finite deadline, then open the browser. Do not wait for any page code or browser-generated notification before opening the browser. This uses Streamlit's existing server health facility, not a new application REST API.

UI-connected/first-render status is optional subsequent diagnostics. It does not control whether the browser is opened or whether server startup is successful. Detect early server exit and port conflicts; never mistake a pre-existing listener for the owned server. Keep an explicit error and unwind owned children on failed startup.

Required Phase 4 regression: launch with browser opening stubbed/delayed and zero connected browser clients; server readiness must complete. Then connect the first client and verify the page. Test server failure/port conflict separately.

## D. Keep IPC narrow

Retain bounded inherited stdin/stdout JSON messages between controller and native children and one current-user local controller endpoint for Streamlit's Python process. Use a small allowlisted command handler, request IDs, desired-state operations, finite timeouts, acknowledgements and explicit errors. No generic RPC framework, broker, service registry or arbitrary command/SQL dispatch. Drain stdout/stderr so a child cannot deadlock on a full pipe; EOF/broken pipes have an explicit lifecycle outcome.

Keep controller ownership outside Streamlit reruns. Readiness uses the server test described above. Tests cover fragmented/oversize input, a lost acknowledgement/idempotent retry, saturated command capacity and shutdown while a normal command is pending.

## E. Capacity includes the write currently in flight

Use one explicit accounting invariant: at most **six immutable persistence batches total**, including queued, retrying and in-flight work, plus **one active aggregation store**. Each contains at most1,024 series. Dequeuing a batch for SQLite does not free a slot. Only confirmed COMMIT frees that slot; rollback/retry keeps it occupied.

At a window boundary with all six persistence slots occupied, freeze the existing active store in place and stop admitting new observations. There is no extra seventh pending-copy allocation and no new active store alongside that frozen store. Drain incoming datagrams and count known refused lines. When a persistence slot becomes free, move the frozen store into that slot, then open a new active window; the pause remains a historical gap. Maximum resident data is six immutable batches plus one active-or-frozen store, with bounded metadata/control buffers.

Use a single writer connection and FIFO write stream; retry identifiers prevent duplicate commits. Report active, queued and in-flight counts separately, with an explicitly defined total; never double-count the in-flight batch as queued. “Accepted” still does not mean committed. An unsuccessful write must never generate a success status or release/drop its accepted data. Shutdown reports remaining uncommitted samples if its bounded drain fails.

Required Phase 1/2 regressions: block writes, fill six slots, reach the active-store boundary, assert bounded accounting and refused-input counts, release the lock, and prove all previously accepted samples commit exactly once. Test final shutdown with the in-flight transaction failing. These are deterministic functional tests, not benchmark infrastructure.

These changes will be applied to V2's schema, source, tests and user-facing semantics in their dependent phases. The approved design package and received implementation remain unchanged as historical references.
