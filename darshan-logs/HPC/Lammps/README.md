### Analysis

The IO for this job was quite simple, and had no conflicts. Rank 0 wrote to log.lammps,
and every rank wrote to stdout.

```
accesses        filename        rank_detail
SINGLE_WO       /scratch1/06280/mcawood/benchpro/results/pending/mcawood_ljmelt_2022-09-02T16-24_001N_56R_01T_00G/log.lammps   readers=none,writers=0
MULTIPLE_RO     <STDIN> readers=all,writers=none
MULTIPLE_WO     <STDOUT>        readers=none,writers=all
```

Commands used:
```
darshan-parser mcawood_lmp_intel_cpu_intelmpi_id4790397_9-2-66022-5765705011183997150_7511551.darshan > lammps.txt
darshan_file_access_summary.py < lammps.txt
```

