To run Strix on the benchmarks, one first has to compile Strix such that the binary is located at
`../bin/strix`. One also has to install the optional dependencies given in the [build instructions](../doc/BUILDING.md).

Then, one can use the benchmark script as follows:
```
./run_benchmarks.sh --memorylimit MEM_LIMIT_GB --timelimit TIME_LIMIT_SECONDS [OPTIONS] BENCHMARK_DIR
```
For example, to run Strix only for checking realizability on the SYNTCOMP 2019 benchmark,
with the default 32 GB memory limit and 1 hour time limit, use:
```
./run_benchmarks.sh --realizability SYNTCOMP2019
```

If also controllers should be produced and additionally verified within a separate time limit, use:
```
./run_benchmarks.sh --verify SYNTCOMP2019
```
