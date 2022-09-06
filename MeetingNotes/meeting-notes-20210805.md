# Aug 5th, 2021

- IOR  and  mdtest  baseline  of  bbThemis  compared  against DAOS with 1, 2, 4, and 8 servers. The goal is show the theoretical peak performance and show bbThemis is faster than SOTA system.

- application baseline of bbThemis and DAOS. We are expecting the perf gap is negligible as the benchmark performance may not be representative of real app perf.

- benchmark sharing with multiple job sizes: 64 node vs. 64node, 32 node vs. 32 node, 64 node vs. 32 node, 64 node vs.16 node, 64 node vs. 8 node. We should measure both MiB/sand IOPS.
	- add I/O workload in the queues as a metric

- FIFO sharing baseline for Figure 1, ResNet-50 on 4 GPU nodes and NAMD on 64 CPU nodes with bbThemis, DAOS, Lustre. We want to show that NAMD is indefinitely blocked by ResNet-50. We may run this case three times and observe the finishing time of NAMD. Ideally, we should see largest dev of running time of NAMD.

- application sharing with bbThemis. 
	- Case 1: ResNet-50 on 4 GPU nodes vs.NAMD on 64 CPU nodes. 
	- Case 2: typical scale of all eight applications at the same time. We may plot the I/O sharing curve, we are expecting the I/O resources can be fairly shared based on job size when all jobs are initializing (frequently reading libraries). Then record the tokens each app gets, and count the distribution of the tokens among apps. I think some apps won’t be I/O active during computation, and the actual I/O share can go beyond what the tokens define, but the number of tokens should reflect if the system had a change to fair share or not, that is if our system is correctly implemented or not.


- run the application suite using user-fair and job-fair policy.
	- Ed

- Let's finishing coding by Aug 12th.
