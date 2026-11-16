import time

import streamlit as st

from pulsemetrics import ipc
from .components import alert_list, chart, database, recent_range, status, timestamp, title, value


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
            if "analytics_page" in st.session_state and st.button("Analyze stored history", disabled=not row["source"]):
                st.session_state["source_key"] = row["source"]
                st.switch_page(st.session_state["analytics_page"])
