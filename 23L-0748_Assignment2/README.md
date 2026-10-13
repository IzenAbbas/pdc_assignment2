# CS3006 Assignment 2 — Build and Run

## Parts 1–4: Spawn, Spinning pool, Sleeping pool

```
cd part1to4_synchronous
make
./runtasks -n 8 mandelbrot_chunked          # -n: max worker threads, -i: timing iterations
./runtasks -?                               # list all tests
./runtasks -n 8 exactly_once_stress         # custom correctness test (Part 1)
./runtasks -n 4 irregular_example_static    # Part 4 irregular workload, static contiguous
./runtasks -n 4 irregular_example_dynamic   # Part 4 irregular workload, dynamic next-task
./runtasks -n 4 irregular_chunk_256         # Part 4 granularity: irregular_chunk_{1,16,256,4096,65536}
```

## Part 5: asynchronous launches with dependencies

```
cd part5_async_deps
make
./runtasks -n 8 strict_graph_deps_large_async
./runtasks -n 4 abcd_dag_async              # custom dependency test (A -> B, C -> D)
./runtasks -n 4 abcd_dag_split_async        # same graph with B split into four sub-launches
```
