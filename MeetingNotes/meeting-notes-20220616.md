# June 16th, 2022

### Cluster submission
- July 4, 2022


### Configurable Consistency
- lead by Ed
- Configurable Consistency 
- insights: the I/O log analysis on consistency
  - visible after write
  - visible after job, Montage
  - visible after workflow, ResNet, BERT
- DeltaFS uses benchmark to measure performance
- How to quantify the server side complexity with 
- Expand the application suite, Zhao to reach out to HPC people

### Max-min Fairness
- lead by Ed
- Max-min fairness with proportional allocation and request time adjustment 
- Algorithm Design
  - Ed, Zhao
- Replace communication layer with MPI
  - Intel MPI does not work with multi-thread, the connection accepting thread blocks all other threads
  - A deadend with the current MPI implmentation, Reach out to DK (Zhao) and Tony (Dan, Bill)
  - libfabric -- Ed

### Log structure file system on persistent memory
- https://www.usenix.org/conference/osdi21/presentation/koo
- https://www.usenix.org/system/files/osdi18-alagappan.pdf
- https://www.usenix.org/conference/osdi14/technical-sessions/presentation/pillai
- https://github.com/pmem/pmdk
