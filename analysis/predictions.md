# Predictions (written 2026-10-13, before any benchmark was run)

Machine: i7-10875H, 8 cores / 16 HW threads, turbo off (2.3 GHz). p = number of worker threads.

1. Best scaling: `recursive_fibonacci`. It is compute-bound (recursion only touches registers and the stack),
   each launch has 256 identical tasks (far more than p), and there are only 30 launches, so per-launch overhead
   is negligible. Expect Sp of about 7 at p = 8, and only 10–30% more at p = 16 because the extra threads are
   SMT siblings sharing cores.
2. Worst scaling: `super_super_light`. Each task only copies 512 ints (well under 1 µs of work) and there are
   400 launches, so thread creation (Spawn) or wake-up and synchronization (pools) dominates. Spawn will be
   slower than serial at every p, and will get worse as p grows.
3. `ping_pong_equal` / `ping_pong_unequal` stream 4 MB per launch through the caches. They are bounded by
   memory/L3 bandwidth and will saturate around p = 4–8. Dynamic assignment of 64 tasks will absorb most of the
   linear cost skew in `ping_pong_unequal`.
4. The first p where adding workers improves runtime by less than 10%: p = 16 for compute-bound workloads
   (fibonacci, mandelbrot) because of SMT; about p = 8 for ping_pong; p = 2 (or never better) for
   super_super_light.
5. The spinning pool will beat Spawn by the most on many-launch, fine-grained tests (super_light,
   super_super_light, math_operations_in_tight_for_loop), and be about equal on `mandelbrot_chunked` (a single
   launch) and on `recursive_fibonacci` (heavy tasks).
6. The sleeping pool will be slightly slower than the spinning pool on `super_super_light` (wake-up latency on
   every launch), and faster than it on `spin_between_run_calls` at p = 16 (idle spinners steal SMT and core
   resources from the two busy threads).
7. Part 4, handout example with 4 workers: static contiguous about 20 ms, dynamic next-task about 11 ms. In the
   chunk sweep, chunk = 1 will be the slowest dynamic setting for the pools (lock contention, H/T at least 1),
   and chunk = N/4 (65536) will be about 1.8× slower than the best chunk size because of load imbalance.
8. Part 5 DAG (A -> {B, C} -> D) with my FIFO ready queue: about 116 ms with 2 workers and about 60 ms with
   4 workers. Splitting B into four sub-launches gives no gain at p = 2 or 4 (FIFO runs C last) and a small gain
   at p = 8 and 16 (about 32 -> 30 ms and 20 -> 16 ms).
