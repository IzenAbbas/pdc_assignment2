#!/usr/bin/env bash
# Performance sweep for the report. Each ./runtasks call runs the test for all
# four task systems and reports the MINIMUM of 5 runs (-i 5) for each one.
set -u
ROOT=/home/izen/Desktop/pdc_assignment
SYNC_BIN=$ROOT/23L-0748_Assignment2/part1to4_synchronous/runtasks
ASYNC_BIN=$ROOT/23L-0748_Assignment2/part5_async_deps/runtasks
OUT=$ROOT/analysis/results
WORKERS="1 2 4 8 16"
mkdir -p "$OUT"

# Only one sweep at a time: concurrent sweeps compete for cores and corrupt timings.
exec 9> "$OUT/.bench.lock"
flock -n 9 || { echo "another benchmark sweep is already running"; exit 1; }

# Parts 1-4: supplied sync workloads + Part 4 irregular workloads (part1to4 build).
SYNC_TESTS="super_super_light super_light ping_pong_equal ping_pong_unequal
recursive_fibonacci math_operations_in_tight_for_loop
math_operations_in_tight_for_loop_fewer_tasks math_operations_in_tight_for_loop_fan_in
math_operations_in_tight_for_loop_reduction_tree spin_between_run_calls mandelbrot_chunked
irregular_example_static irregular_example_dynamic irregular_chunk_1 irregular_chunk_16
irregular_chunk_256 irregular_chunk_4096 irregular_chunk_65536"

# Part 5: custom DAG test, plus sync vs async launches of the same workloads (part5 build).
ASYNC_TESTS="abcd_dag_async abcd_dag_split_async
math_operations_in_tight_for_loop_fewer_tasks math_operations_in_tight_for_loop_fewer_tasks_async
math_operations_in_tight_for_loop_fan_in math_operations_in_tight_for_loop_fan_in_async"

sweep() {  # sweep <binary> <outfile> <tests...>
    local bin=$1 file=$2; shift 2
    : > "$file"
    for t in "$@"; do
        for n in $WORKERS; do
            echo "### test=$t n=$n" >> "$file"
            "$bin" -n "$n" -i 5 "$t" >> "$file" 2>&1 || echo "### FAILED test=$t n=$n" >> "$file"
        done
    done
}

date
sweep "$SYNC_BIN" "$OUT/sync_sweep.txt" $SYNC_TESTS
date
sweep "$ASYNC_BIN" "$OUT/async_sweep.txt" $ASYNC_TESTS
date
echo "BENCHMARKS DONE"
