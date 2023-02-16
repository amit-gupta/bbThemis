# Bulk Lustre loads / stores

This is an experiment to see how to read and write many files from Lustre as efficiently as possible.
Most applications either use a single process to access everything, or use all processes to access
data, with no regard to the Lustre OST on which the data resides. Lustre tends to introduce locking
when many processes access data on the same OST, so this experiment will organize accesses such
that the data on a given OST is only accessed by processes running on a single node.

Small files and large files have different performance effects. Small files usually reside
on a single OST, but the OST is chosen randomly for each file. So with a large collection of
small files, many different OSTs will need to be contacted to access all the files.

Large files can have their stripe count set to a value greater than one, spreading their data
across multiple OSTs. Most applications are unaware of this and simply access the entire file
using a single process. However, these files can be accessed in parallel, with the throughput
scaling nearly linearly up to the stripe count. To do this, the data needs to be accessed
from mulitple process (and when writing, from multiple nodes) concurrently.

This code begins by querying the metadata for all input files, and splitting the subranges of
each file based on the OST on which the data resides. It then distributes those subranges across
the processes of the job, such that the data for each OST is assigned only to processes
on a single node. This is the avoid contention, which hurts performance when writing.

It then does multiple rounds of tests accessing the data in three ways:

1. Single process. A single process accesses all the data.
2. Every process. Every process accesses data, with no overlap, but the data assignments are random, so each OST will serve requests from many processes and nodes.
3. Aligned accesses. IO assignments are aligned with stripe parameters so each OST only serves requests from processes on a single node.

## Initial Results

Only reading is implemented so far, which means contention is not an issue. The only significant improvement
over naive data access is that large striped files are accessed in parallel.

All testing was done on Lonestar $WORK filesystem, as Frontera was down for maintenance at the time I ran the tests,
and Lonestar's $SCRATCH filesystem is BeeGFS, not Lustre. Optimizing access patterns for BeeGFS may also be a useful project.

Reading many small files, 4 nodes, 4 ranks per node:

```
[0] 0.000034 Scanning files...
[0] 3.346354 11100 files scanned in 3.346315s
11639193600 total bytes
Test 0
  single reader: 6.250172s, 1775.951 mb/s
  all ranks read: 2.364061s, 4695.309 mb/s
  aligned readers: 0.759267s, 14619.369 mb/s
Test 1
  single reader: 6.238047s, 1779.403 mb/s
  all ranks read: 0.845897s, 13122.171 mb/s
  aligned readers: 0.743323s, 14932.948 mb/s
Test 2
  single reader: 6.156132s, 1803.080 mb/s
  all ranks read: 0.846447s, 13113.634 mb/s
  aligned readers: 0.744944s, 14900.443 mb/s
Test 3
  single reader: 6.161501s, 1801.509 mb/s
  all ranks read: 0.869692s, 12763.136 mb/s
  aligned readers: 0.746732s, 14864.781 mb/s
Test 4
  single reader: 6.148660s, 1805.271 mb/s
  all ranks read: 0.868102s, 12786.519 mb/s
  aligned readers: 0.736982s, 15061.433 mb/s
```
After the initial access, the data is cached and the performance is far better than with a single reader, 
but not much better than with accesses being assigned randomly to all ranks.

Reading one large file, 4 nodes, 4 ranks per node:

```
[0] 0.000037 Scanning files...
[0] 0.002293 1 files scanned in 0.002250s
total 21474836480 bytes, or 20.000 GB
Test 0
  single reader: 1.918864s, 10672.980 mb/s
  all ranks read: 1.933847s, 10590.290 mb/s
  aligned readers: 0.228225s, 89736.013 mb/s
Test 1
  single reader: 1.938495s, 10564.896 mb/s
  all ranks read: 1.945148s, 10528.764 mb/s
  aligned readers: 0.189084s, 108311.433 mb/s
Test 2
  single reader: 1.929736s, 10612.849 mb/s
  all ranks read: 1.939355s, 10560.213 mb/s
  aligned readers: 0.187872s, 109010.337 mb/s
```

The throughput is far better because the naive methods read the file serially, while the aligned access method reads the file in parallel.
I'm pretty sure the measured throughput exceeds the capability of the interconnect hardware, so the data must be cached locally.

# Write mode

I ran a series of tests on Frontera and its $SCRATCH file system on a bunch of small files and a few large files.
With large files, aligned accesses consistently performed better than single-process or all-ranks methods.
With many small files, aligned access and all-ranks performed much better than single process for the first pass,
but after that all the data appeared to be cached, and performance was very high for all methods.

The output from the tests runs are in the file `bulk_lustre_read.2023-02-10.results`

## Many files - 10000 files, each 1 MB, stripe count 1

Each test was run three times on 16 nodes with 4 processes per node.
The results below are the fastest of the three runs.

```
                       Throughput in MB/s
  Single writer           1310
  All ranks writing       9925
  Aligned writes         18329
```

## One 100 GB file, stripe count 16

```
                       Throughput in MB/s
  Single writer           1054
  All ranks writing        962
  Aligned writes          5693
```

## TODO

- Run some experiments to choose good striping parameters as a function of the file size. Are there different optimal values for reading and writing?
- Try out these strategies on a BeeGFS file system

