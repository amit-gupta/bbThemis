### Analysis

There are no conflicts detected in this job.

3579 files are accessed. 3570 are read-only, 7 are write-only, and 2 are read-write.

The write-only files include some internal system files (`/dev/infiniband/rdma_cm`, `/dev/shm/Intel_MPI_0nJRiv`, `/dev/shm/Intel_MPI_MKBrir`, `/dev/shm/Intel_MPI_jFfejp`) and three checkout/output files written by a single rank: `md.gro`, `md_prev.cpt`, `md_step1000.cpt`.

The read/write files were `md.edr` and `md.log`, both of which were accessed by a single rank.


Commands used:
```
for f in trace*; do echo $f; ../../analysis/strace_to_dxt.py < $f > dxt_trace/$f; done
../../analysis/darshan_dxt_conflicts dxt_trace/trace*
../../analysis/darshan_dxt_conflicts -summary -verbose=3 -file=/scratch1/06058/iwang/benchmarks/gromacs/test-gmx/md.edr -file=/scratch1/06058/iwang/benchmarks/gromacs/test-gmx/md.log dxt_trace/trace*
```
