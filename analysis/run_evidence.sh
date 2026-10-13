#!/usr/bin/env bash
# Correctness evidence for the report appendix: full output of every supplied
# test (and the custom tests) for each implementation. A failing test prints
# "ERROR: Results did not pass correctness check!" and exits non-zero.
set -u
ROOT=/home/izen/Desktop/pdc_assignment
SUB=$ROOT/23L-0748_Assignment2
OUT=$ROOT/analysis/results
mkdir -p "$OUT"

P14_TESTS="simple_test_sync super_super_light super_light ping_pong_equal ping_pong_unequal
recursive_fibonacci math_operations_in_tight_for_loop math_operations_in_tight_for_loop_fewer_tasks
math_operations_in_tight_for_loop_fan_in math_operations_in_tight_for_loop_reduction_tree
spin_between_run_calls mandelbrot_chunked
exactly_once_stress irregular_example_dynamic irregular_example_static
irregular_chunk_1 irregular_chunk_16 irregular_chunk_256 irregular_chunk_4096 irregular_chunk_65536"

P5_TESTS="simple_test_async ping_pong_equal_async ping_pong_unequal_async super_light_async
super_super_light_async recursive_fibonacci_async math_operations_in_tight_for_loop_async
math_operations_in_tight_for_loop_fewer_tasks_async math_operations_in_tight_for_loop_fan_in_async
math_operations_in_tight_for_loop_reduction_tree_async mandelbrot_chunked_async
spin_between_run_calls_async strict_diamond_deps_async strict_graph_deps_small_async
strict_graph_deps_med_async strict_graph_deps_large_async
abcd_dag_async abcd_dag_split_async simple_test_sync exactly_once_stress
super_super_light super_light ping_pong_equal ping_pong_unequal recursive_fibonacci
math_operations_in_tight_for_loop math_operations_in_tight_for_loop_fewer_tasks
math_operations_in_tight_for_loop_fan_in math_operations_in_tight_for_loop_reduction_tree
spin_between_run_calls mandelbrot_chunked"

run_all() {  # run_all <dir> <outfile> <tests...>
    local dir=$1 file=$2; shift 2
    : > "$file"
    for t in "$@"; do
        (cd "$dir" && ./runtasks -n 8 -i 3 "$t") >> "$file" 2>&1 \
            || echo "!!! FAILED: $t" >> "$file"
    done
}

date
run_all "$SUB/part1to4_synchronous" "$OUT/evidence_part1to4.txt" $P14_TESTS
run_all "$SUB/part5_async_deps" "$OUT/evidence_part5.txt" $P5_TESTS
# Slow (tasks sleep for 1/5/10 s), so one iteration and 16 workers.
(cd "$SUB/part5_async_deps" && ./runtasks -n 16 -i 1 simple_run_deps_test) \
    > "$OUT/evidence_part5_simple_run_deps.txt" 2>&1 || echo "!!! FAILED" >> "$OUT/evidence_part5_simple_run_deps.txt"
date
# Side-by-side comparison with the supplied reference binary (sync tests).
chmod +x "$SUB/part1to4_synchronous/runtasks_ref_linux"
(cd "$SUB/part1to4_synchronous" && python3 ../tests/run_test_harness.py -n 16) > "$OUT/harness_part1to4.txt" 2>&1
date
echo "EVIDENCE DONE"
