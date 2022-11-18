### Analysis

There are no conflicts detected in this job.

1298 file were accessed. 1293 were read-only and 5 were write-only. The write-only files were internal files:
```
/dev/infiniband/rdma_cm
/dev/shm/Intel_MPI_Xanzbb
/dev/shm/Intel_MPI_jxLYN8
/dev/shm/Intel_MPI_kccV33
/dev/shm/Intel_MPI_pRzhXE
```


Commands used:
```
for f in trace*; do echo $f; ../../analysis/strace_to_dxt.py < $f > dxt_trace/$f; done
../../analysis/darshan_dxt_conflicts dxt_trace/trace_*
```
