### Analysis

There are no conflicts detected in this job.

The files trace_x_yy appear to from the process running on node x as node-rank yy.
Most ranks just call getrusage(), uname(), sched_yield(), and poll() many times.

153 files were accessed. 147 were read-only and 6 files were written by a single process:
```
/dev/shm/Intel_MPI_8xmWzE
/dev/shm/Intel_MPI_aSF8tC
/work2/06058/iwang/stampede2/benchmarks/amber/mdcrd
/work2/06058/iwang/stampede2/benchmarks/amber/mdinfo
/work2/06058/iwang/stampede2/benchmarks/amber/mdout.CPU
/work2/06058/iwang/stampede2/benchmarks/amber/restrt
```

Commands used:
```
for f in trace*; do ../../analysis/strace_to_dxt.py < $f > dxt_trace/$f; echo $f; done

../../analysis/darshan_dxt_conflicts dxt_trace/trace*
```
