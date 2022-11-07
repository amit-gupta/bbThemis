# Sep 8th, 2022

### Configurable Consistency
- lead by Ed
- Configurable Consistency 
- Add AMBER, GROMACS, MILC, SNAP-LAMMPS, OpenFOAM, VPIC
  - May use BenchPRO, https://github.com/TACC/benchpro
  - Ian is running this
  - done by Sep 8th, 2022
    - VPIC, MPI-IO, all stdio
    - SNAP-LAMMPS dummy run done, Matthew runs for us
    - Ian to check with Matthew
- insights: the I/O log analysis on consistency
  - visible after write
  - visible after close
  - visible after job, Montage
  - visible after workflow, ResNet, BERT
- DeltaFS uses benchmark to measure performance
- How to quantify the server side complexity with 
- Expand the application suite, Zhao to reach out to HPC people

### IPDPS Submission Plan
- Zhao to start from Aug 29th, 2022.
- Abstract Due September 29, 2022.
- Paper Due October 6, 2022.
- https://www.overleaf.com/7435642899jrnjwftfvmgb
- Introduction change
- Problem 1, solution 2,
  - Technical limitation

### Mercury to replace ibverbs
- Yuhong is working on this
- https://github.com/mercury-hpc/mercury

### Max-min Fairness
- lead by Ed
- Max-min fairness with proportional allocation and request time adjustment 
- Algorithm Design
  - Ed, Zhao


### Log structure file system on persistent memory
- https://www.usenix.org/conference/osdi21/presentation/koo
- https://www.usenix.org/system/files/osdi18-alagappan.pdf
- https://www.usenix.org/conference/osdi14/technical-sessions/presentation/pillai
- https://github.com/pmem/pmdk
