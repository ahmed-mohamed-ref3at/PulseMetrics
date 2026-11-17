import time
from datetime import datetime, time as day_time, timezone

import streamlit as st
import plotly.graph_objects as go

from pulsemetrics import ipc
from pulsemetrics.analytics import compare_windows, memory_change, summaries
from .components import CYAN, PURPLE, alert_list, chart, database, recent_range, status, timestamp, title, utc, value


def overview():
    title("YOUR LOCAL PERFORMANCE WORKSPACE", "Overview", "A clear view of this computer. Live observations and committed history, shown separately.")
    state, stale = status()
    live, health = state["live"], state["collector"]
    system = live.get("system", {}) if not stale else {}
    with st.container(horizontal=True):
        st.metric("System CPU", value(system.get("cpu_percent"), "%"), border=True)
        used = system.get("memory_used_bytes")
        st.metric("Memory in use", value(used / 2**30 if used is not None else None, " GiB"), border=True)
        st.metric("Processes discovered", str(live.get("discovered", 0)) if not stale else "Unavailable", border=True)
        st.metric("Actively tracked", str(live.get("tracked", 0)) if not stale else "Unavailable", border=True)
    st.caption("Live sample: " + timestamp(live.get("sampled_at_us")) + "  ·  Last database commit: " + timestamp(health.get("last_commit_us")))
    db = database()
    sources = db.sources()
    system_source = next((s for s in sources if s["kind"] == "system"), None)
    rows = db.history(system_source["source_id"], *recent_range(1800)) if system_source else []
    left, right = st.columns(2)
    with left, st.container(border=True):
        st.subheader("CPU over time", anchor=False)
        st.caption("Last 30 minutes · last observed value per committed window")
        chart(rows, "cpu.percent", "CPU", "%", key="overview_cpu")
    with right, st.container(border=True):
        st.subheader("Memory over time", anchor=False)
        st.caption("Physical memory in use · GiB")
        chart(rows, "memory.used", "Memory", "GiB", 2**30, key="overview_memory")
    left, right = st.columns([1.2, 1])
    with left, st.container(border=True):
        st.subheader("Top tracked processes", anchor=False)
        top = [] if stale else live.get("top", [])
        if top:
            st.dataframe([{"Process": r["name"], "PID": r["pid"], "CPU %": r["cpu_percent"], "Working set MiB": r["memory_bytes"] / 2**20 if r["memory_bytes"] is not None else None} for r in top], hide_index=True,
                         column_config={"CPU %": st.column_config.NumberColumn(format="%.1f"), "Working set MiB": st.column_config.NumberColumn(format="%.1f")})
        else:
            st.info("Track a process in Process Explorer to begin its personal history.")
    with right, st.container(border=True):
        st.subheader("Recent alerts", anchor=False)
        alert_list(db.alerts(), 3)
    with st.container(horizontal=True):
        st.badge("Collector running" if health.get("state") == "running" else "Collector unavailable", color="green" if health.get("state") == "running" else "red")
        st.caption(f"Accepted {health.get('accepted', 0):,} · Rejected {health.get('rejected', 0):,} · Pending batches {health.get('pending_batches', 0)}/6 · Known dropped {health.get('known_dropped', 0):,}")


def explorer():
    title("DISCOVER · SELECT · FOLLOW", "Process Explorer", "Follow exact Windows process instances. Only selected processes are stored in history.")
    state, stale = status()
    left, right = st.columns([2, 1])
    search = left.text_input("Search processes", placeholder="Name or PID", key="process_search")
    sort = right.selectbox("Sort by", ["CPU", "Memory", "Name", "PID"])
    offset = st.number_input("Page", min_value=1, max_value=400, value=1, step=1)
    result = ipc.request("list_processes", {"search": search, "sort": {"CPU": "cpu_percent", "Memory": "memory_bytes", "Name": "name", "PID": "pid"}[sort], "offset": (offset - 1) * 25})
    rows = result["rows"]
    st.caption(f"{result['total']} matching processes · 25 per page · Sample {timestamp(state['live'].get('sampled_at_us'))}")
    stored_sources = database().sources()
    saved = [s for s in stored_sources if s["kind"] == "process" and s["selected"]]
    if saved:
        with st.expander("Saved tracking selections"):
            st.caption("Selections may remain saved while identity access is unavailable. Only revalidated instances are sampled; a saved selection alone does not mean live data.")
            selected = st.selectbox("Saved instance", saved, format_func=lambda s: f"{s['display_name']} · PID {s['pid']} · {s['creation_filetime']}")
            if st.button("Remove saved selection"):
                ipc.request("untrack", {"source": selected["source_key"]})
                st.success("Selection removed. Stored observations remain available.")
    ended = sorted((s for s in stored_sources if s["kind"] == "process" and s["ended_utc_us"]), key=lambda s: s["ended_utc_us"], reverse=True)[:20]
    if ended:
        with st.expander("Ended tracked instances · recent 20"):
            st.dataframe([{"Process": s["display_name"], "PID": s["pid"], "Creation identity": str(s["creation_filetime"]), "State": "Ended", "End observed": timestamp(s["ended_utc_us"])} for s in ended], hide_index=True)
    if not rows:
        st.info("No processes match this search or page. Return to page 1 or clear the search.")
        return
    display = [{"Process": r["name"], "PID": r["pid"], "CPU %": r["cpu_percent"], "Working set MiB": r["memory_bytes"] / 2**20 if r["memory_bytes"] is not None else None,
                "State": "stale" if stale else r["state"].replace("_", " "), "Tracked": r["tracked"]} for r in rows]
    st.dataframe(display, hide_index=True, height=430, column_config={"CPU %": st.column_config.ProgressColumn(min_value=0, max_value=100, format="%.1f%%"), "Working set MiB": st.column_config.NumberColumn(format="%.1f")})
    choice = st.selectbox("Process to manage", range(len(rows)), format_func=lambda i: f"{rows[i]['name']} · PID {rows[i]['pid']}")
    row = rows[choice]
    with st.container(border=True):
        st.write(f"**{row['name']}** · PID {row['pid']}")
        st.caption("Creation identity: " + (row["created"] or "Unavailable — this process cannot be tracked"))
        st.caption("CPU is normalized to total logical processor capacity. Memory is the resident working set. Missing readings are unavailable, never zero-filled.")
        with st.container(horizontal=True):
            if st.button("Untrack process" if row["tracked"] else "Track process", type="primary", disabled=stale or not row["created"]):
                try:
                    args = {"source": row["source"]} if row["tracked"] else {"pid": row["pid"], "created": row["created"], "label": row["name"]}
                    ipc.request("untrack" if row["tracked"] else "track", args)
                    st.success("Selection applied. The next native sample will update this table.")
                except (RuntimeError, OSError, TimeoutError) as exc:
                    st.error(str(exc))
            if "analytics_page" in st.session_state and st.button("Analyze stored history", disabled=row["source"] not in {s["source_key"] for s in stored_sources}):
                st.session_state["source_target"] = row["source"]
                st.switch_page(st.session_state["analytics_page"])


def analytics():
    title("FROM OBSERVATION TO UNDERSTANDING", "Performance Analytics", "Explore measured behavior, with exact units and a path back to every stored window.")
    status()
    db = database()
    sources = db.sources()
    if not sources:
        st.info("Waiting for the collector to register its sources.")
        return
    choices = {s["source_key"]: s for s in sources}
    target = st.session_state.pop("source_target", None)
    if target in choices:
        st.session_state["analytics_source"] = target
    initial = st.session_state.get("source_key", "system")
    keys = list(choices)
    left, right = st.columns([2, 1])
    source_key = left.selectbox("Source", keys, index=keys.index(initial) if initial in keys else 0,
                               format_func=lambda k: f"{choices[k]['display_name']} · {k}", key="analytics_source")
    if choices[source_key]["ended_utc_us"]:
        st.caption("Ended process · " + timestamp(choices[source_key]["ended_utc_us"]) + " · Stored history is preserved.")
    ranges = {"30 minutes": 1800, "1 hour": 3600, "6 hours": 21600, "24 hours": 86400, "7 days": 604800}
    span = right.selectbox("Time range", list(ranges), index=1)
    rows = db.history(choices[source_key]["source_id"], *recent_range(ranges[span]))
    if len(rows) > 2000:
        st.warning("Showing the newest 2,000 metric-window records. Choose a shorter range for complete coverage.")
        rows = rows[:2000]
    if not rows:
        st.info("No committed observations for this source in the selected range. Track the process or run the standalone metric sender, then wait for a window to commit.")
        return
    with st.container(horizontal=True):
        st.metric("Observed samples", f"{sum(r['sample_count'] for r in rows):,}", border=True)
        st.metric("Stored windows", len({r["window_id"] for r in rows}), border=True)
        st.metric("Metric series", len({r["metric_id"] for r in rows}), border=True)
        st.metric("Latest window", utc(max(r["end_utc_us"] for r in rows)).strftime("%H:%M:%S UTC"), border=True)
    latest = max(rows, key=lambda r: (r["session_order"], r["sequence"]))
    if time.time() * 1e6 - latest["last_received_utc_us"] > 30e6:
        st.info("This source has no recent stored observations. These values are historical, not live.")
    catalog = {r["name"]: r for r in rows}
    names = list(catalog)
    if "cpu.percent" in names:
        names.remove("cpu.percent"); names.insert(0, "cpu.percent")
    for offset in range(0, len(names), 2):
        columns = st.columns(2)
        for col, name in zip(columns, names[offset:offset + 2]):
            metric = catalog[name]
            unit = metric["unit"]
            scale = 2**20 if unit == "bytes" else 1
            shown_unit = "MiB" if unit == "bytes" else unit
            field = "sample_sum" if metric["metric_kind"] == "counter" else "sample_mean" if metric["metric_kind"] == "timer" else "last_value"
            with col, st.container(border=True):
                st.subheader(name, anchor=False)
                st.caption(f"{metric['metric_kind'].title()} · {shown_unit} · " + {"sample_sum": "sum per window", "sample_mean": "mean of measured durations", "last_value": "last observation per window"}[field])
                event = chart(rows, name, name, shown_unit, scale, field, key="analytics_" + name)
                points = event.selection.points if event else []
                if points and points[0].get("customdata") and "history_page" in st.session_state:
                    st.session_state["history_target"] = int(points[0]["customdata"])
                    if st.button("Inspect selected window", key="inspect_" + name):
                        st.switch_page(st.session_state["history_page"])
    with st.container(border=True):
        st.subheader("Measured summary", anchor=False)
        st.caption("Average and peak are calculated from actual observations. Counter rate uses only observed windows; gaps are excluded.")
        st.dataframe([{"Metric": r["name"], "Type": r["kind"], "Unit": r["unit"], "Observations": r["observations"], "Average observation": r["mean"], "Peak observation": r["peak"], "Counter total": r["sum"] if r["kind"] == "counter" else None, "Count / second": r["rate"]} for r in summaries(rows)], hide_index=True)
    left, right = st.columns(2)
    with left, st.container(border=True):
        st.subheader("Evidence-based insight", anchor=False)
        insight = memory_change(rows)
        if insight:
            st.write(f"Observed memory changed by {insight['delta_bytes'] / 2**20:+.2f} MiB between two recorded windows in session {insight['session_order']}.")
            st.caption(f"Window {insight['first_window']} → {insight['last_window']} · {insight['first_value'] / 2**20:.2f} → {insight['last_value'] / 2**20:.2f} MiB. This comparison describes observations, not a cause or a continuous trend.")
        else:
            st.caption("A memory comparison needs at least two recorded windows for this source in one session.")
    with right, st.container(border=True):
        st.subheader("Threshold evidence", anchor=False)
        alert_list(db.alerts(choices[source_key]["source_id"]), 4)


def time_machine():
    title("EVERY WINDOW HAS A STORY", "Performance Time Machine", "Revisit committed observations. Missing windows remain gaps; missing metrics remain unavailable.")
    status()
    db = database()
    target_id = st.session_state.pop("history_target", None)
    target = db.window_meta(target_id) if target_id else None
    if target:
        st.session_state["history_day"] = utc(target["start_utc_us"]).date()
        st.session_state["history_from"] = day_time(max(0, utc(target["start_utc_us"]).hour - 1))
        st.session_state["history_to"] = day_time(min(23, utc(target["end_utc_us"]).hour + 1), 59)
        st.session_state["history_window"] = target_id
    left, middle, right = st.columns([2, 1, 1])
    day = left.date_input("History date · UTC", key="history_day")
    beginning = middle.time_input("From · UTC", value=day_time(0), key="history_from")
    ending = right.time_input("To · UTC", value=day_time(23, 59, 59), key="history_to")
    start = int(datetime.combine(day, beginning, timezone.utc).timestamp() * 1e6)
    end = int(datetime.combine(day, ending, timezone.utc).timestamp() * 1e6)
    if end <= start:
        st.warning("Choose an end time later than the start time.")
        return
    windows = db.windows(start, end)
    if len(windows) > 500:
        st.warning("Showing the newest 500 windows in this range. Narrow the UTC times to inspect earlier windows.")
        windows = windows[:500]
    if not windows:
        st.info("No stored windows in this range. Collection may have been stopped, persistence may have failed, or retention may have expired this history.")
        return
    windows.sort(key=lambda w: (w["session_order"], w["sequence"]))
    mapping = {w["window_id"]: w for w in windows}
    ids = list(mapping)
    if st.session_state.get("history_window") not in ids:
        st.session_state["history_window"] = ids[-1]
    alerts = db.alerts()
    alert_windows = {e["first_window_id"] for e in alerts} | {e["last_window_id"] for e in alerts}
    figure = go.Figure(go.Scatter(x=[utc(w["end_utc_us"]) for w in windows], y=[w["metric_count"] for w in windows],
                                mode="markers", customdata=ids, marker=dict(size=9, color=[CYAN if w["window_id"] in alert_windows else PURPLE for w in windows]),
                                hovertemplate="%{x|%d %b %H:%M:%S UTC}<br>%{y} observed series<br>Window %{customdata}<extra></extra>"))
    figure.update_layout(height=220, margin=dict(l=8, r=8, t=8, b=8), paper_bgcolor="rgba(0,0,0,0)", plot_bgcolor="rgba(0,0,0,0)",
                         font=dict(color="#D4DEF4"), xaxis=dict(title="Committed window end · UTC", gridcolor="#202C45"), yaxis=dict(title="Observed metric series", gridcolor="#202C45"))
    selection = st.plotly_chart(figure, key="history_timeline", on_select="rerun", selection_mode="points", config={"displaylogo": False})
    st.caption("Purple: stored window · Cyan: opening or latest evaluation of a recent alert. Points do not imply continuous coverage.")
    points = selection.selection.points
    if points:
        clicked = points[0].get("customdata")
        if clicked in mapping and clicked != st.session_state.get("last_timeline_click"):
            st.session_state["history_window"] = clicked
            st.session_state["last_timeline_click"] = clicked
    selected_id = st.selectbox("Stored window", ids, key="history_window", format_func=lambda key: f"{timestamp(mapping[key]['end_utc_us'])} · Session {mapping[key]['session_order']} / window {mapping[key]['sequence']}")
    index = ids.index(selected_id)
    with st.container(horizontal=True):
        if st.button("Previous window", disabled=index == 0):
            st.session_state["history_target"] = ids[index - 1]
            st.rerun()
        if st.button("Next window", disabled=index == len(ids) - 1):
            st.session_state["history_target"] = ids[index + 1]
            st.rerun()
    selected = mapping[selected_id]
    with st.container(horizontal=True):
        st.metric("Window duration", f"{selected['elapsed_us'] / 1e6:.3f} s", border=True)
        st.metric("Observed series", selected["metric_count"], border=True)
        st.metric("Window type", "Partial" if selected["partial"] else "Complete", border=True)
    st.caption(f"Receipt interval: {timestamp(selected['start_utc_us'])} → {timestamp(selected['end_utc_us'])} · Session {selected['session_order']} · Stored window ID {selected_id}")
    if selected["clock_warning"]:
        st.warning("This session has a clock warning. Session order, not UTC order alone, determines lifecycle sequence.")
    rows = db.window(selected_id)
    if rows:
        st.dataframe([{"Source": r["display_name"], "Metric": r["name"], "Type": r["metric_kind"], "Unit": r["unit"], "Samples": r["sample_count"], "Sum": r["sample_sum"], "Mean": r["sample_mean"], "Minimum": r["sample_min"], "Maximum": r["sample_max"], "Last": r["last_value"], "Rate / second": r["rate"]} for r in rows], hide_index=True)
    else:
        st.info("This window contains collector health but no accepted metric observations.")
    related = [e for e in alerts if e["first_window_id"] == selected_id or e["last_window_id"] == selected_id]
    with st.expander("Related threshold events", expanded=bool(related)):
        alert_list(related)
    if index:
        previous = mapping[ids[index - 1]]
        with st.expander("Compare with previous stored window"):
            comparisons = compare_windows(rows, db.window(previous["window_id"]))
            if comparisons:
                st.caption("Only matching metric identities in the same session are compared. Counter rates account for actual window duration.")
                if selected["start_utc_us"] - previous["end_utc_us"] > 100000:
                    st.info("There is an unobserved gap between these windows.")
                st.dataframe(comparisons, hide_index=True)
            else:
                st.info("No compatible observations in the previous window. Cross-session comparisons are not inferred.")
