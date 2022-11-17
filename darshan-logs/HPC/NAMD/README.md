Please record the CMD, number of nodes, and configuration for each run along with the Darshan log.

### Analysis

No file access conflicts detected for this application. All IO is done by rank 0.

There aren't many files, so I'll list the detailed output:
```
$ darshan_dxt_conflicts.exe -summary namd2_dxt.log
2331 lines read
File /home1/06333/aroraish/stmv_io/stmv2fsx.namd
  rank 0
    read 0..2017
File /home1/06333/aroraish/stmv_io/stmv-output.coor
  rank 0
    write 0..25599076
File /home1/06333/aroraish/stmv_io/stmv-output.dcd
  rank 0
    write 0..8
    read/write 8..12
    write 12..16
    read/write 16..24
    write 24..1279961876
File /home1/06333/aroraish/stmv_io/stmv-output.restart.coor
  rank 0
    write 0..25599076
File /home1/06333/aroraish/stmv_io/stmv-output.vel
  rank 0
    write 0..25599076
File /home1/06333/aroraish/stmv_io/stmv-output.restart.xsc
  rank 0
    write 0..265
File /home1/06333/aroraish/stmv_io/stmv-output.xsc
  rank 0
    write 0..261
File /home1/06333/aroraish/stmv_io/stmv-output.restart.vel
  rank 0
    write 0..25599076
no-conflicts write-only /home1/06333/aroraish/stmv_io/stmv-output.coor
no-conflicts read-write /home1/06333/aroraish/stmv_io/stmv-output.dcd
no-conflicts write-only /home1/06333/aroraish/stmv_io/stmv-output.restart.coor
no-conflicts write-only /home1/06333/aroraish/stmv_io/stmv-output.restart.vel
no-conflicts write-only /home1/06333/aroraish/stmv_io/stmv-output.restart.xsc
no-conflicts write-only /home1/06333/aroraish/stmv_io/stmv-output.vel
no-conflicts write-only /home1/06333/aroraish/stmv_io/stmv-output.xsc
no-conflicts read-only /home1/06333/aroraish/stmv_io/stmv2fsx.namd
```
