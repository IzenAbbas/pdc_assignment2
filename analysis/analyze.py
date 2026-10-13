#!/usr/bin/env python3
"""Parse the benchmark sweeps, write HTML tables and figures for the report,
and print the derived numbers quoted in the text.

Every time in results/*.txt is already the minimum of 5 runs (./runtasks -i 5).
T1 = the supplied Serial implementation (minimum over all of its runs).
"""
import re
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.patches import Patch

ROOT = Path(__file__).resolve().parent
RES, FIG, GEN = ROOT / "results", ROOT / "figures", ROOT / "generated"
FIG.mkdir(exist_ok=True)
GEN.mkdir(exist_ok=True)

P = [1, 2, 4, 8, 16]
SHORT = {"Serial": "Serial", "Parallel + Always Spawn": "Spawn",
         "Parallel + Thread Pool + Spin": "Spin", "Parallel + Thread Pool + Sleep": "Sleep"}
IMPLS = ["Spawn", "Spin", "Sleep"]

# Chart tokens (validated palette slots 1-3 on a white surface; see dataviz skill).
COLOR = {"Spawn": "#2a78d6", "Spin": "#eb6834", "Sleep": "#1baf7a"}
MARKER = {"Spawn": "o", "Spin": "s", "Sleep": "^"}
INK, INK2, MUTED, GRID, AXIS = "#0b0b0b", "#52514e", "#898781", "#e1e0d9", "#c3c2b7"
plt.rcParams.update({
    "font.family": "Noto Sans", "font.size": 8, "axes.edgecolor": AXIS, "axes.linewidth": 0.8,
    "axes.labelcolor": INK2, "axes.titlesize": 8.5, "axes.titlecolor": INK, "xtick.color": MUTED,
    "ytick.color": MUTED, "xtick.labelcolor": INK2, "ytick.labelcolor": INK2, "axes.grid": True,
    "grid.color": GRID, "grid.linewidth": 0.6, "grid.linestyle": "-", "axes.spines.top": False,
    "axes.spines.right": False, "figure.facecolor": "white", "axes.facecolor": "white",
    "savefig.dpi": 220, "legend.frameon": False, "axes.axisbelow": True,
})

# Launch structure of each supplied workload (launches x tasks per launch).
WORKLOADS = {
    "super_super_light": "400 launches x 64 tiny tasks",
    "super_light": "400 x 64 light tasks",
    "ping_pong_equal": "400 x 64, equal cost",
    "ping_pong_unequal": "400 x 64, cost falls with task id",
    "recursive_fibonacci": "30 x 256 heavy tasks",
    "math_operations_in_tight_for_loop": "2000 x 16 medium tasks",
    "math_operations_in_tight_for_loop_fewer_tasks": "2000 x 9 tasks",
    "math_operations_in_tight_for_loop_fan_in": "256 x 64 + 1 reduce",
    "math_operations_in_tight_for_loop_reduction_tree": "32 x 64 + 31 reduces",
    "spin_between_run_calls": "3 launches: 1, 2, 1 tasks",
    "mandelbrot_chunked": "1 x 128 tasks",
}
LAUNCHES = {"super_super_light": 400, "super_light": 400, "ping_pong_equal": 400,
            "ping_pong_unequal": 400, "recursive_fibonacci": 30,
            "math_operations_in_tight_for_loop": 2000,
            "math_operations_in_tight_for_loop_fewer_tasks": 2000}
CHUNKS = [1, 16, 256, 4096, 65536]
N_ITEMS = 1 << 18


def parse(path):
    """{(test, n): {impl: ms}} from a sweep file."""
    data, cur = {}, None
    for line in path.read_text().splitlines():
        m = re.match(r"### test=(\S+) n=(\d+)", line)
        if m:
            cur = (m.group(1), int(m.group(2)))
            data[cur] = {}
            continue
        m = re.match(r"\[(.+?)\]:\s+\[(\d+\.\d+)\] ms", line)
        if m and cur:
            data[cur][SHORT[m.group(1)]] = float(m.group(2))
    return data


SYNC = parse(RES / "sync_sweep.txt") if (RES / "sync_sweep.txt").exists() else {}
ASYNC = parse(RES / "async_sweep.txt") if (RES / "async_sweep.txt").exists() else {}


def t1(data, test):
    return min(v["Serial"] for (t, _), v in data.items() if t == test and "Serial" in v)


def tp(data, test, impl, p):
    return data[(test, p)][impl]


def fmt(x):
    return f"{x:,.1f}" if x >= 10 else f"{x:.2f}"


# ---------------------------------------------------------------- tables
def table_html(tests, data=SYNC, impls=IMPLS, caption=""):
    """One block per workload: rows = implementations, columns = Tp, Sp, Ep for each p."""
    head = "".join(f'<th colspan="3">p = {p}</th>' for p in P)
    sub = "".join("<th>T<sub>p</sub> (ms)</th><th>S<sub>p</sub></th><th>E<sub>p</sub></th>" for _ in P)
    rows = []
    for test in tests:
        T1 = t1(data, test)
        rows.append(f'<tr class="grp"><td colspan="{1 + 3 * len(P)}"><code>{test}</code>'
                    f' &nbsp;·&nbsp; T<sub>1</sub> (Serial) = {fmt(T1)} ms</td></tr>')
        for impl in impls:
            cells = []
            for p in P:
                T = tp(data, test, impl, p)
                S = T1 / T
                cells.append(f"<td>{fmt(T)}</td><td>{S:.2f}</td><td>{S / p:.2f}</td>")
            rows.append(f"<tr><th class='impl'>{impl}</th>{''.join(cells)}</tr>")
    cap = f"<caption>{caption}</caption>" if caption else ""
    return (f'<table class="num">{cap}<thead><tr><th rowspan="2">Impl.</th>{head}</tr>'
            f"<tr>{sub}</tr></thead><tbody>{''.join(rows)}</tbody></table>")


def write(name, html):
    (GEN / name).write_text(html)


if SYNC:
    write("table_all.html", table_html(list(WORKLOADS)))
    write("table_part1.html", table_html(["super_super_light", "recursive_fibonacci", "ping_pong_unequal"]))


# ---------------------------------------------------------------- figures
def xaxis_p(ax):
    ax.set_xscale("log", base=2)
    ax.set_xticks(P)
    ax.set_xticklabels([str(p) for p in P])
    ax.minorticks_off()


def impl_handles(extra=()):
    h = [Line2D([], [], color=COLOR[i], marker=MARKER[i], lw=2, ms=5, mec="white", mew=1, label=i)
         for i in IMPLS]
    return h + list(extra)


def grid_figure(tests, kind, path, ncols=3):
    nrows = -(-len(tests) // ncols)
    fig, axes = plt.subplots(nrows, ncols, figsize=(7.2, 1.95 * nrows + 0.35))
    axes = axes.ravel()
    for ax, test in zip(axes, tests):
        T1 = t1(SYNC, test)
        for impl in IMPLS:
            ys = [tp(SYNC, test, impl, p) for p in P]
            if kind == "speedup":
                ys = [T1 / y for y in ys]
            elif kind == "efficiency":
                ys = [T1 / y / p for y, p in zip(ys, P)]
            ax.plot(P, ys, color=COLOR[impl], marker=MARKER[impl], lw=2, ms=5, mec="white",
                    mew=1, solid_capstyle="round", zorder=3)
        if kind == "runtime":
            ax.axhline(T1, color=MUTED, lw=1, ls="--", zorder=2)
            ax.set_yscale("log")
            ax.set_ylabel("time (ms)")
        elif kind == "speedup":
            top = max(max(T1 / tp(SYNC, test, i, p) for i in IMPLS for p in P) * 1.18, 1.3)
            ax.plot(P, P, color=MUTED, lw=1, ls="--", zorder=2)
            ax.axhline(1, color=AXIS, lw=0.8, zorder=1)
            ax.set_ylim(0, top)
            ax.set_ylabel("speedup")
        else:
            top = max(max(T1 / tp(SYNC, test, i, p) / p for i in IMPLS for p in P) * 1.12, 1.1)
            ax.axhline(1, color=MUTED, lw=1, ls="--", zorder=2)
            ax.set_ylim(0, top)
            ax.set_ylabel("efficiency")
        xaxis_p(ax)
        ax.set_title(test.replace("math_operations_in_tight_for_loop", "math_ops"), loc="left", pad=11)
        ax.text(0.0, 1.02, WORKLOADS.get(test, ""), transform=ax.transAxes, fontsize=6.5,
                color=MUTED, va="bottom", ha="left")
        ax.set_xlabel("workers p", labelpad=1)
    for ax in axes[len(tests):]:
        ax.axis("off")
    ref = {"runtime": "serial T1", "speedup": "ideal (S = p)", "efficiency": "ideal (E = 1)"}[kind]
    extra = [Line2D([], [], color=MUTED, lw=1, ls="--", label=ref)]
    fig.legend(handles=impl_handles(extra), loc="upper center", ncol=4, fontsize=8,
               bbox_to_anchor=(0.5, 1.0))
    fig.tight_layout(rect=(0, 0, 1, 1 - 0.32 / fig.get_figheight()), h_pad=1.6)
    fig.savefig(path)
    plt.close(fig)


EFF_TESTS = ["super_super_light", "math_operations_in_tight_for_loop", "ping_pong_unequal",
             "recursive_fibonacci", "spin_between_run_calls", "mandelbrot_chunked"]
if SYNC:
    grid_figure(list(WORKLOADS), "runtime", FIG / "runtime.png")
    grid_figure(list(WORKLOADS), "speedup", FIG / "speedup.png")
    grid_figure(EFF_TESTS, "efficiency", FIG / "efficiency.png")


def granularity_figure(path):
    fig, axes = plt.subplots(1, 2, figsize=(7.2, 2.6), sharey=True)
    T1 = t1(SYNC, "irregular_chunk_1")
    for ax, p in zip(axes, [4, 16]):
        for impl in IMPLS:
            ys = [tp(SYNC, f"irregular_chunk_{c}", impl, p) for c in CHUNKS]
            ax.plot(CHUNKS, ys, color=COLOR[impl], marker=MARKER[impl], lw=2, ms=5, mec="white",
                    mew=1, zorder=3)
        ax.axhline(T1 / p, color=MUTED, lw=1, ls="--", zorder=2)
        ax.text(1.2, T1 / p, f" ideal T1/p = {T1 / p:.0f} ms", color=INK2, fontsize=7, va="bottom")
        ax.set_xscale("log", base=2)
        ax.set_xticks(CHUNKS)
        ax.set_xticklabels([f"{c}\n({N_ITEMS // c} tasks)" for c in CHUNKS], fontsize=6.5)
        ax.minorticks_off()
        ax.set_yscale("log")
        ax.set_title(f"p = {p} workers", loc="left")
        ax.set_xlabel("chunk size (items per task)")
    axes[0].set_ylabel("time (ms), log scale")
    fig.legend(handles=impl_handles(), loc="upper center", ncol=3, fontsize=8, bbox_to_anchor=(0.5, 1.0))
    fig.tight_layout(rect=(0, 0, 1, 0.9))
    fig.savefig(path)
    plt.close(fig)


if (("irregular_chunk_1", 4) in SYNC):
    granularity_figure(FIG / "granularity.png")


# --------------------------------------------------- analytic timelines (Gantt)
def gantt(ax, rows, makespan, colors, title, label_fn, xmax=None):
    """rows[w] = list of (start, end, key, label); draws worker rows top-down."""
    n = len(rows)
    for w, items in enumerate(rows):
        y = n - 1 - w
        for (s, e, key, lab) in items:
            ax.barh(y, e - s, left=s, height=0.62, color=colors[key], edgecolor="white", linewidth=1.2)
            if lab:
                ax.text((s + e) / 2, y, lab, ha="center", va="center", fontsize=6.5,
                        color=label_fn(key))
    ax.axvline(makespan, color=INK2, lw=1)
    ax.text(makespan, n - 0.45, f" makespan {makespan:g} ms", color=INK, fontsize=7, va="bottom")
    ax.set_yticks(range(n))
    ax.set_yticklabels([f"W{n - 1 - i}" for i in range(n)])
    ax.set_xlim(0, xmax or makespan * 1.12)
    ax.set_ylim(-0.6, n - 0.1)
    ax.grid(axis="y", visible=False)
    ax.set_title(title, loc="left", pad=12)
    ax.set_xlabel("time (ms)")


def part4_timelines(path):
    cost = [1, 1, 1, 1, 10, 10, 10, 10]
    colors = {"cheap": "#86b6ef", "dear": "#1c5cab"}       # blue ramp steps 250 / 550 (cost is ordinal)
    lab = lambda k: INK if k == "cheap" else "white"
    # Static contiguous: worker w gets tasks 2w, 2w+1.
    static = []
    for w in range(4):
        t, row = 0, []
        for i in (2 * w, 2 * w + 1):
            row.append((t, t + cost[i], "cheap" if cost[i] == 1 else "dear", f"T{i}"))
            t += cost[i]
        static.append(row)
    # Dynamic next-task: each free worker takes the next task id.
    dyn, free = [[] for _ in range(4)], [0] * 4
    for i, c in enumerate(cost):
        w = min(range(4), key=lambda k: (free[k], k))
        dyn[w].append((free[w], free[w] + c, "cheap" if c == 1 else "dear", f"T{i}"))
        free[w] += c
    fig, axes = plt.subplots(1, 2, figsize=(7.2, 2.2), sharey=True)
    gantt(axes[0], static, 20, colors, "Static contiguous (2 tasks per worker)", lab, xmax=23)
    gantt(axes[1], dyn, 11, colors, "Dynamic next-task", lab, xmax=23)
    fig.legend(handles=[Patch(color=colors["cheap"], label="1 ms task"),
                        Patch(color=colors["dear"], label="10 ms task")],
               loc="upper center", ncol=2, fontsize=8, bbox_to_anchor=(0.5, 1.0))
    fig.tight_layout(rect=(0, 0, 1, 0.9))
    fig.savefig(path)
    plt.close(fig)


part4_timelines(FIG / "part4_timelines.png")


def dag_schedule(workers, split_b=False):
    """Greedy FIFO list schedule of the A->{B,C}->D graph, phase by phase."""
    b = [("B", 2.0)] * 8 if split_b else [("B", 8.0)] * 2
    phases = [[("A", 1.0)] * 128, b + [("C", 4.0)] * 6, [("D", 2.0)] * 32]
    rows, start = [[] for _ in range(workers)], 0.0
    for tasks in phases:
        free = [start] * workers
        counts = {}
        for key, c in tasks:
            w = min(range(workers), key=lambda k: (free[k], k))
            idx = counts.get(key, 0)
            counts[key] = idx + 1
            rows[w].append((free[w], free[w] + c, key, idx))
            free[w] += c
        start = max(free)
    return rows, start


def dag_figure(path):
    colors = {"A": "#2a78d6", "B": "#eb6834", "C": "#1baf7a", "D": "#eda100"}
    fig, axes = plt.subplots(2, 1, figsize=(7.2, 3.6))
    for ax, workers in zip(axes, [2, 4]):
        rows, makespan = dag_schedule(workers)
        merged = []
        for items in rows:      # one bar per run of same-launch tasks, labelled "A x32", "B0", ...
            out = []
            for s, e, key, idx in items:
                if key in "AD" and out and out[-1][2] == key:
                    out[-1] = (out[-1][0], e, key, out[-1][3] + 1)
                else:
                    out.append((s, e, key, 1 if key in "AD" else idx))
            merged.append([(s, e, k, (f"{k} ×{n}" if k in "AD" else f"{k}{n}")) for s, e, k, n in out])
        gantt(ax, merged, makespan, colors, f"{workers} workers", lambda k: "white" if k == "A" else INK,
              xmax=makespan * 1.12)
    fig.legend(handles=[Patch(color=c, label=f"{k} ({t})") for (k, c), t in
                        zip(colors.items(), ["128×1 ms", "2×8 ms", "6×4 ms", "32×2 ms"])],
               loc="upper center", ncol=4, fontsize=8, bbox_to_anchor=(0.5, 1.0))
    fig.tight_layout(rect=(0, 0, 1, 0.93), h_pad=1.2)
    fig.savefig(path)
    plt.close(fig)


dag_figure(FIG / "part5_schedules.png")

# ---------------------------------------------------------------- summary for the text
if __name__ == "__main__" and SYNC:
    print("== T1 (Serial, ms)")
    for t in WORKLOADS:
        print(f"  {t:52s} {t1(SYNC, t):9.1f}")
    print("\n== Runtime improvement when doubling p (Spawn, Spin, Sleep); first p with <10% marked *")
    for t in WORKLOADS:
        for impl in IMPLS:
            parts, first = [], None
            for a, b in zip(P, P[1:]):
                g = (tp(SYNC, t, impl, a) - tp(SYNC, t, impl, b)) / tp(SYNC, t, impl, a)
                if first is None and g < 0.10:
                    first = b
                parts.append(f"{a}->{b}: {100 * g:+5.1f}%")
            print(f"  {t[:40]:40s} {impl:5s} first<10%: p={first}  | " + "  ".join(parts))
    print("\n== Spawn - Spin per launch (ms) = thread create+join cost per launch")
    for t, L in LAUNCHES.items():
        vals = [(tp(SYNC, t, "Spawn", p) - tp(SYNC, t, "Spin", p)) / L * 1000 for p in P]
        print(f"  {t[:45]:45s} L={L:5d}  " + "  ".join(f"p={p}: {v:7.1f} us" for p, v in zip(P, vals)))
    print("\n== Overhead above ideal: Tp - T1/p (ms)")
    for t in WORKLOADS:
        T1 = t1(SYNC, t)
        for impl in IMPLS:
            print(f"  {t[:40]:40s} {impl:5s} " + "  ".join(
                f"p={p}: {tp(SYNC, t, impl, p) - T1 / p:8.1f}" for p in P))
    if ("irregular_example_static", 4) in SYNC:
        print("\n== Part 4 example (ms)")
        for t in ["irregular_example_static", "irregular_example_dynamic"]:
            for p in P:
                print(f"  {t:28s} p={p:2d} " + "  ".join(f"{i}={SYNC[(t, p)][i]:7.2f}" for i in ["Serial"] + IMPLS))
        print("\n== Part 4 granularity (ms); H estimate = (Tp(chunk) - Tp(256)) * p / tasks(chunk)")
        T1 = t1(SYNC, "irregular_chunk_1")
        print(f"  T1 = {T1:.1f} ms, T per item = {T1 / N_ITEMS * 1e6:.0f} ns")
        for p in P:
            for impl in IMPLS:
                row = [tp(SYNC, f"irregular_chunk_{c}", impl, p) for c in CHUNKS]
                h1 = (row[0] - row[2]) * p / N_ITEMS * 1e6
                print(f"  p={p:2d} {impl:5s} " + "  ".join(f"c{c}={v:8.1f}" for c, v in zip(CHUNKS, row))
                      + f"   H(chunk1)~{h1:6.0f} ns")
    if ASYNC:
        print("\n== Part 5 (Sleep, ms)")
        for t in sorted({t for t, _ in ASYNC}):
            print(f"  {t:52s} " + "  ".join(f"p={p}: {ASYNC[(t, p)]['Sleep']:8.1f}" for p in P)
                  + f"   (Serial {t1(ASYNC, t):.1f})")
