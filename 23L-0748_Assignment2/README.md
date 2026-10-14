# CS3006 Assignment 2 — Build and Run

## Parts 1–4

```
cd part1to4_synchronous
make
./runtasks -n 8 mandelbrot_chunked
./runtasks -n 8 exactly_once_stress
./runtasks -n 4 irregular_example_static
./runtasks -n 4 irregular_example_dynamic
./runtasks -n 4 irregular_chunk_1
./runtasks -n 4 irregular_chunk_16
./runtasks -n 4 irregular_chunk_256
./runtasks -n 4 irregular_chunk_4096
./runtasks -n 4 irregular_chunk_65536
```

## Part 5

```
cd part5_async_deps
make
./runtasks -n 8 strict_graph_deps_large_async
./runtasks -n 4 abcd_dag_async
./runtasks -n 4 abcd_dag_split_async
```
